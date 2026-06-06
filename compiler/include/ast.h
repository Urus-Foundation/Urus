/*
 * URUS AST v0.0.1
 *
 * Designed as discriminated unions (tag + payload). Every node carries
 * SrcLoc so the back-end can attribute generated C lines back to URUS
 * source for #line directives (a planned v0.0.2 feature).
 */
#ifndef URUS_AST_H
#define URUS_AST_H

#include "urus_common.h"
#include "diag.h"
#include "arena.h"

/* ---------- Types ---------- */

typedef enum {
    TY_NAMED,      /* foo, u32, str, MyStruct */
    TY_POINTER,    /* *T or *mut T (mutability stored) */
    TY_REF,        /* &T or &mut T */
    TY_ARRAY,      /* [T; N] — N as expression, evaluated at sema */
    TY_SLICE,      /* [T] — fat pointer */
    TY_TUPLE,      /* (A, B, C) */
    TY_FN,         /* fn(A, B) -> C */
    TY_UNIT,       /* () */
    TY_GENERIC,    /* Path<Args, ...> e.g. Result<T, E> */
    TY_SELF,       /* Self */
    TY_INFER,      /* `_` placeholder, sema fills in */
} TypeExprKind;

typedef struct TypeExpr TypeExpr;

struct TypeExpr {
    TypeExprKind kind;
    SrcLoc       loc;
    union {
        struct { const char *name; } named;
        struct { TypeExpr *inner; bool is_mut; } pointer;
        struct { TypeExpr *inner; bool is_mut; } ref;
        struct { TypeExpr *elem; struct Expr *size; } array;
        struct { TypeExpr *elem; } slice;
        struct { TypeExpr **elems; int n; } tuple;
        struct { TypeExpr **params; int n_params; TypeExpr *ret; } fn;
        struct {
            const char *name;
            TypeExpr  **args;
            int         n_args;
        } generic;
    };
};

/* ---------- Expressions ---------- */

typedef enum {
    EX_INT_LIT,
    EX_FLOAT_LIT,
    EX_STR_LIT,
    EX_FSTR_LIT,
    EX_CHAR_LIT,
    EX_BOOL_LIT,
    EX_IDENT,
    EX_PATH,         /* a::b::c */
    EX_UNARY,
    EX_BINARY,
    EX_ASSIGN,
    EX_CALL,
    EX_METHOD_CALL,
    EX_FIELD,
    EX_INDEX,
    EX_IF,
    EX_MATCH,
    EX_BLOCK,
    EX_RETURN,
    EX_BREAK,
    EX_CONTINUE,
    EX_WHILE,
    EX_FOR,
    EX_LOOP,
    EX_STRUCT_LIT,
    EX_TUPLE_LIT,
    EX_ARRAY_LIT,
    EX_CAST,
    EX_REF,          /* &expr / &mut expr */
    EX_DEREF,        /* *expr */
    EX_RANGE,        /* a..b or a..=b */
    EX_TRY,          /* expr? — Result/Option propagation */
    EX_UNIT,
} ExprKind;

typedef enum {
    UNOP_NEG, UNOP_NOT, UNOP_BIT_NOT,
} UnOp;

typedef enum {
    BIN_ADD, BIN_SUB, BIN_MUL, BIN_DIV, BIN_MOD,
    BIN_EQ, BIN_NEQ, BIN_LT, BIN_LE, BIN_GT, BIN_GE,
    BIN_AND, BIN_OR,
    BIN_BIT_AND, BIN_BIT_OR, BIN_BIT_XOR,
    BIN_SHL, BIN_SHR,
} BinOp;

typedef enum {
    ASSIGN_EQ, ASSIGN_ADD, ASSIGN_SUB, ASSIGN_MUL, ASSIGN_DIV, ASSIGN_MOD,
} AssignOp;

typedef struct Expr Expr;
typedef struct Stmt Stmt;
typedef struct Pattern Pattern;

typedef struct {
    const char *name;
    Expr       *value;   /* may be NULL for shorthand */
} StructFieldInit;

typedef struct {
    Pattern *pat;
    Expr    *guard;      /* optional */
    Expr    *body;
} MatchArm;

struct Expr {
    ExprKind kind;
    SrcLoc   loc;
    TypeExpr *type_annot;    /* user annotation if present; sema fills inferred */
    union {
        uint64_t int_lit;
        double   float_lit;
        struct { const char *ptr; uint32_t len; } str_lit;
        uint32_t char_lit;
        bool     bool_lit;
        const char *ident;
        struct { const char **segs; int n; } path;
        struct { UnOp op; Expr *operand; } unary;
        struct { BinOp op; Expr *lhs; Expr *rhs; } binary;
        struct { AssignOp op; Expr *lhs; Expr *rhs; } assign;
        struct { Expr *callee; Expr **args; int n_args; } call;
        struct { Expr *recv; const char *name; Expr **args; int n_args; } method;
        struct { Expr *obj; const char *name; } field;
        struct { Expr *obj; Expr *idx; } index;
        struct { Expr *cond; Expr *then_blk; Expr *else_blk; } if_;
        struct { Expr *scrut; MatchArm *arms; int n_arms; } match_;
        struct { Stmt **stmts; int n_stmts; Expr *tail; } block;
        struct { Expr *value; } return_;
        struct { const char *label; Expr *value; } break_;
        struct { Expr *cond; Expr *body; } while_;
        struct { Pattern *pat; Expr *iter; Expr *body; } for_;
        struct { Expr *body; } loop_;
        struct {
            const char       *name;
            StructFieldInit  *fields;
            int               n_fields;
        } struct_lit;
        struct { Expr **elems; int n; } tuple_lit;
        struct { Expr **elems; int n; } array_lit;
        struct { Expr *expr; TypeExpr *ty; } cast;
        struct { Expr *inner; bool is_mut; } ref_;
        struct { Expr *inner; } deref;
        struct { Expr *start; Expr *end; bool inclusive; } range;
        struct { Expr *inner; } try_;
    };
};

/* ---------- Patterns ---------- */

typedef enum {
    PAT_WILDCARD,
    PAT_IDENT,
    PAT_LITERAL,
    PAT_TUPLE,
    PAT_STRUCT,
    PAT_ENUM_VARIANT,   /* Some(x), Ok(v), Err(e), None */
} PatternKind;

struct Pattern {
    PatternKind kind;
    SrcLoc      loc;
    union {
        struct { const char *name; bool is_mut; } ident;
        Expr *literal;
        struct { Pattern **elems; int n; } tuple;
        struct { const char *name; Pattern **subs; int n; } variant;
    };
};

/* ---------- Statements ---------- */

typedef enum {
    ST_LET,
    ST_EXPR,        /* expression statement (with ; or trailing return) */
    ST_DEFER,       /* defer expr; — runs at end of enclosing block, LIFO */
    ST_ITEM,        /* nested item — rare; not used in v0.0.1 */
} StmtKind;

typedef struct Item Item;

struct Stmt {
    StmtKind kind;
    SrcLoc   loc;
    union {
        struct {
            Pattern  *pat;
            TypeExpr *type_annot;
            Expr     *init;
            bool      is_mut;
        } let_;
        struct { Expr *expr; bool has_semi; } expr_;
        struct { Expr *expr; } defer_;
        Item *item;
    };
};

/* ---------- Items ---------- */

typedef enum {
    IT_FN,
    IT_STRUCT,
    IT_ENUM,
    IT_IMPL,
    IT_USE,
    IT_CONST,
    IT_TYPE_ALIAS,
} ItemKind;

typedef struct {
    const char *name;
    TypeExpr   *type;
} StructField;

typedef struct {
    const char    *name;
    StructField   *payload;   /* tuple-like payload, can be NULL */
    int            n_payload;
} EnumVariant;

typedef struct {
    Pattern  *pat;        /* identifier pattern */
    TypeExpr *type;
    bool      is_self;    /* &self / &mut self / self */
    bool      is_mut_self;
    bool      is_ref_self;
} FnParam;

typedef struct {
    const char *name;
    FnParam    *params;
    int         n_params;
    TypeExpr   *ret_type;     /* may be NULL → unit () */
    Expr       *body;          /* block expr */
    bool        is_pub;
    bool        is_method;     /* lives inside impl */
    const char *owner_type;    /* for methods: parent struct name */
    SrcLoc      loc;
} FnDecl;

typedef struct {
    const char  *name;
    StructField *fields;
    int          n_fields;
    bool         is_pub;
    SrcLoc       loc;
} StructDecl;

typedef struct {
    const char  *name;
    EnumVariant *variants;
    int          n_variants;
    bool         is_pub;
    SrcLoc       loc;
} EnumDecl;

typedef struct {
    const char *type_name;   /* impl TypeName { ... } */
    FnDecl    **methods;
    int         n_methods;
    SrcLoc      loc;
} ImplBlock;

typedef struct {
    const char **segs;       /* use a::b::c → ["a","b","c"] */
    int          n_segs;
    SrcLoc       loc;
} UseDecl;

typedef struct {
    const char *name;
    TypeExpr   *type;
    Expr       *value;
    bool        is_pub;
    SrcLoc      loc;
} ConstDecl;

typedef struct {
    const char *name;
    TypeExpr   *aliased;
    bool        is_pub;
    SrcLoc      loc;
} TypeAlias;

struct Item {
    ItemKind kind;
    union {
        FnDecl    *fn;
        StructDecl *strct;
        EnumDecl   *enm;
        ImplBlock  *impl;
        UseDecl    *use;
        ConstDecl  *cnst;
        TypeAlias  *alias;
    };
};

/* ---------- Module ---------- */

typedef struct {
    const char *name;        /* from `module foo` declaration, may be NULL */
    Item      **items;
    int         n_items;
    Arena      *arena;
} Module;

/* Helpers */
Expr     *ast_new_expr(Arena *a, ExprKind k, SrcLoc loc);
Stmt     *ast_new_stmt(Arena *a, StmtKind k, SrcLoc loc);
Item     *ast_new_item(Arena *a, ItemKind k);
TypeExpr *ast_new_type(Arena *a, TypeExprKind k, SrcLoc loc);
Pattern  *ast_new_pat(Arena *a, PatternKind k, SrcLoc loc);

void ast_print_module(const Module *m);

#endif
