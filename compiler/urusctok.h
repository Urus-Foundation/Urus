#ifndef URUS_URUSCTOK_H
#define URUS_URUSCTOK_H
//
// Token
//

#include <stddef.h>

typedef enum {
    // Literals
    TOK_INT_LIT,
    TOK_FLOAT_LIT,
    TOK_STR_LIT,
    TOK_FSTR_LIT, // f"..." string interpolation
    TOK_IDENT,

    // Keywords
    TOK_FN,
    TOK_LET,
    TOK_MUT,
    TOK_STRUCT,
    TOK_IF,
    TOK_ELSE,
    TOK_WHILE,
    TOK_FOR,
    TOK_IN,
    TOK_RETURN,
    TOK_BREAK,
    TOK_CONTINUE,
    TOK_TRUE,
    TOK_FALSE,
    TOK_ENUM,
    TOK_MATCH,
    TOK_IMPORT,
    TOK_RUNE,
    TOK_CONST,
    TOK_DO,
    TOK_TYPE,
    TOK_DEFER,
    TOK_EMIT,
    TOK_TRAIT,
    TOK_IMPL,
    TOK_ASYNC,
    TOK_AWAIT,
    TOK_TRY,
    TOK_CATCH,

    // Type keywords
    TOK_INT,
    TOK_FLOAT,
    TOK_BOOL,
    TOK_STR,
    TOK_VOID,

    // Result/Error handling keywords
    TOK_OK,
    TOK_ERR,

    // Operators
    TOK_PLUS, // +
    TOK_MINUS, // -
    TOK_STAR, // *
    TOK_SLASH, // /
    TOK_PERCENT, // %
    TOK_EQ, // ==
    TOK_NEQ, // !=
    TOK_LT, // <
    TOK_GT, // >
    TOK_LTE, // <=
    TOK_GTE, // >=
    TOK_AND, // &&
    TOK_OR, // ||
    TOK_NOT, // !
    TOK_ASSIGN, // =
    TOK_PLUS_EQ, // +=
    TOK_MINUS_EQ, // -=
    TOK_STAR_EQ, // *=
    TOK_SLASH_EQ, // /=
    TOK_PLUSPLUS, // ++
    TOK_MINUSMINUS, // --
    TOK_STARSTAR, // **
    TOK_PERCENT_PERCENT, // %%
    TOK_AMP, // & (bitwise and)
    TOK_CARET, // ^
    TOK_TILDE, // ~
    TOK_SHL, // <<
    TOK_SHR, // >>
    TOK_AMP_TILDE, // &~
    TOK_PERCENT_EQ, // %=
    TOK_AMP_EQ, // &=
    TOK_PIPE_EQ, // |=
    TOK_CARET_EQ, // ^=
    TOK_SHL_EQ, // <<=
    TOK_SHR_EQ, // >>=
    TOK_DOTDOT, // ..
    TOK_DOTDOTEQ, // ..=
    TOK_ARROW, // =>
    TOK_PIPE, // |
    TOK_QUESTION, // ?

    // Punctuation
    TOK_LPAREN, // (
    TOK_RPAREN, // )
    TOK_LBRACE, // {
    TOK_RBRACE, // }
    TOK_LBRACKET, // [
    TOK_RBRACKET, // ]
    TOK_COMMA, // ,
    TOK_COLON, // :
    TOK_SEMICOLON, // ;
    TOK_DOT, // .

    TOK_EOF,
    TOK_ERROR,
} TokenType;

typedef struct {
    TokenType type;
    const char *start;
    size_t length;
    int line;
    int col;
} Token;

// Functions
const char *token_type_name(TokenType type);
#endif
