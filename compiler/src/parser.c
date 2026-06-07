/*
 * URUS Parser v0.0.1
 *
 * Recursive-descent for declarations + statements; Pratt for expressions.
 * Designed to recover at item boundaries — a syntax error in one fn doesn't
 * silence the rest of the file.
 */
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- token plumbing ---------- */

static void advance(Parser *p) {
    p->cur  = p->peek;
    p->peek = lexer_next(&p->lx);
}

static bool check(Parser *p, TokKind k) { return p->cur.kind == k; }

static bool match(Parser *p, TokKind k) {
    if (check(p, k)) { advance(p); return true; }
    return false;
}

static void expect(Parser *p, TokKind k, const char *what) {
    if (check(p, k)) { advance(p); return; }
    diag_error(p->diag, p->cur.loc,
               "expected %s, found '%s'", what, tok_kind_name(p->cur.kind));
    p->panic_mode = true;
}

/* recover by skipping until likely sync point (item start) */
static void sync_to_item(Parser *p) {
    while (!check(p, TOK_EOF)) {
        switch (p->cur.kind) {
            case TOK_KW_FN:
            case TOK_KW_STRUCT:
            case TOK_KW_ENUM:
            case TOK_KW_IMPL:
            case TOK_KW_USE:
            case TOK_KW_CONST:
            case TOK_KW_TYPE:
            case TOK_KW_PUB:
            case TOK_KW_MODULE:
                p->panic_mode = false;
                return;
            default:
                advance(p);
        }
    }
    p->panic_mode = false;
}

static const char *tok_text(Parser *p, Token t) {
    return arena_strndup(p->arena, t.start, t.length);
}

/* ---------- forward decls ---------- */

static TypeExpr *parse_type(Parser *p);
static Expr     *parse_expr(Parser *p);
static Expr     *parse_block(Parser *p);
static Stmt     *parse_stmt(Parser *p);
static Item     *parse_item(Parser *p);
static Pattern  *parse_pattern(Parser *p);

void parser_init(Parser *p, const char *source, const char *file,
                 DiagCtx *diag, Arena *arena) {
    lexer_init(&p->lx, source, file, diag, arena);
    p->diag           = diag;
    p->arena          = arena;
    p->panic_mode     = false;
    p->depth          = 0;
    p->depth_exceeded = false;
    p->cur  = lexer_next(&p->lx);
    p->peek = lexer_next(&p->lx);
}

/*
 * Recursion guard: every parse_type / parse_expr_prec / parse_pattern /
 * parse_block entry calls enter_recursion() and checks the return value.
 * If the depth cap is exceeded we set panic_mode and emit one fatal diag,
 * so the caller can bail without recursing further.  Closes F-COMP-1.
 */
static bool enter_recursion(Parser *p) {
    if (p->depth >= URUS_MAX_PARSE_DEPTH) {
        if (!p->depth_exceeded) {
            diag_error(p->diag, p->cur.loc,
                       "syntactic nesting exceeded %d levels — refusing to parse",
                       URUS_MAX_PARSE_DEPTH);
            p->depth_exceeded = true;
            p->panic_mode     = true;
        }
        return false;
    }
    p->depth++;
    return true;
}

static URUS_INLINE void leave_recursion(Parser *p) {
    if (p->depth > 0) p->depth--;
}

/* ---------- types ---------- */

static TypeExpr *parse_type_inner(Parser *p);

/*
 * Wrapper that manages the recursion guard around parse_type_inner.
 * We guard parse_type (not parse_type_inner) so the recursive `parse_type`
 * calls inside *do* increment the depth counter and trip the cap.
 */
static TypeExpr *parse_type(Parser *p) {
    SrcLoc loc = p->cur.loc;
    if (!enter_recursion(p)) {
        /* depth cap tripped — return harmless filler and let the caller
         * notice panic_mode.  Skip consuming any more tokens. */
        return ast_new_type(p->arena, TY_NAMED, loc);
    }
    TypeExpr *t = parse_type_inner(p);
    leave_recursion(p);
    return t;
}

static TypeExpr *parse_type_inner(Parser *p) {
    SrcLoc loc = p->cur.loc;

    if (match(p, TOK_STAR)) {
        TypeExpr *t = ast_new_type(p->arena, TY_POINTER, loc);
        t->pointer.is_mut = match(p, TOK_KW_MUT);
        t->pointer.inner  = parse_type(p);
        return t;
    }
    if (match(p, TOK_AMP)) {
        TypeExpr *t = ast_new_type(p->arena, TY_REF, loc);
        t->ref.is_mut = match(p, TOK_KW_MUT);
        t->ref.inner  = parse_type(p);
        return t;
    }
    if (match(p, TOK_LBRACKET)) {
        TypeExpr *elem = parse_type(p);
        if (match(p, TOK_SEMI)) {
            TypeExpr *t = ast_new_type(p->arena, TY_ARRAY, loc);
            t->array.elem = elem;
            t->array.size = parse_expr(p);
            expect(p, TOK_RBRACKET, "]");
            return t;
        }
        expect(p, TOK_RBRACKET, "]");
        TypeExpr *t = ast_new_type(p->arena, TY_SLICE, loc);
        t->slice.elem = elem;
        return t;
    }
    if (match(p, TOK_LPAREN)) {
        if (match(p, TOK_RPAREN)) return ast_new_type(p->arena, TY_UNIT, loc);
        TypeExpr **buf = NULL;
        int n = 0, cap = 0;
        for (;;) {
            TypeExpr *t = parse_type(p);
            if (n >= cap) { cap = cap ? cap * 2 : 4; buf = (TypeExpr **)arena_grow(p->arena, buf, n * sizeof(*buf), cap * sizeof(*buf)); }
            buf[n++] = t;
            if (!match(p, TOK_COMMA)) break;
            if (check(p, TOK_RPAREN)) break;
        }
        expect(p, TOK_RPAREN, ")");
        if (n == 1) {
            TypeExpr *only = buf[0];
            /* scratch is arena-backed now — dies with the arena */
            return only;
        }
        TypeExpr *t = ast_new_type(p->arena, TY_TUPLE, loc);
        t->tuple.elems = (TypeExpr **)arena_alloc(p->arena, n * sizeof(TypeExpr *));
        urus_memcpy(t->tuple.elems, buf, n * sizeof(TypeExpr *));
        t->tuple.n = n;
        /* scratch is arena-backed now — dies with the arena */
        return t;
    }
    if (match(p, TOK_KW_SELF_TYPE)) {
        return ast_new_type(p->arena, TY_SELF, loc);
    }

    /* named or generic */
    if (check(p, TOK_IDENT)) {
        const char *name = tok_text(p, p->cur);
        advance(p);
        if (match(p, TOK_LT)) {
            TypeExpr *t = ast_new_type(p->arena, TY_GENERIC, loc);
            t->generic.name = name;
            TypeExpr **buf = NULL;
            int n = 0, cap = 0;
            for (;;) {
                TypeExpr *a = parse_type(p);
                if (n >= cap) { cap = cap ? cap * 2 : 4; buf = (TypeExpr **)arena_grow(p->arena, buf, n * sizeof(*buf), cap * sizeof(*buf)); }
                buf[n++] = a;
                if (!match(p, TOK_COMMA)) break;
                if (check(p, TOK_GT)) break;
            }
            expect(p, TOK_GT, ">");
            t->generic.args = (TypeExpr **)arena_alloc(p->arena, n * sizeof(TypeExpr *));
            urus_memcpy(t->generic.args, buf, n * sizeof(TypeExpr *));
            t->generic.n_args = n;
            /* scratch is arena-backed now — dies with the arena */
            return t;
        }
        TypeExpr *t = ast_new_type(p->arena, TY_NAMED, loc);
        t->named.name = name;
        return t;
    }

    diag_error(p->diag, p->cur.loc, "expected type, found '%s'", tok_kind_name(p->cur.kind));
    advance(p);
    return ast_new_type(p->arena, TY_INFER, loc);
}

/* ---------- patterns ---------- */

static Pattern *parse_pattern_inner(Parser *p);

static Pattern *parse_pattern(Parser *p) {
    SrcLoc loc = p->cur.loc;
    if (!enter_recursion(p)) {
        return ast_new_pat(p->arena, PAT_WILDCARD, loc);
    }
    Pattern *r = parse_pattern_inner(p);
    leave_recursion(p);
    return r;
}

static Pattern *parse_pattern_inner(Parser *p) {
    SrcLoc loc = p->cur.loc;

    if (check(p, TOK_IDENT)) {
        const char *id = arena_strndup(p->arena, p->cur.start, p->cur.length);
        advance(p);

        if (strcmp(id, "_") == 0) {
            return ast_new_pat(p->arena, PAT_WILDCARD, loc);
        }

        /* Variant: Name(sub, ...) or Name */
        if (match(p, TOK_LPAREN)) {
            Pattern *pat = ast_new_pat(p->arena, PAT_ENUM_VARIANT, loc);
            pat->variant.name = id;
            Pattern **buf = NULL;
            int n = 0, cap = 0;
            if (!check(p, TOK_RPAREN)) {
                for (;;) {
                    Pattern *sub = parse_pattern(p);
                    if (n >= cap) { cap = cap ? cap * 2 : 4; buf = (Pattern **)arena_grow(p->arena, buf, n * sizeof(*buf), cap * sizeof(*buf)); }
                    buf[n++] = sub;
                    if (!match(p, TOK_COMMA)) break;
                    if (check(p, TOK_RPAREN)) break;
                }
            }
            expect(p, TOK_RPAREN, ")");
            pat->variant.subs = (Pattern **)arena_alloc(p->arena, n * sizeof(Pattern *));
            urus_memcpy(pat->variant.subs, buf, n * sizeof(Pattern *));
            pat->variant.n = n;
            /* scratch is arena-backed now — dies with the arena */
            return pat;
        }

        /* Treat capitalized identifier as nullary variant (None, Self, etc.) */
        if (id[0] >= 'A' && id[0] <= 'Z') {
            Pattern *pat = ast_new_pat(p->arena, PAT_ENUM_VARIANT, loc);
            pat->variant.name = id;
            pat->variant.subs = NULL;
            pat->variant.n    = 0;
            return pat;
        }

        Pattern *pat = ast_new_pat(p->arena, PAT_IDENT, loc);
        pat->ident.name   = id;
        pat->ident.is_mut = false;
        return pat;
    }

    /* literal patterns */
    if (check(p, TOK_INT) || check(p, TOK_FLOAT) || check(p, TOK_STR) || check(p, TOK_CHAR) ||
        check(p, TOK_KW_TRUE) || check(p, TOK_KW_FALSE)) {
        Pattern *pat = ast_new_pat(p->arena, PAT_LITERAL, loc);
        pat->literal = parse_expr(p);
        return pat;
    }

    diag_error(p->diag, p->cur.loc, "expected pattern, found '%s'", tok_kind_name(p->cur.kind));
    advance(p);
    return ast_new_pat(p->arena, PAT_WILDCARD, loc);
}

/* ---------- Pratt expression parser ---------- */

typedef enum {
    PREC_NONE = 0,
    PREC_ASSIGN,        /* = += -= etc. */
    PREC_RANGE,         /* .. ..= */
    PREC_OR,            /* || */
    PREC_AND,           /* && */
    PREC_BIT_OR,        /* | */
    PREC_BIT_XOR,       /* ^ */
    PREC_BIT_AND,       /* & */
    PREC_EQ,            /* == != */
    PREC_CMP,           /* < <= > >= */
    PREC_SHIFT,         /* << >> */
    PREC_ADD,           /* + - */
    PREC_MUL,           /* * / % */
    PREC_CAST,          /* as */
    PREC_UNARY,         /* unary ops */
    PREC_CALL,          /* () . [] */
    PREC_PRIMARY,
} Precedence;

static Precedence infix_prec(TokKind k) {
    switch (k) {
        case TOK_ASSIGN: case TOK_PLUSEQ: case TOK_MINUSEQ:
        case TOK_STAREQ: case TOK_SLASHEQ: case TOK_PERCENTEQ:
            return PREC_ASSIGN;
        case TOK_DOTDOT: case TOK_DOTDOTEQ:        return PREC_RANGE;
        case TOK_PIPEPIPE:                          return PREC_OR;
        case TOK_AMPAMP:                            return PREC_AND;
        case TOK_PIPE:                              return PREC_BIT_OR;
        case TOK_CARET:                             return PREC_BIT_XOR;
        case TOK_AMP:                               return PREC_BIT_AND;
        case TOK_EQEQ: case TOK_NEQ:                return PREC_EQ;
        case TOK_LT: case TOK_LE: case TOK_GT: case TOK_GE: return PREC_CMP;
        case TOK_SHL: case TOK_SHR:                 return PREC_SHIFT;
        case TOK_PLUS: case TOK_MINUS:              return PREC_ADD;
        case TOK_STAR: case TOK_SLASH: case TOK_PERCENT: return PREC_MUL;
        case TOK_KW_AS:                             return PREC_CAST;
        case TOK_LPAREN: case TOK_DOT: case TOK_LBRACKET: case TOK_QUESTION: return PREC_CALL;
        default: return PREC_NONE;
    }
}

static BinOp tok_to_binop(TokKind k) {
    switch (k) {
        case TOK_PLUS:    return BIN_ADD;
        case TOK_MINUS:   return BIN_SUB;
        case TOK_STAR:    return BIN_MUL;
        case TOK_SLASH:   return BIN_DIV;
        case TOK_PERCENT: return BIN_MOD;
        case TOK_EQEQ:    return BIN_EQ;
        case TOK_NEQ:     return BIN_NEQ;
        case TOK_LT:      return BIN_LT;
        case TOK_LE:      return BIN_LE;
        case TOK_GT:      return BIN_GT;
        case TOK_GE:      return BIN_GE;
        case TOK_AMPAMP:  return BIN_AND;
        case TOK_PIPEPIPE:return BIN_OR;
        case TOK_AMP:     return BIN_BIT_AND;
        case TOK_PIPE:    return BIN_BIT_OR;
        case TOK_CARET:   return BIN_BIT_XOR;
        case TOK_SHL:     return BIN_SHL;
        case TOK_SHR:     return BIN_SHR;
        default:          return BIN_ADD;  /* unreachable */
    }
}

static AssignOp tok_to_assign(TokKind k) {
    switch (k) {
        case TOK_ASSIGN:    return ASSIGN_EQ;
        case TOK_PLUSEQ:    return ASSIGN_ADD;
        case TOK_MINUSEQ:   return ASSIGN_SUB;
        case TOK_STAREQ:    return ASSIGN_MUL;
        case TOK_SLASHEQ:   return ASSIGN_DIV;
        case TOK_PERCENTEQ: return ASSIGN_MOD;
        default:            return ASSIGN_EQ;
    }
}

static Expr *parse_unary(Parser *p);
static Expr *parse_primary(Parser *p);

static Expr *parse_call_args(Parser *p, Expr *callee, SrcLoc loc) {
    expect(p, TOK_LPAREN, "(");
    Expr **args = NULL;
    int n = 0, cap = 0;
    if (!check(p, TOK_RPAREN)) {
        for (;;) {
            Expr *a = parse_expr(p);
            if (n >= cap) { cap = cap ? cap * 2 : 4; args = (Expr **)arena_grow(p->arena, args, n * sizeof(*args), cap * sizeof(*args)); }
            args[n++] = a;
            if (!match(p, TOK_COMMA)) break;
            if (check(p, TOK_RPAREN)) break;
        }
    }
    expect(p, TOK_RPAREN, ")");
    Expr *e = ast_new_expr(p->arena, EX_CALL, loc);
    e->call.callee = callee;
    e->call.args   = (Expr **)arena_alloc(p->arena, n * sizeof(Expr *));
    urus_memcpy(e->call.args, args, n * sizeof(Expr *));
    e->call.n_args = n;
    /* scratch is arena-backed now — dies with the arena */
    return e;
}

static Expr *parse_struct_lit(Parser *p, const char *name, SrcLoc loc) {
    expect(p, TOK_LBRACE, "{");
    StructFieldInit *fields = NULL;
    int n = 0, cap = 0;
    if (!check(p, TOK_RBRACE)) {
        for (;;) {
            if (!check(p, TOK_IDENT)) {
                diag_error(p->diag, p->cur.loc, "expected field name");
                break;
            }
            const char *fname = arena_strndup(p->arena, p->cur.start, p->cur.length);
            SrcLoc floc = p->cur.loc;
            advance(p);
            Expr *value = NULL;
            if (match(p, TOK_COLON)) {
                value = parse_expr(p);
            } else {
                /* shorthand `name` → `name: name` */
                value = ast_new_expr(p->arena, EX_IDENT, floc);
                value->ident = fname;
            }
            if (n >= cap) { cap = cap ? cap * 2 : 4; fields = (StructFieldInit *)arena_grow(p->arena, fields, n * sizeof(*fields), cap * sizeof(*fields)); }
            fields[n].name  = fname;
            fields[n].value = value;
            n++;
            if (!match(p, TOK_COMMA)) break;
            if (check(p, TOK_RBRACE)) break;
        }
    }
    expect(p, TOK_RBRACE, "}");
    Expr *e = ast_new_expr(p->arena, EX_STRUCT_LIT, loc);
    e->struct_lit.name     = name;
    e->struct_lit.fields   = (StructFieldInit *)arena_alloc(p->arena, n * sizeof(StructFieldInit));
    urus_memcpy(e->struct_lit.fields, fields, n * sizeof(StructFieldInit));
    e->struct_lit.n_fields = n;
    /* scratch is arena-backed now — dies with the arena */
    return e;
}

static Expr *parse_primary(Parser *p) {
    SrcLoc loc = p->cur.loc;
    Token  t   = p->cur;

    switch (t.kind) {
        case TOK_INT: {
            advance(p);
            Expr *e = ast_new_expr(p->arena, EX_INT_LIT, loc);
            e->int_lit = t.v.int_val;
            return e;
        }
        case TOK_FLOAT: {
            advance(p);
            Expr *e = ast_new_expr(p->arena, EX_FLOAT_LIT, loc);
            e->float_lit = t.v.float_val;
            return e;
        }
        case TOK_STR: {
            advance(p);
            Expr *e = ast_new_expr(p->arena, EX_STR_LIT, loc);
            e->str_lit.ptr = t.v.str_val.ptr;
            e->str_lit.len = t.v.str_val.len;
            return e;
        }
        case TOK_FSTR: {
            advance(p);
            Expr *e = ast_new_expr(p->arena, EX_FSTR_LIT, loc);
            e->str_lit.ptr = t.v.str_val.ptr;
            e->str_lit.len = t.v.str_val.len;
            return e;
        }
        case TOK_CHAR: {
            advance(p);
            Expr *e = ast_new_expr(p->arena, EX_CHAR_LIT, loc);
            e->char_lit = t.v.char_val;
            return e;
        }
        case TOK_KW_TRUE:  advance(p); { Expr *e = ast_new_expr(p->arena, EX_BOOL_LIT, loc); e->bool_lit = true;  return e; }
        case TOK_KW_FALSE: advance(p); { Expr *e = ast_new_expr(p->arena, EX_BOOL_LIT, loc); e->bool_lit = false; return e; }

        case TOK_KW_SELF: {
            advance(p);
            Expr *e = ast_new_expr(p->arena, EX_IDENT, loc);
            e->ident = arena_strdup(p->arena, "self");
            return e;
        }

        case TOK_KW_IF: {
            advance(p);
            Expr *cond = parse_expr(p);
            Expr *thenb = parse_block(p);
            Expr *elseb = NULL;
            if (match(p, TOK_KW_ELSE)) {
                if (check(p, TOK_KW_IF)) elseb = parse_primary(p);
                else                     elseb = parse_block(p);
            }
            Expr *e = ast_new_expr(p->arena, EX_IF, loc);
            e->if_.cond     = cond;
            e->if_.then_blk = thenb;
            e->if_.else_blk = elseb;
            return e;
        }

        case TOK_KW_WHILE: {
            advance(p);
            /* `while let PAT = EXPR { BODY }` (b024) — desugars in the
             * parser to:
             *     loop { match EXPR { PAT => BODY, _ => break } }
             * No new AST node: sema and codegen see the desugared form, so
             * exhaustiveness (F-COMP-3) and binding rules apply for free. */
            if (check(p, TOK_KW_LET)) {
                advance(p);
                Pattern *pat = parse_pattern(p);
                expect(p, TOK_ASSIGN, "=");
                Expr *scrut = parse_expr(p);
                Expr *body  = parse_block(p);

                Expr *brk = ast_new_expr(p->arena, EX_BREAK, loc);
                brk->break_.label = NULL;
                brk->break_.value = NULL;

                Pattern *wild = ast_new_pat(p->arena, PAT_WILDCARD, loc);

                Expr *mtch = ast_new_expr(p->arena, EX_MATCH, loc);
                mtch->match_.scrut  = scrut;
                mtch->match_.n_arms = 2;
                mtch->match_.arms   = (MatchArm *)arena_alloc(
                                          p->arena, sizeof(MatchArm) * 2);
                mtch->match_.arms[0].pat   = pat;
                mtch->match_.arms[0].guard = NULL;
                mtch->match_.arms[0].body  = body;
                mtch->match_.arms[1].pat   = wild;
                mtch->match_.arms[1].guard = NULL;
                mtch->match_.arms[1].body  = brk;

                Expr *lp = ast_new_expr(p->arena, EX_LOOP, loc);
                lp->loop_.body = mtch;
                return lp;
            }
            Expr *cond = parse_expr(p);
            Expr *body = parse_block(p);
            Expr *e = ast_new_expr(p->arena, EX_WHILE, loc);
            e->while_.cond = cond;
            e->while_.body = body;
            return e;
        }

        case TOK_KW_LOOP: {
            advance(p);
            Expr *body = parse_block(p);
            Expr *e = ast_new_expr(p->arena, EX_LOOP, loc);
            e->loop_.body = body;
            return e;
        }

        case TOK_KW_FOR: {
            advance(p);
            Pattern *pat = parse_pattern(p);
            expect(p, TOK_KW_IN, "in");
            Expr *iter = parse_expr(p);
            Expr *body = parse_block(p);
            Expr *e = ast_new_expr(p->arena, EX_FOR, loc);
            e->for_.pat  = pat;
            e->for_.iter = iter;
            e->for_.body = body;
            return e;
        }

        case TOK_KW_MATCH: {
            advance(p);
            Expr *scrut = parse_expr(p);
            expect(p, TOK_LBRACE, "{");
            MatchArm *arms = NULL;
            int n = 0, cap = 0;
            while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
                Pattern *pat = parse_pattern(p);
                expect(p, TOK_FATARROW, "=>");
                Expr *body = parse_expr(p);
                if (n >= cap) { cap = cap ? cap * 2 : 4; arms = (MatchArm *)arena_grow(p->arena, arms, n * sizeof(*arms), cap * sizeof(*arms)); }
                arms[n].pat   = pat;
                arms[n].guard = NULL;
                arms[n].body  = body;
                n++;
                /* Rust-style separators: comma required after expression
                   bodies, optional after block-like bodies (`{}`, if,
                   match, loops). */
                if (match(p, TOK_COMMA)) continue;
                bool block_like = body && (body->kind == EX_BLOCK ||
                                           body->kind == EX_IF    ||
                                           body->kind == EX_MATCH ||
                                           body->kind == EX_WHILE ||
                                           body->kind == EX_FOR   ||
                                           body->kind == EX_LOOP);
                if (!block_like) break;
            }
            expect(p, TOK_RBRACE, "}");
            Expr *e = ast_new_expr(p->arena, EX_MATCH, loc);
            e->match_.scrut  = scrut;
            e->match_.arms   = (MatchArm *)arena_alloc(p->arena, n * sizeof(MatchArm));
            urus_memcpy(e->match_.arms, arms, n * sizeof(MatchArm));
            e->match_.n_arms = n;
            /* scratch is arena-backed now — dies with the arena */
            return e;
        }

        case TOK_KW_RETURN: {
            advance(p);
            Expr *e = ast_new_expr(p->arena, EX_RETURN, loc);
            if (!check(p, TOK_SEMI) && !check(p, TOK_RBRACE)) {
                e->return_.value = parse_expr(p);
            } else {
                e->return_.value = NULL;
            }
            return e;
        }

        case TOK_KW_BREAK:    advance(p); { Expr *e = ast_new_expr(p->arena, EX_BREAK, loc); e->break_.label = NULL; e->break_.value = NULL; return e; }
        case TOK_KW_CONTINUE: advance(p); return ast_new_expr(p->arena, EX_CONTINUE, loc);

        case TOK_LBRACE: return parse_block(p);

        case TOK_LPAREN: {
            advance(p);
            if (match(p, TOK_RPAREN)) {
                Expr *e = ast_new_expr(p->arena, EX_UNIT, loc);
                return e;
            }
            Expr *first = parse_expr(p);
            if (match(p, TOK_COMMA)) {
                Expr **buf = NULL;
                int n = 1, cap = 4;
                buf = (Expr **)arena_alloc(p->arena, cap * sizeof(*buf));
                buf[0] = first;
                if (!check(p, TOK_RPAREN)) {
                    for (;;) {
                        Expr *x = parse_expr(p);
                        if (n >= cap) { cap *= 2; buf = (Expr **)arena_grow(p->arena, buf, n * sizeof(*buf), cap * sizeof(*buf)); }
                        buf[n++] = x;
                        if (!match(p, TOK_COMMA)) break;
                        if (check(p, TOK_RPAREN)) break;
                    }
                }
                expect(p, TOK_RPAREN, ")");
                Expr *e = ast_new_expr(p->arena, EX_TUPLE_LIT, loc);
                e->tuple_lit.elems = (Expr **)arena_alloc(p->arena, n * sizeof(Expr *));
                urus_memcpy(e->tuple_lit.elems, buf, n * sizeof(Expr *));
                e->tuple_lit.n = n;
                /* scratch is arena-backed now — dies with the arena */
                return e;
            }
            expect(p, TOK_RPAREN, ")");
            return first;
        }

        case TOK_LBRACKET: {
            advance(p);
            Expr **buf = NULL;
            int n = 0, cap = 0;
            if (!check(p, TOK_RBRACKET)) {
                for (;;) {
                    Expr *x = parse_expr(p);
                    if (n >= cap) { cap = cap ? cap * 2 : 4; buf = (Expr **)arena_grow(p->arena, buf, n * sizeof(*buf), cap * sizeof(*buf)); }
                    buf[n++] = x;
                    if (!match(p, TOK_COMMA)) break;
                    if (check(p, TOK_RBRACKET)) break;
                }
            }
            expect(p, TOK_RBRACKET, "]");
            Expr *e = ast_new_expr(p->arena, EX_ARRAY_LIT, loc);
            e->array_lit.elems = (Expr **)arena_alloc(p->arena, n * sizeof(Expr *));
            urus_memcpy(e->array_lit.elems, buf, n * sizeof(Expr *));
            e->array_lit.n = n;
            /* scratch is arena-backed now — dies with the arena */
            return e;
        }

        case TOK_IDENT: {
            const char *name = arena_strndup(p->arena, t.start, t.length);
            advance(p);

            /* path :: chain */
            if (check(p, TOK_COLONCOLON)) {
                const char **segs = NULL;
                int n = 0, cap = 0;
                if (n >= cap) { cap = 4; segs = (const char **)arena_alloc(p->arena, cap * sizeof(*segs)); }
                segs[n++] = name;
                while (match(p, TOK_COLONCOLON)) {
                    if (!check(p, TOK_IDENT)) {
                        diag_error(p->diag, p->cur.loc, "expected identifier after '::'");
                        break;
                    }
                    const char *s = arena_strndup(p->arena, p->cur.start, p->cur.length);
                    advance(p);
                    if (n >= cap) { cap *= 2; segs = (const char **)arena_grow(p->arena, segs, n * sizeof(*segs), cap * sizeof(*segs)); }
                    segs[n++] = s;
                }
                Expr *e = ast_new_expr(p->arena, EX_PATH, loc);
                e->path.segs = (const char **)arena_alloc(p->arena, n * sizeof(char *));
                urus_memcpy(e->path.segs, segs, n * sizeof(char *));
                e->path.n = n;
                /* scratch is arena-backed now — dies with the arena */
                return e;
            }

            /* struct literal: only triggers if the identifier starts uppercase
               and is immediately followed by `{`. Lowercase idents followed by
               `{` are blocks in a statement context — we err on the side of
               not eating them. */
            if (check(p, TOK_LBRACE) && name[0] >= 'A' && name[0] <= 'Z') {
                return parse_struct_lit(p, name, loc);
            }

            Expr *e = ast_new_expr(p->arena, EX_IDENT, loc);
            e->ident = name;
            return e;
        }

        default:
            diag_error(p->diag, p->cur.loc,
                       "expected expression, found '%s'", tok_kind_name(p->cur.kind));
            advance(p);
            Expr *e = ast_new_expr(p->arena, EX_UNIT, loc);
            return e;
    }
}

static Expr *parse_unary(Parser *p) {
    SrcLoc loc = p->cur.loc;
    if (match(p, TOK_MINUS)) {
        Expr *e = ast_new_expr(p->arena, EX_UNARY, loc);
        e->unary.op = UNOP_NEG;
        e->unary.operand = parse_unary(p);
        return e;
    }
    if (match(p, TOK_BANG)) {
        Expr *e = ast_new_expr(p->arena, EX_UNARY, loc);
        e->unary.op = UNOP_NOT;
        e->unary.operand = parse_unary(p);
        return e;
    }
    if (match(p, TOK_TILDE)) {
        Expr *e = ast_new_expr(p->arena, EX_UNARY, loc);
        e->unary.op = UNOP_BIT_NOT;
        e->unary.operand = parse_unary(p);
        return e;
    }
    if (match(p, TOK_AMP)) {
        Expr *e = ast_new_expr(p->arena, EX_REF, loc);
        e->ref_.is_mut = match(p, TOK_KW_MUT);
        e->ref_.inner  = parse_unary(p);
        return e;
    }
    if (match(p, TOK_STAR)) {
        Expr *e = ast_new_expr(p->arena, EX_DEREF, loc);
        e->deref.inner = parse_unary(p);
        return e;
    }
    return parse_primary(p);
}

static Expr *parse_precedence_inner(Parser *p, Precedence min_prec);

static Expr *parse_precedence(Parser *p, Precedence min_prec) {
    SrcLoc loc = p->cur.loc;
    if (!enter_recursion(p)) {
        return ast_new_expr(p->arena, EX_INT_LIT, loc);
    }
    Expr *r = parse_precedence_inner(p, min_prec);
    leave_recursion(p);
    return r;
}

static Expr *parse_precedence_inner(Parser *p, Precedence min_prec) {
    Expr *lhs = parse_unary(p);
    for (;;) {
        TokKind kind = p->cur.kind;
        Precedence prec = infix_prec(kind);
        if (prec == PREC_NONE || prec < min_prec) break;

        SrcLoc loc = p->cur.loc;

        /* assignment is right-associative */
        if (prec == PREC_ASSIGN) {
            AssignOp op = tok_to_assign(kind);
            advance(p);
            Expr *rhs = parse_precedence(p, PREC_ASSIGN);
            Expr *e = ast_new_expr(p->arena, EX_ASSIGN, loc);
            e->assign.op  = op;
            e->assign.lhs = lhs;
            e->assign.rhs = rhs;
            lhs = e;
            continue;
        }

        /* postfix call / dot / index / try */
        if (kind == TOK_QUESTION) {
            advance(p);
            Expr *e = ast_new_expr(p->arena, EX_TRY, loc);
            e->try_.inner = lhs;
            lhs = e;
            continue;
        }
        if (kind == TOK_LPAREN) {
            lhs = parse_call_args(p, lhs, loc);
            continue;
        }
        if (kind == TOK_LBRACKET) {
            advance(p);
            Expr *idx = parse_expr(p);
            expect(p, TOK_RBRACKET, "]");
            Expr *e = ast_new_expr(p->arena, EX_INDEX, loc);
            e->index.obj = lhs;
            e->index.idx = idx;
            lhs = e;
            continue;
        }
        if (kind == TOK_DOT) {
            advance(p);
            if (!check(p, TOK_IDENT)) {
                diag_error(p->diag, p->cur.loc, "expected field or method name after '.'");
                break;
            }
            const char *name = arena_strndup(p->arena, p->cur.start, p->cur.length);
            advance(p);
            if (check(p, TOK_LPAREN)) {
                /* method call */
                advance(p);
                Expr **args = NULL;
                int n = 0, cap = 0;
                if (!check(p, TOK_RPAREN)) {
                    for (;;) {
                        Expr *a = parse_expr(p);
                        if (n >= cap) { cap = cap ? cap * 2 : 4; args = (Expr **)arena_grow(p->arena, args, n * sizeof(*args), cap * sizeof(*args)); }
                        args[n++] = a;
                        if (!match(p, TOK_COMMA)) break;
                        if (check(p, TOK_RPAREN)) break;
                    }
                }
                expect(p, TOK_RPAREN, ")");
                Expr *e = ast_new_expr(p->arena, EX_METHOD_CALL, loc);
                e->method.recv   = lhs;
                e->method.name   = name;
                e->method.args   = (Expr **)arena_alloc(p->arena, n * sizeof(Expr *));
                urus_memcpy(e->method.args, args, n * sizeof(Expr *));
                e->method.n_args = n;
                /* scratch is arena-backed now — dies with the arena */
                lhs = e;
            } else {
                Expr *e = ast_new_expr(p->arena, EX_FIELD, loc);
                e->field.obj  = lhs;
                e->field.name = name;
                lhs = e;
            }
            continue;
        }

        if (kind == TOK_KW_AS) {
            advance(p);
            TypeExpr *ty = parse_type(p);
            Expr *e = ast_new_expr(p->arena, EX_CAST, loc);
            e->cast.expr = lhs;
            e->cast.ty   = ty;
            lhs = e;
            continue;
        }

        if (kind == TOK_DOTDOT || kind == TOK_DOTDOTEQ) {
            bool inclusive = (kind == TOK_DOTDOTEQ);
            advance(p);
            Expr *end = parse_precedence(p, (Precedence)(PREC_RANGE + 1));
            Expr *e = ast_new_expr(p->arena, EX_RANGE, loc);
            e->range.start     = lhs;
            e->range.end       = end;
            e->range.inclusive = inclusive;
            lhs = e;
            continue;
        }

        /* binary */
        BinOp op = tok_to_binop(kind);
        advance(p);
        Expr *rhs = parse_precedence(p, (Precedence)(prec + 1));
        Expr *e = ast_new_expr(p->arena, EX_BINARY, loc);
        e->binary.op  = op;
        e->binary.lhs = lhs;
        e->binary.rhs = rhs;
        lhs = e;
    }
    return lhs;
}

static Expr *parse_expr(Parser *p) {
    return parse_precedence(p, PREC_ASSIGN);
}

static Expr *parse_block_inner(Parser *p);

static Expr *parse_block(Parser *p) {
    SrcLoc loc = p->cur.loc;
    if (!enter_recursion(p)) {
        /* skip past the brace pair we cannot enter, then return an empty block */
        if (check(p, TOK_LBRACE)) advance(p);
        return ast_new_expr(p->arena, EX_BLOCK, loc);
    }
    Expr *r = parse_block_inner(p);
    leave_recursion(p);
    return r;
}

static Expr *parse_block_inner(Parser *p) {
    SrcLoc loc = p->cur.loc;
    expect(p, TOK_LBRACE, "{");
    Stmt **stmts = NULL;
    int n = 0, cap = 0;
    Expr *tail = NULL;

    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        Stmt *s = parse_stmt(p);
        if (!s) continue;
        /* if last "stmt" is expression without ; and next is `}`, treat as tail */
        if (s->kind == ST_EXPR && !s->expr_.has_semi && check(p, TOK_RBRACE)) {
            tail = s->expr_.expr;
            break;
        }
        if (n >= cap) { cap = cap ? cap * 2 : 8; stmts = (Stmt **)arena_grow(p->arena, stmts, n * sizeof(*stmts), cap * sizeof(*stmts)); }
        stmts[n++] = s;
    }
    expect(p, TOK_RBRACE, "}");
    Expr *blk = ast_new_expr(p->arena, EX_BLOCK, loc);
    blk->block.stmts   = (Stmt **)arena_alloc(p->arena, n * sizeof(Stmt *));
    urus_memcpy(blk->block.stmts, stmts, n * sizeof(Stmt *));
    blk->block.n_stmts = n;
    blk->block.tail    = tail;
    /* scratch is arena-backed now — dies with the arena */
    return blk;
}

/* ---------- statements ---------- */

static Stmt *parse_let(Parser *p) {
    SrcLoc loc = p->cur.loc;
    advance(p);  /* let */
    bool is_mut = match(p, TOK_KW_MUT);
    Pattern *pat = parse_pattern(p);
    TypeExpr *ty = NULL;
    if (match(p, TOK_COLON)) ty = parse_type(p);
    Expr *init = NULL;
    if (match(p, TOK_ASSIGN)) init = parse_expr(p);
    match(p, TOK_SEMI); /* trailing ; optional, per spec */
    Stmt *s = ast_new_stmt(p->arena, ST_LET, loc);
    s->let_.pat        = pat;
    s->let_.type_annot = ty;
    s->let_.init       = init;
    s->let_.is_mut     = is_mut;
    return s;
}

static Stmt *parse_stmt(Parser *p) {
    if (check(p, TOK_KW_LET)) return parse_let(p);

    if (check(p, TOK_KW_DEFER)) {
        SrcLoc loc = p->cur.loc;
        advance(p);  /* defer */
        Expr *e = parse_expr(p);
        match(p, TOK_SEMI);  /* optional */
        Stmt *s = ast_new_stmt(p->arena, ST_DEFER, loc);
        s->defer_.expr = e;
        return s;
    }

    SrcLoc loc = p->cur.loc;
    Expr  *e   = parse_expr(p);
    bool   semi = match(p, TOK_SEMI);

    Stmt *s = ast_new_stmt(p->arena, ST_EXPR, loc);
    s->expr_.expr     = e;
    s->expr_.has_semi = semi;
    return s;
}

/* ---------- items ---------- */

static StructField parse_struct_field(Parser *p) {
    StructField f = {0};
    if (!check(p, TOK_IDENT)) {
        diag_error(p->diag, p->cur.loc, "expected field name");
        f.name = "<error>";
    } else {
        f.name = arena_strndup(p->arena, p->cur.start, p->cur.length);
        advance(p);
    }
    expect(p, TOK_COLON, ":");
    f.type = parse_type(p);
    return f;
}

static Item *parse_struct(Parser *p, bool is_pub) {
    SrcLoc loc = p->cur.loc;
    advance(p);  /* struct */
    if (!check(p, TOK_IDENT)) {
        diag_error(p->diag, p->cur.loc, "expected struct name");
        return NULL;
    }
    const char *name = arena_strndup(p->arena, p->cur.start, p->cur.length);
    advance(p);
    expect(p, TOK_LBRACE, "{");
    StructField *buf = NULL;
    int n = 0, cap = 0;
    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        StructField f = parse_struct_field(p);
        if (n >= cap) { cap = cap ? cap * 2 : 4; buf = (StructField *)arena_grow(p->arena, buf, n * sizeof(*buf), cap * sizeof(*buf)); }
        buf[n++] = f;
        if (!match(p, TOK_COMMA)) break;
    }
    expect(p, TOK_RBRACE, "}");

    StructDecl *sd = (StructDecl *)arena_alloc_zero(p->arena, sizeof(StructDecl));
    sd->name     = name;
    sd->is_pub   = is_pub;
    sd->loc      = loc;
    sd->n_fields = n;
    sd->fields   = (StructField *)arena_alloc(p->arena, n * sizeof(StructField));
    urus_memcpy(sd->fields, buf, n * sizeof(StructField));
    /* scratch is arena-backed now — dies with the arena */
    Item *it = ast_new_item(p->arena, IT_STRUCT);
    it->strct = sd;
    return it;
}

static Item *parse_enum(Parser *p, bool is_pub) {
    SrcLoc loc = p->cur.loc;
    advance(p);  /* enum */
    if (!check(p, TOK_IDENT)) { diag_error(p->diag, p->cur.loc, "expected enum name"); return NULL; }
    const char *name = arena_strndup(p->arena, p->cur.start, p->cur.length);
    advance(p);
    expect(p, TOK_LBRACE, "{");
    EnumVariant *vars = NULL;
    int n = 0, cap = 0;
    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        if (!check(p, TOK_IDENT)) { diag_error(p->diag, p->cur.loc, "expected variant name"); break; }
        EnumVariant v = {0};
        v.name = arena_strndup(p->arena, p->cur.start, p->cur.length);
        advance(p);
        if (match(p, TOK_LPAREN)) {
            StructField *pf = NULL;
            int pn = 0, pcap = 0;
            if (!check(p, TOK_RPAREN)) {
                for (;;) {
                    StructField f = {0};
                    f.name = "_0";
                    f.type = parse_type(p);
                    if (pn >= pcap) { pcap = pcap ? pcap * 2 : 4; pf = (StructField *)arena_grow(p->arena, pf, pn * sizeof(*pf), pcap * sizeof(*pf)); }
                    pf[pn++] = f;
                    if (!match(p, TOK_COMMA)) break;
                    if (check(p, TOK_RPAREN)) break;
                }
            }
            expect(p, TOK_RPAREN, ")");
            v.payload   = (StructField *)arena_alloc(p->arena, pn * sizeof(StructField));
            urus_memcpy(v.payload, pf, pn * sizeof(StructField));
            v.n_payload = pn;
            /* scratch is arena-backed now — dies with the arena */
        }
        if (n >= cap) { cap = cap ? cap * 2 : 4; vars = (EnumVariant *)arena_grow(p->arena, vars, n * sizeof(*vars), cap * sizeof(*vars)); }
        vars[n++] = v;
        if (!match(p, TOK_COMMA)) break;
    }
    expect(p, TOK_RBRACE, "}");

    EnumDecl *ed = (EnumDecl *)arena_alloc_zero(p->arena, sizeof(EnumDecl));
    ed->name = name;
    ed->is_pub = is_pub;
    ed->loc = loc;
    ed->n_variants = n;
    ed->variants = (EnumVariant *)arena_alloc(p->arena, n * sizeof(EnumVariant));
    urus_memcpy(ed->variants, vars, n * sizeof(EnumVariant));
    /* scratch is arena-backed now — dies with the arena */
    Item *it = ast_new_item(p->arena, IT_ENUM);
    it->enm = ed;
    return it;
}

static FnDecl *parse_fn(Parser *p, bool is_pub, const char *owner_type) {
    SrcLoc loc = p->cur.loc;
    advance(p);  /* fn */
    if (!check(p, TOK_IDENT)) { diag_error(p->diag, p->cur.loc, "expected function name"); return NULL; }
    const char *name = arena_strndup(p->arena, p->cur.start, p->cur.length);
    advance(p);
    expect(p, TOK_LPAREN, "(");

    FnParam *params = NULL;
    int n = 0, cap = 0;
    if (!check(p, TOK_RPAREN)) {
        for (;;) {
            FnParam fp = {0};
            /* &self / &mut self / self */
            if (check(p, TOK_AMP)) {
                advance(p);
                bool mut = match(p, TOK_KW_MUT);
                if (check(p, TOK_KW_SELF)) {
                    advance(p);
                    fp.is_self     = true;
                    fp.is_ref_self = true;
                    fp.is_mut_self = mut;
                    fp.type        = NULL;
                    fp.pat         = NULL;
                } else {
                    diag_error(p->diag, p->cur.loc, "expected self after '&'");
                }
            } else if (check(p, TOK_KW_SELF)) {
                advance(p);
                fp.is_self     = true;
                fp.is_ref_self = false;
            } else {
                Pattern *pat = parse_pattern(p);
                expect(p, TOK_COLON, ":");
                TypeExpr *ty = parse_type(p);
                fp.pat  = pat;
                fp.type = ty;
            }
            if (n >= cap) { cap = cap ? cap * 2 : 4; params = (FnParam *)arena_grow(p->arena, params, n * sizeof(*params), cap * sizeof(*params)); }
            params[n++] = fp;
            if (!match(p, TOK_COMMA)) break;
            if (check(p, TOK_RPAREN)) break;
        }
    }
    expect(p, TOK_RPAREN, ")");

    TypeExpr *ret = NULL;
    if (match(p, TOK_ARROW)) ret = parse_type(p);

    Expr *body = parse_block(p);

    FnDecl *f = (FnDecl *)arena_alloc_zero(p->arena, sizeof(FnDecl));
    f->name       = name;
    f->params     = (FnParam *)arena_alloc(p->arena, n * sizeof(FnParam));
    urus_memcpy(f->params, params, n * sizeof(FnParam));
    f->n_params   = n;
    f->ret_type   = ret;
    f->body       = body;
    f->is_pub     = is_pub;
    f->is_method  = owner_type != NULL;
    f->owner_type = owner_type;
    f->loc        = loc;
    /* scratch is arena-backed now — dies with the arena */
    return f;
}

static Item *parse_impl(Parser *p) {
    SrcLoc loc = p->cur.loc;
    advance(p);  /* impl */
    if (!check(p, TOK_IDENT)) { diag_error(p->diag, p->cur.loc, "expected type name"); return NULL; }
    const char *tname = arena_strndup(p->arena, p->cur.start, p->cur.length);
    advance(p);
    expect(p, TOK_LBRACE, "{");

    FnDecl **methods = NULL;
    int n = 0, cap = 0;
    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        bool is_pub = match(p, TOK_KW_PUB);
        if (!check(p, TOK_KW_FN)) {
            diag_error(p->diag, p->cur.loc, "only fn items allowed inside impl");
            advance(p);
            continue;
        }
        FnDecl *f = parse_fn(p, is_pub, tname);
        if (n >= cap) { cap = cap ? cap * 2 : 4; methods = (FnDecl **)arena_grow(p->arena, methods, n * sizeof(*methods), cap * sizeof(*methods)); }
        methods[n++] = f;
    }
    expect(p, TOK_RBRACE, "}");

    ImplBlock *ib = (ImplBlock *)arena_alloc_zero(p->arena, sizeof(ImplBlock));
    ib->type_name = tname;
    ib->methods   = (FnDecl **)arena_alloc(p->arena, n * sizeof(FnDecl *));
    urus_memcpy(ib->methods, methods, n * sizeof(FnDecl *));
    ib->n_methods = n;
    ib->loc       = loc;
    /* scratch is arena-backed now — dies with the arena */

    Item *it = ast_new_item(p->arena, IT_IMPL);
    it->impl = ib;
    return it;
}

static Item *parse_use(Parser *p) {
    SrcLoc loc = p->cur.loc;
    advance(p); /* use */
    const char **segs = NULL;
    int n = 0, cap = 0;
    for (;;) {
        if (!check(p, TOK_IDENT)) { diag_error(p->diag, p->cur.loc, "expected identifier"); break; }
        const char *s = arena_strndup(p->arena, p->cur.start, p->cur.length);
        advance(p);
        if (n >= cap) { cap = cap ? cap * 2 : 4; segs = (const char **)arena_grow(p->arena, segs, n * sizeof(*segs), cap * sizeof(*segs)); }
        segs[n++] = s;
        if (!match(p, TOK_COLONCOLON) && !match(p, TOK_DOT)) break;
    }
    match(p, TOK_SEMI); /* trailing ; optional, per spec */

    UseDecl *ud = (UseDecl *)arena_alloc_zero(p->arena, sizeof(UseDecl));
    ud->segs   = (const char **)arena_alloc(p->arena, n * sizeof(char *));
    urus_memcpy(ud->segs, segs, n * sizeof(char *));
    ud->n_segs = n;
    ud->loc    = loc;
    /* scratch is arena-backed now — dies with the arena */
    Item *it = ast_new_item(p->arena, IT_USE);
    it->use  = ud;
    return it;
}

static Item *parse_const(Parser *p, bool is_pub) {
    SrcLoc loc = p->cur.loc;
    advance(p); /* const */
    if (!check(p, TOK_IDENT)) { diag_error(p->diag, p->cur.loc, "expected const name"); return NULL; }
    const char *name = arena_strndup(p->arena, p->cur.start, p->cur.length);
    advance(p);
    expect(p, TOK_COLON, ":");
    TypeExpr *ty = parse_type(p);
    expect(p, TOK_ASSIGN, "=");
    Expr *val = parse_expr(p);
    match(p, TOK_SEMI); /* trailing ; optional, per spec */
    ConstDecl *cd = (ConstDecl *)arena_alloc_zero(p->arena, sizeof(ConstDecl));
    cd->name = name; cd->type = ty; cd->value = val; cd->is_pub = is_pub; cd->loc = loc;
    Item *it = ast_new_item(p->arena, IT_CONST);
    it->cnst = cd;
    return it;
}

static Item *parse_type_alias(Parser *p, bool is_pub) {
    SrcLoc loc = p->cur.loc;
    advance(p); /* type */
    if (!check(p, TOK_IDENT)) { diag_error(p->diag, p->cur.loc, "expected name"); return NULL; }
    const char *name = arena_strndup(p->arena, p->cur.start, p->cur.length);
    advance(p);
    expect(p, TOK_ASSIGN, "=");
    TypeExpr *t = parse_type(p);
    match(p, TOK_SEMI); /* trailing ; optional, per spec */
    TypeAlias *ta = (TypeAlias *)arena_alloc_zero(p->arena, sizeof(TypeAlias));
    ta->name = name; ta->aliased = t; ta->is_pub = is_pub; ta->loc = loc;
    Item *it = ast_new_item(p->arena, IT_TYPE_ALIAS);
    it->alias = ta;
    return it;
}

static Item *parse_item(Parser *p) {
    bool is_pub = match(p, TOK_KW_PUB);

    switch (p->cur.kind) {
        case TOK_KW_FN: {
            FnDecl *f = parse_fn(p, is_pub, NULL);
            if (!f) return NULL;
            Item *it = ast_new_item(p->arena, IT_FN);
            it->fn = f;
            return it;
        }
        case TOK_KW_STRUCT: return parse_struct(p, is_pub);
        case TOK_KW_ENUM:   return parse_enum(p, is_pub);
        case TOK_KW_IMPL:   return parse_impl(p);
        case TOK_KW_USE:    return parse_use(p);
        case TOK_KW_CONST:  return parse_const(p, is_pub);
        case TOK_KW_TYPE:   return parse_type_alias(p, is_pub);
        default:
            diag_error(p->diag, p->cur.loc,
                       "expected item (fn, struct, enum, impl, use, const, type), found '%s'",
                       tok_kind_name(p->cur.kind));
            advance(p);
            sync_to_item(p);
            return NULL;
    }
}

Module *parser_parse_module(Parser *p) {
    Module *m = (Module *)arena_alloc_zero(p->arena, sizeof(Module));
    m->arena  = p->arena;

    if (match(p, TOK_KW_MODULE)) {
        if (!check(p, TOK_IDENT)) {
            diag_error(p->diag, p->cur.loc, "expected module name");
        } else {
            m->name = arena_strndup(p->arena, p->cur.start, p->cur.length);
            advance(p);
        }
        /* Optional ; — common in many languages */
        match(p, TOK_SEMI);
    }

    Item **items = NULL;
    int n = 0, cap = 0;
    while (!check(p, TOK_EOF)) {
        Item *it = parse_item(p);
        if (it) {
            if (n >= cap) { cap = cap ? cap * 2 : 8; items = (Item **)arena_grow(p->arena, items, n * sizeof(*items), cap * sizeof(*items)); }
            items[n++] = it;
        }
        if (p->panic_mode) sync_to_item(p);
    }
    m->items   = (Item **)arena_alloc(p->arena, n * sizeof(Item *));
    urus_memcpy(m->items, items, n * sizeof(Item *));
    m->n_items = n;
    /* scratch is arena-backed now — dies with the arena */
    return m;
}
