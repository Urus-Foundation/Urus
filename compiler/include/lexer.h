/*
 * URUS Lexer v0.0.1
 *
 * Token taxonomy:
 *   Literals:    INT, FLOAT, STR, CHAR, TRUE, FALSE
 *   Identifier:  IDENT
 *   Keywords:    module use fn struct impl enum let mut const
 *                if else match return while for loop break continue
 *                true false pub type trait self Self as
 *                Result Ok Err Option Some None
 *   Punct:       ( ) { } [ ] , ; : :: -> => . .. ..=
 *                + - * / % & | ^ ~ ! < > <= >= == != = += -= *= /= %=
 *                && || << >>
 */
#ifndef URUS_LEXER_H
#define URUS_LEXER_H

#include "urus_common.h"
#include "diag.h"

typedef enum {
    /* trivia */
    TOK_EOF = 0,

    /* literals */
    TOK_INT,
    TOK_FLOAT,
    TOK_STR,
    TOK_FSTR,    /* f"...{expr}..." formatted string */
    TOK_CHAR,

    /* identifiers and keywords */
    TOK_IDENT,
    TOK_KW_MODULE,
    TOK_KW_USE,
    TOK_KW_FN,
    TOK_KW_STRUCT,
    TOK_KW_IMPL,
    TOK_KW_ENUM,
    TOK_KW_TRAIT,
    TOK_KW_TYPE,
    TOK_KW_LET,
    TOK_KW_MUT,
    TOK_KW_CONST,
    TOK_KW_PUB,
    TOK_KW_IF,
    TOK_KW_ELSE,
    TOK_KW_MATCH,
    TOK_KW_RETURN,
    TOK_KW_WHILE,
    TOK_KW_FOR,
    TOK_KW_IN,
    TOK_KW_LOOP,
    TOK_KW_BREAK,
    TOK_KW_CONTINUE,
    TOK_KW_TRUE,
    TOK_KW_FALSE,
    TOK_KW_SELF,
    TOK_KW_SELF_TYPE,   /* Self */
    TOK_KW_AS,
    TOK_KW_DEFER,

    /* punctuation */
    TOK_LPAREN,    /* ( */
    TOK_RPAREN,    /* ) */
    TOK_LBRACE,    /* { */
    TOK_RBRACE,    /* } */
    TOK_LBRACKET,  /* [ */
    TOK_RBRACKET,  /* ] */
    TOK_COMMA,     /* , */
    TOK_SEMI,      /* ; */
    TOK_COLON,     /* : */
    TOK_COLONCOLON,/* :: */
    TOK_ARROW,     /* -> */
    TOK_FATARROW,  /* => */
    TOK_DOT,       /* . */
    TOK_DOTDOT,    /* .. */
    TOK_DOTDOTEQ,  /* ..= */
    TOK_AT,        /* @ */
    TOK_AMP,       /* & */
    TOK_AMPAMP,    /* && */
    TOK_PIPE,      /* | */
    TOK_PIPEPIPE,  /* || */
    TOK_CARET,     /* ^ */
    TOK_TILDE,     /* ~ */
    TOK_BANG,      /* ! */
    TOK_QUESTION,  /* ? */

    TOK_PLUS,      /* + */
    TOK_MINUS,     /* - */
    TOK_STAR,      /* * */
    TOK_SLASH,     /* / */
    TOK_PERCENT,   /* % */

    TOK_LT,        /* < */
    TOK_GT,        /* > */
    TOK_LE,        /* <= */
    TOK_GE,        /* >= */
    TOK_EQEQ,      /* == */
    TOK_NEQ,       /* != */
    TOK_ASSIGN,    /* = */
    TOK_PLUSEQ,    /* += */
    TOK_MINUSEQ,   /* -= */
    TOK_STAREQ,    /* *= */
    TOK_SLASHEQ,   /* /= */
    TOK_PERCENTEQ, /* %= */
    TOK_SHL,       /* << */
    TOK_SHR,       /* >> */

    TOK_COUNT
} TokKind;

typedef struct {
    TokKind     kind;
    SrcLoc      loc;
    const char *start;     /* points into source buffer */
    uint32_t    length;

    /* parsed values for literals (filled by lexer when convenient) */
    union {
        uint64_t int_val;
        double   float_val;
        struct {
            const char *ptr;   /* unescaped string in arena */
            uint32_t    len;
        } str_val;
        uint32_t char_val;
    } v;
} Token;

typedef struct {
    const char *source;
    const char *cur;
    const char *file;
    uint32_t    line;
    uint32_t    col;
    DiagCtx    *diag;
    struct Arena *arena;   /* for string literal storage */
} Lexer;

void lexer_init(Lexer *lx, const char *source, const char *file,
                DiagCtx *diag, struct Arena *arena);

Token lexer_next(Lexer *lx);

const char *tok_kind_name(TokKind k);

#endif
