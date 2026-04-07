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

// runtime embbeded
extern const unsigned char urus_runtime_header_data[];
extern const unsigned int urus_runtime_header_data_len;

// ---- Forward declarations ----
static void gen_expr(CodeBuf *buf, AstNode *node);
static void gen_stmt(CodeBuf *buf, AstNode *node);
static void gen_block(CodeBuf *buf, AstNode *node);
static void gen_type(CodeBuf *buf, AstType *t);
static void emit(CodeBuf *buf, const char *fmt, ...);
static void emit_indent(CodeBuf *buf);
static bool type_needs_drop(AstType *t);

// ---- Defer tracking ----
static AstNode *defer_stack[64];
static int defer_count = 0;

static void emit_defers(CodeBuf *buf)
{
    for (int i = defer_count - 1; i >= 0; i--) {
        gen_block(buf, defer_stack[i]);
        emit(buf, "\n");
    }
}

// ---- Generic monomorphization tracking ----

typedef struct {
    char *fn_name;           // original generic function name
    AstType **type_args;     // concrete type arguments
    int type_arg_count;
    char *mangled_name;      // mangled C function name
    AstNode *fn_node;        // original AST node
} MonoInstance;

#define MAX_MONO 256
static MonoInstance mono_instances[MAX_MONO];
static int mono_count = 0;

// Build a mangled name for a generic function instantiation
static char *mono_mangle_name(const char *fn_name, AstType **type_args,
                              int type_arg_count)
{
    char buf[512];
    int pos = snprintf(buf, sizeof(buf), "%s", fn_name);
    for (int i = 0; i < type_arg_count; i++) {
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "_%s",
                        ast_type_str(type_args[i]));
    }
    // Sanitize: replace special chars with _
    for (int i = 0; buf[i]; i++) {
        if (buf[i] == '<' || buf[i] == '>' || buf[i] == ',' ||
            buf[i] == ' ' || buf[i] == '*' || buf[i] == '[' || buf[i] == ']' ||
            buf[i] == '(' || buf[i] == ')')
            buf[i] = '_';
    }
    return strdup(buf);
}

// Find or register a monomorphization instance. Returns the mangled name.
static const char *mono_get_or_add(const char *fn_name, AstType **type_args,
                                    int type_arg_count, AstNode *fn_node)
{
    // Check if already exists
    for (int i = 0; i < mono_count; i++) {
        if (strcmp(mono_instances[i].fn_name, fn_name) == 0 &&
            mono_instances[i].type_arg_count == type_arg_count) {
            bool match = true;
            for (int j = 0; j < type_arg_count; j++) {
                if (!ast_types_equal(mono_instances[i].type_args[j],
                                     type_args[j])) {
                    match = false;
                    break;
                }
            }
            if (match) return mono_instances[i].mangled_name;
        }
    }
    // Add new
    if (mono_count >= MAX_MONO) {
        fprintf(stderr, "Error: too many generic instantiations (max %d)\n",
                MAX_MONO);
        exit(1);
    }
    MonoInstance *m = &mono_instances[mono_count++];
    m->fn_name = strdup(fn_name);
    m->type_args = type_args;
    m->type_arg_count = type_arg_count;
    m->mangled_name = mono_mangle_name(fn_name, type_args, type_arg_count);
    m->fn_node = fn_node;
    return m->mangled_name;
}

// Find the AST node for a generic function by name
static AstNode *find_generic_fn(AstNode *program, const char *name)
{
    for (int i = 0; i < program->as.program.decl_count; i++) {
        AstNode *d = program->as.program.decls[i];
        if (d->kind == NODE_FN_DECL &&
            d->as.fn_decl.generic_param_count > 0 &&
            strcmp(d->as.fn_decl.name, name) == 0) {
            return d;
        }
    }
    return NULL;
}

// Stored program root for generic fn lookup during codegen
static AstNode *_codegen_program = NULL;
static bool _codegen_test_mode = false;

// ---- Lambda tracking ----

#define MAX_LAMBDAS 256
static AstNode *lambda_nodes[MAX_LAMBDAS];
static int lambda_count = 0;

static void collect_lambdas(AstNode *node)
{
    if (!node) return;
    if (node->kind == NODE_LAMBDA) {
        if (lambda_count < MAX_LAMBDAS)
            lambda_nodes[lambda_count++] = node;
        // Also collect from lambda body
        collect_lambdas(node->as.lambda.body);
        return;
    }
    // Walk all child nodes
    switch (node->kind) {
    case NODE_PROGRAM:
        for (int i = 0; i < node->as.program.decl_count; i++)
            collect_lambdas(node->as.program.decls[i]);
        break;
    case NODE_FN_DECL:
        collect_lambdas(node->as.fn_decl.body);
        break;
    case NODE_BLOCK:
        for (int i = 0; i < node->as.block.stmt_count; i++)
            collect_lambdas(node->as.block.stmts[i]);
        break;
    case NODE_LET_STMT:
        collect_lambdas(node->as.let_stmt.init);
        break;
    case NODE_ASSIGN_STMT:
        collect_lambdas(node->as.assign_stmt.value);
        break;
    case NODE_RETURN_STMT:
        collect_lambdas(node->as.return_stmt.value);
        break;
    case NODE_IF_STMT:
        collect_lambdas(node->as.if_stmt.condition);
        collect_lambdas(node->as.if_stmt.then_block);
        collect_lambdas(node->as.if_stmt.else_branch);
        break;
    case NODE_WHILE_STMT:
        collect_lambdas(node->as.while_stmt.condition);
        collect_lambdas(node->as.while_stmt.body);
        break;
    case NODE_FOR_STMT:
        collect_lambdas(node->as.for_stmt.iterable);
        collect_lambdas(node->as.for_stmt.body);
        break;
    case NODE_CALL:
        collect_lambdas(node->as.call.callee);
        for (int i = 0; i < node->as.call.arg_count; i++)
            collect_lambdas(node->as.call.args[i]);
        break;
    case NODE_BINARY:
        collect_lambdas(node->as.binary.left);
        collect_lambdas(node->as.binary.right);
        break;
    case NODE_UNARY:
        collect_lambdas(node->as.unary.operand);
        break;
    case NODE_EXPR_STMT:
        collect_lambdas(node->as.expr_stmt.expr);
        break;
    case NODE_IMPL_BLOCK:
        for (int i = 0; i < node->as.impl_block.method_count; i++)
            collect_lambdas(node->as.impl_block.methods[i]);
        break;
    default:
        break;
    }
}

static void gen_lambda_fn(CodeBuf *buf, AstNode *node)
{
    // Generate: static ReturnType _urus_lambda_N(Params...) { body }
    gen_type(buf, node->as.lambda.return_type);
    emit(buf, " _urus_lambda_%d(", node->as.lambda.lambda_id);
    if (node->as.lambda.param_count == 0) {
        emit(buf, "void");
    } else {
        for (int i = 0; i < node->as.lambda.param_count; i++) {
            if (i > 0) emit(buf, ", ");
            gen_type(buf, node->as.lambda.params[i].type);
            emit(buf, " %s", node->as.lambda.params[i].name);
        }
    }
    emit(buf, ")");
}

// ---- Tuple typedef tracking ----
static bool tuple_needs_drop(AstType *t);
static bool type_needs_drop(AstType *t);

static const char *tuple_type_name(AstType *t)
{
    static char buf[512];
    int pos = snprintf(buf, sizeof(buf), "_urus_tuple");
    for (int i = 0; i < t->element_count; i++) {
        if (pos >= (int)sizeof(buf) - 1) {
            fprintf(stderr,
                    "Error: tuple type name too long (exceeds %d bytes)\n",
                    (int)sizeof(buf));
            exit(1);
        }
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "_%s",
                        ast_type_str(t->element_types[i]));
    }
    // Sanitize: replace non-alnum with _
    for (int i = 0; buf[i]; i++) {
        if (buf[i] != '_' && !((buf[i] >= 'a' && buf[i] <= 'z') ||
                               (buf[i] >= 'A' && buf[i] <= 'Z') ||
                               (buf[i] >= '0' && buf[i] <= '9'))) {
            buf[i] = '_';
        }
    }
    return buf;
}

#define MAX_TUPLE_TYPES 64
static char *tuple_typedefs[MAX_TUPLE_TYPES];
static int tuple_typedef_count = 0;

static bool tuple_typedef_exists(const char *name)
{
    for (int i = 0; i < tuple_typedef_count; i++) {
        if (strcmp(tuple_typedefs[i], name) == 0)
            return true;
    }
    return false;
}

// Emit a single tuple typedef given a TYPE_TUPLE AstType
static void emit_single_tuple_typedef(CodeBuf *buf, AstType *t)
{
    const char *name = tuple_type_name(t);
    if (tuple_typedef_exists(name))
        return;
    // First emit typedefs for nested tuple element types
    for (int i = 0; i < t->element_count; i++) {
        if (t->element_types[i]->kind == TYPE_TUPLE) {
            emit_single_tuple_typedef(buf, t->element_types[i]);
        }
    }
    tuple_typedefs[tuple_typedef_count++] = strdup(name);
    emit(buf, "typedef struct { ");
    for (int i = 0; i < t->element_count; i++) {
        gen_type(buf, t->element_types[i]);
        emit(buf, " f%d; ", i);
    }
    emit(buf, "} %s;\n", name);

    // Generate drop function if tuple contains heap types
    bool needs_drop = false;
    for (int i = 0; i < t->element_count; i++) {
        if (t->element_types[i]->kind == TYPE_STR ||
            t->element_types[i]->kind == TYPE_ARRAY ||
            t->element_types[i]->kind == TYPE_RESULT ||
            t->element_types[i]->kind == TYPE_NAMED ||
            (t->element_types[i]->kind == TYPE_TUPLE &&
             tuple_needs_drop(t->element_types[i]))) {
            needs_drop = true;
            break;
        }
    }
    if (needs_drop) {
        emit(buf, "static void %s_drop(%s *tp) {\n", name, name);
        for (int i = 0; i < t->element_count; i++) {
            AstType *ft = t->element_types[i];
            if (ft->kind == TYPE_STR)
                emit(buf, "    urus_str_drop(&tp->f%d);\n", i);
            else if (ft->kind == TYPE_ARRAY)
                emit(buf, "    urus_array_drop(&tp->f%d);\n", i);
            else if (ft->kind == TYPE_RESULT)
                emit(buf, "    urus_result_drop(&tp->f%d);\n", i);
            else if (ft->kind == TYPE_NAMED)
                emit(buf, "    %s_drop(&tp->f%d);\n", ft->name, i);
            else if (ft->kind == TYPE_TUPLE && tuple_needs_drop(ft))
                emit(buf, "    %s_drop(&tp->f%d);\n", tuple_type_name(ft), i);
        }
        emit(buf, "}\n");
    }
}

static void collect_and_emit_tuple_typedefs_from_type(CodeBuf *buf, AstType *t)
{
    if (!t)
        return;
    if (t->kind == TYPE_TUPLE) {
        emit_single_tuple_typedef(buf, t);
    }
    if (t->kind == TYPE_ARRAY)
        collect_and_emit_tuple_typedefs_from_type(buf, t->element);
    if (t->kind == TYPE_RESULT) {
        collect_and_emit_tuple_typedefs_from_type(buf, t->ok_type);
        collect_and_emit_tuple_typedefs_from_type(buf, t->err_type);
    }
}

static void collect_and_emit_tuple_typedefs(CodeBuf *buf, AstNode *node)
{
    if (!node)
        return;
    if (node->resolved_type)
        collect_and_emit_tuple_typedefs_from_type(buf, node->resolved_type);
    switch (node->kind) {
    case NODE_PROGRAM:
        for (int i = 0; i < node->as.program.decl_count; i++)
            collect_and_emit_tuple_typedefs(buf, node->as.program.decls[i]);
        break;
    case NODE_FN_DECL:
        collect_and_emit_tuple_typedefs_from_type(buf,
                                                  node->as.fn_decl.return_type);
        for (int i = 0; i < node->as.fn_decl.param_count; i++)
            collect_and_emit_tuple_typedefs_from_type(
                buf, node->as.fn_decl.params[i].type);
        collect_and_emit_tuple_typedefs(buf, node->as.fn_decl.body);
        break;
    case NODE_BLOCK:
        for (int i = 0; i < node->as.block.stmt_count; i++)
            collect_and_emit_tuple_typedefs(buf, node->as.block.stmts[i]);
        break;
    case NODE_LET_STMT:
        collect_and_emit_tuple_typedefs_from_type(buf, node->as.let_stmt.type);
        collect_and_emit_tuple_typedefs(buf, node->as.let_stmt.init);
        break;
    case NODE_ASSIGN_STMT:
        collect_and_emit_tuple_typedefs(buf, node->as.assign_stmt.target);
        collect_and_emit_tuple_typedefs(buf, node->as.assign_stmt.value);
        break;
    case NODE_IF_STMT:
        collect_and_emit_tuple_typedefs(buf, node->as.if_stmt.condition);
        collect_and_emit_tuple_typedefs(buf, node->as.if_stmt.then_block);
        collect_and_emit_tuple_typedefs(buf, node->as.if_stmt.else_branch);
        break;
    case NODE_WHILE_STMT:
        collect_and_emit_tuple_typedefs(buf, node->as.while_stmt.condition);
        collect_and_emit_tuple_typedefs(buf, node->as.while_stmt.body);
        break;
    case NODE_DO_WHILE_STMT:
        collect_and_emit_tuple_typedefs(buf, node->as.do_while_stmt.body);
        collect_and_emit_tuple_typedefs(buf, node->as.do_while_stmt.condition);
        break;
    case NODE_FOR_STMT:
        collect_and_emit_tuple_typedefs(buf, node->as.for_stmt.start);
        collect_and_emit_tuple_typedefs(buf, node->as.for_stmt.end);
        collect_and_emit_tuple_typedefs(buf, node->as.for_stmt.iterable);
        collect_and_emit_tuple_typedefs(buf, node->as.for_stmt.body);
        break;
    case NODE_RETURN_STMT:
        collect_and_emit_tuple_typedefs(buf, node->as.return_stmt.value);
        break;
    case NODE_EXPR_STMT:
        collect_and_emit_tuple_typedefs(buf, node->as.expr_stmt.expr);
        break;
    case NODE_BINARY:
        collect_and_emit_tuple_typedefs(buf, node->as.binary.left);
        collect_and_emit_tuple_typedefs(buf, node->as.binary.right);
        break;
    case NODE_UNARY:
        collect_and_emit_tuple_typedefs(buf, node->as.unary.operand);
        break;
    case NODE_CALL:
        collect_and_emit_tuple_typedefs(buf, node->as.call.callee);
        for (int i = 0; i < node->as.call.arg_count; i++)
            collect_and_emit_tuple_typedefs(buf, node->as.call.args[i]);
        break;
    case NODE_FIELD_ACCESS:
        collect_and_emit_tuple_typedefs(buf, node->as.field_access.object);
        break;
    case NODE_INDEX:
        collect_and_emit_tuple_typedefs(buf, node->as.index_expr.object);
        collect_and_emit_tuple_typedefs(buf, node->as.index_expr.index);
        break;
    case NODE_ARRAY_LIT:
        for (int i = 0; i < node->as.array_lit.count; i++)
            collect_and_emit_tuple_typedefs(buf,
                                            node->as.array_lit.elements[i]);
        break;
    case NODE_TUPLE_LIT:
        for (int i = 0; i < node->as.tuple_lit.count; i++)
            collect_and_emit_tuple_typedefs(buf,
                                            node->as.tuple_lit.elements[i]);
        break;
    case NODE_STRUCT_LIT:
        for (int i = 0; i < node->as.struct_lit.field_count; i++)
            collect_and_emit_tuple_typedefs(
                buf, node->as.struct_lit.fields[i].value);
        break;
    case NODE_MATCH:
        collect_and_emit_tuple_typedefs(buf, node->as.match_stmt.target);
        for (int i = 0; i < node->as.match_stmt.arm_count; i++)
            collect_and_emit_tuple_typedefs(buf,
                                            node->as.match_stmt.arms[i].body);
        break;
    case NODE_ENUM_INIT:
        for (int i = 0; i < node->as.enum_init.arg_count; i++)
            collect_and_emit_tuple_typedefs(buf, node->as.enum_init.args[i]);
        break;
    case NODE_OK_EXPR:
    case NODE_ERR_EXPR:
        collect_and_emit_tuple_typedefs(buf, node->as.result_expr.value);
        break;
    case NODE_LAMBDA:
        collect_and_emit_tuple_typedefs(buf, node->as.lambda.body);
        break;
    default:
        break;
    }
}

// ---- Buffer helpers ----

void codegen_init(CodeBuf *buf)
{
    buf->cap = 8192;
    buf->len = 0;
    buf->data = xmalloc(buf->cap);
    buf->data[0] = '\0';
    buf->indent = 0;
    buf->tmp_counter = 0;
}

void codegen_free(CodeBuf *buf)
{
    xfree(buf->data);
}

static void buf_ensure(CodeBuf *buf, size_t extra)
{
    while (buf->len + extra + 1 >= buf->cap) {
        buf->cap *= 2;
        buf->data = xrealloc(buf->data, buf->cap);
    }
}

static void emit(CodeBuf *buf, const char *fmt, ...)
{
    va_list args;
    va_list args_copy; // for finding actual size
    va_start(args, fmt);
    va_copy(args_copy, args);
    int n = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    if (n > 0) {
        buf_ensure(buf, n);
        vsnprintf(buf->data + buf->len, (size_t)n + 1, fmt, args);
        buf->len += (size_t)n;
        buf->data[buf->len] = '\0';
    }
    va_end(args);
}

static void emit_indent(CodeBuf *buf)
{
    for (int i = 0; i < buf->indent; i++)
        emit(buf, "    ");
}

static void emit_type_drop(CodeBuf *buf, AstType *t)
{
    if (!t)
        return;
    if (!type_needs_drop(t))
        return;

    char dtor[128];
    if (t->kind == TYPE_STR)
        snprintf(dtor, sizeof(dtor), "urus_str_drop");
    else if (t->kind == TYPE_ARRAY)
        snprintf(dtor, sizeof(dtor), "urus_array_drop");
    else if (t->kind == TYPE_RESULT)
        snprintf(dtor, sizeof(dtor), "urus_result_drop");
    else if (t->kind == TYPE_NAMED)
        snprintf(dtor, sizeof(dtor), "%s_drop", t->name);

    emit(buf, "%s", dtor);
}

static void emit_type_drop_cname(CodeBuf *buf, AstType *t, const char *c_name)
{
    if (!t)
        return;
    if (!type_needs_drop(t))
        return;
    emit_type_drop(buf, t);
    emit(buf, "(&%s);\n", c_name);
}

// ---- Type emission ----

static void gen_type(CodeBuf *buf, AstType *t)
{
    if (!t) {
        emit(buf, "void");
        return;
    }
    switch (t->kind) {
    case TYPE_INT:
        emit(buf, "int64_t");
        break;
    case TYPE_FLOAT:
        emit(buf, "double");
        break;
    case TYPE_BOOL:
        emit(buf, "bool");
        break;
    case TYPE_STR:
        emit(buf, "urus_str*");
        break;
    case TYPE_VOID:
        emit(buf, "void");
        break;
    case TYPE_ARRAY:
        emit(buf, "urus_array*");
        break;
    case TYPE_NAMED:
        emit(buf, "%s*", t->name);
        break;
    case TYPE_RESULT:
        emit(buf, "urus_result*");
        break;
    case TYPE_FN:
        emit(buf, "void*");
        break; // function pointers as void*
    case TYPE_TUPLE:
        emit(buf, "%s", tuple_type_name(t));
        break;
    case TYPE_GENERIC:
        // Should not appear in final codegen (substituted during monomorphization)
        emit(buf, "/* generic %s */ void*", t->name);
        break;
    }
}

static bool type_needs_drop(AstType *t)
{
    if (!t)
        return false;
    if (t->kind == TYPE_STR || t->kind == TYPE_ARRAY || t->kind == TYPE_NAMED ||
        t->kind == TYPE_RESULT)
        return true;
    if (t->kind == TYPE_TUPLE)
        return tuple_needs_drop(t);
    return false;
}

static bool tuple_needs_drop(AstType *t)
{
    for (int i = 0; i < t->element_count; i++) {
        if (type_needs_drop(t->element_types[i]))
            return true;
    }
    return false;
}

// Return the C sizeof expression for an array element type
static const char *elem_sizeof(AstType *t)
{
    if (!t)
        return "sizeof(int64_t)";
    switch (t->kind) {
    case TYPE_INT:
        return "sizeof(int64_t)";
    case TYPE_FLOAT:
        return "sizeof(double)";
    case TYPE_BOOL:
        return "sizeof(bool)";
    case TYPE_STR:
        return "sizeof(urus_str*)";
    case TYPE_NAMED:
        return "sizeof(void*)";
    case TYPE_ARRAY:
        return "sizeof(urus_array*)";
    case TYPE_RESULT:
        return "sizeof(urus_result*)";
    case TYPE_TUPLE: {
        static char buf[128];
        snprintf(buf, sizeof(buf), "sizeof(%s)", tuple_type_name(t));
        return buf;
    }
    default:
        return "sizeof(int64_t)";
    }
}

// Return the C type cast for compound literal in push
static const char *elem_ctype(AstType *t)
{
    if (!t)
        return "int64_t";
    switch (t->kind) {
    case TYPE_INT:
        return "int64_t";
    case TYPE_FLOAT:
        return "double";
    case TYPE_BOOL:
        return "bool";
    case TYPE_STR:
        return "urus_str*";
    case TYPE_NAMED:
        return "void*";
    case TYPE_ARRAY:
        return "urus_array*";
    case TYPE_RESULT:
        return "urus_result*";
    case TYPE_TUPLE:
        return tuple_type_name(t);
    default:
        return "int64_t";
    }
}

// ---- Expression emission ----

static const char *binop_str(TokenType op)
{
    switch (op) {
    case TOK_PLUS:
        return "+";
    case TOK_MINUS:
        return "-";
    case TOK_STAR:
        return "*";
    case TOK_SLASH:
        return "/";
    case TOK_PERCENT:
        return "%";
    case TOK_PERCENT_PERCENT:
        return "%"; // floored mod handled specially
    case TOK_EQ:
        return "==";
    case TOK_NEQ:
        return "!=";
    case TOK_LT:
        return "<";
    case TOK_GT:
        return ">";
    case TOK_LTE:
        return "<=";
    case TOK_GTE:
        return ">=";
    case TOK_AND:
        return "&&";
    case TOK_OR:
        return "||";
    case TOK_AMP:
        return "&";
    case TOK_PIPE:
        return "|";
    case TOK_CARET:
        return "^";
    case TOK_SHL:
        return "<<";
    case TOK_SHR:
        return ">>";
    case TOK_AMP_TILDE:
        return "&~"; // handled specially
    default:
        return "?";
    }
}

static bool expr_is_string(AstNode *n)
{
    if (!n)
        return false;
    if (n->resolved_type && n->resolved_type->kind == TYPE_STR)
        return true;
    if (n->kind == NODE_STR_LIT)
        return true;
    if (n->kind == NODE_BINARY && n->as.binary.op == TOK_PLUS) {
        return expr_is_string(n->as.binary.left) ||
               expr_is_string(n->as.binary.right);
    }
    return false;
}

static void gen_array_get(CodeBuf *buf, AstNode *node)
{
    AstType *elem = node->resolved_type;
    const char *getter = "urus_array_get_int";
    if (elem) {
        switch (elem->kind) {
        case TYPE_FLOAT:
            getter = "urus_array_get_float";
            break;
        case TYPE_BOOL:
            getter = "urus_array_get_bool";
            break;
        case TYPE_STR:
            getter = "urus_array_get_str";
            break;
        case TYPE_NAMED:
            getter = "urus_array_get_ptr";
            break;
        case TYPE_ARRAY:
            getter = "urus_array_get_ptr";
            break;
        default:
            break;
        }
    }
    emit(buf, "%s(", getter);
    gen_expr(buf, node->as.index_expr.object);
    emit(buf, ", ");
    gen_expr(buf, node->as.index_expr.index);
    emit(buf, ")");
}

static void gen_expr(CodeBuf *buf, AstNode *node)
{
    if (!node)
        return;
    switch (node->kind) {
    case NODE_INT_LIT:
        emit(buf, "((int64_t)%lld)", node->as.int_lit.value);
        break;
    case NODE_FLOAT_LIT:
        emit(buf, "%f", node->as.float_lit.value);
        break;
    case NODE_STR_LIT: {
        emit(buf, "urus_str_from(\"");
        const char *s = node->as.str_lit.value;
        for (size_t i = 0; s[i]; i++) {
            if (s[i] == '"')
                emit(buf, "\\\"");
            else if (s[i] == '\\' && s[i + 1]) {
                char next = s[i + 1];
                if (next == 'n' || next == 'r' || next == 't' || next == '0' ||
                    next == '\\' || next == '"') {
                    emit(buf, "\\%c", next);
                    i++; // skip next char, already emitted
                } else {
                    emit(buf, "\\\\");
                }
            } else if (s[i] == '\n')
                emit(buf, "\\n");
            else
                emit(buf, "%c", s[i]);
        }
        emit(buf, "\")");
        break;
    }
    case NODE_BOOL_LIT:
        emit(buf, "%s", node->as.bool_lit.value ? "true" : "false");
        break;
    case NODE_IDENT:
        emit(buf, "%s", node->as.ident.name);
        break;
    case NODE_BINARY:
        if ((node->as.binary.op == TOK_EQ || node->as.binary.op == TOK_NEQ) &&
            node->as.binary.left->resolved_type &&
            node->as.binary.left->resolved_type->kind == TYPE_STR) {
            emit(buf, "(%surus_%s_equal(",
                 node->as.binary.op == TOK_EQ ? "" : "!",
                 ast_type_str(node->as.binary.left->resolved_type));
            gen_expr(buf, node->as.binary.left);
            emit(buf, ", ");
            gen_expr(buf, node->as.binary.right);
            emit(buf, "))");
            break;
        }

        if (node->as.binary.op == TOK_PLUS && expr_is_string(node)) {
            emit(buf, "urus_str_concat(");
            gen_expr(buf, node->as.binary.left);
            emit(buf, ", ");
            gen_expr(buf, node->as.binary.right);
            emit(buf, ")");
        } else if (node->as.binary.op == TOK_STARSTAR) {
            // exponent: a ** b -> pow((double)a, (double)b) or integer cast
            bool is_int = node->as.binary.left->resolved_type &&
                          node->as.binary.left->resolved_type->kind == TYPE_INT;
            if (is_int)
                emit(buf, "(int64_t)");
            emit(buf, "pow((double)");
            gen_expr(buf, node->as.binary.left);
            emit(buf, ", (double)");
            gen_expr(buf, node->as.binary.right);
            emit(buf, ")");
        } else if (node->as.binary.op == TOK_PERCENT_PERCENT) {
            // floored remainder: ((a % b) + b) % b
            emit(buf, "((");
            gen_expr(buf, node->as.binary.left);
            emit(buf, " %% ");
            gen_expr(buf, node->as.binary.right);
            emit(buf, " + ");
            gen_expr(buf, node->as.binary.right);
            emit(buf, ") %% ");
            gen_expr(buf, node->as.binary.right);
            emit(buf, ")");
        } else if (node->as.binary.op == TOK_AMP_TILDE) {
            // and-not: a &~ b -> (a & ~b)
            emit(buf, "(");
            gen_expr(buf, node->as.binary.left);
            emit(buf, " & ~");
            gen_expr(buf, node->as.binary.right);
            emit(buf, ")");
        } else {
            emit(buf, "(");
            gen_expr(buf, node->as.binary.left);
            emit(buf, " %s ", binop_str(node->as.binary.op));
            gen_expr(buf, node->as.binary.right);
            emit(buf, ")");
        }
        break;
    case NODE_UNARY:
        emit(buf, "(");
        emit(buf, "%s",
             node->as.unary.op == TOK_NOT     ? "!"
             : node->as.unary.op == TOK_TILDE ? "~"
                                              : "-");
        gen_expr(buf, node->as.unary.operand);
        emit(buf, ")");
        break;
    case NODE_CALL: {
        const char *fn_name = NULL;
        if (node->as.call.callee->kind == NODE_IDENT) {
            fn_name = node->as.call.callee->as.ident.name;
        }

        if (fn_name && strcmp(fn_name, "unwrap") == 0) {
            // Determine unwrap variant based on result's ok type
            const char *unwrap_fn = "urus_result_unwrap";
            if (node->resolved_type) {
                switch (node->resolved_type->kind) {
                case TYPE_FLOAT:
                    unwrap_fn = "urus_result_unwrap_float";
                    break;
                case TYPE_BOOL:
                    unwrap_fn = "urus_result_unwrap_bool";
                    break;
                case TYPE_STR:
                    unwrap_fn = "urus_result_unwrap_str";
                    break;
                case TYPE_NAMED:
                case TYPE_ARRAY:
                    unwrap_fn = "urus_result_unwrap_ptr";
                    break;
                default:
                    break;
                }
            }
            emit(buf, "%s(", unwrap_fn);
            if (node->as.call.arg_count > 0)
                gen_expr(buf, node->as.call.args[0]);
            emit(buf, ")");
        } else if (fn_name && strcmp(fn_name, "print") == 0) {
            emit(buf, "urus_print(");
            if (node->as.call.arg_count > 0)
                gen_expr(buf, node->as.call.args[0]);
            emit(buf, ")");
        } else if (fn_name && strcmp(fn_name, "to_str") == 0) {
            emit(buf, "to_str(");
            if (node->as.call.arg_count > 0)
                gen_expr(buf, node->as.call.args[0]);
            emit(buf, ")");
        } else if (fn_name && strcmp(fn_name, "to_int") == 0) {
            emit(buf, "to_int(");
            if (node->as.call.arg_count > 0)
                gen_expr(buf, node->as.call.args[0]);
            emit(buf, ")");
        } else if (fn_name && strcmp(fn_name, "to_float") == 0) {
            emit(buf, "to_float(");
            if (node->as.call.arg_count > 0)
                gen_expr(buf, node->as.call.args[0]);
            emit(buf, ")");
        } else if (fn_name && (strcmp(fn_name, "assert_eq") == 0 ||
                                  strcmp(fn_name, "assert_ne") == 0)) {
            bool is_eq = (strcmp(fn_name, "assert_eq") == 0);
            AstType *t = node->as.call.args[0]->resolved_type;
            if (t && t->kind == TYPE_STR) {
                emit(buf, "do { urus_str *_a = ");
                gen_expr(buf, node->as.call.args[0]);
                emit(buf, "; urus_str *_b = ");
                gen_expr(buf, node->as.call.args[1]);
                if (is_eq) {
                    emit(buf, "; if (strcmp(_a->data, _b->data) != 0) {"
                         " fprintf(stderr, \"Assertion failed: "
                         "assert_eq\\n  left:  %%s\\n  right: %%s\\n\","
                         " _a->data, _b->data); exit(1); } } while(0)");
                } else {
                    emit(buf, "; if (strcmp(_a->data, _b->data) == 0) {"
                         " fprintf(stderr, \"Assertion failed: "
                         "assert_ne\\n  both: %%s\\n\","
                         " _a->data); exit(1); } } while(0)");
                }
            } else if (t && t->kind == TYPE_FLOAT) {
                emit(buf, "do { double _a = ");
                gen_expr(buf, node->as.call.args[0]);
                emit(buf, "; double _b = ");
                gen_expr(buf, node->as.call.args[1]);
                if (is_eq) {
                    emit(buf, "; if (_a != _b) {"
                         " fprintf(stderr, \"Assertion failed: "
                         "assert_eq\\n  left:  %%g\\n  right: %%g\\n\","
                         " _a, _b); exit(1); } } while(0)");
                } else {
                    emit(buf, "; if (_a == _b) {"
                         " fprintf(stderr, \"Assertion failed: "
                         "assert_ne\\n  both: %%g\\n\","
                         " _a); exit(1); } } while(0)");
                }
            } else if (t && t->kind == TYPE_BOOL) {
                emit(buf, "do { bool _a = ");
                gen_expr(buf, node->as.call.args[0]);
                emit(buf, "; bool _b = ");
                gen_expr(buf, node->as.call.args[1]);
                if (is_eq) {
                    emit(buf, "; if (_a != _b) {"
                         " fprintf(stderr, \"Assertion failed: "
                         "assert_eq\\n  left:  %%s\\n  right: %%s\\n\","
                         " _a?\"true\":\"false\","
                         " _b?\"true\":\"false\"); exit(1); } } while(0)");
                } else {
                    emit(buf, "; if (_a == _b) {"
                         " fprintf(stderr, \"Assertion failed: "
                         "assert_ne\\n  both: %%s\\n\","
                         " _a?\"true\":\"false\"); exit(1); } } while(0)");
                }
            } else {
                // Default: int
                emit(buf, "do { int64_t _a = ");
                gen_expr(buf, node->as.call.args[0]);
                emit(buf, "; int64_t _b = ");
                gen_expr(buf, node->as.call.args[1]);
                if (is_eq) {
                    emit(buf, "; if (_a != _b) {"
                         " fprintf(stderr, \"Assertion failed: "
                         "assert_eq\\n  left:  %%lld\\n  right: %%lld\\n\","
                         " (long long)_a, (long long)_b);"
                         " exit(1); } } while(0)");
                } else {
                    emit(buf, "; if (_a == _b) {"
                         " fprintf(stderr, \"Assertion failed: "
                         "assert_ne\\n  both: %%lld\\n\","
                         " (long long)_a);"
                         " exit(1); } } while(0)");
                }
            }
        } else if (fn_name && strcmp(fn_name, "push") == 0) {
            // Determine element type from array arg's resolved_type
            AstType *elem = NULL;
            if (node->as.call.arg_count > 0) {
                AstType *arr_type = node->as.call.args[0]->resolved_type;
                if (arr_type && arr_type->kind == TYPE_ARRAY) {
                    elem = arr_type->element;
                }
            }
            const char *ctype = elem_ctype(elem);
            emit(buf, "urus_array_push(");
            if (node->as.call.arg_count > 0)
                gen_expr(buf, node->as.call.args[0]);
            emit(buf, ", &(%s){", ctype);
            if (node->as.call.arg_count > 1)
                gen_expr(buf, node->as.call.args[1]);
            emit(buf, "})");
        } else {
            const char *c_name = NULL;
            if (fn_name) {
                for (const BuiltinMap *m = urus_builtin_direct_maps; m->urus;
                     m++) {
                    if (strcmp(fn_name, m->urus) == 0) {
                        c_name = m->c;
                        break;
                    }
                }
            }
            if (c_name) {
                emit(buf, "%s(", c_name);
            } else if (fn_name && node->as.call.type_arg_count > 0 &&
                       node->as.call.type_args) {
                // Generic function call — use monomorphized name
                AstNode *gen_fn = _codegen_program
                    ? find_generic_fn(_codegen_program, fn_name) : NULL;
                const char *mangled = mono_get_or_add(
                    fn_name, node->as.call.type_args,
                    node->as.call.type_arg_count, gen_fn);
                emit(buf, "%s(", mangled);
            } else if (node->as.call.callee->resolved_type &&
                       node->as.call.callee->resolved_type->kind == TYPE_FN) {
                // Calling a function pointer variable — cast void* to proper fn ptr
                AstType *ft = node->as.call.callee->resolved_type;
                emit(buf, "((");
                gen_type(buf, ft->return_type);
                emit(buf, "(*)(");
                if (ft->param_count == 0) {
                    emit(buf, "void");
                } else {
                    for (int i = 0; i < ft->param_count; i++) {
                        if (i > 0) emit(buf, ", ");
                        gen_type(buf, ft->param_types[i]);
                    }
                }
                emit(buf, "))");
                gen_expr(buf, node->as.call.callee);
                emit(buf, ")(");
            } else {
                gen_expr(buf, node->as.call.callee);
                emit(buf, "(");
            }
            for (int i = 0; i < node->as.call.arg_count; i++) {
                if (i > 0)
                    emit(buf, ", ");
                gen_expr(buf, node->as.call.args[i]);
            }
            emit(buf, ")");
        }
        break;
    }
    case NODE_FIELD_ACCESS:
        if (node->as.field_access.object->resolved_type &&
            node->as.field_access.object->resolved_type->kind == TYPE_TUPLE) {
            gen_expr(buf, node->as.field_access.object);
            emit(buf, ".f%s", node->as.field_access.field);
        } else {
            gen_expr(buf, node->as.field_access.object);
            emit(buf, "->%s", node->as.field_access.field);
        }
        break;
    case NODE_TUPLE_LIT:
        emit(buf, "_urus_tup_%d", node->_codegen_tmp);
        break;
    case NODE_INDEX:
        gen_array_get(buf, node);
        break;
    case NODE_ARRAY_LIT:
        emit(buf, "_urus_arr_%d", node->_codegen_tmp);
        break;
    case NODE_STRUCT_LIT:
        emit(buf, "_urus_st_%d", node->_codegen_tmp);
        break;
    case NODE_ENUM_INIT:
        emit(buf, "_urus_en_%d", node->_codegen_tmp);
        break;
    case NODE_OK_EXPR:
        emit(buf, "_urus_res_%d", node->_codegen_tmp);
        break;
    case NODE_ERR_EXPR:
        emit(buf, "_urus_res_%d", node->_codegen_tmp);
        break;
    case NODE_IF_EXPR:
        emit(buf, "(");
        gen_expr(buf, node->as.if_expr.condition);
        emit(buf, " ? ");
        gen_expr(buf, node->as.if_expr.then_expr);
        emit(buf, " : ");
        gen_expr(buf, node->as.if_expr.else_expr);
        emit(buf, ")");
        break;
    case NODE_LAMBDA:
        // Lambda becomes a function pointer cast to void*
        emit(buf, "(void*)_urus_lambda_%d", node->as.lambda.lambda_id);
        break;
    case NODE_AWAIT_EXPR: {
        AstType *t = node->resolved_type;
        if (t && t->kind == TYPE_INT) {
            emit(buf, "urus_future_get_int(");
        } else if (t && t->kind == TYPE_FLOAT) {
            emit(buf, "urus_future_get_float(");
        } else if (t && t->kind == TYPE_BOOL) {
            emit(buf, "urus_future_get_bool(");
        } else if (t && t->kind == TYPE_STR) {
            emit(buf, "urus_future_get_str(");
        } else {
            emit(buf, "urus_future_get(");
        }
        gen_expr(buf, node->as.await_expr.expr);
        emit(buf, ")");
        break;
    }
    default:
        emit(buf, "/* unsupported expr */0");
        break;
    }
}

// Emit pre-statements for complex expressions (array/struct/enum literals)
// Returns the tmp variable name index, or -1 if no pre-statement needed
static int gen_expr_pre(CodeBuf *buf, AstNode *node)
{
    if (!node)
        return -1;

    switch (node->kind) {
    case NODE_ARRAY_LIT: {
        // First emit pre-statements for sub-expressions
        for (int i = 0; i < node->as.array_lit.count; i++) {
            gen_expr_pre(buf, node->as.array_lit.elements[i]);
        }
        int tmp = buf->tmp_counter++;
        node->_codegen_tmp = tmp;
        AstType *elem = NULL;
        if (node->resolved_type && node->resolved_type->kind == TYPE_ARRAY) {
            elem = node->resolved_type->element;
        }

        const char *sz = elem_sizeof(elem);
        const char *ctype = elem_ctype(elem);

        emit_indent(buf);
        emit(buf, "urus_array* _urus_arr_%d = urus_array_new(%s, %d, ", tmp, sz,
             node->as.array_lit.count > 0 ? node->as.array_lit.count : 4);
        if (elem && type_needs_drop(elem)) {
            emit(buf, "(urus_drop_fn)");
            emit_type_drop(buf, elem);
        } else {
            emit(buf, "NULL");
        }
        emit(buf, ");\n");
        for (int i = 0; i < node->as.array_lit.count; i++) {
            emit_indent(buf);
            emit(buf, "urus_array_push(_urus_arr_%d, &(%s){", tmp, ctype);
            gen_expr(buf, node->as.array_lit.elements[i]);
            emit(buf, "});\n");
        }

        return tmp;
    }
    case NODE_STRUCT_LIT: {
        for (int i = 0; i < node->as.struct_lit.field_count; i++) {
            gen_expr_pre(buf, node->as.struct_lit.fields[i].value);
        }
        if (node->as.struct_lit.spread) {
            gen_expr_pre(buf, node->as.struct_lit.spread);
        }
        int tmp = buf->tmp_counter++;
        node->_codegen_tmp = tmp;
        emit_indent(buf);
        emit(buf, "%s* _urus_st_%d = malloc(sizeof(%s));\n",
             node->as.struct_lit.name, tmp, node->as.struct_lit.name);

        // Spread: copy all fields from source, then override explicit ones
        if (node->as.struct_lit.spread) {
            emit_indent(buf);
            emit(buf, "*_urus_st_%d = *", tmp);
            gen_expr(buf, node->as.struct_lit.spread);
            emit(buf, "; // spread copy\n");
        }

        // Field assign (overrides spread fields if present)
        for (int i = 0; i < node->as.struct_lit.field_count; i++) {
            emit_indent(buf);
            emit(buf, "_urus_st_%d->%s = ", tmp,
                 node->as.struct_lit.fields[i].name);
            gen_expr(buf, node->as.struct_lit.fields[i].value);
            emit(buf, ";\n");

            if (type_needs_drop(
                    node->as.struct_lit.fields[i].value->resolved_type) &&
                node->as.struct_lit.fields[i].value->kind == NODE_IDENT) {
                emit_indent(buf);
                emit(buf, "%s = NULL; // move to struct field\n",
                     node->as.struct_lit.fields[i].value->as.ident.name);
            }
        }
        return tmp;
    }
    case NODE_ENUM_INIT: {
        for (int i = 0; i < node->as.enum_init.arg_count; i++) {
            gen_expr_pre(buf, node->as.enum_init.args[i]);
        }
        int tmp = buf->tmp_counter++;
        node->_codegen_tmp = tmp;
        const char *ename = node->as.enum_init.enum_name;
        const char *vname = node->as.enum_init.variant_name;
        emit_indent(buf);
        emit(buf, "%s* _urus_en_%d = malloc(sizeof(%s));\n", ename, tmp, ename);

        emit_indent(buf);
        emit(buf, "_urus_en_%d->tag = %s_TAG_%s;\n", tmp, ename, vname);
        for (int i = 0; i < node->as.enum_init.arg_count; i++) {
            emit_indent(buf);
            emit(buf, "_urus_en_%d->data.%s.f%d = ", tmp, vname, i);
            gen_expr(buf, node->as.enum_init.args[i]);
            emit(buf, ";\n");
            if (node->as.enum_init.args[i]->resolved_type &&
                type_needs_drop(node->as.enum_init.args[i]->resolved_type) &&
                node->as.enum_init.args[i]->kind == NODE_IDENT) {
                emit_indent(buf);
                emit(buf, "%s = NULL; // move to enum variant\n",
                     node->as.enum_init.args[i]->as.ident.name);
            }
        }
        return tmp;
    }
    case NODE_OK_EXPR: {
        gen_expr_pre(buf, node->as.result_expr.value);
        int tmp = buf->tmp_counter++;
        node->_codegen_tmp = tmp;
        emit_indent(buf);
        emit(buf, "urus_result* _urus_res_%d = urus_result_ok(", tmp);
        // Determine the correct box field based on value type
        AstType *val_type = node->as.result_expr.value
                                ? node->as.result_expr.value->resolved_type
                                : NULL;
        if (val_type && val_type->kind == TYPE_FLOAT) {
            emit(buf, "&(urus_box){.as_float = ");
            gen_expr(buf, node->as.result_expr.value);
            emit(buf, "}");
        } else if (val_type && val_type->kind == TYPE_BOOL) {
            emit(buf, "&(urus_box){.as_bool = ");
            gen_expr(buf, node->as.result_expr.value);
            emit(buf, "}");
        } else if (val_type && (val_type->kind == TYPE_STR ||
                                val_type->kind == TYPE_NAMED ||
                                val_type->kind == TYPE_ARRAY)) {
            emit(buf, "&(urus_box){.as_ptr = (void*)(");
            gen_expr(buf, node->as.result_expr.value);
            emit(buf, ")}");
        } else {
            emit(buf, "&(urus_box){.as_int = (int64_t)(");
            gen_expr(buf, node->as.result_expr.value);
            emit(buf, ")}");
        }

        // fill ok_drop
        if (val_type &&
            (val_type->kind == TYPE_STR || val_type->kind == TYPE_NAMED ||
             val_type->kind == TYPE_ARRAY)) {
            emit(buf, ", (urus_drop_fn)");
            emit_type_drop(buf, val_type);
            emit(buf, ");\n");
        } else {
            emit(buf, ", NULL);\n");
        }
        return tmp;
    }
    case NODE_ERR_EXPR: {
        gen_expr_pre(buf, node->as.result_expr.value);
        int tmp = buf->tmp_counter++;
        node->_codegen_tmp = tmp;
        emit_indent(buf);
        emit(buf, "urus_result* _urus_res_%d = urus_result_err(", tmp);
        gen_expr(buf, node->as.result_expr.value);
        emit(buf, ");\n");
        return tmp;
    }
    case NODE_CALL: {
        for (int i = 0; i < node->as.call.arg_count; i++) {
            gen_expr_pre(buf, node->as.call.args[i]);
        }
        return -1;
    }
    case NODE_TUPLE_LIT: {
        for (int i = 0; i < node->as.tuple_lit.count; i++) {
            gen_expr_pre(buf, node->as.tuple_lit.elements[i]);
        }
        int tmp = buf->tmp_counter++;
        node->_codegen_tmp = tmp;
        emit_indent(buf);
        gen_type(buf, node->resolved_type);
        emit(buf, " _urus_tup_%d = { ", tmp);
        for (int i = 0; i < node->as.tuple_lit.count; i++) {
            if (i > 0)
                emit(buf, ", ");
            gen_expr(buf, node->as.tuple_lit.elements[i]);
        }
        emit(buf, " };\n");
        return tmp;
    }
    case NODE_IF_EXPR:
        gen_expr_pre(buf, node->as.if_expr.condition);
        gen_expr_pre(buf, node->as.if_expr.then_expr);
        gen_expr_pre(buf, node->as.if_expr.else_expr);
        return -1;
    default:
        return -1;
    }
}

// ---- Statement emission ----

static void gen_stmt(CodeBuf *buf, AstNode *node)
{
    if (!node)
        return;

    switch (node->kind) {
    case NODE_LET_STMT:
        // Emit pre-statements for complex initializers
        gen_expr_pre(buf, node->as.let_stmt.init);

        if (node->as.let_stmt.is_destructure) {
            // Tuple destructuring: let (x, y): (int, str) = expr;
            AstType *tuple_t = node->as.let_stmt.type;
            int tmp = buf->tmp_counter++;
            emit_indent(buf);
            gen_type(buf, tuple_t);
            emit(buf, " _urus_dtmp_%d = ", tmp);
            gen_expr(buf, node->as.let_stmt.init);
            emit(buf, ";\n");
            for (int i = 0; i < node->as.let_stmt.name_count; i++) {
                AstType *ft = tuple_t->element_types[i];
                emit_indent(buf);
                if (type_needs_drop(ft)) {
                    const char *dtor = "NULL";
                    if (ft->kind == TYPE_STR)
                        dtor = "urus_str_drop";
                    else if (ft->kind == TYPE_ARRAY)
                        dtor = "urus_array_drop";
                    else if (ft->kind == TYPE_RESULT)
                        dtor = "urus_result_drop";
                    else if (ft->kind == TYPE_NAMED)
                        dtor = NULL;
                    else if (ft->kind == TYPE_TUPLE)
                        dtor = NULL;

                    if (dtor)
                        emit(buf, "URUS_RAII(%s) ", dtor);
                    else if (ft->kind == TYPE_NAMED)
                        emit(buf, "URUS_RAII(%s_drop) ", ft->name);
                    else if (ft->kind == TYPE_TUPLE)
                        emit(buf, "URUS_RAII(%s_drop) ", tuple_type_name(ft));
                }
                gen_type(buf, ft);
                emit(buf, " %s = _urus_dtmp_%d.f%d;\n",
                     node->as.let_stmt.names[i], tmp, i);
            }
            break;
        }

        emit_indent(buf);

        // Async call returns urus_future*, not the declared type
        if (node->as.let_stmt.init && node->as.let_stmt.init->is_async_call) {
            emit(buf, "URUS_RAII(urus_future_drop) urus_future* %s = ", node->as.let_stmt.name);
            gen_expr(buf, node->as.let_stmt.init);
            emit(buf, ";\n");
            break;
        }

        // Emit RAII auto destruct __attribute((cleanup()))
        bool needs_rc = type_needs_drop(node->as.let_stmt.type);
        if (needs_rc) {
            const char *dtor = "NULL";
            if (node->as.let_stmt.type->kind == TYPE_STR)
                dtor = "urus_str_drop";
            else if (node->as.let_stmt.type->kind == TYPE_ARRAY)
                dtor = "urus_array_drop";
            else if (node->as.let_stmt.type->kind == TYPE_RESULT)
                dtor = "urus_result_drop";
            else if (node->as.let_stmt.type->kind == TYPE_NAMED) {
                emit(buf, "URUS_RAII(%s_drop) ", node->as.let_stmt.type->name);
                needs_rc = false;
            } else if (node->as.let_stmt.type->kind == TYPE_TUPLE) {
                emit(buf, "URUS_RAII(%s_drop) ",
                     tuple_type_name(node->as.let_stmt.type));
                needs_rc = false;
            }
            if (needs_rc)
                emit(buf, "URUS_RAII(%s) ", dtor);
        }

        gen_type(buf, node->as.let_stmt.type);
        emit(buf, " %s = ", node->as.let_stmt.name);
        gen_expr(buf, node->as.let_stmt.init);
        emit(buf, ";\n");
        break;
    case NODE_ASSIGN_STMT: {
        // Check if target is array index - use setter instead
        // TODO: Add drop RAII handle here
        if (node->as.assign_stmt.target->kind == NODE_INDEX &&
            node->as.assign_stmt.op == TOK_ASSIGN) {
            gen_expr_pre(buf, node->as.assign_stmt.value);
            AstType *elem = node->as.assign_stmt.target->resolved_type;
            const char *ctype = elem_ctype(elem);
            emit_indent(buf);
            emit(buf, "urus_array_set(");
            gen_expr(buf, node->as.assign_stmt.target->as.index_expr.object);
            emit(buf, ", ");
            gen_expr(buf, node->as.assign_stmt.target->as.index_expr.index);
            emit(buf, ", &(%s){", ctype);
            gen_expr(buf, node->as.assign_stmt.value);
            emit(buf, "});\n");
        } else if (node->as.assign_stmt.op == TOK_PLUS_EQ &&
                   node->as.assign_stmt.target->resolved_type &&
                   node->as.assign_stmt.target->resolved_type->kind ==
                       TYPE_STR) {
            // String +=: expand to target = urus_str_concat(target, value)
            gen_expr_pre(buf, node->as.assign_stmt.value);
            emit_indent(buf);
            gen_expr(buf, node->as.assign_stmt.target);
            emit(buf, " = urus_str_concat(");
            gen_expr(buf, node->as.assign_stmt.target);
            emit(buf, ", ");
            gen_expr(buf, node->as.assign_stmt.value);
            emit(buf, ");\n");
        } else {
            gen_expr_pre(buf, node->as.assign_stmt.value);
            emit_indent(buf);
            gen_expr(buf, node->as.assign_stmt.target);
            const char *op = "=";
            switch (node->as.assign_stmt.op) {
            case TOK_PLUS_EQ:
                op = "+=";
                break;
            case TOK_MINUS_EQ:
                op = "-=";
                break;
            case TOK_STAR_EQ:
                op = "*=";
                break;
            case TOK_SLASH_EQ:
                op = "/=";
                break;
            case TOK_PERCENT_EQ:
                op = "%=";
                break;
            case TOK_AMP_EQ:
                op = "&=";
                break;
            case TOK_PIPE_EQ:
                op = "|=";
                break;
            case TOK_CARET_EQ:
                op = "^=";
                break;
            case TOK_SHL_EQ:
                op = "<<=";
                break;
            case TOK_SHR_EQ:
                op = ">>=";
                break;
            default:
                break;
            }
            emit(buf, " %s ", op);
            gen_expr(buf, node->as.assign_stmt.value);
            emit(buf, ";\n");
        }
        break;
    }
    case NODE_IF_STMT:
        emit_indent(buf);
        emit(buf, "if (");
        gen_expr(buf, node->as.if_stmt.condition);
        emit(buf, ") ");
        gen_block(buf, node->as.if_stmt.then_block);
        if (node->as.if_stmt.else_branch) {
            emit(buf, " else ");
            if (node->as.if_stmt.else_branch->kind == NODE_IF_STMT) {
                gen_stmt(buf, node->as.if_stmt.else_branch);
            } else {
                gen_block(buf, node->as.if_stmt.else_branch);
                emit(buf, "\n");
            }
        } else {
            emit(buf, "\n");
        }
        break;
    case NODE_WHILE_STMT:
        emit_indent(buf);
        emit(buf, "while (");
        gen_expr(buf, node->as.while_stmt.condition);
        emit(buf, ") ");
        gen_block(buf, node->as.while_stmt.body);
        emit(buf, "\n");
        break;
    case NODE_DO_WHILE_STMT:
        emit_indent(buf);
        emit(buf, "do ");
        gen_block(buf, node->as.do_while_stmt.body);
        emit(buf, " while (");
        gen_expr(buf, node->as.do_while_stmt.condition);
        emit(buf, ");\n");
        break;
    case NODE_FOR_STMT:
        if (node->as.for_stmt.is_foreach) {
            // For-each: for item in array { ... }
            gen_expr_pre(buf, node->as.for_stmt.iterable);
            int tmp = buf->tmp_counter++;
            char iterator_name[64];
            snprintf(iterator_name, sizeof(iterator_name), "_urus_iter_%d",
                     tmp);
            emit_indent(buf);
            emit(buf, "urus_array* %s = ", iterator_name);
            gen_expr(buf, node->as.for_stmt.iterable);
            emit(buf, ";\n");

            emit_indent(buf);
            emit(buf,
                 "for (int64_t _urus_idx_%d = 0; _urus_idx_%d < "
                 "(int64_t)_urus_iter_%d->len; _urus_idx_%d++) ",
                 tmp, tmp, tmp, tmp);
            emit(buf, "{\n");
            buf->indent++;

            // Determine element type and getter
            AstType *elem = NULL;
            if (node->as.for_stmt.iterable->resolved_type &&
                node->as.for_stmt.iterable->resolved_type->kind == TYPE_ARRAY) {
                elem = node->as.for_stmt.iterable->resolved_type->element;
            }
            const char *getter = "urus_array_get_int";
            if (elem) {
                switch (elem->kind) {
                case TYPE_FLOAT:
                    getter = "urus_array_get_float";
                    break;
                case TYPE_BOOL:
                    getter = "urus_array_get_bool";
                    break;
                case TYPE_STR:
                    getter = "urus_array_get_str";
                    break;
                case TYPE_NAMED:
                case TYPE_ARRAY:
                    getter = "urus_array_get_ptr";
                    break;
                default:
                    break;
                }
            }
            if (node->as.for_stmt.is_destructure && elem &&
                elem->kind == TYPE_TUPLE) {
                // Tuple destructuring: for (k, v) in arr { ... }
                emit_indent(buf);
                gen_type(buf, elem);
                emit(buf,
                     " _urus_dtup_%d; memcpy(&_urus_dtup_%d, "
                     "urus_array_get_ptr(_urus_iter_%d, _urus_idx_%d), sizeof(",
                     tmp, tmp, tmp, tmp);
                gen_type(buf, elem);
                emit(buf, "));\n");
                for (int i = 0; i < node->as.for_stmt.var_count; i++) {
                    emit_indent(buf);
                    gen_type(buf, elem->element_types[i]);
                    emit(buf, " %s = _urus_dtup_%d.f%d;\n",
                         node->as.for_stmt.var_names[i], tmp, i);
                }
            } else {
                emit_indent(buf);
                if (elem)
                    gen_type(buf, elem);
                else
                    emit(buf, "int64_t");
                emit(buf, " %s = %s(_urus_iter_%d, _urus_idx_%d);\n",
                     node->as.for_stmt.var_name, getter, tmp, tmp);
            }

            // Emit body statements
            for (int i = 0; i < node->as.for_stmt.body->as.block.stmt_count;
                 i++) {
                gen_stmt(buf, node->as.for_stmt.body->as.block.stmts[i]);
            }
            buf->indent--;
            emit_indent(buf);
            emit(buf, "}\n");
        } else {
            emit_indent(buf);
            emit(buf, "for (int64_t %s = ", node->as.for_stmt.var_name);
            gen_expr(buf, node->as.for_stmt.start);
            emit(buf, "; %s %s ", node->as.for_stmt.var_name,
                 node->as.for_stmt.inclusive ? "<=" : "<");
            gen_expr(buf, node->as.for_stmt.end);
            emit(buf, "; %s++) ", node->as.for_stmt.var_name);
            gen_block(buf, node->as.for_stmt.body);
            emit(buf, "\n");
        }
        break;
    case NODE_RETURN_STMT:
        if (node->as.return_stmt.value) {
            gen_expr_pre(buf, node->as.return_stmt.value);

            AstType *t = node->as.return_stmt.value->resolved_type;

            // For generic types, return directly without temp variable
            if (t && t->kind == TYPE_GENERIC) {
                emit_defers(buf);
                emit_indent(buf);
                emit(buf, "return ");
                gen_expr(buf, node->as.return_stmt.value);
                emit(buf, ";\n");
            } else {
                int tmp = buf->tmp_counter++;

                emit_indent(buf);
                gen_type(buf, t);
                emit(buf, " _urus_ret_%d = ", tmp);
                gen_expr(buf, node->as.return_stmt.value);
                emit(buf, ";\n");

                if (type_needs_drop(t) &&
                    node->as.return_stmt.value->kind == NODE_IDENT) {
                    emit_indent(buf);
                    emit(buf, "%s = NULL; // move to _urus_ret_%d\n",
                         node->as.return_stmt.value->as.ident.name, tmp);
                }

                emit_defers(buf);
                emit_indent(buf);
                emit(buf, "return _urus_ret_%d;\n", tmp);
            }
        } else {
            emit_defers(buf);
            emit_indent(buf);
            emit(buf, "return;\n");
        }
        break;
    case NODE_DEFER_STMT:
        if (defer_count >= 64) {
            fprintf(stderr, "Error: too many defer statements (max 64)\n");
            exit(1);
        }
        defer_stack[defer_count++] = node->as.defer_stmt.body;
        break;
    case NODE_BREAK_STMT:
        emit_indent(buf);
        emit(buf, "break;\n");
        break;
    case NODE_CONTINUE_STMT:
        emit_indent(buf);
        emit(buf, "continue;\n");
        break;
    case NODE_EXPR_STMT:
        gen_expr_pre(buf, node->as.expr_stmt.expr);
        emit_indent(buf);
        gen_expr(buf, node->as.expr_stmt.expr);
        emit(buf, ";\n");
        break;
    case NODE_EMIT_STMT:
        emit_indent(buf);
        emit(buf, "/* raw emit block statement */\n");
        emit_indent(buf);
        emit(buf, "%s\n", node->as.emit_stmt.content);
        break;
    case NODE_BLOCK:
        gen_block(buf, node);
        emit(buf, "\n");
        break;
    case NODE_MATCH: {
        gen_expr_pre(buf, node->as.match_stmt.target);
        int tmp = buf->tmp_counter++;
        AstType *target_type = node->as.match_stmt.target->resolved_type;
        bool is_primitive = target_type && (target_type->kind == TYPE_INT ||
                                            target_type->kind == TYPE_STR ||
                                            target_type->kind == TYPE_BOOL);

        if (is_primitive) {
            // Primitive match: store target in temp variable
            emit_indent(buf);
            gen_type(buf, target_type);
            emit(buf, " _urus_match_%d = ", tmp);
            gen_expr(buf, node->as.match_stmt.target);
            emit(buf, ";\n");

            bool first = true;
            for (int i = 0; i < node->as.match_stmt.arm_count; i++) {
                MatchArm *arm = &node->as.match_stmt.arms[i];
                emit_indent(buf);
                if (arm->is_wildcard) {
                    if (!first)
                        emit(buf, "else ");
                    emit(buf, "{\n");
                } else {
                    if (first)
                        emit(buf, "if (");
                    else
                        emit(buf, "else if (");

                    if (target_type->kind == TYPE_STR) {
                        emit(buf, "urus_str_equal(_urus_match_%d, ", tmp);
                        gen_expr(buf, arm->pattern_expr);
                        emit(buf, ")");
                    } else {
                        emit(buf, "_urus_match_%d == ", tmp);
                        gen_expr(buf, arm->pattern_expr);
                    }
                    emit(buf, ") {\n");
                    first = false;
                }
                buf->indent++;
                for (int s = 0; s < arm->body->as.block.stmt_count; s++) {
                    gen_stmt(buf, arm->body->as.block.stmts[s]);
                }
                buf->indent--;
                emit_indent(buf);
                emit(buf, "}\n");
            }
        } else {
            // Enum match
            emit_indent(buf);
            const char *ename = "";
            if (node->as.match_stmt.arm_count > 0 &&
                node->as.match_stmt.arms[0].enum_name) {
                ename = node->as.match_stmt.arms[0].enum_name;
            }
            emit(buf, "%s* _urus_match_%d = ", ename, tmp);
            gen_expr(buf, node->as.match_stmt.target);
            emit(buf, ";\n");

            for (int i = 0; i < node->as.match_stmt.arm_count; i++) {
                MatchArm *arm = &node->as.match_stmt.arms[i];
                emit_indent(buf);
                if (i == 0)
                    emit(buf, "if ");
                else
                    emit(buf, "else if ");
                emit(buf, "(_urus_match_%d->tag == %s_TAG_%s) ", tmp,
                     arm->enum_name, arm->variant_name);
                emit(buf, "{\n");
                buf->indent++;
                // Bind variant fields
                for (int b = 0; b < arm->binding_count; b++) {
                    emit_indent(buf);
                    if (arm->binding_types && arm->binding_types[b]) {
                        gen_type(buf, arm->binding_types[b]);
                    } else {
                        emit(buf, "int64_t");
                    }
                    emit(buf, " %s = _urus_match_%d->data.%s.f%d;\n",
                         arm->bindings[b], tmp, arm->variant_name, b);
                }
                // Emit body statements
                for (int s = 0; s < arm->body->as.block.stmt_count; s++) {
                    gen_stmt(buf, arm->body->as.block.stmts[s]);
                }
                buf->indent--;
                emit_indent(buf);
                emit(buf, "}\n");
            }
        }
        break;
    }
    default:
        emit_indent(buf);
        emit(buf, "/* unsupported stmt */\n");
        break;
    }
}

static void gen_block(CodeBuf *buf, AstNode *node)
{
    emit(buf, "{\n");
    buf->indent++;
    for (int i = 0; i < node->as.block.stmt_count; i++) {
        gen_stmt(buf, node->as.block.stmts[i]);
    }
    buf->indent--;
    emit_indent(buf);
    emit(buf, "}");
}

// ---- Top-level declarations ----

static void gen_enum_decl(CodeBuf *buf, AstNode *node)
{
    const char *name = node->as.enum_decl.name;

    // Tag enum
    emit(buf, "enum {\n");
    for (int i = 0; i < node->as.enum_decl.variant_count; i++) {
        emit(buf, "    %s_TAG_%s = %d,\n", name,
             node->as.enum_decl.variants[i].name, i);
    }
    emit(buf, "};\n\n");

    // Tagged union struct
    emit(buf, "typedef struct %s {\n", name);
    emit(buf, "    int tag;\n");
    emit(buf, "    union {\n");
    for (int i = 0; i < node->as.enum_decl.variant_count; i++) {
        EnumVariant *v = &node->as.enum_decl.variants[i];
        if (v->field_count > 0) {
            emit(buf, "        struct {\n");
            for (int j = 0; j < v->field_count; j++) {
                emit(buf, "            ");
                gen_type(buf, v->fields[j].type);
                emit(buf, " f%d; // %s\n", j, v->fields[j].name);
            }
            emit(buf, "        } %s;\n", v->name);
        }
    }
    emit(buf, "    } data;\n");
    emit(buf, "} %s;\n\n", name);
}

static void gen_fn_forward(CodeBuf *buf, AstNode *node)
{
    bool is_main = strcmp(node->as.fn_decl.name, "main") == 0;
    if (node->as.fn_decl.is_async && !is_main) {
        emit(buf, "urus_future* %s(", node->as.fn_decl.name);
    } else {
        gen_type(buf, node->as.fn_decl.return_type);
        emit(buf, " %s(", is_main ? "urus_main" : node->as.fn_decl.name);
    }
    if (node->as.fn_decl.param_count == 0) {
        emit(buf, "void");
    } else {
        for (int i = 0; i < node->as.fn_decl.param_count; i++) {
            if (i > 0)
                emit(buf, ", ");
            gen_type(buf, node->as.fn_decl.params[i].type);
            emit(buf, " %s", node->as.fn_decl.params[i].name);
        }
    }
    emit(buf, ");\n");
}

static void gen_fn_decl(CodeBuf *buf, AstNode *node)
{
    bool is_main = strcmp(node->as.fn_decl.name, "main") == 0;
    const char *fn_name = is_main ? "urus_main" : node->as.fn_decl.name;

    if (node->as.fn_decl.is_async && !is_main) {
        // Generate async function: args struct + thread body + wrapper
        const char *name = node->as.fn_decl.name;
        AstType *ret = node->as.fn_decl.return_type;
        int pc = node->as.fn_decl.param_count;

        // 1. Args struct
        if (pc > 0) {
            emit(buf, "typedef struct { urus_future *_fut;");
            for (int i = 0; i < pc; i++) {
                emit(buf, " ");
                gen_type(buf, node->as.fn_decl.params[i].type);
                emit(buf, " %s;", node->as.fn_decl.params[i].name);
            }
            emit(buf, " } _async_%s_args;\n", name);
        }

        // 2. Inner body function (does the actual work)
        gen_type(buf, ret);
        emit(buf, " _async_%s_body(", name);
        if (pc == 0) {
            emit(buf, "void");
        } else {
            for (int i = 0; i < pc; i++) {
                if (i > 0) emit(buf, ", ");
                gen_type(buf, node->as.fn_decl.params[i].type);
                emit(buf, " %s", node->as.fn_decl.params[i].name);
            }
        }
        emit(buf, ") ");
        {
            int saved_defer_count = defer_count;
            defer_count = 0;
            AstNode *body = node->as.fn_decl.body;
            emit(buf, "{\n");
            buf->indent++;
            for (int i = 0; i < body->as.block.stmt_count; i++) {
                gen_stmt(buf, body->as.block.stmts[i]);
            }
            if (defer_count > 0 && ret && ret->kind == TYPE_VOID) {
                emit_defers(buf);
            }
            buf->indent--;
            emit_indent(buf);
            emit(buf, "}\n\n");
            defer_count = saved_defer_count;
        }

        // 3. Thread entry point
        emit(buf, "#ifdef _WIN32\n");
        emit(buf, "static unsigned __stdcall _async_%s_thread(void *_arg) {\n", name);
        emit(buf, "#else\n");
        emit(buf, "static void *_async_%s_thread(void *_arg) {\n", name);
        emit(buf, "#endif\n");
        if (pc > 0) {
            emit(buf, "    _async_%s_args *args = (_async_%s_args *)_arg;\n", name, name);
            emit(buf, "    ");
            if (ret && ret->kind != TYPE_VOID) {
                gen_type(buf, ret);
                emit(buf, " _result = ");
            }
            emit(buf, "_async_%s_body(", name);
            for (int i = 0; i < pc; i++) {
                if (i > 0) emit(buf, ", ");
                emit(buf, "args->%s", node->as.fn_decl.params[i].name);
            }
            emit(buf, ");\n");
            if (ret && ret->kind != TYPE_VOID) {
                emit(buf, "    urus_future_set_result(args->_fut, &_result, sizeof(_result));\n");
            }
            emit(buf, "    free(args);\n");
        } else {
            emit(buf, "    urus_future *_fut = (urus_future *)_arg;\n");
            emit(buf, "    ");
            if (ret && ret->kind != TYPE_VOID) {
                gen_type(buf, ret);
                emit(buf, " _result = ");
            }
            emit(buf, "_async_%s_body();\n", name);
            if (ret && ret->kind != TYPE_VOID) {
                emit(buf, "    urus_future_set_result(_fut, &_result, sizeof(_result));\n");
            }
        }
        emit(buf, "#ifdef _WIN32\n");
        emit(buf, "    return 0;\n");
        emit(buf, "#else\n");
        emit(buf, "    return NULL;\n");
        emit(buf, "#endif\n");
        emit(buf, "}\n\n");

        // 4. Public wrapper that returns urus_future*
        emit(buf, "urus_future* %s(", name);
        if (pc == 0) {
            emit(buf, "void");
        } else {
            for (int i = 0; i < pc; i++) {
                if (i > 0) emit(buf, ", ");
                gen_type(buf, node->as.fn_decl.params[i].type);
                emit(buf, " %s", node->as.fn_decl.params[i].name);
            }
        }
        emit(buf, ") {\n");
        emit(buf, "    urus_future *_fut = urus_future_new();\n");
        if (pc > 0) {
            emit(buf, "    _async_%s_args *_args = malloc(sizeof(_async_%s_args));\n", name, name);
            emit(buf, "    _args->_fut = _fut;\n");
            for (int i = 0; i < pc; i++) {
                emit(buf, "    _args->%s = %s;\n",
                     node->as.fn_decl.params[i].name,
                     node->as.fn_decl.params[i].name);
            }
            emit(buf, "    urus_future_start(_fut, _async_%s_thread, _args);\n", name);
        } else {
            emit(buf, "    urus_future_start(_fut, _async_%s_thread, _fut);\n", name);
        }
        emit(buf, "    return _fut;\n}\n\n");
        return;
    }

    gen_type(buf, node->as.fn_decl.return_type);
    emit(buf, " %s(", fn_name);
    if (node->as.fn_decl.param_count == 0) {
        emit(buf, "void");
    } else {
        for (int i = 0; i < node->as.fn_decl.param_count; i++) {
            if (i > 0)
                emit(buf, ", ");
            gen_type(buf, node->as.fn_decl.params[i].type);
            emit(buf, " %s", node->as.fn_decl.params[i].name);
        }
    }
    emit(buf, ") ");

    // Reset defer stack for this function
    int saved_defer_count = defer_count;
    defer_count = 0;

    // Emit function body with defers at end for void functions
    AstNode *body = node->as.fn_decl.body;
    emit(buf, "{\n");
    buf->indent++;
    for (int i = 0; i < body->as.block.stmt_count; i++) {
        gen_stmt(buf, body->as.block.stmts[i]);
    }
    if (defer_count > 0 && node->as.fn_decl.return_type &&
        node->as.fn_decl.return_type->kind == TYPE_VOID) {
        emit_defers(buf);
    }
    buf->indent--;
    emit_indent(buf);
    emit(buf, "}\n\n");

    defer_count = saved_defer_count;
}

// ---- Program ----

// Generate a monomorphized (specialized) version of a generic function
static void gen_mono_fn(CodeBuf *buf, MonoInstance *m)
{
    AstNode *node = m->fn_node;
    if (!node) return;

    // Create substituted types for params and return type
    int gpc = node->as.fn_decl.generic_param_count;
    char **gnames = node->as.fn_decl.generic_params;

    // Emit forward declaration
    AstType *ret = sema_substitute_type(node->as.fn_decl.return_type,
                                        gnames, m->type_args, gpc);
    gen_type(buf, ret);
    emit(buf, " %s(", m->mangled_name);
    if (node->as.fn_decl.param_count == 0) {
        emit(buf, "void");
    } else {
        for (int i = 0; i < node->as.fn_decl.param_count; i++) {
            if (i > 0) emit(buf, ", ");
            AstType *pt = sema_substitute_type(node->as.fn_decl.params[i].type,
                                               gnames, m->type_args, gpc);
            gen_type(buf, pt);
            emit(buf, " %s", node->as.fn_decl.params[i].name);
        }
    }
    emit(buf, ") {\n");

    // Emit body
    int saved_defer_count = defer_count;
    defer_count = 0;
    buf->indent++;
    AstNode *body = node->as.fn_decl.body;
    for (int i = 0; i < body->as.block.stmt_count; i++) {
        gen_stmt(buf, body->as.block.stmts[i]);
    }
    if (defer_count > 0 && ret && ret->kind == TYPE_VOID) {
        emit_defers(buf);
    }
    buf->indent--;
    emit_indent(buf);
    emit(buf, "}\n\n");
    defer_count = saved_defer_count;
}

void codegen_generate(CodeBuf *buf, AstNode *program)
{
    _codegen_program = program;
    mono_count = 0;

    emit(buf, "// Generated by: URUS Compiler, version %s\n",
         URUS_COMPILER_VERSION);
    emit(buf, "%.*s\n", urus_runtime_header_data_len, urus_runtime_header_data);
    emit(buf, "\n\n/* +---+ Program start +---+ */\n\n");

    // Pass 0: emit raw toplevel emit block
    for (int i = 0; i < program->as.program.decl_count; i++) {
        AstNode *d = program->as.program.decls[i];
        if (d->kind == NODE_EMIT_STMT && d->as.emit_stmt.is_toplevel) {
            emit(buf, "/* toplevel raw emit block statement */\n");
            emit(buf, "%s\n", d->as.emit_stmt.content);
        }
    }

    // Pass 1b: collect and emit tuple typedefs
    tuple_typedef_count = 0;
    collect_and_emit_tuple_typedefs(buf, program);
    if (tuple_typedef_count > 0)
        emit(buf, "\n");

    // Pass 1c: struct and enum forward declarations
    for (int i = 0; i < program->as.program.decl_count; i++) {
        AstNode *d = program->as.program.decls[i];
        if (d->kind == NODE_STRUCT_DECL) {
            emit(buf, "typedef struct %s %s;\n", d->as.struct_decl.name,
                 d->as.struct_decl.name);
        }
        // Enums get forward-declared via typedef
        if (d->kind == NODE_ENUM_DECL) {
            emit(buf, "typedef struct %s %s;\n", d->as.enum_decl.name,
                 d->as.enum_decl.name);
        }
    }

    // Const declarations
    for (int i = 0; i < program->as.program.decl_count; i++) {
        AstNode *d = program->as.program.decls[i];
        if (d->kind == NODE_CONST_DECL) {
            AstNode *val = d->as.const_decl.value;
            if (val->kind == NODE_INT_LIT) {
                emit(buf, "#define %s ((int64_t)%lld)\n", d->as.const_decl.name,
                     val->as.int_lit.value);
            } else if (val->kind == NODE_FLOAT_LIT) {
                emit(buf, "#define %s ((double)%g)\n", d->as.const_decl.name,
                     val->as.float_lit.value);
            } else if (val->kind == NODE_BOOL_LIT) {
                emit(buf, "#define %s %s\n", d->as.const_decl.name,
                     val->as.bool_lit.value ? "true" : "false");
            } else if (val->kind == NODE_STR_LIT) {
                emit(buf, "static urus_str *%s;\n", d->as.const_decl.name);
            }
        }
    }
    emit(buf, "\n");

    // Pass 2: struct definitions
    for (int i = 0; i < program->as.program.decl_count; i++) {
        AstNode *d = program->as.program.decls[i];
        if (d->kind == NODE_STRUCT_DECL) {
            emit(buf, "struct %s {\n", d->as.struct_decl.name);
            for (int j = 0; j < d->as.struct_decl.field_count; j++) {
                emit(buf, "    ");
                gen_type(buf, d->as.struct_decl.fields[j].type);
                emit(buf, " %s;\n", d->as.struct_decl.fields[j].name);
            }
            emit(buf, "};\n\n");
        }
    }

    // Pass 2b: enum definitions
    for (int i = 0; i < program->as.program.decl_count; i++) {
        AstNode *d = program->as.program.decls[i];
        if (d->kind == NODE_ENUM_DECL) {
            gen_enum_decl(buf, d);
        }
    }

    // Pass 3: function forward declarations (skip generic templates)
    for (int i = 0; i < program->as.program.decl_count; i++) {
        AstNode *d = program->as.program.decls[i];
        if (d->kind == NODE_FN_DECL && d->as.fn_decl.generic_param_count == 0) {
            gen_fn_forward(buf, d);
        }
    }
    // Test function forward declarations
    {
        int test_idx = 0;
        for (int i = 0; i < program->as.program.decl_count; i++) {
            AstNode *d = program->as.program.decls[i];
            if (d->kind == NODE_TEST_DECL) {
                emit(buf, "static void _urus_test_%d(void);\n", test_idx++);
            }
        }
    }
    emit(buf, "\n");

    // Pass 3 (impl): forward declarations for impl methods
    for (int i = 0; i < program->as.program.decl_count; i++) {
        AstNode *d = program->as.program.decls[i];
        if (d->kind == NODE_IMPL_BLOCK) {
            for (int j = 0; j < d->as.impl_block.method_count; j++) {
                gen_fn_forward(buf, d->as.impl_block.methods[j]);
            }
        }
    }

    // Pass 3B: struct drop forward declarations
    for (int i = 0; i < program->as.program.decl_count; i++) {
        AstNode *d = program->as.program.decls[i];
        if (d->kind == NODE_STRUCT_DECL) {
            emit(buf, "static void %s_drop(%s **obj);\n",
                 d->as.struct_decl.name, d->as.struct_decl.name);
        }
    }

    // Pass 3C: enum drop forward declarations
    for (int i = 0; i < program->as.program.decl_count; i++) {
        AstNode *d = program->as.program.decls[i];
        if (d->kind == NODE_ENUM_DECL) {
            emit(buf, "static void %s_drop(%s **obj);\n", d->as.enum_decl.name,
                 d->as.enum_decl.name);
        }
    }
    emit(buf, "\n");

    // Pass 3D: struct drop function
    for (int i = 0; i < program->as.program.decl_count; i++) {
        AstNode *d = program->as.program.decls[i];
        if (d->kind == NODE_STRUCT_DECL) {
            emit(buf, "static void %s_drop(%s **obj) {\n",
                 d->as.struct_decl.name, d->as.struct_decl.name);
            emit(buf, "    if (obj && *obj) {\n");
            for (int j = 0; j < d->as.struct_decl.field_count; j++) {
                AstType *ft = d->as.struct_decl.fields[j].type;
                if (type_needs_drop(ft)) {
                    char field_acc[512];
                    int fa_len = snprintf(field_acc, sizeof(field_acc), "(*obj)->%s",
                             d->as.struct_decl.fields[j].name);
                    if (fa_len >= (int)sizeof(field_acc)) {
                        fprintf(stderr, "Error: field name too long: %s\n",
                                d->as.struct_decl.fields[j].name);
                        exit(1);
                    }
                    emit(buf, "        ");
                    emit_type_drop_cname(buf, ft, field_acc);
                }
            }
            emit(buf, "        free(*obj);\n"
                      "        *obj = NULL;\n"
                      "    }\n"
                      "}\n\n");
        }
    }

    // Pass 3E: enum drop function
    for (int i = 0; i < program->as.program.decl_count; i++) {
        AstNode *d = program->as.program.decls[i];
        if (d->kind == NODE_ENUM_DECL) {
            emit(buf, "static void %s_drop(%s **obj) {\n", d->as.enum_decl.name,
                 d->as.enum_decl.name);
            emit(buf, "    if (obj && *obj) {\n");
            emit(buf, "        switch ((*obj)->tag) {\n");

            for (int j = 0; j < d->as.enum_decl.variant_count; j++) {
                EnumVariant *v = &d->as.enum_decl.variants[j];
                if (v->field_count > 0) {
                    emit(buf, "            case %s_TAG_%s:\n",
                         d->as.enum_decl.name, v->name);
                    for (int k = 0; k < v->field_count; k++) {
                        if (type_needs_drop(v->fields[k].type)) {
                            char field_acc[512];
                            int fa_len = snprintf(field_acc, sizeof(field_acc),
                                     "(*obj)->data.%s.f%d", v->name, k);
                            if (fa_len >= (int)sizeof(field_acc)) {
                                fprintf(stderr, "Error: variant name too long: %s\n", v->name);
                                exit(1);
                            }
                            emit(buf, "                ");
                            emit_type_drop_cname(buf, v->fields[k].type,
                                                 field_acc);
                        }
                    }
                    emit(buf, "                break;\n");
                }
            }

            emit(buf, "        }\n"
                      "        free(*obj);\n"
                      "        *obj = NULL;\n"
                      "    }\n"
                      "}\n\n");
        }
    }

    // Pass 3F: collect and emit lambda functions
    lambda_count = 0;
    collect_lambdas(program);
    for (int i = 0; i < lambda_count; i++) {
        // Forward declaration
        gen_lambda_fn(buf, lambda_nodes[i]);
        emit(buf, ";\n");
    }
    emit(buf, "\n");
    for (int i = 0; i < lambda_count; i++) {
        gen_lambda_fn(buf, lambda_nodes[i]);
        emit(buf, " ");
        gen_block(buf, lambda_nodes[i]->as.lambda.body);
        emit(buf, "\n");
    }

    // Pass 4: function definitions (skip generic templates)
    for (int i = 0; i < program->as.program.decl_count; i++) {
        AstNode *d = program->as.program.decls[i];
        if (d->kind == NODE_FN_DECL && d->as.fn_decl.generic_param_count == 0) {
            gen_fn_decl(buf, d);
        }
    }

    // Pass 4 (impl): impl method definitions
    for (int i = 0; i < program->as.program.decl_count; i++) {
        AstNode *d = program->as.program.decls[i];
        if (d->kind == NODE_IMPL_BLOCK) {
            for (int j = 0; j < d->as.impl_block.method_count; j++) {
                gen_fn_decl(buf, d->as.impl_block.methods[j]);
            }
        }
    }

    // Pass 4 (test): test function definitions
    {
        int test_idx = 0;
        for (int i = 0; i < program->as.program.decl_count; i++) {
            AstNode *d = program->as.program.decls[i];
            if (d->kind == NODE_TEST_DECL) {
                emit(buf, "static void _urus_test_%d(void) ", test_idx++);
                gen_block(buf, d->as.test_decl.body);
                emit(buf, "\n");
            }
        }
    }

    // Pass 4b: emit monomorphized generic function specializations
    // We iterate with index because gen_mono_fn may add new instances
    for (int i = 0; i < mono_count; i++) {
        // Emit forward decl first
        MonoInstance *m = &mono_instances[i];
        if (!m->fn_node) continue;
        int gpc = m->fn_node->as.fn_decl.generic_param_count;
        char **gnames = m->fn_node->as.fn_decl.generic_params;
        AstType *ret = sema_substitute_type(
            m->fn_node->as.fn_decl.return_type, gnames, m->type_args, gpc);
        gen_type(buf, ret);
        emit(buf, " %s(", m->mangled_name);
        if (m->fn_node->as.fn_decl.param_count == 0) {
            emit(buf, "void");
        } else {
            for (int j = 0; j < m->fn_node->as.fn_decl.param_count; j++) {
                if (j > 0) emit(buf, ", ");
                AstType *pt = sema_substitute_type(
                    m->fn_node->as.fn_decl.params[j].type,
                    gnames, m->type_args, gpc);
                gen_type(buf, pt);
                emit(buf, " %s", m->fn_node->as.fn_decl.params[j].name);
            }
        }
        emit(buf, ");\n");
    }
    emit(buf, "\n");
    for (int i = 0; i < mono_count; i++) {
        gen_mono_fn(buf, &mono_instances[i]);
    }

    if (_codegen_test_mode) {
        // Test runner main
        emit(buf, "int main() {\n");
        // Initialize string constants
        for (int i = 0; i < program->as.program.decl_count; i++) {
            AstNode *d = program->as.program.decls[i];
            if (d->kind == NODE_CONST_DECL &&
                d->as.const_decl.value->kind == NODE_STR_LIT) {
                emit(buf, "   %s = urus_str_from(\"%s\");\n",
                     d->as.const_decl.name,
                     d->as.const_decl.value->as.str_lit.value);
            }
        }
        // Count tests
        int test_count = 0;
        for (int i = 0; i < program->as.program.decl_count; i++) {
            if (program->as.program.decls[i]->kind == NODE_TEST_DECL)
                test_count++;
        }
        emit(buf, "   int _passed = 0, _failed = 0;\n");
        emit(buf, "   fprintf(stderr, \"Running %d tests...\\n\");\n",
             test_count);
        int test_idx = 0;
        for (int i = 0; i < program->as.program.decl_count; i++) {
            AstNode *d = program->as.program.decls[i];
            if (d->kind == NODE_TEST_DECL) {
                char *escaped = d->as.test_decl.name;
                emit(buf,
                     "   fprintf(stderr, \"  test \\\"%s\\\"... \");\n",
                     escaped);
                emit(buf, "   _urus_test_%d();\n", test_idx);
                emit(buf, "   fprintf(stderr, \"PASSED\\n\");\n");
                emit(buf, "   _passed++;\n");
                test_idx++;
            }
        }
        emit(buf, "   fprintf(stderr, \"\\nResults: %%d passed, "
                  "%%d failed\\n\", _passed, _failed);\n");
        emit(buf, "   return _failed > 0 ? 1 : 0;\n");
        emit(buf, "}\n");
    } else {
        // C main wrapper — check if urus main accepts argc/argv
        bool main_has_args = false;
        for (int i = 0; i < program->as.program.decl_count; i++) {
            AstNode *d = program->as.program.decls[i];
            if (d->kind == NODE_FN_DECL &&
                strcmp(d->as.fn_decl.name, "main") == 0) {
                main_has_args = d->as.fn_decl.param_count >= 2;
                break;
            }
        }

        if (main_has_args) {
            emit(buf, "int main(int argc, char **argv) {\n");
        } else {
            emit(buf, "int main() {\n");
        }

        // Initialize string constants
        for (int i = 0; i < program->as.program.decl_count; i++) {
            AstNode *d = program->as.program.decls[i];
            if (d->kind == NODE_CONST_DECL &&
                d->as.const_decl.value->kind == NODE_STR_LIT) {
                emit(buf, "   %s = urus_str_from(\"%s\");\n",
                     d->as.const_decl.name,
                     d->as.const_decl.value->as.str_lit.value);
            }
        }

        if (main_has_args) {
            emit(buf,
                 "   urus_array *_urus_argv = urus_array_new(sizeof(urus_str "
                 "*), (size_t)argc, (urus_drop_fn)urus_str_drop);\n"
                 "   for (int i = 0; i < argc; i++) {\n"
                 "       urus_str *s = urus_str_from(argv[i]);\n"
                 "       urus_array_push(_urus_argv, &s);\n"
                 "   }\n"
                 "   urus_main((int64_t)argc, _urus_argv);\n"
                 "   urus_array_drop(&_urus_argv);\n");
        } else {
            emit(buf, "   urus_main();\n");
        }

        emit(buf, "   return 0;\n"
                  "}\n");
    }
}

void codegen_generate_tests(CodeBuf *buf, AstNode *program)
{
    _codegen_test_mode = true;
    codegen_generate(buf, program);
    _codegen_test_mode = false;
}
