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

#ifndef URUS_URUSC_H
#define URUS_URUSC_H

#ifndef _WIN32
#   define _POSIX_C_SOURCE 200809L 
#endif

#include "config.h"
#include "urusctok.h"
#include <stddef.h>
#include <stdbool.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>

#ifdef _WIN32
#   define URUSC_PATHSEP '\\'
#else
#   define URUSC_PATHSEP '/'
#endif

#ifndef PATH_MAX
#   ifdef MAX_PATH
#       define PATH_MAX MAX_PATH
#   else
#       define PATH_MAX 4096 /* safe default value */
#   endif
#endif

//
// Error reporting
//
  
// <filename>:ln:col: <error_type>: <msg>
//       |
//  <ln> | <file_content_ln>
//       |    ^^^ (Token carret)
void report_error(const char *filename, Token *t, const char *msg);
void report_warn(const char *filename, Token *t, const char *msg);

// filename: message
void report(const char *filename, const char *fmt, ...);

//
// Lexer
//

typedef struct {
    const char *source;
    size_t length;
    size_t pos;
    int line;
    int line_start;
} Lexer;

void lexer_init(Lexer *l, const char *source, size_t length);
Token lexer_next(Lexer *l);

// Tokenize entire source, returns malloc'd array, sets *count
Token *lexer_tokenize(Lexer *l, int *count);

//
// Ast
//

// Forward declarations
typedef struct AstNode AstNode;
typedef struct AstType AstType;

// ---- Type representation ----
typedef enum {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_BOOL,
    TYPE_STR,
    TYPE_VOID,
    TYPE_ARRAY, // element type in child
    TYPE_NAMED, // struct or enum name
    TYPE_RESULT, // Result<ok_type, err_type>
    TYPE_FN, // fn(T1, T2) -> R
    TYPE_TUPLE, // (T1, T2, ...)
} TypeKind;

struct AstType {
    TypeKind kind;
    char *name; // for TYPE_NAMED
    AstType *element; // for TYPE_ARRAY
    AstType *ok_type; // for TYPE_RESULT
    AstType *err_type; // for TYPE_RESULT
    // for TYPE_FN
    AstType **param_types;
    int param_count;
    AstType *return_type;
    // for TYPE_TUPLE
    AstType **element_types;
    int element_count;
};

// ---- AST Node kinds ----
typedef enum {
    // Top-level
    NODE_PROGRAM,
    NODE_FN_DECL,
    NODE_STRUCT_DECL,
    NODE_ENUM_DECL,
    NODE_IMPORT,
    NODE_TYPE_ALIAS,

    // Statements
    NODE_BLOCK,
    NODE_LET_STMT,
    NODE_ASSIGN_STMT,
    NODE_IF_STMT,
    NODE_WHILE_STMT,
    NODE_DO_WHILE_STMT,
    NODE_FOR_STMT,
    NODE_RETURN_STMT,
    NODE_BREAK_STMT,
    NODE_CONTINUE_STMT,
    NODE_EXPR_STMT,
    NODE_DEFER_STMT,
    NODE_EMIT_STMT,
    NODE_MATCH,

    // Expressions
    NODE_BINARY,
    NODE_UNARY,
    NODE_CALL,
    NODE_FIELD_ACCESS,
    NODE_INDEX,
    NODE_INT_LIT,
    NODE_FLOAT_LIT,
    NODE_STR_LIT,
    NODE_BOOL_LIT,
    NODE_IDENT,
    NODE_ARRAY_LIT,
    NODE_STRUCT_LIT,
    NODE_ENUM_INIT, // EnumName.Variant or EnumName.Variant(args)
    NODE_OK_EXPR, // Ok(value)
    NODE_ERR_EXPR, // Err(value)
    NODE_LAMBDA, // |params| -> type { body }
    NODE_TUPLE_LIT, // (expr1, expr2, ...)
    NODE_IF_EXPR, // if cond { expr } else { expr } (expression context)
    NODE_RUNE_DECL, // rune name(params) { body }
    NODE_CONST_DECL, // const NAME: type = value;
} NodeKind;

// ---- Param ----
typedef struct {
    char *name;
    AstType *type;
    AstNode *default_value;
    bool is_mut;
    Token tok;
} Param;

// ---- Struct field init ----
typedef struct {
    char *name;
    AstNode *value;
} FieldInit;

// ---- Enum variant ----
typedef struct {
    char *name;
    Param *fields; // NULL if no data
    int field_count;
} EnumVariant;

// ---- Match arm ----
typedef struct {
    char *enum_name; // e.g. "Shape"
    char *variant_name; // e.g. "Circle"
    char **bindings; // bound variable names, e.g. ["r"]
    AstType **binding_types; // resolved types for each binding (filled by sema)
    int binding_count;
    bool is_wildcard; // true for _ arm
    AstNode *pattern_expr; // for primitive match (int/str/bool literal), NULL
                           // for enum
    AstNode *body; // block
} MatchArm;

// ---- AST Node ----
struct AstNode {
    NodeKind kind;
    Token tok;
    int ref_count; // prevent unexpected free

    union {
        // NODE_PROGRAM
        struct {
            AstNode **decls;
            int decl_count;
        } program;

        // NODE_FN_DECL
        struct {
            char *name;
            Param *params;
            int param_count;
            AstType *return_type;
            AstNode *body; // block
        } fn_decl;

        // NODE_STRUCT_DECL
        struct {
            char *name;
            Param *fields;
            int field_count;
        } struct_decl;

        // NODE_BLOCK
        struct {
            AstNode **stmts;
            int stmt_count;
        } block;

        // NODE_LET_STMT
        struct {
            char *name;
            bool is_mut;
            AstType *type;
            AstNode *init;
            // Tuple destructuring: let (x, y) = expr;
            bool is_destructure;
            char **names;
            int name_count;
        } let_stmt;

        // NODE_ASSIGN_STMT
        struct {
            AstNode *target; // lvalue
            TokenType op; // ASSIGN, PLUS_EQ, etc.
            AstNode *value;
        } assign_stmt;

        // NODE_IF_STMT
        struct {
            AstNode *condition;
            AstNode *then_block;
            AstNode *else_branch; // block or another if_stmt, or NULL
        } if_stmt;

        // NODE_IF_EXPR (ternary-like: if cond { expr } else { expr })
        struct {
            AstNode *condition;
            AstNode *then_expr;
            AstNode *else_expr;
        } if_expr;

        // NODE_WHILE_STMT
        struct {
            AstNode *condition;
            AstNode *body;
        } while_stmt;

        // NODE_DO_WHILE_STMT
        struct {
            AstNode *body;
            AstNode *condition;
        } do_while_stmt;

        // NODE_FOR_STMT
        struct {
            char *var_name;
            AstNode *start; // NULL for foreach
            AstNode *end; // NULL for foreach
            AstNode *iterable; // non-NULL for foreach
            bool inclusive;
            bool is_foreach;
            AstNode *body;
            // Tuple destructuring: for (k, v) in arr { }
            bool is_destructure;
            char **var_names;
            int var_count;
        } for_stmt;

        // NODE_RETURN_STMT
        struct {
            AstNode *value; /* NULL if void */
        } return_stmt;

        // NODE_DEFER_STMT
        struct {
            AstNode *body;
        } defer_stmt;

        // NODE_EXPR_STMT
        struct {
            AstNode *expr;
        } expr_stmt;

        // NODE_EMIT_STMT
        struct {
            char *content;
            bool is_toplevel;
        } emit_stmt;

        // NODE_BINARY
        struct {
            AstNode *left;
            TokenType op;
            AstNode *right;
        } binary;

        // NODE_UNARY
        struct {
            TokenType op;
            AstNode *operand;
        } unary;

        // NODE_CALL
        struct {
            AstNode *callee;
            AstNode **args;
            int arg_count;
        } call;

        // NODE_FIELD_ACCESS
        struct {
            AstNode *object;
            char *field;
        } field_access;

        // NODE_INDEX
        struct {
            AstNode *object;
            AstNode *index;
        } index_expr;

        // NODE_INT_LIT
        struct {
            long long value;
        } int_lit;

        // NODE_FLOAT_LIT
        struct {
            double value;
        } float_lit;

        // NODE_STR_LIT
        struct {
            char *value;
        } str_lit;

        // NODE_BOOL_LIT
        struct {
            bool value;
        } bool_lit;

        // NODE_IDENT
        struct {
            char *name;
        } ident;

        // NODE_ARRAY_LIT
        struct {
            AstNode **elements;
            int count;
        } array_lit;

        // NODE_STRUCT_LIT
        struct {
            char *name;
            FieldInit *fields;
            int field_count;
            AstNode *spread;
        } struct_lit;

        // NODE_ENUM_DECL
        struct {
            char *name;
            EnumVariant *variants;
            int variant_count;
        } enum_decl;

        // NODE_MATCH
        struct {
            AstNode *target;
            MatchArm *arms;
            int arm_count;
        } match_stmt;

        // NODE_ENUM_INIT  (EnumName.Variant(args...))
        struct {
            char *enum_name;
            char *variant_name;
            AstNode **args;
            int arg_count;
        } enum_init;

        // NODE_IMPORT
        struct {
            char *path;
            bool is_stdlib;
        } import_decl;

        // NODE_OK_EXPR / NODE_ERR_EXPR
        struct {
            AstNode *value;
        } result_expr;

        // NODE_TUPLE_LIT
        struct {
            AstNode **elements;
            int count;
        } tuple_lit;

        // NODE_LAMBDA
        struct {
            Param *params;
            int param_count;
            AstType *return_type;
            AstNode *body;
            char **captures; // captured variable names (filled by sema)
            AstType **capture_types; // captured variable types
            int capture_count;
            int lambda_id; // unique ID for codegen
        } lambda;

        // NODE_RUNE_DECL
        struct {
            char *name;
            char **param_names;
            int param_count;
            Token *body_tokens;
            int body_token_count;
        } rune_decl;

        // NODE_CONST_DECL
        struct {
            char *name;
            AstType *type;
            AstNode *value;
        } const_decl;

        // NODE_TYPE_ALIAS
        struct {
            char *name;
            AstType *type;
        } type_alias;
    } as;

    // Filled by semantic analysis
    AstType *resolved_type;

    // Parser flags
    bool is_imported; // prevent unused warning
    bool parenthesized; // wrapped in redundant ()

    // Filled by codegen (temp variable ID)
    int _codegen_tmp;
};

// Constructors
AstNode *ast_new(NodeKind kind, Token tok);
AstType *ast_type_simple(TypeKind kind);
AstType *ast_type_array(AstType *element);
AstType *ast_type_named(const char *name);
AstType *ast_type_result(AstType *ok_type, AstType *err_type);
AstType *ast_type_fn(AstType **param_types, int param_count,
                     AstType *return_type);
AstType *ast_type_tuple(AstType **elems, int count);
char *ast_strdup(const char *s, size_t len);

// Type utilities
AstType *ast_type_clone(AstType *t);
bool ast_types_equal(AstType *a, AstType *b);
bool ast_types_compatible(
    AstType *from,
    AstType *to); // check if its compatible (fair). Example: let x: float = 1;
const char *ast_type_str(AstType *t); // returns static buffer (round-robin)

// Debug printing
void ast_print(AstNode *node, int indent);

// Cleanup
void ast_free(AstNode *node);
void ast_type_free(AstType *type);

//
// Pre-process
//

// Resolves and merges all import declarations in the program AST.
bool preprocess_imports(AstNode *program, const char *base_file);

// Still an Idea: reset state between file compilations (if you ever do
// multi-file compilation in one process lifetime) void preprocess_reset(void);

//
// Parser
//

typedef struct {
    Token *tokens;
    const char *filename;
    int count;
    int pos;
    bool had_error;
} Parser;

void parser_init(Parser *p, Token *tokens, int count);
AstNode *parser_parse(Parser *p);

//
// Semantic analysis
//

enum {
    FN_SYM_TAG = 1,
    STRUCT_SYM_TAG,
    ENUM_SYM_TAG,
    TYPE_SYM_TAG
};

typedef struct {
    char *name;
    AstType *type;
    Token tok; // For error tracking
    uint8_t tag;
    bool is_mut;
    bool is_referenced;
    bool is_imported; // prevent unused warning on imported decl
    bool is_builtin; // prevent unused warning on builtin function/variable

    // function
    Param *params;
    int param_count;
    AstType *return_type;

    // struct
    Param *fields;
    int field_count;

    // enum
    EnumVariant *variants;
    int variant_count;

    // alias (type ID = int;);
    AstType *alias_type;
} SemaSymbol;

typedef struct Scope {
    SemaSymbol *syms;
    int count, cap;
    struct Scope *parent;
} SemaScope;

typedef struct {
    SemaScope *current;
    AstType *current_fn_return;
    const char *current_fn_name; // in function '...' tracking
    const char *filename;
    int errors;
    int loop_depth;
} SemaCtx;

// "str_len", "urus_str_len"
typedef struct {
    const char *urus;
    const char *c;
} BuiltinMap;
// defined in builtins.c
extern const BuiltinMap urus_builtin_direct_maps[];

// Returns true if analysis succeeded (no errors)
bool sema_analyze(AstNode *program, const char *filename);

// Builtin registration
// register all builtins function into semantic scope to prevent undefined error on builtin function
void sema_register_builtins(SemaScope *global);

// scope function
SemaScope *scope_new(SemaScope *parent);
void scope_free(SemaScope *s);
SemaSymbol *scope_lookup_local(SemaScope *s, const char *name);
SemaSymbol *scope_lookup(SemaScope *s, const char *name);
SemaSymbol *scope_add(SemaScope *s, const char *name, Token tok);

//
// codegen
//

typedef struct {
    char *data;
    size_t len;
    size_t cap;
    int indent;
    int tmp_counter;
} CodeBuf;

// Functions
void codegen_init(CodeBuf *buf);
void codegen_free(CodeBuf *buf);
void codegen_generate(CodeBuf *buf, AstNode *program);

//
// MISC
//

char *read_file(const char *path, size_t *out_len);

// --  Memory Management  --
#define xfree(ptr) __xfree((void **)&(ptr))
#define xrealloc(ptr, size) __xrealloc((void **)&(ptr), size)

void *xmalloc(size_t size);
void *__xrealloc(void **ptr, size_t size);
void __xfree(void **ptr);

#endif
