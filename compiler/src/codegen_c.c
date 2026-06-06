#include "codegen_c.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

/* Maximum pending defers across all nested blocks of one function.
 * 64 is generous — deeper means generated code, which can split fns. */
#define URUS_CG_MAX_DEFERS 64

typedef struct {
    StrBuf  *sb;
    Module  *mod;
    DiagCtx *diag;
    int      indent;
    /* If we're inside an impl, the parent type name (for `Self`) */
    const char *cur_type;
    /* b028: pending-defer stack.  cg_block pushes ST_DEFER statements as
     * it walks; EX_RETURN flushes the whole stack (LIFO) before emitting
     * the actual `return`, so early returns no longer skip cleanup.
     * Block end flushes down to the block's watermark. */
    Stmt *defers[URUS_CG_MAX_DEFERS];
    int   n_defers;
    /* True while emitting a fn whose C return type is `void` — the block
     * tail must then be emitted as a plain statement, not `return expr;`
     * (constraint violation in C, and `return return …` for EX_RETURN
     * tails). */
    bool  fn_ret_void;
    /* Minimal local type tracking so Result/Option payload *extraction*
     * picks the right union arm (F-TY-2's read side).  Append-only per
     * function; lookup scans newest-first so shadowing works.  Overflow
     * degrades to the int64 accessor — wrong arm, but never UB at the
     * C level (union read), matching pre-b030 behaviour. */
#define URUS_CG_MAX_LOCALS 256
    struct { const char *name; TypeExpr *type; } locals[URUS_CG_MAX_LOCALS];
    int n_locals;
    /* Current fn's receiver, if any.  The C parameter is spelled `self_`
     * (avoids user-shadowing issues) and is a pointer for `&self` — so
     * URUS-level `self` must render as `(*self_)` / `self_` accordingly. */
    bool fn_has_self;
    bool fn_self_is_ref;
} CG;

/* ---------- helpers ---------- */

static void emit(CG *g, const char *s) { sb_puts(g->sb, s); }
static void emitf(CG *g, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    /* delegate to sb_printf via temporary */
    char buf[1024];
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t)n < sizeof(buf)) {
        sb_puts(g->sb, buf);
    } else {
        char *big = (char *)malloc((size_t)n + 1);
        va_list aq; va_start(aq, fmt);
        vsnprintf(big, (size_t)n + 1, fmt, aq);
        va_end(aq);
        sb_puts(g->sb, big);
        free(big);
    }
}
static void emit_indent(CG *g) { sb_indent(g->sb, g->indent); }

static bool is_c_keyword(const char *s) {
    static const char *kws[] = {
        "auto","break","case","char","const","continue","default","do","double",
        "else","enum","extern","float","for","goto","if","int","long","register",
        "return","short","signed","sizeof","static","struct","switch","typedef",
        "union","unsigned","void","volatile","while","inline","restrict","_Bool",
        "_Complex","_Imaginary","_Atomic","_Generic","_Alignof","_Alignas",
        "_Static_assert","_Noreturn","_Thread_local","main", NULL
    };
    for (int i = 0; kws[i]; i++) if (strcmp(kws[i], s) == 0) return true;
    return false;
}

static void emit_ident(CG *g, const char *name) {
    if (is_c_keyword(name)) { emitf(g, "%s_", name); }
    else                    { emit(g, name); }
}

/* Map URUS type names to C type names. Unknown names are passed through, which
   lets user-defined structs work. */
static const char *map_primitive(const char *name) {
    if (!name) return "void";
    if (strcmp(name, "u8")  == 0) return "uint8_t";
    if (strcmp(name, "u16") == 0) return "uint16_t";
    if (strcmp(name, "u32") == 0) return "uint32_t";
    if (strcmp(name, "u64") == 0) return "uint64_t";
    if (strcmp(name, "i8")  == 0) return "int8_t";
    if (strcmp(name, "i16") == 0) return "int16_t";
    if (strcmp(name, "i32") == 0) return "int32_t";
    if (strcmp(name, "i64") == 0) return "int64_t";
    if (strcmp(name, "f32") == 0) return "float";
    if (strcmp(name, "f64") == 0) return "double";
    if (strcmp(name, "usize") == 0) return "size_t";
    if (strcmp(name, "isize") == 0) return "ptrdiff_t";
    if (strcmp(name, "bool")== 0) return "urus_bool";
    if (strcmp(name, "str") == 0) return "urus_str";
    if (strcmp(name, "char")== 0) return "uint32_t";  /* URUS char = rune */
    return NULL;
}

static void emit_type(CG *g, TypeExpr *t) {
    if (!t) { emit(g, "void"); return; }
    switch (t->kind) {
        case TY_UNIT:  emit(g, "void"); return;
        case TY_INFER: emit(g, "/*infer*/ int"); return;
        case TY_SELF:
            if (g->cur_type) emit(g, g->cur_type);
            else             emit(g, "void");
            return;
        case TY_NAMED: {
            const char *m = map_primitive(t->named.name);
            if (m) emit(g, m);
            else   emit_ident(g, t->named.name);
            return;
        }
        case TY_POINTER:
            emit_type(g, t->pointer.inner);
            emit(g, t->pointer.is_mut ? " *" : " const *");
            return;
        case TY_REF:
            /* v0.0.1: references compile down to pointers */
            emit_type(g, t->ref.inner);
            emit(g, t->ref.is_mut ? " *" : " const *");
            return;
        case TY_SLICE:
            /* v0.0.1: degrade to raw pointer; length tracking lands in v0.0.2 */
            emit_type(g, t->slice.elem);
            emit(g, " *");
            return;
        case TY_ARRAY:
            emit_type(g, t->array.elem);
            emit(g, " *");
            return;
        case TY_TUPLE:
            /* v0.0.1: emit as void* placeholder. Real tuple structs land later. */
            emit(g, "/*tuple*/ void*");
            return;
        case TY_FN:
            emit(g, "/*fnptr*/ void*");
            return;
        case TY_GENERIC: {
            /* Special-case Result/Option to the runtime's generic types. */
            if (strcmp(t->generic.name, "Result") == 0) {
                emit(g, "urus_Result");
                return;
            }
            if (strcmp(t->generic.name, "Option") == 0) {
                emit(g, "urus_Option");
                return;
            }
            emit_ident(g, t->generic.name);
            return;
        }
    }
}

/* ---------- forward decls ---------- */

static void cg_expr(CG *g, Expr *e);
static void cg_block(CG *g, Expr *blk, bool is_expr_position);

/* ---------- minimal local type tracking (payload read side) ---------- */

static void cg_local_add(CG *g, const char *name, TypeExpr *type) {
    if (!name || !type) return;
    if (g->n_locals >= URUS_CG_MAX_LOCALS) return;  /* degrade, don't error */
    g->locals[g->n_locals].name = name;
    g->locals[g->n_locals].type = type;
    g->n_locals++;
}

static TypeExpr *cg_local_lookup(CG *g, const char *name) {
    for (int i = g->n_locals - 1; i >= 0; i--)       /* newest-first: shadowing */
        if (strcmp(g->locals[i].name, name) == 0) return g->locals[i].type;
    return NULL;
}

static FnDecl *cg_find_fn(CG *g, const char *name) {
    for (int i = 0; i < g->mod->n_items; i++) {
        Item *it = g->mod->items[i];
        if (it->kind == IT_FN && strcmp(it->fn->name, name) == 0) return it->fn;
    }
    return NULL;
}

/* Find the user enum that owns `variant`.  Returns the EnumDecl or NULL.
 * Variant names are module-unique in practice for v0.0.1; first hit wins. */
static EnumDecl *cg_find_enum_of_variant(CG *g, const char *variant) {
    for (int i = 0; i < g->mod->n_items; i++) {
        Item *it = g->mod->items[i];
        if (it->kind != IT_ENUM) continue;
        for (int j = 0; j < it->enm->n_variants; j++)
            if (strcmp(it->enm->variants[j].name, variant) == 0) return it->enm;
    }
    return NULL;
}

/* Best-effort static type of an expression.  Only needs to be right for
 * the cases the payload accessor cares about (Result/Option generics);
 * NULL means "unknown" and the caller falls back to the int64 arm. */
static TypeExpr *cg_expr_type(CG *g, Expr *e) {
    if (!e) return NULL;
    switch (e->kind) {
        case EX_IDENT: return cg_local_lookup(g, e->ident);
        case EX_CALL:
            if (e->call.callee && e->call.callee->kind == EX_IDENT) {
                FnDecl *f = cg_find_fn(g, e->call.callee->ident);
                if (f) return f->ret_type;
                /* Builtins with known generic returns (not module items). */
                if (strcmp(e->call.callee->ident, "read_line") == 0 ||
                    strcmp(e->call.callee->ident, "urus_read_line") == 0) {
                    static TypeExpr str_ty = { .kind = TY_NAMED, .named = { "str" } };
                    static TypeExpr *opt_args[1] = { &str_ty };
                    static TypeExpr opt_str = { .kind = TY_GENERIC,
                                                .generic = { "Option", opt_args, 1 } };
                    return &opt_str;  /* read_line() -> Option<str> */
                }
            }
            return NULL;
        case EX_TRY: {
            /* `r?` yields the Ok payload type. */
            TypeExpr *t = cg_expr_type(g, e->try_.inner);
            if (t && t->kind == TY_GENERIC && t->generic.n_args >= 1)
                return t->generic.args[0];
            return NULL;
        }
        default: return NULL;
    }
}

/* The right payload accessor for a binding of type `t` (the *payload*
 * type, i.e. the generic argument — not the Result/Option itself). */
static const char *payload_accessor(TypeExpr *t) {
    if (!t) return "urus_payload";
    if (t->kind == TY_NAMED) {
        const char *n = t->named.name;
        if (strcmp(n, "str") == 0)                          return "urus_payload_str";
        if (strcmp(n, "f32") == 0 || strcmp(n, "f64") == 0) return "urus_payload_f";
        if (n[0] == 'u' && (strcmp(n, "u8") == 0 || strcmp(n, "u16") == 0 ||
                            strcmp(n, "u32") == 0 || strcmp(n, "u64") == 0 ||
                            strcmp(n, "usize") == 0))       return "urus_payload_u";
    }
    return "urus_payload";  /* ints, bools, pointers: int64 arm is exact */
}

/* For a scrutinee of type Result<T,E> / Option<T>, the payload type the
 * given variant carries: Ok/Some -> arg0, Err -> arg1. */
static TypeExpr *variant_payload_type(TypeExpr *scrut_t, const char *variant) {
    if (!scrut_t || scrut_t->kind != TY_GENERIC) return NULL;
    if (strcmp(variant, "Err") == 0)
        return scrut_t->generic.n_args >= 2 ? scrut_t->generic.args[1] : NULL;
    return scrut_t->generic.n_args >= 1 ? scrut_t->generic.args[0] : NULL;
}

/* ---------- expressions ---------- */

static const char *binop_c(BinOp op) {
    switch (op) {
        case BIN_ADD: return "+";    case BIN_SUB: return "-";
        case BIN_MUL: return "*";    case BIN_DIV: return "/";
        case BIN_MOD: return "%";
        case BIN_EQ:  return "==";   case BIN_NEQ: return "!=";
        case BIN_LT:  return "<";    case BIN_LE:  return "<=";
        case BIN_GT:  return ">";    case BIN_GE:  return ">=";
        case BIN_AND: return "&&";   case BIN_OR:  return "||";
        case BIN_BIT_AND: return "&"; case BIN_BIT_OR: return "|";
        case BIN_BIT_XOR: return "^";
        case BIN_SHL: return "<<";   case BIN_SHR: return ">>";
    }
    return "?";
}

static const char *assignop_c(AssignOp op) {
    switch (op) {
        case ASSIGN_EQ:  return "=";
        case ASSIGN_ADD: return "+=";
        case ASSIGN_SUB: return "-=";
        case ASSIGN_MUL: return "*=";
        case ASSIGN_DIV: return "/=";
        case ASSIGN_MOD: return "%=";
    }
    return "=";
}

/* Emit a `urus_fmt_arg[]{ ... }` initialiser by scanning `{name}` placeholders.
   Used by both println-style desugaring and f"..." literals.  Returns the
   number of array elements emitted (excluding the trailing URUS_FMT_END
   sentinel, which is kept for runtime back-compat).  The count is what the
   length-aware `urus_*_fmt_n` entry points consume — closes F-MEM-7. */
static uint32_t cg_fmt_arg_array(CG *g, const char *s, uint32_t n, SrcLoc loc);

static void cg_string_literal(CG *g, const char *s, uint32_t len) {
    emit(g, "urus_str_from_lit(\"");
    for (uint32_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
            case '"':  emit(g, "\\\""); break;
            case '\\': emit(g, "\\\\"); break;
            case '\n': emit(g, "\\n");  break;
            case '\r': emit(g, "\\r");  break;
            case '\t': emit(g, "\\t");  break;
            default:
                if (c < 0x20 || c == 0x7f) emitf(g, "\\x%02x", c);
                else                       sb_putc(g->sb, (char)c);
        }
    }
    emitf(g, "\", %u)", (unsigned)len);
}

/*
 * F-MEM-1 (closed in v0.0.1-b014): validate that an f-string `{…}` placeholder
 * is a sequence of identifiers joined by dots — `name`, `obj.field`,
 * `a.b.c` — and *nothing else*.  Before this guard, the bracket contents
 * were pasted verbatim into emitted C, which gave anyone authoring a
 * `.urus` file a code-injection primitive against the host C compiler
 * (`f"{system(\"…\")}"` would compile + link the call).
 *
 * Each identifier must start with `_` or `A-Za-z` and may continue with
 * `_`, digits, or `A-Za-z`.  Dots separate identifiers; no leading,
 * trailing, or consecutive dots are accepted.  Empty placeholders, length
 * over 256 bytes, NUL inside the placeholder, or any other byte all
 * trigger a hard codegen diagnostic.
 *
 * Returns true on accept, false (and emits a diag) on reject.
 */
#define URUS_FMT_MAX_PLACEHOLDER 256

static bool is_ident_start(unsigned char c) {
    return c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}
static bool is_ident_cont(unsigned char c) {
    return is_ident_start(c) || (c >= '0' && c <= '9');
}

static bool validate_fmt_placeholder(CG *g, const char *p, uint32_t len, SrcLoc loc) {
    if (len == 0) {
        diag_error(g->diag, loc, "empty f-string placeholder `{}` — name a binding instead");
        return false;
    }
    if (len > URUS_FMT_MAX_PLACEHOLDER) {
        diag_error(g->diag, loc,
                   "f-string placeholder too long (%u bytes; max %u)",
                   (unsigned)len, (unsigned)URUS_FMT_MAX_PLACEHOLDER);
        return false;
    }
    uint32_t i = 0;
    while (i < len) {
        if (!is_ident_start((unsigned char)p[i])) {
            diag_error(g->diag, loc,
                       "invalid f-string placeholder: expected identifier, got '%c' "
                       "(allowed: `name`, `obj.field`, `a.b.c`)",
                       p[i] ? p[i] : '?');
            return false;
        }
        i++;
        while (i < len && is_ident_cont((unsigned char)p[i])) i++;
        if (i == len) break;
        if (p[i] != '.') {
            diag_error(g->diag, loc,
                       "invalid character '%c' in f-string placeholder — only "
                       "identifiers separated by `.` are allowed", p[i]);
            return false;
        }
        i++; /* consume dot */
        if (i == len) {
            diag_error(g->diag, loc,
                       "f-string placeholder ends with `.` — expected identifier after it");
            return false;
        }
    }
    return true;
}

static uint32_t cg_fmt_arg_array(CG *g, const char *s, uint32_t n, SrcLoc loc) {
    bool     first = true;
    uint32_t count = 0;
    uint32_t i = 0;
    while (i < n) {
        uint32_t start = i;
        while (i < n && s[i] != '{') i++;
        if (i > start) {
            if (!first) emit(g, ", ");
            first = false;
            emit(g, "URUS_FMT_STR(");
            cg_string_literal(g, s + start, i - start);
            emit(g, ")");
            count++;
        }
        if (i < n && s[i] == '{') {
            i++;
            uint32_t ks = i;
            while (i < n && s[i] != '}') i++;
            if (i >= n) {
                diag_error(g->diag, loc,
                           "unterminated f-string placeholder — missing `}`");
                break;
            }
            uint32_t plen = i - ks;
            if (!validate_fmt_placeholder(g, s + ks, plen, loc)) {
                /* diag already emitted; skip past the closing brace but do
                   NOT splice attacker bytes into the C output. */
                i++;
                continue;
            }
            if (!first) emit(g, ", ");
            first = false;
            /* Safe to splice: every byte has been verified as `[A-Za-z_][A-Za-z0-9_]*`
               or `.`, which is also a legal C identifier / field-access syntax.
               Length already bounded by URUS_FMT_MAX_PLACEHOLDER.
               `self` gets the same rewrite as EX_IDENT (C param is self_,
               a pointer for `&self`). */
            emit(g, "URUS_FMT_ANY(");
            uint32_t j = 0;
            if (g->fn_has_self && (plen == 4 || (plen > 4 && s[ks + 4] == '.')) &&
                memcmp(s + ks, "self", 4) == 0) {
                emit(g, g->fn_self_is_ref ? "(*self_)" : "self_");
                j = 4;
            }
            for (; j < plen; j++) sb_putc(g->sb, s[ks + j]);
            emit(g, ")");
            i++;  /* skip } */
            count++;
        }
    }
    /* Always trail a URUS_FMT_END so the back-compat sentinel runtime path
     * stays correct.  The length-aware path simply ignores trailing END. */
    if (first) emit(g, "URUS_FMT_END");
    else       emit(g, ", URUS_FMT_END");
    return count;
}

/* Handle the common `println("...")` and `println("hello, {name}")` cases.
   v0.0.1: we expand string interpolation here at codegen time rather than at
   parse time. The runtime exposes `urus_println_fmt(fmt, ...)` which behaves
   like printf with `%s` for urus_str and `%lld`/`%f` for numbers via _Generic. */
static bool try_emit_println_call(CG *g, Expr *e) {
    if (e->kind != EX_CALL) return false;
    Expr *cal = e->call.callee;
    if (cal->kind != EX_IDENT) return false;
    if (!(strcmp(cal->ident, "println") == 0 ||
          strcmp(cal->ident, "print")   == 0 ||
          strcmp(cal->ident, "eprintln")== 0)) {
        return false;
    }
    const char *fn = strcmp(cal->ident, "println")   == 0 ? "urus_println"   :
                     strcmp(cal->ident, "print")     == 0 ? "urus_print"     :
                                                            "urus_eprintln";

    if (e->call.n_args == 0) {
        emitf(g, "%s(urus_str_from_lit(\"\", 0))", fn);
        return true;
    }

    /* Single string-literal arg: scan for `{ident}` and split into chunks.
       f-strings (EX_FSTR_LIT) also reach this path because they reuse
       str_lit's payload shape.

       Calls go through the *_fmt_n length-aware entry points (F-MEM-7) — the
       count is captured by cg_fmt_arg_array's return value. */
    Expr *a0 = e->call.args[0];
    if (e->call.n_args == 1 && (a0->kind == EX_STR_LIT || a0->kind == EX_FSTR_LIT)) {
        emitf(g, "%s_fmt_n((urus_fmt_arg[]){", fn);
        uint32_t cnt = cg_fmt_arg_array(g, a0->str_lit.ptr, a0->str_lit.len, a0->loc);
        emitf(g, "}, %u)", (unsigned)cnt);
        return true;
    }

    /* Generic: just pass the first arg through (treat as str) */
    emitf(g, "%s_fmt_n((urus_fmt_arg[]){ URUS_FMT_ANY(", fn);
    cg_expr(g, a0);
    emit(g, "), URUS_FMT_END }, 1)");
    return true;
}

static void cg_expr(CG *g, Expr *e) {
    if (!e) { emit(g, "/*null*/0"); return; }

    switch (e->kind) {
        case EX_INT_LIT:   emitf(g, "((int64_t)%llu)", (unsigned long long)e->int_lit); return;
        case EX_FLOAT_LIT: emitf(g, "%.17g", e->float_lit); return;
        case EX_STR_LIT:   cg_string_literal(g, e->str_lit.ptr, e->str_lit.len); return;
        case EX_FSTR_LIT:
            /* f"...": render through urus_fmt_to_str_n (length-aware, F-MEM-7).
             * Stand-alone, returns a urus_str. */
            emit(g, "urus_fmt_to_str_n((urus_fmt_arg[]){");
            {
                uint32_t cnt = cg_fmt_arg_array(g, e->str_lit.ptr, e->str_lit.len, e->loc);
                emitf(g, "}, %u)", (unsigned)cnt);
            }
            return;
        case EX_CHAR_LIT:  emitf(g, "((uint32_t)%u)", (unsigned)e->char_lit); return;
        case EX_BOOL_LIT:  emit(g, e->bool_lit ? "urus_true" : "urus_false"); return;
        case EX_UNIT:      emit(g, "((void)0)"); return;
        case EX_IDENT:
            /* Recognise Ok/Err/Some/None as runtime constructors. */
            if (strcmp(e->ident, "None") == 0) { emit(g, "urus_none()"); return; }
            /* `self` is spelled `self_` in C; deref the &self pointer so
             * field access keeps working with `.` */
            if (g->fn_has_self && strcmp(e->ident, "self") == 0) {
                emit(g, g->fn_self_is_ref ? "(*self_)" : "self_");
                return;
            }
            emit_ident(g, e->ident);
            return;

        case EX_PATH: {
            /* `Enum::Variant` in value position constructs the enum value
             * — a compound literal, since the C-side enum is a tag-holding
             * struct, not the bare Grade_Tag constant. */
            if (e->path.n == 2) {
                EnumDecl *ed = cg_find_enum_of_variant(g, e->path.segs[1]);
                if (ed && strcmp(ed->name, e->path.segs[0]) == 0) {
                    emitf(g, "((%s){ .tag = %s_%s })",
                          ed->name, ed->name, e->path.segs[1]);
                    return;
                }
            }
            /* Fallback: mangled `A_B_C` name. */
            for (int i = 0; i < e->path.n; i++) {
                if (i) emit(g, "_");
                emit_ident(g, e->path.segs[i]);
            }
            return;
        }

        case EX_UNARY: {
            switch (e->unary.op) {
                case UNOP_NEG:     emit(g, "(-("); cg_expr(g, e->unary.operand); emit(g, "))"); return;
                case UNOP_NOT:     emit(g, "(!("); cg_expr(g, e->unary.operand); emit(g, "))"); return;
                case UNOP_BIT_NOT: emit(g, "(~("); cg_expr(g, e->unary.operand); emit(g, "))"); return;
            }
            return;
        }
        case EX_BINARY:
            emit(g, "(");
            cg_expr(g, e->binary.lhs);
            emitf(g, " %s ", binop_c(e->binary.op));
            cg_expr(g, e->binary.rhs);
            emit(g, ")");
            return;

        case EX_ASSIGN:
            cg_expr(g, e->assign.lhs);
            emitf(g, " %s ", assignop_c(e->assign.op));
            cg_expr(g, e->assign.rhs);
            return;

        case EX_CALL: {
            if (try_emit_println_call(g, e)) return;
            /* Ok(x)/Err(x)/Some(x) → runtime constructors */
            if (e->call.callee->kind == EX_IDENT) {
                const char *n = e->call.callee->ident;
                if (strcmp(n, "Ok") == 0 && e->call.n_args == 1) {
                    emit(g, "urus_ok("); cg_expr(g, e->call.args[0]); emit(g, ")");
                    return;
                }
                if (strcmp(n, "Err") == 0 && e->call.n_args == 1) {
                    emit(g, "urus_err("); cg_expr(g, e->call.args[0]); emit(g, ")");
                    return;
                }
                if (strcmp(n, "Some") == 0 && e->call.n_args == 1) {
                    emit(g, "urus_some("); cg_expr(g, e->call.args[0]); emit(g, ")");
                    return;
                }
                if (strcmp(n, "panic") == 0) {
                    emit(g, "urus_panic(");
                    if (e->call.n_args > 0) cg_expr(g, e->call.args[0]);
                    else                    emit(g, "urus_str_from_lit(\"panic\", 5)");
                    emit(g, ")");
                    return;
                }
                /* urus.io.read_line — surface name maps to the runtime
                   symbol (v0.0.1-b019). */
                if (strcmp(n, "read_line") == 0 && e->call.n_args == 0) {
                    emit(g, "urus_read_line()");
                    return;
                }
            }
            cg_expr(g, e->call.callee);
            emit(g, "(");
            for (int i = 0; i < e->call.n_args; i++) {
                if (i) emit(g, ", ");
                cg_expr(g, e->call.args[i]);
            }
            emit(g, ")");
            return;
        }

        case EX_METHOD_CALL: {
            /* Mangled call: Type__method(&recv, args...). We don't know the
               receiver type statically in v0.0.1 sema, so we let the C compiler
               complain if there's a mismatch. Use a small heuristic: if the
               receiver is an EX_IDENT that names a type used in an impl block,
               prefer that. Otherwise we emit a generic mangled call assuming
               the parser/sema is sound. */
            /* Find the type that owns a method with this name. */
            const char *type_name = NULL;
            for (int i = 0; i < g->mod->n_items; i++) {
                Item *it = g->mod->items[i];
                if (it->kind != IT_IMPL) continue;
                for (int j = 0; j < it->impl->n_methods; j++) {
                    if (strcmp(it->impl->methods[j]->name, e->method.name) == 0) {
                        type_name = it->impl->type_name;
                        break;
                    }
                }
                if (type_name) break;
            }
            if (!type_name) type_name = "Unknown";

            /* Static call?  `Type.method(args)` — the receiver is the type
             * name itself, not a value; no self argument is passed. */
            bool is_static = false;
            if (e->method.recv->kind == EX_IDENT) {
                for (int i = 0; i < g->mod->n_items; i++) {
                    Item *it = g->mod->items[i];
                    const char *tn =
                        it->kind == IT_STRUCT ? it->strct->name :
                        it->kind == IT_ENUM   ? it->enm->name   : NULL;
                    if (tn && strcmp(tn, e->method.recv->ident) == 0) {
                        is_static = true;
                        type_name = tn;   /* exact owner, skip the heuristic */
                        break;
                    }
                }
            }

            if (is_static) {
                emitf(g, "%s__%s(", type_name, e->method.name);
                for (int i = 0; i < e->method.n_args; i++) {
                    if (i) emit(g, ", ");
                    cg_expr(g, e->method.args[i]);
                }
                emit(g, ")");
                return;
            }

            emitf(g, "%s__%s(&(", type_name, e->method.name);
            cg_expr(g, e->method.recv);
            emit(g, ")");
            for (int i = 0; i < e->method.n_args; i++) {
                emit(g, ", ");
                cg_expr(g, e->method.args[i]);
            }
            emit(g, ")");
            return;
        }

        case EX_FIELD:
            cg_expr(g, e->field.obj);
            emit(g, ".");
            emit_ident(g, e->field.name);
            return;

        case EX_INDEX:
            cg_expr(g, e->index.obj);
            emit(g, "[");
            cg_expr(g, e->index.idx);
            emit(g, "]");
            return;

        case EX_IF:
            emit(g, "(");
            cg_expr(g, e->if_.cond);
            emit(g, " ? ");
            cg_expr(g, e->if_.then_blk);
            emit(g, " : ");
            if (e->if_.else_blk) cg_expr(g, e->if_.else_blk);
            else                 emit(g, "((void)0)");
            emit(g, ")");
            return;

        case EX_BLOCK:
            cg_block(g, e, true);
            return;

        case EX_RETURN:
            /* b028: flush ALL pending defers before returning.  For a
             * value return, the value is evaluated FIRST (into _ret),
             * then defers run, then the return — so a defer that
             * releases a resource cannot invalidate the value mid-
             * construction.  Value-less return just runs defers. */
            if (g->n_defers > 0) {
                if (e->return_.value) {
                    emit(g, "({ __auto_type _ret = (");
                    cg_expr(g, e->return_.value);
                    emit(g, "); ");
                    for (int i = g->n_defers - 1; i >= 0; i--) {
                        emit(g, "/* defer */ ");
                        cg_expr(g, g->defers[i]->defer_.expr);
                        emit(g, "; ");
                    }
                    emit(g, "return _ret; })");
                } else {
                    emit(g, "({ ");
                    for (int i = g->n_defers - 1; i >= 0; i--) {
                        emit(g, "/* defer */ ");
                        cg_expr(g, g->defers[i]->defer_.expr);
                        emit(g, "; ");
                    }
                    emit(g, "return; })");
                }
                return;
            }
            emit(g, "return");
            if (e->return_.value) { emit(g, " "); cg_expr(g, e->return_.value); }
            return;

        case EX_BREAK:    emit(g, "break"); return;
        case EX_CONTINUE: emit(g, "continue"); return;

        case EX_WHILE:
            emit(g, "({ while (");
            cg_expr(g, e->while_.cond);
            emit(g, ") ");
            cg_expr(g, e->while_.body);
            emit(g, "; (void)0; })");
            return;

        case EX_LOOP:
            emit(g, "({ for(;;) ");
            cg_expr(g, e->loop_.body);
            emit(g, "; (void)0; })");
            return;

        case EX_FOR: {
            /* For now: only supports `for x in start..end`.  b024: an
             * unsupported iterator (or a half-open range with a missing
             * bound) is a *diagnostic*, not a silently broken C comment. */
            if (!e->for_.iter || e->for_.iter->kind != EX_RANGE) {
                diag_error(g->diag, e->loc,
                           "v0.0.1 `for` only supports range iterators "
                           "(`for x in a..b` / `a..=b`)");
                emit(g, "((void)0)");
                return;
            }
            if (!e->for_.iter->range.start || !e->for_.iter->range.end) {
                diag_error(g->diag, e->loc,
                           "`for` needs both range bounds — "
                           "`a..` / `..b` cannot be iterated in v0.0.1");
                emit(g, "((void)0)");
                return;
            }
            const char *var = "_i";
            if (e->for_.pat && e->for_.pat->kind == PAT_IDENT)
                var = e->for_.pat->ident.name;
            emit(g, "({ for (int64_t ");
            emit_ident(g, var);
            emit(g, " = ");
            cg_expr(g, e->for_.iter->range.start);
            emit(g, "; ");
            emit_ident(g, var);
            emit(g, e->for_.iter->range.inclusive ? " <= " : " < ");
            cg_expr(g, e->for_.iter->range.end);
            emit(g, "; ");
            emit_ident(g, var);
            emit(g, "++) ");
            cg_expr(g, e->for_.body);
            emit(g, "; (void)0; })");
            return;
        }

        case EX_STRUCT_LIT: {
            emit(g, "((");
            emit_ident(g, e->struct_lit.name);
            emit(g, "){");
            for (int i = 0; i < e->struct_lit.n_fields; i++) {
                if (i) emit(g, ", ");
                emit(g, ".");
                emit_ident(g, e->struct_lit.fields[i].name);
                emit(g, " = ");
                cg_expr(g, e->struct_lit.fields[i].value);
            }
            emit(g, "})");
            return;
        }

        case EX_TUPLE_LIT: emit(g, "/*tuple*/0"); return;
        case EX_ARRAY_LIT:
            emit(g, "{");
            for (int i = 0; i < e->array_lit.n; i++) {
                if (i) emit(g, ", ");
                cg_expr(g, e->array_lit.elems[i]);
            }
            emit(g, "}");
            return;

        case EX_CAST:
            emit(g, "((");
            emit_type(g, e->cast.ty);
            emit(g, ")(");
            cg_expr(g, e->cast.expr);
            emit(g, "))");
            return;

        case EX_REF:
            emit(g, "(&(");
            cg_expr(g, e->ref_.inner);
            emit(g, "))");
            return;

        case EX_DEREF:
            emit(g, "(*(");
            cg_expr(g, e->deref.inner);
            emit(g, "))");
            return;

        case EX_RANGE:
            emit(g, "/*range*/0");
            return;

        case EX_TRY:
            /* expr? on a Result: if Err, return it; otherwise yield payload.
               v0.0.1 limits ? to Result. Option support waits for typed
               distinction in v0.0.2 (Result and Option share the wire layout
               today, so a single helper cannot tell them apart at runtime). */
            emit(g, "({ urus_Result _t = (");
            cg_expr(g, e->try_.inner);
            emit(g, "); if (urus_is_err(_t)) { ");
            /* b028: `?` is an early return — pending defers must fire on
             * the error path too, after _t is already materialised. */
            for (int i = g->n_defers - 1; i >= 0; i--) {
                emit(g, "/* defer */ ");
                cg_expr(g, g->defers[i]->defer_.expr);
                emit(g, "; ");
            }
            emit(g, "return _t; } ");
            /* F-TY-2 read side: yield the Ok payload through the right
             * union arm based on the inner expression's Result<T,_>. */
            {
                TypeExpr *inner_t = cg_expr_type(g, e->try_.inner);
                TypeExpr *ok_t = (inner_t && inner_t->kind == TY_GENERIC &&
                                  inner_t->generic.n_args >= 1)
                                     ? inner_t->generic.args[0] : NULL;
                emitf(g, "%s(_t); })", payload_accessor(ok_t));
            }
            return;

        case EX_MATCH: {
            /* Minimal: compile to a *value-producing* conditional chain
               so `let x = match … { … }` and tail-position matches work:

                   ({ __auto_type _scrut = (scrut);
                      cond1 ? ({ bind1; body1; })
                            : cond2 ? ({ bind2; body2; })
                                    : ({ bindN; bodyN; }); })

               The final arm is emitted unconditionally — sema's
               exhaustiveness check (F-COMP-3) guarantees it is reached
               only when everything before it failed.  v0.0.1 supports
               Ok/Err/Some/None, integer literals, and ident/wildcard
               catch-alls; guards are sema-only for now. */
            TypeExpr *scrut_t = cg_expr_type(g, e->match_.scrut);
            emit(g, "({ __auto_type _scrut = (");
            cg_expr(g, e->match_.scrut);
            emit(g, "); ");
            int n = e->match_.n_arms;
            for (int i = 0; i < n; i++) {
                Pattern *p = e->match_.arms[i].pat;
                bool catchall = (p->kind == PAT_WILDCARD || p->kind == PAT_IDENT);
                bool terminal = catchall || i == n - 1;

                if (!terminal) {
                    emit(g, "(");
                    if (p->kind == PAT_LITERAL) {
                        emit(g, "_scrut == (");
                        cg_expr(g, p->literal);
                        emit(g, ")");
                    } else if (p->kind == PAT_ENUM_VARIANT) {
                        if (strcmp(p->variant.name, "Ok")   == 0)      emit(g, "urus_is_ok(_scrut)");
                        else if (strcmp(p->variant.name, "Err")  == 0) emit(g, "urus_is_err(_scrut)");
                        else if (strcmp(p->variant.name, "Some") == 0) emit(g, "urus_is_some(_scrut)");
                        else if (strcmp(p->variant.name, "None") == 0) emit(g, "urus_is_none(_scrut)");
                        else {
                            /* User enum variant: compare the tag field. */
                            EnumDecl *ed = cg_find_enum_of_variant(g, p->variant.name);
                            if (ed) emitf(g, "_scrut.tag == %s_%s", ed->name, p->variant.name);
                            else    emit(g, "1 /*unknown variant*/");
                        }
                    } else {
                        emit(g, "1 /*unsupported pattern*/");
                    }
                    emit(g, ") ? ");
                }

                emit(g, "({ ");
                if (p->kind == PAT_ENUM_VARIANT &&
                    p->variant.n == 1 && p->variant.subs[0]->kind == PAT_IDENT) {
                    /* F-TY-2 read side: pick the union arm matching the
                     * variant's payload type (str/f64/u64/…). */
                    TypeExpr *pt = variant_payload_type(scrut_t, p->variant.name);
                    emitf(g, "__auto_type %s = %s(_scrut); ",
                          p->variant.subs[0]->ident.name, payload_accessor(pt));
                    cg_local_add(g, p->variant.subs[0]->ident.name, pt);
                } else if (p->kind == PAT_IDENT) {
                    emitf(g, "__auto_type %s = _scrut; (void)%s; ",
                          p->ident.name, p->ident.name);
                }
                cg_expr(g, e->match_.arms[i].body);
                emit(g, "; })");

                if (terminal) break;
                emit(g, " : ");
            }
            emit(g, "; })");
            return;
        }
    }
}

/* ---------- statements / blocks ---------- */

static void cg_let(CG *g, Stmt *st) {
    /* Track the binding's type for payload-accessor selection.  The
     * annotation wins; otherwise infer from the initializer (call /
     * ident / try). */
    if (st->let_.pat && st->let_.pat->kind == PAT_IDENT) {
        TypeExpr *t = st->let_.type_annot ? st->let_.type_annot
                                          : cg_expr_type(g, st->let_.init);
        cg_local_add(g, st->let_.pat->ident.name, t);
    }
    emit_indent(g);
    /* Use auto-type so we don't have to fully infer in v0.0.1. */
    if (st->let_.type_annot) {
        emit_type(g, st->let_.type_annot);
        emit(g, " ");
    } else {
        emit(g, "__auto_type ");
    }
    if (st->let_.pat && st->let_.pat->kind == PAT_IDENT) {
        emit_ident(g, st->let_.pat->ident.name);
    } else {
        emit(g, "_tmp");
    }
    if (st->let_.init) {
        emit(g, " = ");
        cg_expr(g, st->let_.init);
    }
    emit(g, ";\n");
}

static void cg_stmt(CG *g, Stmt *st) {
    switch (st->kind) {
        case ST_LET:   cg_let(g, st); break;
        case ST_EXPR:
            emit_indent(g);
            cg_expr(g, st->expr_.expr);
            emit(g, ";\n");
            break;
        case ST_DEFER:
            /* defer is handled by cg_block — it collects them and emits at
               block exit. If we ever see one here it's a bare-statement-stream
               and we should warn (currently no-op). */
            break;
        case ST_ITEM:
            break;
    }
}

/* b028: defers are tracked on a per-function stack in CG.
 *
 * cg_block records a watermark on entry and pushes each ST_DEFER as it
 * walks the statements *in order*; flushing emits from the top of the
 * stack down to a floor (LIFO).  Block end flushes to the block's own
 * watermark; EX_RETURN flushes the *entire* stack (floor 0) so an early
 * return runs every pending defer in every enclosing block before the
 * `return` statement itself.
 *
 * Defer expressions must not contain `return` themselves — sema does not
 * enforce that yet (v0.0.2); the parser's grammar makes it unlikely. */
static void cg_flush_defers(CG *g, int floor) {
    for (int i = g->n_defers - 1; i >= floor; i--) {
        emit_indent(g);
        emit(g, "/* defer */ ");
        cg_expr(g, g->defers[i]->defer_.expr);
        emit(g, ";\n");
    }
}

static void cg_block(CG *g, Expr *blk, bool is_expr_position) {
    int watermark = g->n_defers;

    if (is_expr_position) {
        /* GCC statement-expression: ({ ... ; tail }) */
        emit(g, "({");
        g->indent++;
        emit(g, "\n");
        for (int i = 0; i < blk->block.n_stmts; i++) {
            Stmt *st = blk->block.stmts[i];
            if (st->kind == ST_DEFER) {
                if (g->n_defers < URUS_CG_MAX_DEFERS) {
                    g->defers[g->n_defers++] = st;
                } else {
                    diag_error(g->diag, st->loc,
                               "too many pending `defer`s (max %d per function)",
                               URUS_CG_MAX_DEFERS);
                }
                continue;
            }
            cg_stmt(g, st);
        }
        cg_flush_defers(g, watermark);
        if (blk->block.tail) {
            emit_indent(g);
            cg_expr(g, blk->block.tail);
            emit(g, ";\n");
        } else {
            emit_indent(g);
            emit(g, "(void)0;\n");
        }
        g->indent--;
        emit_indent(g);
        emit(g, "})");
    } else {
        emit(g, "{\n");
        g->indent++;
        for (int i = 0; i < blk->block.n_stmts; i++) {
            Stmt *st = blk->block.stmts[i];
            if (st->kind == ST_DEFER) {
                if (g->n_defers < URUS_CG_MAX_DEFERS) {
                    g->defers[g->n_defers++] = st;
                } else {
                    diag_error(g->diag, st->loc,
                               "too many pending `defer`s (max %d per function)",
                               URUS_CG_MAX_DEFERS);
                }
                continue;
            }
            cg_stmt(g, st);
        }
        cg_flush_defers(g, watermark);
        if (blk->block.tail) {
            /* Tail return: evaluate the value FIRST, then nothing left to
               defer here (already flushed above), then return.  Note the
               flush above runs before tail evaluation — acceptable in
               v0.0.1 because tails are values, not resource users; the
               value-before-defers ordering is only critical for explicit
               `return expr`, handled at EX_RETURN.

               Two shapes need a plain statement instead of `return expr;`:
               - tail is itself control flow (EX_RETURN/EX_BREAK/EX_CONTINUE)
                 — prefixing `return` would emit `return return …`;
               - the fn returns void — `return expr;` is a C constraint
                 violation, so evaluate the tail for effect only. */
            Expr *tail = blk->block.tail;
            bool stmt_like = (tail->kind == EX_RETURN ||
                              tail->kind == EX_BREAK  ||
                              tail->kind == EX_CONTINUE);
            emit_indent(g);
            if (!stmt_like && !g->fn_ret_void) emit(g, "return ");
            cg_expr(g, tail);
            emit(g, ";\n");
        }
        g->indent--;
        emit_indent(g);
        emit(g, "}\n");
    }

    /* Pop this block's defers off the stack — they are out of scope. */
    g->n_defers = watermark;
}

/* ---------- items ---------- */

static void cg_struct(CG *g, StructDecl *sd) {
    emitf(g, "typedef struct %s {\n", sd->name);
    for (int i = 0; i < sd->n_fields; i++) {
        g->indent = 1;
        emit_indent(g);
        emit_type(g, sd->fields[i].type);
        emit(g, " ");
        emit_ident(g, sd->fields[i].name);
        emit(g, ";\n");
        g->indent = 0;
    }
    emitf(g, "} %s;\n\n", sd->name);
}

static void cg_enum(CG *g, EnumDecl *ed) {
    emitf(g, "typedef enum %s_Tag {\n", ed->name);
    for (int i = 0; i < ed->n_variants; i++) {
        emitf(g, "    %s_%s,\n", ed->name, ed->variants[i].name);
    }
    emitf(g, "} %s_Tag;\n\n", ed->name);

    emitf(g, "typedef struct %s {\n    %s_Tag tag;\n", ed->name, ed->name);
    /* simple v0.0.1: store first payload field as int64 / pointer */
    bool has_payload = false;
    for (int i = 0; i < ed->n_variants; i++) {
        if (ed->variants[i].n_payload > 0) { has_payload = true; break; }
    }
    if (has_payload) {
        emit(g, "    union {\n");
        for (int i = 0; i < ed->n_variants; i++) {
            EnumVariant *v = &ed->variants[i];
            if (v->n_payload == 0) continue;
            emitf(g, "        struct { ");
            for (int j = 0; j < v->n_payload; j++) {
                emit_type(g, v->payload[j].type);
                emitf(g, " _%d; ", j);
            }
            emitf(g, "} %s;\n", v->name);
        }
        emit(g, "    } as;\n");
    }
    emitf(g, "} %s;\n\n", ed->name);
}

static void cg_fn(CG *g, FnDecl *f, const char *owner_type) {
    /* return type */
    if (f->ret_type) emit_type(g, f->ret_type);
    else             emit(g, "void");
    emit(g, " ");
    if (owner_type) emitf(g, "%s__%s", owner_type, f->name);
    else            emit_ident(g, f->name);
    emit(g, "(");
    bool wrote = false;
    for (int i = 0; i < f->n_params; i++) {
        FnParam *fp = &f->params[i];
        if (i) emit(g, ", ");
        wrote = true;
        if (fp->is_self) {
            if (owner_type) {
                emitf(g, "%s %sself_", owner_type, fp->is_ref_self ? "*" : "");
            } else {
                emit(g, "void *self_");
            }
        } else {
            emit_type(g, fp->type);
            emit(g, " ");
            if (fp->pat && fp->pat->kind == PAT_IDENT) emit_ident(g, fp->pat->ident.name);
            else                                       emitf(g, "_p%d", i);
        }
    }
    if (!wrote) emit(g, "void");
    emit(g, ") ");

    g->cur_type = owner_type;
    g->indent = 0;
    g->fn_ret_void = (!f->ret_type || f->ret_type->kind == TY_UNIT);
    g->n_locals = 0;  /* fresh type-tracking scope per function */
    g->fn_has_self = false;
    g->fn_self_is_ref = false;
    for (int i = 0; i < f->n_params; i++) {
        FnParam *fp = &f->params[i];
        if (fp->is_self) {
            g->fn_has_self = true;
            g->fn_self_is_ref = fp->is_ref_self;
        } else if (fp->pat && fp->pat->kind == PAT_IDENT) {
            cg_local_add(g, fp->pat->ident.name, fp->type);
        }
    }
    if (f->body) cg_block(g, f->body, false);
    else         emit(g, "{}\n");
    g->cur_type = NULL;
    g->fn_ret_void = false;
    g->n_locals = 0;
    g->fn_has_self = false;
    emit(g, "\n");
}

static void cg_forward_decls(CG *g) {
    for (int i = 0; i < g->mod->n_items; i++) {
        Item *it = g->mod->items[i];
        if (it->kind == IT_STRUCT) emitf(g, "typedef struct %s %s;\n", it->strct->name, it->strct->name);
        if (it->kind == IT_ENUM)   emitf(g, "typedef struct %s %s;\n", it->enm->name,   it->enm->name);
    }
    emit(g, "\n");

    /* fn prototypes */
    for (int i = 0; i < g->mod->n_items; i++) {
        Item *it = g->mod->items[i];
        if (it->kind == IT_FN) {
            FnDecl *f = it->fn;
            if (f->ret_type) emit_type(g, f->ret_type);
            else             emit(g, "void");
            emit(g, " ");
            emit_ident(g, f->name);
            emit(g, "(");
            bool wrote = false;
            for (int j = 0; j < f->n_params; j++) {
                if (j) emit(g, ", ");
                wrote = true;
                if (f->params[j].is_self) emit(g, "void*");
                else                      emit_type(g, f->params[j].type);
            }
            if (!wrote) emit(g, "void");
            emit(g, ");\n");
        } else if (it->kind == IT_IMPL) {
            for (int j = 0; j < it->impl->n_methods; j++) {
                FnDecl *f = it->impl->methods[j];
                if (f->ret_type) emit_type(g, f->ret_type);
                else             emit(g, "void");
                emitf(g, " %s__%s(", it->impl->type_name, f->name);
                bool wrote = false;
                for (int k = 0; k < f->n_params; k++) {
                    if (k) emit(g, ", ");
                    wrote = true;
                    if (f->params[k].is_self) emitf(g, "%s%s", it->impl->type_name, f->params[k].is_ref_self ? "*" : "");
                    else                      emit_type(g, f->params[k].type);
                }
                if (!wrote) emit(g, "void");
                emit(g, ");\n");
            }
        }
    }
    emit(g, "\n");
}

bool codegen_c_emit(StrBuf *sb, Module *m, DiagCtx *diag) {
    CG g = { .sb = sb, .mod = m, .diag = diag, .indent = 0, .cur_type = NULL };

    emit(&g, "/* === Generated by URUS " URUS_VERSION " — DO NOT EDIT === */\n");
    emit(&g, "/* Requires a C11 compiler with statement-expressions and __auto_type. */\n");
    emit(&g, "/* GCC / Clang / clang-cl: supported. Native MSVC (cl.exe): rejected. */\n");
    emit(&g, "#if defined(_MSC_VER) && !defined(__clang__)\n");
    emit(&g, "#  error \"URUS-emitted C requires Clang, GCC, or clang-cl. Native MSVC is not supported. Use clang-cl on Windows.\"\n");
    emit(&g, "#endif\n");
    emit(&g, "#include \"urus_rt.h\"\n\n");

    /* Type decls */
    for (int i = 0; i < m->n_items; i++) {
        Item *it = m->items[i];
        if (it->kind == IT_STRUCT) cg_struct(&g, it->strct);
        if (it->kind == IT_ENUM)   cg_enum(&g, it->enm);
    }

    cg_forward_decls(&g);

    /* Fn definitions */
    for (int i = 0; i < m->n_items; i++) {
        Item *it = m->items[i];
        if (it->kind == IT_FN) cg_fn(&g, it->fn, NULL);
        if (it->kind == IT_IMPL) {
            for (int j = 0; j < it->impl->n_methods; j++) {
                cg_fn(&g, it->impl->methods[j], it->impl->type_name);
            }
        }
        if (it->kind == IT_CONST) {
            emit(&g, "static const ");
            emit_type(&g, it->cnst->type);
            emit(&g, " ");
            emit_ident(&g, it->cnst->name);
            emit(&g, " = ");
            cg_expr(&g, it->cnst->value);
            emit(&g, ";\n\n");
        }
    }

    /* Add a tiny shim if user defined `main` returning Result. We always emit
       a C `main` that calls user's `urus_user_main`. To keep v0.0.1 simple we
       just rely on the user's `main` having signature `void main()` or
       `Result main()`. The C compiler will accept either via this shim. */
    emit(&g, "\n#ifndef URUS_MAIN_SHIM_DEFINED\n");
    emit(&g, "#define URUS_MAIN_SHIM_DEFINED\n");
    emit(&g, "int main(int argc, char** argv) {\n");
    emit(&g, "    urus_runtime_init(argc, argv);\n");
    emit(&g, "    extern void main_(void);\n");  /* if user has `main`, it's renamed `main_` because `main` is a C keyword in our reserved list */
    emit(&g, "    main_();\n");
    emit(&g, "    return 0;\n");
    emit(&g, "}\n");
    emit(&g, "#endif\n");

    return !diag_has_errors(diag);
}
