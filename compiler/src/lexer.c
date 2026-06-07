#include "lexer.h"
#include "arena.h"
#include "diag.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

static const struct { const char *s; TokKind k; } KEYWORDS[] = {
    {"module",   TOK_KW_MODULE},
    {"use",      TOK_KW_USE},
    {"fn",       TOK_KW_FN},
    {"struct",   TOK_KW_STRUCT},
    {"impl",     TOK_KW_IMPL},
    {"enum",     TOK_KW_ENUM},
    {"trait",    TOK_KW_TRAIT},
    {"type",     TOK_KW_TYPE},
    {"let",      TOK_KW_LET},
    {"mut",      TOK_KW_MUT},
    {"const",    TOK_KW_CONST},
    {"pub",      TOK_KW_PUB},
    {"if",       TOK_KW_IF},
    {"else",     TOK_KW_ELSE},
    {"match",    TOK_KW_MATCH},
    {"return",   TOK_KW_RETURN},
    {"while",    TOK_KW_WHILE},
    {"for",      TOK_KW_FOR},
    {"in",       TOK_KW_IN},
    {"loop",     TOK_KW_LOOP},
    {"break",    TOK_KW_BREAK},
    {"continue", TOK_KW_CONTINUE},
    {"true",     TOK_KW_TRUE},
    {"false",    TOK_KW_FALSE},
    {"self",     TOK_KW_SELF},
    {"Self",     TOK_KW_SELF_TYPE},
    {"as",       TOK_KW_AS},
    {"defer",    TOK_KW_DEFER},
};

void lexer_init(Lexer *lx, const char *source, const char *file,
                DiagCtx *diag, Arena *arena) {
    lx->source = source;
    lx->cur    = source;
    lx->file   = file;
    lx->line   = 1;
    lx->col    = 1;
    lx->diag   = diag;
    lx->arena  = arena;
}

static URUS_INLINE char peek0(Lexer *lx) { return *lx->cur; }
static URUS_INLINE char peek1(Lexer *lx) { return *lx->cur ? lx->cur[1] : '\0'; }

static char advance(Lexer *lx) {
    char c = *lx->cur++;
    if (c == '\n') { lx->line++; lx->col = 1; }
    else           { lx->col++; }
    return c;
}

static SrcLoc loc_here(Lexer *lx, const char *start) {
    SrcLoc l;
    l.file   = lx->file;
    l.line   = lx->line;
    l.col    = lx->col;
    l.offset = (uint32_t)(start - lx->source);
    l.length = (uint32_t)(lx->cur - start);
    return l;
}

static void skip_whitespace_and_comments(Lexer *lx) {
    for (;;) {
        char c = peek0(lx);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance(lx);
        } else if (c == '/' && peek1(lx) == '/') {
            while (peek0(lx) && peek0(lx) != '\n') advance(lx);
        } else if (c == '/' && peek1(lx) == '*') {
            advance(lx); advance(lx);
            int depth = 1;
            while (peek0(lx) && depth > 0) {
                if (peek0(lx) == '/' && peek1(lx) == '*') {
                    advance(lx); advance(lx); depth++;
                } else if (peek0(lx) == '*' && peek1(lx) == '/') {
                    advance(lx); advance(lx); depth--;
                } else {
                    advance(lx);
                }
            }
        } else {
            return;
        }
    }
}

static TokKind kw_lookup(const char *s, size_t len) {
    for (size_t i = 0; i < sizeof(KEYWORDS) / sizeof(KEYWORDS[0]); i++) {
        if (strlen(KEYWORDS[i].s) == len && memcmp(KEYWORDS[i].s, s, len) == 0)
            return KEYWORDS[i].k;
    }
    return TOK_IDENT;
}

static Token make_tok(Lexer *lx, TokKind k, const char *start) {
    Token t;
    t.kind   = k;
    t.start  = start;
    t.length = (uint32_t)(lx->cur - start);
    t.loc.file   = lx->file;
    t.loc.line   = lx->line;
    t.loc.col    = lx->col - t.length;
    t.loc.offset = (uint32_t)(start - lx->source);
    t.loc.length = t.length;
    memset(&t.v, 0, sizeof(t.v));
    return t;
}

static Token lex_number(Lexer *lx) {
    const char *start = lx->cur;
    bool is_float = false;
    bool is_hex   = false;
    bool is_bin   = false;
    bool is_oct   = false;

    if (peek0(lx) == '0' && (peek1(lx) == 'x' || peek1(lx) == 'X')) {
        advance(lx); advance(lx); is_hex = true;
        while (isxdigit((unsigned char)peek0(lx)) || peek0(lx) == '_') advance(lx);
    } else if (peek0(lx) == '0' && (peek1(lx) == 'b' || peek1(lx) == 'B')) {
        advance(lx); advance(lx); is_bin = true;
        while (peek0(lx) == '0' || peek0(lx) == '1' || peek0(lx) == '_') advance(lx);
    } else if (peek0(lx) == '0' && (peek1(lx) == 'o' || peek1(lx) == 'O')) {
        advance(lx); advance(lx); is_oct = true;
        while ((peek0(lx) >= '0' && peek0(lx) <= '7') || peek0(lx) == '_') advance(lx);
    } else {
        while (isdigit((unsigned char)peek0(lx)) || peek0(lx) == '_') advance(lx);
        if (peek0(lx) == '.' && isdigit((unsigned char)peek1(lx))) {
            is_float = true;
            advance(lx);
            while (isdigit((unsigned char)peek0(lx)) || peek0(lx) == '_') advance(lx);
        }
        if (peek0(lx) == 'e' || peek0(lx) == 'E') {
            is_float = true;
            advance(lx);
            if (peek0(lx) == '+' || peek0(lx) == '-') advance(lx);
            while (isdigit((unsigned char)peek0(lx))) advance(lx);
        }
    }

    /* optional type suffix: i32, u64, f32, f64, etc. — skipped for v0.0.1
       but tolerated so users can write 10u32; sema decides later. */
    while (isalnum((unsigned char)peek0(lx))) advance(lx);

    Token t = make_tok(lx, is_float ? TOK_FLOAT : TOK_INT, start);

    /* Parse value (best-effort).  Strips underscores, drops a trailing
     * type suffix (`123u32`, `3.14f64`) so the numeric core is what we
     * hand to strtoull / strtod.  Overflow is *detected* (b016 Tier-1
     * hardening): pre-b016 we silently used the saturated ULLONG_MAX
     * that strtoull returns, which means a source like `999999999999u8`
     * lexed as `2^64 - 1` instead of triggering a diagnostic. */
    size_t len = t.length;
    char  *buf = (char *)malloc(len + 1);
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (start[i] != '_') buf[j++] = start[i];
    }
    buf[j] = '\0';

    /* Trim trailing suffix: scan back to the first byte that is not a
     * letter (or 'e' / 'E' inside a float exponent, which we leave alone
     * because is_float==true triggered earlier). */
    size_t core_len = j;
    while (core_len > 0) {
        char c = buf[core_len - 1];
        if (isalpha((unsigned char)c) && !(is_float && (c == 'e' || c == 'E'))) {
            core_len--;
        } else break;
    }
    /* Be defensive: if the user wrote `1.0e` with no exponent digits, leave it
     * to strtod to fail gracefully — don't chop further. */

    char saved = buf[core_len];
    buf[core_len] = '\0';

    errno = 0;
    if (is_float) {
        t.v.float_val = strtod(buf, NULL);
        if (errno == ERANGE) {
            diag_error(lx->diag, t.loc,
                       "floating-point literal out of range for f64");
        }
    } else {
        const char *digits = buf;
        int base = 10;
        if      (is_hex) { digits = buf + 2; base = 16; }
        else if (is_bin) { digits = buf + 2; base = 2;  }
        else if (is_oct) { digits = buf + 2; base = 8;  }

        if (*digits == '\0') {
            diag_error(lx->diag, t.loc,
                       "integer literal has no digits after base prefix");
            t.v.int_val = 0;
        } else {
            char *endp = NULL;
            unsigned long long v = strtoull(digits, &endp, base);
            if (errno == ERANGE || v > (unsigned long long)UINT64_MAX) {
                diag_error(lx->diag, t.loc,
                           "integer literal does not fit in u64 (max %llu)",
                           (unsigned long long)UINT64_MAX);
                t.v.int_val = 0;
            } else if (endp == digits) {
                diag_error(lx->diag, t.loc,
                           "integer literal failed to parse");
                t.v.int_val = 0;
            } else {
                t.v.int_val = (uint64_t)v;
            }
        }
    }
    buf[core_len] = saved;
    free(buf);
    return t;
}

static int unescape(Lexer *lx, char c) {
    switch (c) {
        case 'n':  return '\n';
        case 'r':  return '\r';
        case 't':  return '\t';
        case '0':  return '\0';
        case '\\': return '\\';
        case '\'': return '\'';
        case '"':  return '"';
        case '{':  return '{';   /* {{ in strings */
        case '}':  return '}';
        default: {
            SrcLoc l = loc_here(lx, lx->cur - 1);
            diag_warn(lx->diag, l, "unknown escape sequence '\\%c'", c);
            return c;
        }
    }
}

/*
 * Hard upper bound on a single string literal.  Closes Tier-0 item #12.
 * Picked so that even a pathological 16 MiB literal fits comfortably in
 * a sensible host but a 4 GiB attack does not.
 */
#define URUS_MAX_STR_LITERAL_BYTES (16ull * 1024ull * 1024ull)

static Token lex_string(Lexer *lx) {
    const char *start = lx->cur;
    advance(lx);  /* opening " */

    /*
     * Build the unescaped contents in a growable heap buffer, then copy
     * once into the arena.  Hardening compared to v0.0.1-b001:
     *   - realloc result is checked and the old pointer is freed on
     *     failure (closes F-MEM-2);
     *   - growth is capped at URUS_MAX_STR_LITERAL_BYTES — past that we
     *     emit a fatal diagnostic and stop consuming bytes.
     */
    size_t cap = 32, len = 0;
    char  *buf = (char *)malloc(cap);
    if (!buf) {
        SrcLoc l = loc_here(lx, start);
        diag_error(lx->diag, l, "out of memory while reading string literal");
        Token t = make_tok(lx, TOK_STR, start);
        t.v.str_val.ptr = "";
        t.v.str_val.len = 0;
        return t;
    }

    bool too_big = false;

    while (peek0(lx) && peek0(lx) != '"') {
        char c;
        if (peek0(lx) == '\\') {
            advance(lx);
            if (peek0(lx) == '\0') break;  /* `\` at EOF — same guard as
                                              lex_char; the unterminated-
                                              literal diag fires below. */
            c = (char)unescape(lx, peek0(lx));
            advance(lx);
        } else {
            c = advance(lx);
        }

        if (!too_big && len + 1 >= cap) {
            /* Refuse to grow past the cap. */
            if ((size_t)cap > (size_t)URUS_MAX_STR_LITERAL_BYTES / 2) {
                SrcLoc l = loc_here(lx, start);
                diag_error(lx->diag, l,
                           "string literal exceeds the %llu-byte limit",
                           (unsigned long long)URUS_MAX_STR_LITERAL_BYTES);
                too_big = true;
            } else {
                size_t new_cap = cap * 2;
                char  *grown   = (char *)realloc(buf, new_cap);
                if (!grown) {
                    /* realloc failed — old pointer is still valid; free it. */
                    SrcLoc l = loc_here(lx, start);
                    diag_error(lx->diag, l,
                               "out of memory while growing string literal");
                    too_big = true;
                } else {
                    buf = grown;
                    cap = new_cap;
                }
            }
        }

        if (!too_big) {
            buf[len++] = c;
        }
    }

    if (peek0(lx) != '"') {
        SrcLoc l = loc_here(lx, start);
        diag_error(lx->diag, l, "unterminated string literal");
    } else {
        advance(lx);  /* closing " */
    }

    Token t = make_tok(lx, TOK_STR, start);
    char *stored = (char *)arena_alloc(lx->arena, len + 1);
    urus_memcpy(stored, buf, len);
    stored[len] = '\0';
    free(buf);
    t.v.str_val.ptr = stored;
    t.v.str_val.len = (uint32_t)len;
    return t;
}

static Token lex_char(Lexer *lx) {
    const char *start = lx->cur;
    advance(lx);   /* ' */
    int value = 0;
    /* Never advance past the NUL terminator: `'` (or `'\`) at EOF must
     * diagnose, not walk off the end of the buffer (fuzzer finding,
     * heap-buffer-overflow in peek0). */
    if (peek0(lx) == '\0') {
        SrcLoc l = loc_here(lx, start);
        diag_error(lx->diag, l, "unterminated char literal");
        Token t = make_tok(lx, TOK_CHAR, start);
        t.v.char_val = 0;
        return t;
    }
    if (peek0(lx) == '\\') {
        advance(lx);
        if (peek0(lx) == '\0') {
            SrcLoc l = loc_here(lx, start);
            diag_error(lx->diag, l, "unterminated char literal");
            Token t = make_tok(lx, TOK_CHAR, start);
            t.v.char_val = 0;
            return t;
        }
        value = unescape(lx, advance(lx));
    } else {
        value = (unsigned char)advance(lx);
    }
    if (peek0(lx) != '\'') {
        SrcLoc l = loc_here(lx, start);
        diag_error(lx->diag, l, "unterminated char literal");
    } else {
        advance(lx);
    }
    Token t = make_tok(lx, TOK_CHAR, start);
    t.v.char_val = (uint32_t)value;
    return t;
}

static Token lex_ident(Lexer *lx) {
    const char *start = lx->cur;
    while (isalnum((unsigned char)peek0(lx)) || peek0(lx) == '_') advance(lx);
    Token t = make_tok(lx, TOK_IDENT, start);
    TokKind k = kw_lookup(start, t.length);
    t.kind = k;
    return t;
}

#define ONE(c, kind) case c: advance(lx); return make_tok(lx, kind, start)

Token lexer_next(Lexer *lx) {
    skip_whitespace_and_comments(lx);
    const char *start = lx->cur;

    if (*lx->cur == '\0') return make_tok(lx, TOK_EOF, start);

    char c = peek0(lx);

    if (isdigit((unsigned char)c))                return lex_number(lx);
    if (c == '"')                                 return lex_string(lx);
    if (c == '\'')                                return lex_char(lx);
    if (c == 'f' && peek1(lx) == '"') {
        /* f-string: same scan as a regular string, marked as TOK_FSTR.
           Placeholder splitting is performed by codegen. */
        advance(lx);   /* consume 'f' */
        Token t = lex_string(lx);
        t.kind = TOK_FSTR;
        return t;
    }
    if (isalpha((unsigned char)c) || c == '_')    return lex_ident(lx);

    switch (c) {
        ONE('(', TOK_LPAREN);
        ONE(')', TOK_RPAREN);
        ONE('{', TOK_LBRACE);
        ONE('}', TOK_RBRACE);
        ONE('[', TOK_LBRACKET);
        ONE(']', TOK_RBRACKET);
        ONE(',', TOK_COMMA);
        ONE(';', TOK_SEMI);
        ONE('@', TOK_AT);
        ONE('~', TOK_TILDE);
        ONE('?', TOK_QUESTION);
        ONE('^', TOK_CARET);

        case ':':
            advance(lx);
            if (peek0(lx) == ':') { advance(lx); return make_tok(lx, TOK_COLONCOLON, start); }
            return make_tok(lx, TOK_COLON, start);

        case '-':
            advance(lx);
            if (peek0(lx) == '>') { advance(lx); return make_tok(lx, TOK_ARROW, start); }
            if (peek0(lx) == '=') { advance(lx); return make_tok(lx, TOK_MINUSEQ, start); }
            return make_tok(lx, TOK_MINUS, start);

        case '=':
            advance(lx);
            if (peek0(lx) == '=') { advance(lx); return make_tok(lx, TOK_EQEQ, start); }
            if (peek0(lx) == '>') { advance(lx); return make_tok(lx, TOK_FATARROW, start); }
            return make_tok(lx, TOK_ASSIGN, start);

        case '.':
            advance(lx);
            if (peek0(lx) == '.') {
                advance(lx);
                if (peek0(lx) == '=') { advance(lx); return make_tok(lx, TOK_DOTDOTEQ, start); }
                return make_tok(lx, TOK_DOTDOT, start);
            }
            return make_tok(lx, TOK_DOT, start);

        case '&':
            advance(lx);
            if (peek0(lx) == '&') { advance(lx); return make_tok(lx, TOK_AMPAMP, start); }
            return make_tok(lx, TOK_AMP, start);

        case '|':
            advance(lx);
            if (peek0(lx) == '|') { advance(lx); return make_tok(lx, TOK_PIPEPIPE, start); }
            return make_tok(lx, TOK_PIPE, start);

        case '!':
            advance(lx);
            if (peek0(lx) == '=') { advance(lx); return make_tok(lx, TOK_NEQ, start); }
            return make_tok(lx, TOK_BANG, start);

        case '+':
            advance(lx);
            if (peek0(lx) == '=') { advance(lx); return make_tok(lx, TOK_PLUSEQ, start); }
            return make_tok(lx, TOK_PLUS, start);

        case '*':
            advance(lx);
            if (peek0(lx) == '=') { advance(lx); return make_tok(lx, TOK_STAREQ, start); }
            return make_tok(lx, TOK_STAR, start);

        case '/':
            advance(lx);
            if (peek0(lx) == '=') { advance(lx); return make_tok(lx, TOK_SLASHEQ, start); }
            return make_tok(lx, TOK_SLASH, start);

        case '%':
            advance(lx);
            if (peek0(lx) == '=') { advance(lx); return make_tok(lx, TOK_PERCENTEQ, start); }
            return make_tok(lx, TOK_PERCENT, start);

        case '<':
            advance(lx);
            if (peek0(lx) == '=') { advance(lx); return make_tok(lx, TOK_LE, start); }
            if (peek0(lx) == '<') { advance(lx); return make_tok(lx, TOK_SHL, start); }
            return make_tok(lx, TOK_LT, start);

        case '>':
            advance(lx);
            if (peek0(lx) == '=') { advance(lx); return make_tok(lx, TOK_GE, start); }
            if (peek0(lx) == '>') { advance(lx); return make_tok(lx, TOK_SHR, start); }
            return make_tok(lx, TOK_GT, start);
    }

    /* unknown */
    SrcLoc l = loc_here(lx, start);
    advance(lx);
    diag_error(lx->diag, l, "unexpected character '%c' (U+%04X)", c, (unsigned)c);
    return make_tok(lx, TOK_EOF, start);
}

const char *tok_kind_name(TokKind k) {
    switch (k) {
        case TOK_EOF: return "EOF";
        case TOK_INT: return "int";
        case TOK_FLOAT: return "float";
        case TOK_STR: return "str";
        case TOK_FSTR: return "fstr";
        case TOK_CHAR: return "char";
        case TOK_IDENT: return "ident";
        case TOK_KW_MODULE: return "module";
        case TOK_KW_USE: return "use";
        case TOK_KW_FN: return "fn";
        case TOK_KW_STRUCT: return "struct";
        case TOK_KW_IMPL: return "impl";
        case TOK_KW_ENUM: return "enum";
        case TOK_KW_TRAIT: return "trait";
        case TOK_KW_TYPE: return "type";
        case TOK_KW_LET: return "let";
        case TOK_KW_MUT: return "mut";
        case TOK_KW_CONST: return "const";
        case TOK_KW_PUB: return "pub";
        case TOK_KW_IF: return "if";
        case TOK_KW_ELSE: return "else";
        case TOK_KW_MATCH: return "match";
        case TOK_KW_RETURN: return "return";
        case TOK_KW_WHILE: return "while";
        case TOK_KW_FOR: return "for";
        case TOK_KW_IN: return "in";
        case TOK_KW_LOOP: return "loop";
        case TOK_KW_BREAK: return "break";
        case TOK_KW_CONTINUE: return "continue";
        case TOK_KW_TRUE: return "true";
        case TOK_KW_FALSE: return "false";
        case TOK_KW_SELF: return "self";
        case TOK_KW_SELF_TYPE: return "Self";
        case TOK_KW_AS: return "as";
        case TOK_KW_DEFER: return "defer";
        case TOK_LPAREN: return "(";
        case TOK_RPAREN: return ")";
        case TOK_LBRACE: return "{";
        case TOK_RBRACE: return "}";
        case TOK_LBRACKET: return "[";
        case TOK_RBRACKET: return "]";
        case TOK_COMMA: return ",";
        case TOK_SEMI: return ";";
        case TOK_COLON: return ":";
        case TOK_COLONCOLON: return "::";
        case TOK_ARROW: return "->";
        case TOK_FATARROW: return "=>";
        case TOK_DOT: return ".";
        case TOK_DOTDOT: return "..";
        case TOK_DOTDOTEQ: return "..=";
        case TOK_AT: return "@";
        case TOK_AMP: return "&";
        case TOK_AMPAMP: return "&&";
        case TOK_PIPE: return "|";
        case TOK_PIPEPIPE: return "||";
        case TOK_CARET: return "^";
        case TOK_TILDE: return "~";
        case TOK_BANG: return "!";
        case TOK_QUESTION: return "?";
        case TOK_PLUS: return "+";
        case TOK_MINUS: return "-";
        case TOK_STAR: return "*";
        case TOK_SLASH: return "/";
        case TOK_PERCENT: return "%";
        case TOK_LT: return "<";
        case TOK_GT: return ">";
        case TOK_LE: return "<=";
        case TOK_GE: return ">=";
        case TOK_EQEQ: return "==";
        case TOK_NEQ: return "!=";
        case TOK_ASSIGN: return "=";
        case TOK_PLUSEQ: return "+=";
        case TOK_MINUSEQ: return "-=";
        case TOK_STAREQ: return "*=";
        case TOK_SLASHEQ: return "/=";
        case TOK_PERCENTEQ: return "%=";
        case TOK_SHL: return "<<";
        case TOK_SHR: return ">>";
        case TOK_COUNT: break;
    }
    return "?";
}
