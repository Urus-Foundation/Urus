#ifndef URUS_PARSER_H
#define URUS_PARSER_H

#include "lexer.h"
#include "ast.h"

/*
 * URUS_MAX_PARSE_DEPTH bounds recursion inside parse_type / parse_expr_prec /
 * parse_pattern / parse_block.  Closes F-COMP-1: a pathological "*****T"
 * 1 M-deep type used to blow the C call stack; now it errors cleanly.
 *
 * 256 is large enough for any human-authored URUS source — the deepest
 * test in tests/run/ is well under 20.
 */
#define URUS_MAX_PARSE_DEPTH 256

typedef struct {
    Lexer    lx;
    Token    cur;
    Token    peek;
    DiagCtx *diag;
    Arena   *arena;
    bool     panic_mode;
    int      depth;             /* current recursion depth */
    bool     depth_exceeded;    /* one-shot flag so we only diag once */
} Parser;

void    parser_init(Parser *p, const char *source, const char *file,
                    DiagCtx *diag, Arena *arena);
Module *parser_parse_module(Parser *p);

#endif
