/*
 * Copyright 2026 Urus Foundation (https://github.com/Urus-Foundation)
 *
 * This file is part of the Urus Programming Language.
 * For more about this language check at
 *
 *    https://github.com/Urus-Foundatation/Urus
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "urusc.h"

void lexer_init(Lexer *l, const char *source, size_t length)
{
    l->source = source;
    l->length = length;
    l->pos = 0;
    l->line = 1;
    l->line_start = 0;
}

static char peek(Lexer *l)
{
    if (l->pos >= l->length)
        return '\0';
    return l->source[l->pos];
}

static char peek_at(Lexer *l, size_t offset)
{
    if (l->pos + offset >= l->length)
        return '\0';
    return l->source[l->pos + offset];
}

static char peek_next(Lexer *l)
{
    if (l->pos + 1 >= l->length)
        return '\0';
    return l->source[l->pos + 1];
}

static char advance(Lexer *l)
{
    char c = l->source[l->pos++];
    if (c == '\n') {
        l->line++;
        l->line_start = l->pos;
    }
    return c;
}

static void skip_whitespace(Lexer *l)
{
    while (l->pos < l->length) {
        char c = peek(l);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance(l);
        } else if (c == '/' && peek_next(l) == '/') {
            while (l->pos < l->length && peek(l) != '\n')
                advance(l);
        } else if (c == '/' && peek_next(l) == '*') {
            advance(l);
            advance(l);
            while (l->pos < l->length) {
                if (peek(l) == '*' && peek_next(l) == '/') {
                    advance(l);
                    advance(l);
                    break;
                }
                advance(l);
            }
        } else {
            break;
        }
    }
}

static Token make_token(Lexer *l, TokenType type, const char *start, size_t len)
{
    return (Token){.type = type,
                   .start = start,
                   .length = len,
                   .line = l->line,
                   .col = (int)(start - (l->source + l->line_start)) + 1};
}

static Token error_token(Lexer *l, const char *msg)
{
    return (Token){.type = TOK_ERROR,
                   .start = msg,
                   .length = strlen(msg),
                   .line = l->line};
}

static TokenType check_keyword(const char *start, size_t len)
{
    typedef struct {
        const char *kw;
        size_t kw_len;
        TokenType type;
    } KW;
    static const KW keywords[] = {
        {"fn", 2, TOK_FN},         {"let", 3, TOK_LET},
        {"mut", 3, TOK_MUT},       {"struct", 6, TOK_STRUCT},
        {"if", 2, TOK_IF},         {"else", 4, TOK_ELSE},
        {"while", 5, TOK_WHILE},   {"for", 3, TOK_FOR},
        {"in", 2, TOK_IN},         {"return", 6, TOK_RETURN},
        {"break", 5, TOK_BREAK},   {"continue", 8, TOK_CONTINUE},
        {"true", 4, TOK_TRUE},     {"false", 5, TOK_FALSE},
        {"enum", 4, TOK_ENUM},     {"match", 5, TOK_MATCH},
        {"import", 6, TOK_IMPORT}, {"rune", 4, TOK_RUNE},
        {"const", 5, TOK_CONST},   {"do", 2, TOK_DO},
        {"__emit__", 8, TOK_EMIT}, {"type", 4, TOK_TYPE},
        {"defer", 5, TOK_DEFER},   {"int", 3, TOK_INT},
        {"float", 5, TOK_FLOAT},   {"bool", 4, TOK_BOOL},
        {"str", 3, TOK_STR},       {"void", 4, TOK_VOID},
        {"Ok", 2, TOK_OK},         {"Err", 3, TOK_ERR},
    };
    for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
        if (len == keywords[i].kw_len &&
            memcmp(start, keywords[i].kw, len) == 0) {
            return keywords[i].type;
        }
    }
    return TOK_IDENT;
}

static Token lex_string(Lexer *l)
{
    const char *start = l->source + l->pos;
    bool is_multiline = false;

    // check if this multiline string (""") start
    if (peek(l) == '"' && peek_next(l) == '"' && peek_at(l, 2) == '"') {
        is_multiline = true;
        advance(l);
        advance(l);
    }

    advance(l); // skip opening "

    while (l->pos < l->length) {
        if (is_multiline) {
            if (peek(l) == '"' && peek_next(l) == '"' && peek_at(l, 2) == '"') {
                advance(l);
                advance(l);
                advance(l);
                size_t len = (size_t)(l->source + l->pos - start);
                return make_token(l, TOK_STR_LIT, start, len);
            }
        } else {
            if (peek(l) == '"') {
                advance(l);
                size_t len = (size_t)(l->source + l->pos - start);
                return make_token(l, TOK_STR_LIT, start, len);
            }
            if (peek(l) == '\n')
                break;
        }

        if (peek(l) == '\\')
            advance(l);
        advance(l); // skip closing "
    }

    return error_token(l, is_multiline ? "Unterminated multiline string"
                                       : "Unterminated string");
}

static Token lex_fstring(Lexer *l)
{
    // f" already matched: 'f' consumed, now at '"'
    const char *start = l->source + l->pos - 1; // include the 'f'
    advance(l); // skip opening "
    int brace_depth = 0;
    while (l->pos < l->length) {
        char c = peek(l);
        if (c == '{' && peek_next(l) == '{') {
            advance(l);
            advance(l); // skip escaped {{
        } else if (c == '}' && peek_next(l) == '}') {
            advance(l);
            advance(l); // skip escaped }}
        } else if (c == '{') {
            brace_depth++;
            advance(l);
        } else if (c == '}') {
            brace_depth--;
            advance(l);
        } else if (c == '"' && brace_depth == 0) {
            break; // end of f-string
        } else if (c == '"' && brace_depth > 0) {
            // String literal inside expression — skip it
            advance(l); // skip opening "
            while (l->pos < l->length && peek(l) != '"') {
                if (peek(l) == '\\')
                    advance(l);
                advance(l);
            }
            if (l->pos < l->length)
                advance(l); // skip closing "
        } else {
            if (c == '\\')
                advance(l);
            advance(l);
        }
    }
    if (l->pos >= l->length)
        return error_token(l, "unterminated f-string");
    advance(l); // skip closing "
    return make_token(l, TOK_FSTR_LIT, start,
                      (size_t)(l->source + l->pos - start));
}

static int is_hex(char c)
{
    return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static Token lex_number(Lexer *l)
{
    const char *start = l->source + l->pos;

    // Hex: 0x..., Binary: 0b..., Octal: 0o...
    if (peek(l) == '0' && l->pos + 1 < l->length) {
        char next = l->source[l->pos + 1];
        if (next == 'x' || next == 'X') {
            advance(l);
            advance(l); // skip 0x
            while (is_hex(peek(l)) || (peek(l) == '_' && is_hex(peek_next(l))))
                advance(l);
            return make_token(l, TOK_INT_LIT, start,
                              (size_t)(l->source + l->pos - start));
        }
        if (next == 'b' || next == 'B') {
            advance(l);
            advance(l); // skip 0b
            while (peek(l) == '0' || peek(l) == '1' ||
                   (peek(l) == '_' &&
                    (peek_next(l) == '0' || peek_next(l) == '1')))
                advance(l);
            return make_token(l, TOK_INT_LIT, start,
                              (size_t)(l->source + l->pos - start));
        }
        if (next == 'o' || next == 'O') {
            advance(l);
            advance(l); // skip 0o
            while (
                (peek(l) >= '0' && peek(l) <= '7') ||
                (peek(l) == '_' && peek_next(l) >= '0' && peek_next(l) <= '7'))
                advance(l);
            return make_token(l, TOK_INT_LIT, start,
                              (size_t)(l->source + l->pos - start));
        }
    }

    while (isdigit(peek(l)) || (peek(l) == '_' && isdigit(peek_next(l))))
        advance(l);
    if (peek(l) == '.' && peek_next(l) != '.') {
        // '.' followed by '.' is range operator (e.g. 0..10), not float
        advance(l); // consume '.'
        while (isdigit(peek(l)) || (peek(l) == '_' && isdigit(peek_next(l))))
            advance(l);
    }
    // Scientific notation: e/E followed by optional +/- and digits
    if (peek(l) == 'e' || peek(l) == 'E') {
        advance(l); // consume e/E
        if (peek(l) == '+' || peek(l) == '-')
            advance(l);
        while (isdigit(peek(l)))
            advance(l);
        return make_token(l, TOK_FLOAT_LIT, start,
                          (size_t)(l->source + l->pos - start));
    }
    // Check if we consumed a '.' (float)
    for (const char *p = start; p < l->source + l->pos; p++) {
        if (*p == '.')
            return make_token(l, TOK_FLOAT_LIT, start,
                              (size_t)(l->source + l->pos - start));
    }
    return make_token(l, TOK_INT_LIT, start,
                      (size_t)(l->source + l->pos - start));
}

static Token lex_ident(Lexer *l)
{
    const char *start = l->source + l->pos;
    while (isalnum(peek(l)) || peek(l) == '_')
        advance(l);
    size_t len = (size_t)(l->source + l->pos - start);
    TokenType type = check_keyword(start, len);

    // Check for f-string: identifier 'f' followed by '"'
    if (type == TOK_IDENT && len == 1 && start[0] == 'f' && peek(l) == '"') {
        return lex_fstring(l);
    }

    return make_token(l, type, start, len);
}

Token lexer_next(Lexer *l)
{
    skip_whitespace(l);
    if (l->pos >= l->length) {
        return make_token(l, TOK_EOF, l->source + l->pos, 0);
    }

    char c = peek(l);

    if (c == '"')
        return lex_string(l);
    if (isdigit(c))
        return lex_number(l);
    if (isalpha(c) || c == '_')
        return lex_ident(l);

    const char *start = l->source + l->pos;
    advance(l);

    switch (c) {
    case '(':
        return make_token(l, TOK_LPAREN, start, 1);
    case ')':
        return make_token(l, TOK_RPAREN, start, 1);
    case '{':
        return make_token(l, TOK_LBRACE, start, 1);
    case '}':
        return make_token(l, TOK_RBRACE, start, 1);
    case '[':
        return make_token(l, TOK_LBRACKET, start, 1);
    case ']':
        return make_token(l, TOK_RBRACKET, start, 1);
    case ',':
        return make_token(l, TOK_COMMA, start, 1);
    case ':':
        return make_token(l, TOK_COLON, start, 1);
    case ';':
        return make_token(l, TOK_SEMICOLON, start, 1);
    case '%':
        if (peek(l) == '%') {
            advance(l);
            return make_token(l, TOK_PERCENT_PERCENT, start, 2);
        }
        if (peek(l) == '=') {
            advance(l);
            return make_token(l, TOK_PERCENT_EQ, start, 2);
        }
        return make_token(l, TOK_PERCENT, start, 1);
    case '+':
        if (peek(l) == '+') {
            advance(l);
            return make_token(l, TOK_PLUSPLUS, start, 2);
        }
        if (peek(l) == '=') {
            advance(l);
            return make_token(l, TOK_PLUS_EQ, start, 2);
        }
        return make_token(l, TOK_PLUS, start, 1);
    case '-':
        if (peek(l) == '-') {
            advance(l);
            return make_token(l, TOK_MINUSMINUS, start, 2);
        }
        if (peek(l) == '=') {
            advance(l);
            return make_token(l, TOK_MINUS_EQ, start, 2);
        }
        return make_token(l, TOK_MINUS, start, 1);
    case '*':
        if (peek(l) == '*') {
            advance(l);
            return make_token(l, TOK_STARSTAR, start, 2);
        }
        if (peek(l) == '=') {
            advance(l);
            return make_token(l, TOK_STAR_EQ, start, 2);
        }
        return make_token(l, TOK_STAR, start, 1);
    case '/':
        if (peek(l) == '=') {
            advance(l);
            return make_token(l, TOK_SLASH_EQ, start, 2);
        }
        return make_token(l, TOK_SLASH, start, 1);
    case '=':
        if (peek(l) == '=') {
            advance(l);
            return make_token(l, TOK_EQ, start, 2);
        }
        if (peek(l) == '>') {
            advance(l);
            return make_token(l, TOK_ARROW, start, 2);
        }
        return make_token(l, TOK_ASSIGN, start, 1);
    case '!':
        if (peek(l) == '=') {
            advance(l);
            return make_token(l, TOK_NEQ, start, 2);
        }
        return make_token(l, TOK_NOT, start, 1);
    case '<':
        if (peek(l) == '<') {
            advance(l);
            if (peek(l) == '=') {
                advance(l);
                return make_token(l, TOK_SHL_EQ, start, 3);
            }
            return make_token(l, TOK_SHL, start, 2);
        }
        if (peek(l) == '=') {
            advance(l);
            return make_token(l, TOK_LTE, start, 2);
        }
        return make_token(l, TOK_LT, start, 1);
    case '>':
        if (peek(l) == '>') {
            advance(l);
            if (peek(l) == '=') {
                advance(l);
                return make_token(l, TOK_SHR_EQ, start, 3);
            }
            return make_token(l, TOK_SHR, start, 2);
        }
        if (peek(l) == '=') {
            advance(l);
            return make_token(l, TOK_GTE, start, 2);
        }
        return make_token(l, TOK_GT, start, 1);
    case '&':
        if (peek(l) == '&') {
            advance(l);
            return make_token(l, TOK_AND, start, 2);
        }
        if (peek(l) == '~') {
            advance(l);
            return make_token(l, TOK_AMP_TILDE, start, 2);
        }
        if (peek(l) == '=') {
            advance(l);
            return make_token(l, TOK_AMP_EQ, start, 2);
        }
        return make_token(l, TOK_AMP, start, 1);
    case '|':
        if (peek(l) == '|') {
            advance(l);
            return make_token(l, TOK_OR, start, 2);
        }
        if (peek(l) == '=') {
            advance(l);
            return make_token(l, TOK_PIPE_EQ, start, 2);
        }
        return make_token(l, TOK_PIPE, start, 1);
    case '^':
        if (peek(l) == '=') {
            advance(l);
            return make_token(l, TOK_CARET_EQ, start, 2);
        }
        return make_token(l, TOK_CARET, start, 1);
    case '~':
        return make_token(l, TOK_TILDE, start, 1);
    case '.':
        if (peek(l) == '.') {
            advance(l);
            if (peek(l) == '=') {
                advance(l);
                return make_token(l, TOK_DOTDOTEQ, start, 3);
            }
            return make_token(l, TOK_DOTDOT, start, 2);
        }
        return make_token(l, TOK_DOT, start, 1);
    }

    return error_token(l, "unexpected character");
}

Token *lexer_tokenize(Lexer *l, int *count)
{
    int cap = 256;
    int n = 0;
    Token *tokens = xmalloc(sizeof(Token) * (size_t)cap);

    while (1) {
        if (n >= cap) {
            if (cap > INT_MAX / 2) {
                fprintf(stderr, "Error: source file too large (token limit exceeded)\n");
                exit(1);
            }
            cap *= 2;
            tokens = xrealloc(tokens, sizeof(Token) * (size_t)cap);
        }
        tokens[n] = lexer_next(l);
        if (tokens[n].type == TOK_EOF) {
            n++;
            break;
        }
        if (tokens[n].type == TOK_ERROR) {
            fprintf(stderr, "Lexer error at line %d: %.*s\n", tokens[n].line,
                    (int)tokens[n].length, tokens[n].start);
            xfree(tokens);
            *count = 0;
            return NULL;
        }
        n++;
    }
    *count = n;
    return tokens;
}

const char *token_type_name(TokenType type)
{
    switch (type) {
    case TOK_INT_LIT:
        return "INT_LIT";
    case TOK_FLOAT_LIT:
        return "FLOAT_LIT";
    case TOK_STR_LIT:
        return "STR_LIT";
    case TOK_FSTR_LIT:
        return "FSTR_LIT";
    case TOK_IDENT:
        return "IDENT";
    case TOK_FN:
        return "FN";
    case TOK_LET:
        return "LET";
    case TOK_MUT:
        return "MUT";
    case TOK_STRUCT:
        return "STRUCT";
    case TOK_IF:
        return "IF";
    case TOK_ELSE:
        return "ELSE";
    case TOK_WHILE:
        return "WHILE";
    case TOK_FOR:
        return "FOR";
    case TOK_IN:
        return "IN";
    case TOK_RETURN:
        return "RETURN";
    case TOK_BREAK:
        return "BREAK";
    case TOK_CONTINUE:
        return "CONTINUE";
    case TOK_TRUE:
        return "TRUE";
    case TOK_FALSE:
        return "FALSE";
    case TOK_ENUM:
        return "ENUM";
    case TOK_MATCH:
        return "MATCH";
    case TOK_IMPORT:
        return "IMPORT";
    case TOK_EMIT:
        return "EMIT";
    case TOK_RUNE:
        return "RUNE";
    case TOK_CONST:
        return "CONST";
    case TOK_DO:
        return "DO";
    case TOK_TYPE:
        return "TYPE";
    case TOK_DEFER:
        return "DEFER";
    case TOK_ARROW:
        return "ARROW";
    case TOK_INT:
        return "INT";
    case TOK_FLOAT:
        return "FLOAT";
    case TOK_BOOL:
        return "BOOL";
    case TOK_STR:
        return "STR";
    case TOK_VOID:
        return "VOID";
    case TOK_OK:
        return "OK";
    case TOK_ERR:
        return "ERR";
    case TOK_PLUS:
        return "PLUS";
    case TOK_MINUS:
        return "MINUS";
    case TOK_STAR:
        return "STAR";
    case TOK_SLASH:
        return "SLASH";
    case TOK_PERCENT:
        return "PERCENT";
    case TOK_EQ:
        return "EQ";
    case TOK_NEQ:
        return "NEQ";
    case TOK_LT:
        return "LT";
    case TOK_GT:
        return "GT";
    case TOK_LTE:
        return "LTE";
    case TOK_GTE:
        return "GTE";
    case TOK_AND:
        return "AND";
    case TOK_OR:
        return "OR";
    case TOK_NOT:
        return "NOT";
    case TOK_ASSIGN:
        return "ASSIGN";
    case TOK_PLUS_EQ:
        return "PLUS_EQ";
    case TOK_MINUS_EQ:
        return "MINUS_EQ";
    case TOK_STAR_EQ:
        return "STAR_EQ";
    case TOK_SLASH_EQ:
        return "SLASH_EQ";
    case TOK_PLUSPLUS:
        return "PLUSPLUS";
    case TOK_MINUSMINUS:
        return "MINUSMINUS";
    case TOK_STARSTAR:
        return "STARSTAR";
    case TOK_PERCENT_PERCENT:
        return "PERCENT_PERCENT";
    case TOK_AMP:
        return "AMP";
    case TOK_CARET:
        return "CARET";
    case TOK_TILDE:
        return "TILDE";
    case TOK_SHL:
        return "SHL";
    case TOK_SHR:
        return "SHR";
    case TOK_AMP_TILDE:
        return "AMP_TILDE";
    case TOK_PERCENT_EQ:
        return "PERCENT_EQ";
    case TOK_AMP_EQ:
        return "AMP_EQ";
    case TOK_PIPE_EQ:
        return "PIPE_EQ";
    case TOK_CARET_EQ:
        return "CARET_EQ";
    case TOK_SHL_EQ:
        return "SHL_EQ";
    case TOK_SHR_EQ:
        return "SHR_EQ";
    case TOK_DOTDOT:
        return "DOTDOT";
    case TOK_DOTDOTEQ:
        return "DOTDOTEQ";
    case TOK_LPAREN:
        return "LPAREN";
    case TOK_RPAREN:
        return "RPAREN";
    case TOK_LBRACE:
        return "LBRACE";
    case TOK_RBRACE:
        return "RBRACE";
    case TOK_LBRACKET:
        return "LBRACKET";
    case TOK_RBRACKET:
        return "RBRACKET";
    case TOK_COMMA:
        return "COMMA";
    case TOK_COLON:
        return "COLON";
    case TOK_SEMICOLON:
        return "SEMICOLON";
    case TOK_DOT:
        return "DOT";
    case TOK_PIPE:
        return "PIPE";
    case TOK_EOF:
        return "EOF";
    case TOK_ERROR:
        return "ERROR";
    }
    return "UNKNOWN";
}
