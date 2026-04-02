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

// ---- Reporting system ----

static void sema_error(SemaCtx *ctx, Token *t, const char *fmt, ...)
{
    if (ctx->current_fn_name[0]) {
        report(ctx->filename,
               "in function \033[1m'%s'\033[0m:", ctx->current_fn_name);
    }
    char msg[1024];

    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    report_error(ctx->filename, t, msg);
    ctx->errors++;
}

static void sema_warn(SemaCtx *ctx, Token *t, const char *fmt, ...)
{
    if (ctx->current_fn_name[0]) {
        report(ctx->filename,
               "in function \033[1m'%s'\033[0m:", ctx->current_fn_name);
    }
    char msg[1024];

    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    report_warn(ctx->filename, t, msg);
}

// ---- Forward declarations ----
static AstType *check_expr(SemaCtx *ctx, AstNode *node);
static void check_stmt(SemaCtx *ctx, AstNode *node);
static void check_block(SemaCtx *ctx, AstNode *node);

// ---- Type alias resolution ----

static AstType *sema_resolve_type(SemaCtx *ctx, AstType *t)
{
    if (!t)
        return t;
    if (t->kind == TYPE_NAMED) {
        SemaSymbol *sym = scope_lookup(ctx->current, t->name);
        if (sym && sym->tag == TYPE_SYM_TAG) {
            return ast_type_clone(sym->alias_type);
        }
    }
    if (t->kind == TYPE_ARRAY && t->element) {
        t->element = sema_resolve_type(ctx, t->element);
    }
    if (t->kind == TYPE_RESULT) {
        t->ok_type = sema_resolve_type(ctx, t->ok_type);
        t->err_type = sema_resolve_type(ctx, t->err_type);
    }
    if (t->kind == TYPE_TUPLE) {
        for (int i = 0; i < t->element_count; i++)
            t->element_types[i] = sema_resolve_type(ctx, t->element_types[i]);
    }
    return t;
}

// ---- Scope management ---

SemaScope *scope_new(SemaScope *parent)
{
    SemaScope *s = calloc(1, sizeof(SemaScope));
    s->parent = parent;
    s->cap = 8;
    s->syms = xmalloc(sizeof(SemaSymbol) * (size_t)s->cap);
    return s;
}

void scope_free(SemaScope *s)
{
    xfree(s->syms);
    xfree(s);
}

SemaSymbol *scope_lookup_local(SemaScope *s, const char *name)
{
    for (int i = 0; i < s->count; i++) {
        if (strcmp(s->syms[i].name, name) == 0)
            return &s->syms[i];
    }
    return NULL;
}

SemaSymbol *scope_lookup(SemaScope *s, const char *name)
{
    for (SemaScope *cur = s; cur; cur = cur->parent) {
        SemaSymbol *sym = scope_lookup_local(cur, name);
        if (sym)
            return sym;
    }
    return NULL;
}

SemaSymbol *scope_add(SemaScope *s, const char *name, Token tok)
{
    if (s->count >= s->cap) {
        s->cap *= 2;
        s->syms = xrealloc(s->syms, sizeof(SemaSymbol) * (size_t)s->cap);
    }
    SemaSymbol *sym = &s->syms[s->count++];
    memset(sym, 0, sizeof(SemaSymbol));
    sym->name = (char *)name;
    sym->tok = tok;
    return sym;
}

// ---- Generic type substitution ----

// Substitute TYPE_GENERIC nodes in a type tree with concrete types.
// generic_names/concrete_types arrays must be parallel with count entries.
AstType *sema_substitute_type(AstType *t, char **generic_names,
                              AstType **concrete_types, int count)
{
    if (!t) return NULL;
    if (t->kind == TYPE_GENERIC || t->kind == TYPE_NAMED) {
        for (int i = 0; i < count; i++) {
            if (strcmp(t->name, generic_names[i]) == 0)
                return ast_type_clone(concrete_types[i]);
        }
        return ast_type_clone(t);
    }
    if (t->kind == TYPE_ARRAY) {
        return ast_type_array(
            sema_substitute_type(t->element, generic_names, concrete_types, count));
    }
    if (t->kind == TYPE_RESULT) {
        return ast_type_result(
            sema_substitute_type(t->ok_type, generic_names, concrete_types, count),
            sema_substitute_type(t->err_type, generic_names, concrete_types, count));
    }
    if (t->kind == TYPE_TUPLE) {
        AstType **elems = xmalloc(sizeof(AstType *) * (size_t)t->element_count);
        for (int i = 0; i < t->element_count; i++)
            elems[i] = sema_substitute_type(t->element_types[i], generic_names,
                                            concrete_types, count);
        return ast_type_tuple(elems, t->element_count);
    }
    if (t->kind == TYPE_FN) {
        AstType **params = xmalloc(sizeof(AstType *) * (size_t)t->param_count);
        for (int i = 0; i < t->param_count; i++)
            params[i] = sema_substitute_type(t->param_types[i], generic_names,
                                             concrete_types, count);
        return ast_type_fn(params, t->param_count,
            sema_substitute_type(t->return_type, generic_names, concrete_types, count));
    }
    return ast_type_clone(t);
}

// Try to infer generic type arguments from actual argument types.
// Returns true if successful, fills inferred_types.
static bool sema_infer_type_args(SemaSymbol *sym, AstType **arg_types,
                                 int arg_count, AstType **inferred_types)
{
    // Initialize all as NULL
    for (int i = 0; i < sym->generic_param_count; i++)
        inferred_types[i] = NULL;

    // For each parameter, try to match its type against the argument type
    int check_count = arg_count < sym->param_count ? arg_count : sym->param_count;
    for (int i = 0; i < check_count; i++) {
        AstType *param_type = sym->params[i].type;
        AstType *actual_type = arg_types[i];
        if (!param_type || !actual_type) continue;

        // Direct generic parameter match: param is T, arg is int → T = int
        if (param_type->kind == TYPE_GENERIC || param_type->kind == TYPE_NAMED) {
            for (int g = 0; g < sym->generic_param_count; g++) {
                if (strcmp(param_type->name, sym->generic_params[g]) == 0) {
                    if (!inferred_types[g]) {
                        inferred_types[g] = actual_type;
                    }
                    break;
                }
            }
        }
        // Array<T> match: param is [T], arg is [int] → T = int
        if (param_type->kind == TYPE_ARRAY && actual_type->kind == TYPE_ARRAY &&
            param_type->element) {
            for (int g = 0; g < sym->generic_param_count; g++) {
                if (param_type->element->kind == TYPE_GENERIC &&
                    strcmp(param_type->element->name, sym->generic_params[g]) == 0) {
                    if (!inferred_types[g] && actual_type->element) {
                        inferred_types[g] = actual_type->element;
                    }
                    break;
                }
            }
        }
    }

    // Check all type params were inferred
    for (int i = 0; i < sym->generic_param_count; i++) {
        if (!inferred_types[i]) return false;
    }
    return true;
}

// ---- Expression type checking ----

static AstType *set_type(AstNode *node, AstType *t)
{
    node->resolved_type = t;
    return t;
}

static AstType *check_expr(SemaCtx *ctx, AstNode *node)
{
    if (!node)
        return NULL;

    // Warn about unnecessary parentheses: ((expr)), (literal), (ident)
    if (node->parenthesized) {
        if (node->kind == NODE_INT_LIT || node->kind == NODE_FLOAT_LIT ||
            node->kind == NODE_BOOL_LIT || node->kind == NODE_STR_LIT ||
            node->kind == NODE_IDENT) {
            sema_warn(ctx, &node->tok, "unnecessary parentheses");
        }
    }

    switch (node->kind) {
    case NODE_INT_LIT:
        return set_type(node, ast_type_simple(TYPE_INT));

    case NODE_FLOAT_LIT:
        return set_type(node, ast_type_simple(TYPE_FLOAT));

    case NODE_STR_LIT:
        return set_type(node, ast_type_simple(TYPE_STR));

    case NODE_BOOL_LIT:
        return set_type(node, ast_type_simple(TYPE_BOOL));

    case NODE_IDENT: {
        SemaSymbol *sym = scope_lookup(ctx->current, node->as.ident.name);
        if (!sym || (sym->tag == FN_SYM_TAG || sym->tag == ENUM_SYM_TAG ||
                     sym->tag == STRUCT_SYM_TAG)) {
            sema_error(ctx, &node->tok, "undefined variable '%s'",
                       node->as.ident.name);
            return set_type(node, ast_type_simple(TYPE_VOID));
        }
        sym->is_referenced = true;
        return set_type(node, ast_type_clone(sym->type));
    }

    case NODE_BINARY: {
        AstType *lt = check_expr(ctx, node->as.binary.left);
        AstType *rt = check_expr(ctx, node->as.binary.right);
        if (!lt || !rt)
            return set_type(node, ast_type_simple(TYPE_VOID));

        TokenType op = node->as.binary.op;

        if (op == TOK_EQ || op == TOK_NEQ || op == TOK_LT || op == TOK_GT ||
            op == TOK_LTE || op == TOK_GTE) {
            if (!ast_types_equal(lt, rt)) {
                sema_error(ctx, &node->as.binary.left->tok,
                           "cannot compare '%s' with '%s'", ast_type_str(lt),
                           ast_type_str(rt));
            }
            return set_type(node, ast_type_simple(TYPE_BOOL));
        }

        if (op == TOK_AND || op == TOK_OR) {
            if (lt->kind != TYPE_BOOL) {
                sema_error(ctx, &node->as.binary.left->tok,
                           "left operand of '%s' must be bool, got '%s'",
                           token_type_name(op), ast_type_str(lt));
            }
            if (rt->kind != TYPE_BOOL) {
                sema_error(ctx, &node->as.binary.right->tok,
                           "right operand of '%s' must be bool, got '%s'",
                           token_type_name(op), ast_type_str(rt));
            }
            return set_type(node, ast_type_simple(TYPE_BOOL));
        }

        if (op == TOK_PLUS && lt->kind == TYPE_STR && rt->kind == TYPE_STR) {
            return set_type(node, ast_type_simple(TYPE_STR));
        }

        if (op == TOK_AMP || op == TOK_PIPE || op == TOK_CARET ||
            op == TOK_SHL || op == TOK_SHR || op == TOK_AMP_TILDE) {
            if (lt->kind != TYPE_INT) {
                sema_error(ctx, &node->tok,
                           "bitwise operator '%s' requires int, got '%s'",
                           token_type_name(op), ast_type_str(lt));
            }
            if (rt->kind != TYPE_INT) {
                sema_error(ctx, &node->tok,
                           "bitwise operator '%s' requires int, got '%s'",
                           token_type_name(op), ast_type_str(rt));
            }
            return set_type(node, ast_type_simple(TYPE_INT));
        }

        if (op == TOK_STARSTAR) {
            if (!ast_types_equal(lt, rt)) {
                sema_error(ctx, &node->tok,
                           "mismatched types in '**': '%s' and '%s'",
                           ast_type_str(lt), ast_type_str(rt));
            }
            if (lt->kind != TYPE_INT && lt->kind != TYPE_FLOAT) {
                sema_error(ctx, &node->tok,
                           "'**' requires numeric types, got '%s'",
                           ast_type_str(lt));
            }
            return set_type(node, ast_type_clone(lt));
        }

        if (op == TOK_PLUS || op == TOK_MINUS || op == TOK_STAR ||
            op == TOK_SLASH || op == TOK_PERCENT || op == TOK_PERCENT_PERCENT) {
            if (!ast_types_equal(lt, rt)) {
                sema_error(
                    ctx, &node->tok, "mismatched types in '%s': '%s' and '%s'",
                    token_type_name(op), ast_type_str(lt), ast_type_str(rt));
                return set_type(node, ast_type_clone(lt));
            }
            if (op != TOK_PLUS && lt->kind != TYPE_INT &&
                lt->kind != TYPE_FLOAT) {
                sema_error(ctx, &node->tok,
                           "operator '%s' requires numeric types, got '%s'",
                           token_type_name(op), ast_type_str(lt));
            }
            return set_type(node, ast_type_clone(lt));
        }

        return set_type(node, ast_type_simple(TYPE_VOID));
    }

    case NODE_UNARY: {
        AstType *t = check_expr(ctx, node->as.unary.operand);
        if (!t)
            return set_type(node, ast_type_simple(TYPE_VOID));
        if (node->as.unary.op == TOK_NOT) {
            if (t->kind != TYPE_BOOL) {
                sema_error(ctx, &node->as.unary.operand->tok,
                           "'!' requires bool, got '%s'", ast_type_str(t));
            }
            return set_type(node, ast_type_simple(TYPE_BOOL));
        }
        if (node->as.unary.op == TOK_MINUS) {
            if (t->kind != TYPE_INT && t->kind != TYPE_FLOAT) {
                sema_error(ctx, &node->as.unary.operand->tok,
                           "unary '-' requires numeric type, got '%s'",
                           ast_type_str(t));
            }
            return set_type(node, ast_type_clone(t));
        }
        if (node->as.unary.op == TOK_TILDE) {
            if (t->kind != TYPE_INT) {
                sema_error(ctx, &node->as.unary.operand->tok,
                           "'~' requires int, got '%s'", ast_type_str(t));
            }
            return set_type(node, ast_type_simple(TYPE_INT));
        }
        return set_type(node, ast_type_clone(t));
    }

    case NODE_CALL: {
        // Method call: obj.method(args) -> StructName_method(obj, args)
        if (node->as.call.callee->kind == NODE_FIELD_ACCESS) {
            AstNode *obj = node->as.call.callee->as.field_access.object;
            const char *method = node->as.call.callee->as.field_access.field;
            AstType *obj_type = check_expr(ctx, obj);

            if (obj_type &&
                (obj_type->kind == TYPE_NAMED || obj_type->kind == TYPE_STR ||
                 obj_type->kind == TYPE_ARRAY)) {
                // Build the function name
                char fn_name_buf[512];
                int fn_len;
                if (obj_type->kind == TYPE_STR) {
                    fn_len = snprintf(fn_name_buf, sizeof(fn_name_buf), "str_%s",
                             method);
                } else if (obj_type->kind == TYPE_ARRAY) {
                    // Array methods map directly: arr.len() -> len(arr)
                    fn_len = snprintf(fn_name_buf, sizeof(fn_name_buf), "%s", method);
                } else {
                    fn_len = snprintf(fn_name_buf, sizeof(fn_name_buf), "%s_%s",
                             obj_type->name, method);
                }
                if (fn_len >= (int)sizeof(fn_name_buf)) {
                    sema_error(ctx, &node->tok, "method name too long");
                    return NULL;
                }
                SemaSymbol *method_sym =
                    scope_lookup(ctx->current, fn_name_buf);

                if (method_sym && method_sym->tag == FN_SYM_TAG) {
                    // Rewrite: change callee to ident, prepend obj as first arg
                    node->as.call.callee->kind = NODE_IDENT;
                    node->as.call.callee->as.ident.name = strdup(fn_name_buf);

                    int new_count = node->as.call.arg_count + 1;
                    AstNode **new_args =
                        xmalloc(sizeof(AstNode *) * (size_t)new_count);
                    new_args[0] = obj;
                    for (int i = 0; i < node->as.call.arg_count; i++)
                        new_args[i + 1] = node->as.call.args[i];
                    xfree(node->as.call.args);
                    node->as.call.args = new_args;
                    node->as.call.arg_count = new_count;
                    // Fall through to normal call checking below
                } else {
                    const char *type_name = obj_type->kind == TYPE_STR ? "str"
                                            : obj_type->kind == TYPE_ARRAY
                                                ? "array"
                                                : obj_type->name;
                    sema_error(ctx, &node->tok, "no method '%s' on type '%s'",
                               method, type_name);
                    for (int i = 0; i < node->as.call.arg_count; i++)
                        check_expr(ctx, node->as.call.args[i]);
                    return set_type(node, ast_type_simple(TYPE_VOID));
                }
            } else {
                sema_error(ctx, &node->tok,
                           "method call on non-struct type '%s'",
                           ast_type_str(obj_type));
                for (int i = 0; i < node->as.call.arg_count; i++)
                    check_expr(ctx, node->as.call.args[i]);
                return set_type(node, ast_type_simple(TYPE_VOID));
            }
        }

        if (node->as.call.callee->kind != NODE_IDENT) {
            // Try calling expression result (e.g. lambda call)
            AstType *callee_type = check_expr(ctx, node->as.call.callee);
            for (int i = 0; i < node->as.call.arg_count; i++)
                check_expr(ctx, node->as.call.args[i]);
            if (callee_type && callee_type->kind == TYPE_FN) {
                return set_type(node, callee_type->return_type
                    ? ast_type_clone(callee_type->return_type)
                    : ast_type_simple(TYPE_VOID));
            }
            sema_error(ctx, &node->as.call.callee->tok,
                       "callee must be a function name");
            return set_type(node, ast_type_simple(TYPE_VOID));
        }

        const char *fn_name = node->as.call.callee->as.ident.name;
        SemaSymbol *sym = scope_lookup(ctx->current, fn_name);
        if (!sym) {
            sema_error(ctx, &node->as.call.callee->tok,
                       "undefined function '%s'", fn_name);
            for (int i = 0; i < node->as.call.arg_count; i++)
                check_expr(ctx, node->as.call.args[i]);
            return set_type(node, ast_type_simple(TYPE_VOID));
        }
        // Allow calling variables that hold function types
        if (sym->tag != FN_SYM_TAG && sym->type && sym->type->kind == TYPE_FN) {
            sym->is_referenced = true;
            // Set callee resolved_type so codegen can emit proper fn ptr cast
            set_type(node->as.call.callee, ast_type_clone(sym->type));
            for (int i = 0; i < node->as.call.arg_count; i++)
                check_expr(ctx, node->as.call.args[i]);
            return set_type(node, sym->type->return_type
                ? ast_type_clone(sym->type->return_type)
                : ast_type_simple(TYPE_VOID));
        }
        if (sym->tag != FN_SYM_TAG) {
            sema_error(ctx, &node->as.call.callee->tok,
                       "'%s' is not a function", fn_name);
            for (int i = 0; i < node->as.call.arg_count; i++)
                check_expr(ctx, node->as.call.args[i]);
            return set_type(node, ast_type_simple(TYPE_VOID));
        }

        sym->is_referenced = true;
        if (sym->is_async) node->is_async_call = true;

        // Generic function handling
        AstType **resolved_param_types = NULL;
        AstType *resolved_return_type = NULL;
        bool is_generic = sym->generic_param_count > 0;

        if (is_generic) {
            AstType **type_args = NULL;
            int type_arg_count = 0;

            if (node->as.call.type_arg_count > 0) {
                // Explicit type arguments: fn<int, str>(...)
                type_args = node->as.call.type_args;
                type_arg_count = node->as.call.type_arg_count;
                if (type_arg_count != sym->generic_param_count) {
                    sema_error(ctx, &node->tok,
                               "'%s' expects %d type arguments, got %d",
                               fn_name, sym->generic_param_count,
                               type_arg_count);
                }
            } else {
                // Infer type arguments from actual arguments
                // First, check all argument expressions to get their types
                AstType **arg_types = xmalloc(
                    sizeof(AstType *) * (size_t)(node->as.call.arg_count + 1));
                for (int i = 0; i < node->as.call.arg_count; i++)
                    arg_types[i] = check_expr(ctx, node->as.call.args[i]);

                type_args = xmalloc(
                    sizeof(AstType *) * (size_t)sym->generic_param_count);
                type_arg_count = sym->generic_param_count;
                if (!sema_infer_type_args(sym, arg_types,
                                          node->as.call.arg_count, type_args)) {
                    sema_error(ctx, &node->tok,
                               "cannot infer type arguments for generic "
                               "function '%s'; use explicit type arguments",
                               fn_name);
                    xfree(arg_types);
                    xfree(type_args);
                    return set_type(node, ast_type_simple(TYPE_VOID));
                }
                xfree(arg_types);
            }

            // Substitute generic params with concrete types
            if (type_arg_count == sym->generic_param_count) {
                resolved_param_types = xmalloc(
                    sizeof(AstType *) * (size_t)sym->param_count);
                for (int i = 0; i < sym->param_count; i++) {
                    resolved_param_types[i] = sema_substitute_type(
                        sym->params[i].type, sym->generic_params,
                        type_args, type_arg_count);
                }
                resolved_return_type = sema_substitute_type(
                    sym->return_type, sym->generic_params,
                    type_args, type_arg_count);
            }

            // Store resolved type args on the call node for codegen
            node->as.call.type_args = type_args;
            node->as.call.type_arg_count = type_arg_count;
        }

        int min_args = 0;
        for (int i = 0; i < sym->param_count; i++) {
            if (sym->params[i].default_value == NULL) {
                min_args++;
            }
        }

        if (node->as.call.arg_count < min_args ||
            node->as.call.arg_count > sym->param_count) {
            sema_error(ctx, &node->as.call.callee->tok,
                       "'%s' expects %d arguments, got %d", fn_name,
                       sym->param_count, node->as.call.arg_count);
        }

        for (int i = 0; i < node->as.call.arg_count; i++) {
            AstType *at = is_generic ? node->as.call.args[i]->resolved_type
                                     : check_expr(ctx, node->as.call.args[i]);
            AstType *expected = (is_generic && resolved_param_types)
                                    ? resolved_param_types[i]
                                    : (sym->param_count > 0 && i < sym->param_count
                                           ? sym->params[i].type
                                           : NULL);
            if (at && expected && expected->kind != TYPE_VOID) {
                if (!ast_types_compatible(at, expected)) {
                    sema_error(
                        ctx, &node->as.call.args[i]->tok,
                        "argument %d of '%s': expected '%s', got '%s'",
                        i + 1, fn_name, ast_type_str(expected),
                        ast_type_str(at));
                }
            }
        }

        if (resolved_param_types) xfree(resolved_param_types);

        // Inject default parameter values to args
        if (node->as.call.arg_count < sym->param_count &&
            node->as.call.arg_count >= min_args) {
            AstNode **new_args =
                xmalloc(sizeof(AstNode *) * (size_t)sym->param_count);

            for (int i = 0; i < node->as.call.arg_count; i++) {
                new_args[i] = node->as.call.args[i];
            }

            for (int i = node->as.call.arg_count; i < sym->param_count; i++) {
                if (sym->params[i].default_value != NULL) {
                    new_args[i] = sym->params[i].default_value;
                    new_args[i]->ref_count++;
                } else {
                    new_args[i] = NULL;
                }
            }

            xfree(node->as.call.args);
            node->as.call.args = new_args;
            node->as.call.arg_count = sym->param_count;
        }

        // Special: unwrap returns the ok_type of the Result argument
        AstType *final_return = resolved_return_type ? resolved_return_type
                                                     : sym->return_type;
        if (fn_name && strcmp(fn_name, "unwrap") == 0 &&
            node->as.call.arg_count > 0) {
            AstType *arg_type = node->as.call.args[0]->resolved_type;
            if (arg_type && arg_type->kind == TYPE_RESULT &&
                arg_type->ok_type) {
                final_return = arg_type->ok_type;
            }
        }
        if (fn_name && strcmp(fn_name, "unwrap_err") == 0 &&
            node->as.call.arg_count > 0) {
            AstType *arg_type = node->as.call.args[0]->resolved_type;
            if (arg_type && arg_type->kind == TYPE_RESULT &&
                arg_type->err_type) {
                final_return = arg_type->err_type;
            }
        }

        node->as.call.callee->resolved_type = ast_type_clone(final_return);
        return set_type(node, ast_type_clone(final_return));
    }

    case NODE_FIELD_ACCESS: {
        AstType *obj_type = check_expr(ctx, node->as.field_access.object);
        if (obj_type && obj_type->kind == TYPE_TUPLE) {
            const char *field = node->as.field_access.field;
            char *end;
            long idx = strtol(field, &end, 10);
            if (*end != '\0' || idx < 0) {
                sema_error(ctx, &node->tok, "invalid tuple index '%s'", field);
                return set_type(node, ast_type_simple(TYPE_VOID));
            }
            if (idx >= obj_type->element_count) {
                sema_error(
                    ctx, &node->tok,
                    "tuple index %ld out of range (tuple has %d elements)", idx,
                    obj_type->element_count);
                return set_type(node, ast_type_simple(TYPE_VOID));
            }
            return set_type(node, ast_type_clone(obj_type->element_types[idx]));
        }
        if (!obj_type || obj_type->kind != TYPE_NAMED) {
            sema_error(ctx, &node->as.field_access.object->tok,
                       "field access on non-struct type '%s'",
                       ast_type_str(obj_type));
            return set_type(node, ast_type_simple(TYPE_VOID));
        }
        SemaSymbol *st = scope_lookup(ctx->current, obj_type->name);
        if (!st || st->tag != STRUCT_SYM_TAG) {
            if (st->tag != STRUCT_SYM_TAG) {
                sema_error(ctx, &node->tok, "'%s' is not struct",
                           obj_type->name);
            } else {
                sema_error(ctx, &node->tok, "unknown struct '%s'",
                           obj_type->name);
            }
            return set_type(node, ast_type_simple(TYPE_VOID));
        }
        for (int i = 0; i < st->field_count; i++) {
            if (strcmp(st->fields[i].name, node->as.field_access.field) == 0) {
                st->is_referenced = true;
                return set_type(node, ast_type_clone(st->fields[i].type));
            }
        }
        sema_error(ctx, &node->tok, "struct '%s' has no field '%s'",
                   obj_type->name, node->as.field_access.field);
        return set_type(node, ast_type_simple(TYPE_VOID));
    }

    case NODE_INDEX: {
        AstType *obj_type = check_expr(ctx, node->as.index_expr.object);
        AstType *idx_type = check_expr(ctx, node->as.index_expr.index);
        if (idx_type && idx_type->kind != TYPE_INT) {
            sema_error(ctx, &node->as.index_expr.index->tok,
                       "array index must be int, got '%s'",
                       ast_type_str(idx_type));
        }
        if (!obj_type || obj_type->kind != TYPE_ARRAY) {
            sema_error(ctx, &node->as.index_expr.object->tok,
                       "index operator on non-array type '%s'",
                       ast_type_str(obj_type));
            return set_type(node, ast_type_simple(TYPE_VOID));
        }
        return set_type(node, ast_type_clone(obj_type->element));
    }

    case NODE_ARRAY_LIT: {
        AstType *elem_type = NULL;
        for (int i = 0; i < node->as.array_lit.count; i++) {
            AstType *t = check_expr(ctx, node->as.array_lit.elements[i]);
            if (i == 0) {
                elem_type = t;
            } else if (!ast_types_equal(elem_type, t)) {
                sema_error(
                    ctx, &node->as.array_lit.elements[i]->tok,
                    "array element type mismatch: expected '%s', got '%s'",
                    ast_type_str(elem_type), ast_type_str(t));
            }
        }
        // element type is default considered as INT type
        if (!elem_type)
            elem_type = ast_type_simple(TYPE_INT);
        return set_type(node, ast_type_array(ast_type_clone(elem_type)));
    }

    case NODE_STRUCT_LIT: {
        const char *name = node->as.struct_lit.name;
        SemaSymbol *st = scope_lookup(ctx->current, name);
        if (!st || st->tag != STRUCT_SYM_TAG) {
            sema_error(ctx, &node->tok, "unknown struct '%s'", name);
            for (int i = 0; i < node->as.struct_lit.field_count; i++)
                check_expr(ctx, node->as.struct_lit.fields[i].value);
            if (node->as.struct_lit.spread)
                check_expr(ctx, node->as.struct_lit.spread);
            return set_type(node, ast_type_simple(TYPE_VOID));
        }
        // Validate spread expression type
        if (node->as.struct_lit.spread) {
            AstType *spread_type = check_expr(ctx, node->as.struct_lit.spread);
            if (spread_type->kind != TYPE_NAMED ||
                strcmp(spread_type->name, name) != 0) {
                sema_error(ctx, &node->tok,
                           "spread expression must be of type '%s', got '%s'",
                           name, ast_type_str(spread_type));
            }
        }
        // Without spread, field count must match exactly
        if (!node->as.struct_lit.spread &&
            node->as.struct_lit.field_count != st->field_count) {
            sema_error(ctx, &node->tok, "struct '%s' has %d fields, got %d",
                       name, st->field_count, node->as.struct_lit.field_count);
        }
        // With spread, explicit fields must not exceed struct field count
        if (node->as.struct_lit.spread &&
            node->as.struct_lit.field_count > st->field_count) {
            sema_error(
                ctx, &node->tok,
                "struct '%s' has %d fields, got %d explicit fields with spread",
                name, st->field_count, node->as.struct_lit.field_count);
        }
        for (int i = 0; i < node->as.struct_lit.field_count; i++) {
            AstType *vt = check_expr(ctx, node->as.struct_lit.fields[i].value);
            bool found = false;
            for (int j = 0; j < st->field_count; j++) {
                if (strcmp(node->as.struct_lit.fields[i].name,
                           st->fields[j].name) == 0) {
                    found = true;
                    if (!ast_types_equal(vt, st->fields[j].type)) {
                        sema_error(ctx, &node->tok,
                                   "field '%s': expected '%s', got '%s'",
                                   st->fields[j].name,
                                   ast_type_str(st->fields[j].type),
                                   ast_type_str(vt));
                    }
                    break;
                }
            }
            if (!found) {
                sema_error(ctx, &node->tok, "struct '%s' has no field '%s'",
                           name, node->as.struct_lit.fields[i].name);
            }
        }
        st->is_referenced = true;
        return set_type(node, ast_type_named(name));
    }

    case NODE_ENUM_INIT: {
        const char *ename = node->as.enum_init.enum_name;
        SemaSymbol *sym = scope_lookup(ctx->current, ename);
        if (!sym || sym->tag != ENUM_SYM_TAG) {
            if (sym->tag != ENUM_SYM_TAG) {
                sema_error(ctx, &node->tok, "'%s' is not enum", ename);
            } else {
                sema_error(ctx, &node->tok, "unknown enum '%s'", ename);
            }
            for (int i = 0; i < node->as.enum_init.arg_count; i++)
                check_expr(ctx, node->as.enum_init.args[i]);
            return set_type(node, ast_type_simple(TYPE_VOID));
        }
        // Find variant
        const char *vname = node->as.enum_init.variant_name;
        EnumVariant *variant = NULL;
        for (int i = 0; i < sym->variant_count; i++) {
            if (strcmp(sym->variants[i].name, vname) == 0) {
                variant = &sym->variants[i];
                sym->is_referenced = true;
                break;
            }
        }
        if (!variant) {
            sema_error(ctx, &node->tok, "enum '%s' has no variant '%s'", ename,
                       vname);
            for (int i = 0; i < node->as.enum_init.arg_count; i++)
                check_expr(ctx, node->as.enum_init.args[i]);
            return set_type(node, ast_type_simple(TYPE_VOID));
        }
        if (node->as.enum_init.arg_count != variant->field_count) {
            sema_error(ctx, &node->tok,
                       "variant '%s.%s' expects %d args, got %d", ename, vname,
                       variant->field_count, node->as.enum_init.arg_count);
        }
        for (int i = 0;
             i < node->as.enum_init.arg_count && i < variant->field_count;
             i++) {
            AstType *at = check_expr(ctx, node->as.enum_init.args[i]);
            if (!ast_types_equal(at, variant->fields[i].type)) {
                sema_error(ctx, &node->tok,
                           "variant '%s.%s' arg %d: expected '%s', got '%s'",
                           ename, vname, i + 1,
                           ast_type_str(variant->fields[i].type),
                           ast_type_str(at));
            }
        }
        return set_type(node, ast_type_named(ename));
    }

    case NODE_OK_EXPR: {
        AstType *val_type = check_expr(ctx, node->as.result_expr.value);
        // We don't know the error type here, use void as placeholder
        return set_type(node,
                        ast_type_result(val_type ? ast_type_clone(val_type)
                                                 : ast_type_simple(TYPE_VOID),
                                        ast_type_simple(TYPE_STR)));
    }

    case NODE_ERR_EXPR: {
        AstType *val_type = check_expr(ctx, node->as.result_expr.value);
        return set_type(node,
                        ast_type_result(ast_type_simple(TYPE_VOID),
                                        val_type ? ast_type_clone(val_type)
                                                 : ast_type_simple(TYPE_STR)));
    }

    case NODE_TUPLE_LIT: {
        int count = node->as.tuple_lit.count;
        AstType **elem_types = xmalloc(sizeof(AstType *) * (size_t)count);
        for (int i = 0; i < count; i++) {
            AstType *t = check_expr(ctx, node->as.tuple_lit.elements[i]);
            elem_types[i] = t ? ast_type_clone(t) : ast_type_simple(TYPE_VOID);
        }
        return set_type(node, ast_type_tuple(elem_types, count));
    }

    case NODE_LAMBDA: {
        // Create a child scope for lambda body
        SemaScope *lambda_scope = scope_new(ctx->current);
        // Register parameters
        for (int i = 0; i < node->as.lambda.param_count; i++) {
            Param *param = &node->as.lambda.params[i];
            SemaSymbol sym = {0};
            sym.name = param->name;
            sym.type = param->type;
            sym.tok = param->tok;
            sym.tag = 'V';
            sym.is_mut = param->is_mut;
            sym.is_referenced = true; // suppress unused warnings for params
            if (lambda_scope->count >= lambda_scope->cap) {
                lambda_scope->cap *= 2;
                lambda_scope->syms = xrealloc(lambda_scope->syms,
                    sizeof(SemaSymbol) * (size_t)lambda_scope->cap);
            }
            lambda_scope->syms[lambda_scope->count++] = sym;
        }
        // Check body with lambda return type
        AstType *saved_ret = ctx->current_fn_return;
        const char *saved_fn = ctx->current_fn_name;
        ctx->current_fn_return = node->as.lambda.return_type;
        ctx->current_fn_name = "<lambda>";
        ctx->current = lambda_scope;
        check_block(ctx, node->as.lambda.body);
        ctx->current = lambda_scope->parent;
        ctx->current_fn_return = saved_ret;
        ctx->current_fn_name = saved_fn;

        // Capture analysis: walk lambda body for idents not in lambda scope
        // (simplified — captures detected during check_block above via normal lookup)

        // Build TYPE_FN for the lambda
        AstType **ptypes = xmalloc(sizeof(AstType *) * (size_t)(node->as.lambda.param_count > 0 ? node->as.lambda.param_count : 1));
        for (int i = 0; i < node->as.lambda.param_count; i++) {
            ptypes[i] = ast_type_clone(node->as.lambda.params[i].type);
        }
        AstType *fn_type = ast_type_fn(ptypes, node->as.lambda.param_count,
                                        ast_type_clone(node->as.lambda.return_type));
        return set_type(node, fn_type);
    }

    case NODE_IF_EXPR: {
        check_expr(ctx, node->as.if_expr.condition);
        AstType *then_t = check_expr(ctx, node->as.if_expr.then_expr);
        check_expr(ctx, node->as.if_expr.else_expr);
        return set_type(node, then_t ? ast_type_clone(then_t)
                                     : ast_type_simple(TYPE_VOID));
    }

    case NODE_AWAIT_EXPR: {
        AstType *t = check_expr(ctx, node->as.await_expr.expr);
        // await unwraps the future — returns the inner type
        // For now, the awaited expression's type IS the result type
        return set_type(node, t ? ast_type_clone(t) : ast_type_simple(TYPE_VOID));
    }

    case NODE_PROPAGATE: {
        AstType *t = check_expr(ctx, node->as.propagate.expr);
        if (t && t->kind == TYPE_RESULT) {
            // ? unwraps Result<T, E> → returns T on Ok, propagates Err
            return set_type(node, t->ok_type ? ast_type_clone(t->ok_type)
                                             : ast_type_simple(TYPE_VOID));
        }
        sema_error(ctx, &node->tok, "'?' operator requires a Result type");
        return set_type(node, ast_type_simple(TYPE_VOID));
    }

    default:
        return set_type(node, ast_type_simple(TYPE_VOID));
    }
}

// ---- Statement checking ----

static void check_stmt(SemaCtx *ctx, AstNode *node)
{
    if (!node)
        return;

    switch (node->kind) {
    case NODE_LET_STMT: {
        AstType *init_type = check_expr(ctx, node->as.let_stmt.init);
        AstType *decl_type = sema_resolve_type(ctx, node->as.let_stmt.type);
        node->as.let_stmt.type = decl_type;

        if (node->as.let_stmt.is_destructure) {
            // Tuple destructuring: let (x, y): (int, str) = expr;
            if (decl_type && decl_type->kind != TYPE_TUPLE) {
                sema_error(ctx, &node->tok,
                           "destructuring requires a tuple type");
            } else if (decl_type && decl_type->element_count !=
                                        node->as.let_stmt.name_count) {
                sema_error(ctx, &node->tok,
                           "destructuring expects %d variables, got %d",
                           decl_type->element_count,
                           node->as.let_stmt.name_count);
            }
            for (int i = 0; i < node->as.let_stmt.name_count; i++) {
                if (scope_lookup_local(ctx->current,
                                       node->as.let_stmt.names[i])) {
                    sema_error(ctx, &node->tok,
                               "variable '%s' already declared in this scope",
                               node->as.let_stmt.names[i]);
                }
                SemaSymbol *sym = scope_add(
                    ctx->current, node->as.let_stmt.names[i], node->tok);
                sym->type = (decl_type && i < decl_type->element_count)
                                ? decl_type->element_types[i]
                                : ast_type_simple(TYPE_VOID);
                sym->is_mut = node->as.let_stmt.is_mut;
            }
            break;
        }

        // Type inference: if no type annotation, use init type
        if (!decl_type && init_type) {
            decl_type = ast_type_clone(init_type);
            node->as.let_stmt.type = decl_type;
        } else if (!decl_type && !init_type) {
            sema_error(ctx, &node->tok, "cannot infer type for variable '%s'",
                       node->as.let_stmt.name);
        }

        if (init_type && decl_type &&
            (!ast_types_equal(init_type, decl_type) &&
             !ast_types_compatible(init_type, decl_type))) {
            // Allow Result type coercion (Ok/Err assign to Result<T,E>)
            if (!(decl_type->kind == TYPE_RESULT &&
                  (node->as.let_stmt.init->kind == NODE_OK_EXPR ||
                   node->as.let_stmt.init->kind == NODE_ERR_EXPR))) {
                sema_error(ctx, &node->as.let_stmt.init->tok,
                           "cannot assign '%s' to variable of type '%s'",
                           ast_type_str(init_type), ast_type_str(decl_type));
            }
        }

        if (scope_lookup_local(ctx->current, node->as.let_stmt.name)) {
            sema_error(ctx, &node->tok,
                       "variable '%s' already declared in this scope",
                       node->as.let_stmt.name);
        }

        SemaSymbol *sym =
            scope_add(ctx->current, node->as.let_stmt.name, node->tok);
        sym->type = decl_type;
        sym->is_mut = node->as.let_stmt.is_mut;
        break;
    }

    case NODE_ASSIGN_STMT: {
        AstType *target_type = check_expr(ctx, node->as.assign_stmt.target);
        AstType *val_type = check_expr(ctx, node->as.assign_stmt.value);

        if (node->as.assign_stmt.target->kind == NODE_IDENT) {
            SemaSymbol *sym = scope_lookup(
                ctx->current, node->as.assign_stmt.target->as.ident.name);
            if (sym && !sym->is_mut && sym->tag != FN_SYM_TAG) {
                sema_error(ctx, &node->as.assign_stmt.value->tok,
                           "cannot assign to immutable variable '%s'",
                           node->as.assign_stmt.target->as.ident.name);
            }
        }

        if (target_type && val_type &&
            (!ast_types_equal(target_type, val_type)) &&
            !ast_types_compatible(val_type, target_type)) {
            sema_error(ctx, &node->as.assign_stmt.value->tok,
                       "cannot assign '%s' to '%s'", ast_type_str(val_type),
                       ast_type_str(target_type));
        }
        break;
    }

    case NODE_IF_STMT: {
        AstType *cond = check_expr(ctx, node->as.if_stmt.condition);
        if (cond && cond->kind != TYPE_BOOL) {
            sema_error(ctx, &node->as.if_stmt.condition->tok,
                       "if condition must be bool, got '%s'",
                       ast_type_str(cond));
        }
        check_block(ctx, node->as.if_stmt.then_block);
        if (node->as.if_stmt.else_branch) {
            if (node->as.if_stmt.else_branch->kind == NODE_IF_STMT) {
                check_stmt(ctx, node->as.if_stmt.else_branch);
            } else {
                check_block(ctx, node->as.if_stmt.else_branch);
            }
        }
        break;
    }

    case NODE_WHILE_STMT: {
        AstType *cond = check_expr(ctx, node->as.while_stmt.condition);
        if (cond && cond->kind != TYPE_BOOL) {
            sema_error(ctx, &node->as.while_stmt.condition->tok,
                       "while condition must be bool, got '%s'",
                       ast_type_str(cond));
        }
        ctx->loop_depth++;
        check_block(ctx, node->as.while_stmt.body);
        ctx->loop_depth--;
        break;
    }

    case NODE_DO_WHILE_STMT: {
        ctx->loop_depth++;
        check_block(ctx, node->as.do_while_stmt.body);
        ctx->loop_depth--;
        AstType *cond = check_expr(ctx, node->as.do_while_stmt.condition);
        if (cond && cond->kind != TYPE_BOOL) {
            sema_error(ctx, &node->as.do_while_stmt.condition->tok,
                       "do..while condition must be bool, got '%s'",
                       ast_type_str(cond));
        }
        break;
    }

    case NODE_FOR_STMT: {
        if (node->as.for_stmt.is_foreach) {
            // For-each over array
            AstType *iter_type = check_expr(ctx, node->as.for_stmt.iterable);
            if (!iter_type || iter_type->kind != TYPE_ARRAY) {
                sema_error(ctx, &node->as.for_stmt.iterable->tok,
                           "for-each requires array type, got '%s'",
                           ast_type_str(iter_type));
            }
            AstType *elem_type = (iter_type && iter_type->kind == TYPE_ARRAY &&
                                  iter_type->element)
                                     ? ast_type_clone(iter_type->element)
                                     : ast_type_simple(TYPE_INT);

            SemaScope *body_scope = scope_new(ctx->current);
            ctx->current = body_scope;

            if (node->as.for_stmt.is_destructure) {
                // for (k, v) in arr { }
                if (elem_type->kind != TYPE_TUPLE) {
                    sema_error(ctx, &node->tok,
                               "for destructuring requires array of tuples");
                } else if (elem_type->element_count !=
                           node->as.for_stmt.var_count) {
                    sema_error(ctx, &node->tok,
                               "for destructuring expects %d variables, got %d",
                               elem_type->element_count,
                               node->as.for_stmt.var_count);
                }
                for (int i = 0; i < node->as.for_stmt.var_count; i++) {
                    SemaSymbol *sym = scope_add(
                        body_scope, node->as.for_stmt.var_names[i], node->tok);
                    sym->type = (elem_type->kind == TYPE_TUPLE &&
                                 i < elem_type->element_count)
                                    ? elem_type->element_types[i]
                                    : ast_type_simple(TYPE_VOID);
                    sym->is_mut = false;
                }
            } else {
                SemaSymbol *loop_var = scope_add(
                    body_scope, node->as.for_stmt.var_name, node->tok);
                loop_var->type = elem_type;
                loop_var->is_mut = false;
            }

            ctx->loop_depth++;
            AstNode *body = node->as.for_stmt.body;
            for (int i = 0; i < body->as.block.stmt_count; i++) {
                check_stmt(ctx, body->as.block.stmts[i]);
            }
            ctx->loop_depth--;

            ctx->current = body_scope->parent;
            scope_free(body_scope);
        } else {
            // Range for
            AstType *start = check_expr(ctx, node->as.for_stmt.start);
            AstType *end = check_expr(ctx, node->as.for_stmt.end);
            if (start && start->kind != TYPE_INT) {
                sema_error(ctx, &node->as.for_stmt.start->tok,
                           "for range start must be int, got '%s'",
                           ast_type_str(start));
            }
            if (end && end->kind != TYPE_INT) {
                sema_error(ctx, &node->as.for_stmt.end->tok,
                           "for range end must be int, got '%s'",
                           ast_type_str(end));
            }

            SemaScope *body_scope = scope_new(ctx->current);
            ctx->current = body_scope;
            SemaSymbol *loop_var =
                scope_add(body_scope, node->as.for_stmt.var_name, node->tok);
            loop_var->type = ast_type_simple(TYPE_INT);
            loop_var->is_mut = false;

            ctx->loop_depth++;
            AstNode *body = node->as.for_stmt.body;
            for (int i = 0; i < body->as.block.stmt_count; i++) {
                check_stmt(ctx, body->as.block.stmts[i]);
            }
            ctx->loop_depth--;

            ctx->current = body_scope->parent;
            scope_free(body_scope);
        }
        break;
    }

    case NODE_RETURN_STMT: {
        if (node->as.return_stmt.value) {
            AstType *t = check_expr(ctx, node->as.return_stmt.value);
            if (ctx->current_fn_return && t &&
                !ast_types_equal(t, ctx->current_fn_return)) {
                // Allow Result coercion
                if (!(ctx->current_fn_return->kind == TYPE_RESULT &&
                      (node->as.return_stmt.value->kind == NODE_OK_EXPR ||
                       node->as.return_stmt.value->kind == NODE_ERR_EXPR))) {
                    sema_error(ctx, &node->as.return_stmt.value->tok,
                               "return type mismatch: expected '%s', got '%s'",
                               ast_type_str(ctx->current_fn_return),
                               ast_type_str(t));
                }
            }
        } else {
            if (ctx->current_fn_return &&
                ctx->current_fn_return->kind != TYPE_VOID) {
                sema_error(ctx, &node->tok,
                           "function expects return value of type '%s'",
                           ast_type_str(ctx->current_fn_return));
            }
        }
        break;
    }

    case NODE_BREAK_STMT:
        if (ctx->loop_depth == 0) {
            sema_error(ctx, &node->tok, "break outside of loop");
        }
        break;

    case NODE_CONTINUE_STMT:
        if (ctx->loop_depth == 0) {
            sema_error(ctx, &node->tok, "continue outside of loop");
        }
        break;

    case NODE_EXPR_STMT:
        check_expr(ctx, node->as.expr_stmt.expr);
        break;

    case NODE_DEFER_STMT:
        check_block(ctx, node->as.defer_stmt.body);
        break;

    case NODE_EMIT_STMT:
        // Raw emit; No check. Trust the author.
        break;

    case NODE_TRY_CATCH: {
        check_block(ctx, node->as.try_catch.try_block);
        // Create scope for catch block with error variable
        SemaScope *catch_scope = scope_new(ctx->current);
        SemaSymbol *err_sym = scope_add(catch_scope, node->as.try_catch.catch_var,
                                         node->tok);
        err_sym->tag = 'V';
        err_sym->type = ast_type_simple(TYPE_STR);
        err_sym->is_mut = false;
        err_sym->is_referenced = true;
        ctx->current = catch_scope;
        check_block(ctx, node->as.try_catch.catch_block);
        ctx->current = catch_scope->parent;
        break;
    }

    case NODE_BLOCK:
        check_block(ctx, node);
        break;

    case NODE_MATCH: {
        AstType *target_type = check_expr(ctx, node->as.match_stmt.target);
        if (!target_type)
            break;

        // Determine if this is a primitive match (int/str/bool) or enum match
        bool is_primitive =
            (target_type->kind == TYPE_INT || target_type->kind == TYPE_STR ||
             target_type->kind == TYPE_BOOL);

        if (is_primitive) {
            // Primitive match: arms are literals or _
            for (int i = 0; i < node->as.match_stmt.arm_count; i++) {
                MatchArm *arm = &node->as.match_stmt.arms[i];
                if (!arm->is_wildcard && arm->pattern_expr) {
                    AstType *pat_type = check_expr(ctx, arm->pattern_expr);
                    if (pat_type && !ast_types_equal(pat_type, target_type)) {
                        sema_error(ctx, &arm->pattern_expr->tok,
                                   "match arm pattern type '%s' does not match "
                                   "target type '%s'",
                                   ast_type_str(pat_type),
                                   ast_type_str(target_type));
                    }
                }
                // Check arm body
                AstNode *arm_body = arm->body;
                for (int s = 0; s < arm_body->as.block.stmt_count; s++) {
                    check_stmt(ctx, arm_body->as.block.stmts[s]);
                }
            }
        } else if (target_type->kind == TYPE_NAMED) {
            // Enum match
            SemaSymbol *enum_sym =
                scope_lookup(ctx->current, target_type->name);
            if (!enum_sym || enum_sym->tag != ENUM_SYM_TAG) {
                sema_error(ctx, &node->as.match_stmt.target->tok,
                           "match target type '%s' is not an enum",
                           target_type->name);
                break;
            }

            for (int i = 0; i < node->as.match_stmt.arm_count; i++) {
                MatchArm *arm = &node->as.match_stmt.arms[i];
                // Find variant
                EnumVariant *variant = NULL;
                for (int v = 0; v < enum_sym->variant_count; v++) {
                    if (strcmp(enum_sym->variants[v].name, arm->variant_name) ==
                        0) {
                        variant = &enum_sym->variants[v];
                        break;
                    }
                }
                if (!variant) {
                    sema_error(ctx, &node->as.match_stmt.target->tok,
                               "enum '%s' has no variant '%s'",
                               target_type->name, arm->variant_name);
                    continue;
                }
                if (arm->binding_count != variant->field_count) {
                    sema_error(ctx, &node->as.match_stmt.target->tok,
                               "variant '%s' has %d fields, got %d bindings",
                               arm->variant_name, variant->field_count,
                               arm->binding_count);
                }

                // Store binding types for codegen
                if (arm->binding_count > 0) {
                    arm->binding_types =
                        xmalloc(sizeof(AstType *) * (size_t)arm->binding_count);
                    for (int b = 0;
                         b < arm->binding_count && b < variant->field_count;
                         b++) {
                        arm->binding_types[b] =
                            ast_type_clone(variant->fields[b].type);
                    }
                }

                // Check arm body with bindings in scope
                SemaScope *arm_scope = scope_new(ctx->current);
                ctx->current = arm_scope;
                for (int b = 0;
                     b < arm->binding_count && b < variant->field_count; b++) {
                    SemaSymbol *binding =
                        scope_add(arm_scope, arm->bindings[b], node->tok);
                    binding->type = ast_type_clone(variant->fields[b].type);
                    binding->is_mut = false;
                }
                AstNode *arm_body = arm->body;
                for (int s = 0; s < arm_body->as.block.stmt_count; s++) {
                    check_stmt(ctx, arm_body->as.block.stmts[s]);
                }
                ctx->current = arm_scope->parent;
                scope_free(arm_scope);
            }
        } else {
            sema_error(ctx, &node->as.match_stmt.target->tok,
                       "match target must be an enum, int, str, or bool type, "
                       "got '%s'",
                       ast_type_str(target_type));
        }
        break;
    }

    default:
        break;
    }
}

static void check_unused_symbols(SemaCtx *ctx, SemaScope *s)
{
    for (int i = 0; i < s->count; i++) {
        SemaSymbol *sym = &s->syms[i];
        if (!sym->is_imported && sym->name[0] != '_' &&
            strcmp(sym->name, "main") != 0 && !sym->is_builtin &&
            !sym->is_referenced) {
            char *type = "variable";
            if (sym->tag == FN_SYM_TAG)
                type = "function";
            else if (sym->tag == STRUCT_SYM_TAG)
                type = "struct";
            else if (sym->tag == ENUM_SYM_TAG)
                type = "enum";

            sema_warn(ctx, &sym->tok, "unused %s '%s'", type, sym->name);
        }
    }
}

static void check_block(SemaCtx *ctx, AstNode *node)
{
    SemaScope *block_scope = scope_new(ctx->current);
    ctx->current = block_scope;
    for (int i = 0; i < node->as.block.stmt_count; i++) {
        check_stmt(ctx, node->as.block.stmts[i]);
    }
    check_unused_symbols(ctx, block_scope);
    ctx->current = block_scope->parent;
    scope_free(block_scope);
}

// ---- Top-level ----

bool sema_analyze(AstNode *program, const char *filename)
{
    SemaCtx ctx = {0};
    SemaScope *global = scope_new(NULL);
    ctx.current_fn_name = "";
    ctx.filename = filename;
    ctx.current = global;

    sema_register_builtins(global);

    // Pass 1: register all structs, enums, and function signatures
    for (int i = 0; i < program->as.program.decl_count; i++) {
        AstNode *d = program->as.program.decls[i];
        if (d->kind == NODE_STRUCT_DECL) {
            if (scope_lookup_local(global, d->as.struct_decl.name)) {
                sema_error(&ctx, &d->tok, "duplicate struct '%s'",
                           d->as.struct_decl.name);
                continue;
            }
            SemaSymbol *s = scope_add(global, d->as.struct_decl.name, d->tok);
            s->tag = STRUCT_SYM_TAG;
            s->is_imported = d->is_imported;
            s->fields = d->as.struct_decl.fields;
            s->field_count = d->as.struct_decl.field_count;
            s->type = ast_type_named(d->as.struct_decl.name);
            s->generic_params = d->as.struct_decl.generic_params;
            s->generic_param_count = d->as.struct_decl.generic_param_count;
        } else if (d->kind == NODE_ENUM_DECL) {
            if (scope_lookup_local(global, d->as.enum_decl.name)) {
                sema_error(&ctx, &d->tok, "duplicate enum '%s'",
                           d->as.enum_decl.name);
                continue;
            }
            SemaSymbol *s = scope_add(global, d->as.enum_decl.name, d->tok);
            s->tag = ENUM_SYM_TAG;
            s->is_imported = d->is_imported;
            s->variants = d->as.enum_decl.variants;
            s->variant_count = d->as.enum_decl.variant_count;
            s->type = ast_type_named(d->as.enum_decl.name);
        } else if (d->kind == NODE_FN_DECL) {
            if (scope_lookup_local(global, d->as.fn_decl.name)) {
                sema_error(&ctx, &d->tok, "duplicate function '%s'",
                           d->as.fn_decl.name);
                continue;
            } else if (d->as.fn_decl.return_type->kind == TYPE_NAMED &&
                       !scope_lookup(ctx.current,
                                     d->as.fn_decl.return_type->name)) {
                // Check if return type is a generic param — that's OK
                bool is_generic_param = false;
                for (int g = 0; g < d->as.fn_decl.generic_param_count; g++) {
                    if (strcmp(d->as.fn_decl.return_type->name,
                              d->as.fn_decl.generic_params[g]) == 0) {
                        is_generic_param = true;
                        break;
                    }
                }
                if (!is_generic_param) {
                    sema_error(&ctx, &d->tok,
                               "function '%s' has unknown return type '%s'",
                               d->as.fn_decl.name,
                               d->as.fn_decl.return_type->name);
                    continue;
                }
            }
            SemaSymbol *s = scope_add(global, d->as.fn_decl.name, d->tok);
            s->tag = FN_SYM_TAG;
            s->is_imported = d->is_imported;
            s->params = d->as.fn_decl.params;
            s->param_count = d->as.fn_decl.param_count;
            s->return_type = d->as.fn_decl.return_type;
            s->generic_params = d->as.fn_decl.generic_params;
            s->generic_param_count = d->as.fn_decl.generic_param_count;
            s->is_async = d->as.fn_decl.is_async;
        } else if (d->kind == NODE_CONST_DECL) {
            if (scope_lookup_local(global, d->as.const_decl.name)) {
                sema_error(&ctx, &d->tok, "duplicate constant '%s'",
                           d->as.const_decl.name);
                continue;
            }
            AstNode *val = d->as.const_decl.value;
            if (val->kind != NODE_INT_LIT && val->kind != NODE_FLOAT_LIT &&
                val->kind != NODE_STR_LIT && val->kind != NODE_BOOL_LIT) {
                sema_error(&ctx, &val->tok, "const value must be a literal");
            }
            AstType *vt = check_expr(&ctx, val);
            if (vt && d->as.const_decl.type &&
                !ast_types_equal(vt, d->as.const_decl.type)) {
                sema_error(&ctx, &val->tok,
                           "const type mismatch: expected '%s', got '%s'",
                           ast_type_str(d->as.const_decl.type),
                           ast_type_str(vt));
            }
            SemaSymbol *s = scope_add(global, d->as.const_decl.name, d->tok);
            s->type = d->as.const_decl.type;
            s->is_mut = false;
            s->is_imported = d->is_imported;
            s->is_referenced = true; // don't warn unused for constants
        } else if (d->kind == NODE_TYPE_ALIAS) {
            if (scope_lookup_local(global, d->as.type_alias.name)) {
                sema_error(&ctx, &d->tok, "duplicate type alias '%s'",
                           d->as.type_alias.name);
                continue;
            }
            SemaSymbol *s = scope_add(global, d->as.type_alias.name, d->tok);
            s->tag = TYPE_SYM_TAG;
            s->is_imported = d->is_imported;
            s->alias_type = d->as.type_alias.type;
            s->type = d->as.type_alias.type;
            s->is_referenced = true;
        } else if (d->kind == NODE_TRAIT_DECL) {
            // Register trait (for now, just mark as known)
            if (scope_lookup_local(global, d->as.trait_decl.name)) {
                sema_error(&ctx, &d->tok, "duplicate trait '%s'",
                           d->as.trait_decl.name);
                continue;
            }
            SemaSymbol *s = scope_add(global, d->as.trait_decl.name, d->tok);
            s->tag = TYPE_SYM_TAG;
            s->is_referenced = true;
            s->alias_type = ast_type_named(d->as.trait_decl.name);
        } else if (d->kind == NODE_IMPL_BLOCK) {
            // Register impl methods as TypeName_methodName functions
            const char *type_name = d->as.impl_block.type_name;
            for (int j = 0; j < d->as.impl_block.method_count; j++) {
                AstNode *m = d->as.impl_block.methods[j];
                // Mangle name: TypeName_methodName
                char mangled[512];
                snprintf(mangled, sizeof(mangled), "%s_%s",
                         type_name, m->as.fn_decl.name);
                char *mname = strdup(mangled);

                if (scope_lookup_local(global, mname)) {
                    sema_error(&ctx, &m->tok, "duplicate method '%s' on '%s'",
                               m->as.fn_decl.name, type_name);
                    continue;
                }

                // Fill 'self' parameter type
                for (int k = 0; k < m->as.fn_decl.param_count; k++) {
                    if (strcmp(m->as.fn_decl.params[k].name, "self") == 0 &&
                        !m->as.fn_decl.params[k].type) {
                        m->as.fn_decl.params[k].type = ast_type_named(type_name);
                    }
                }

                // Register as a normal function with mangled name
                SemaSymbol *s = scope_add(global, mname, m->tok);
                s->tag = FN_SYM_TAG;
                s->is_imported = d->is_imported;
                s->params = m->as.fn_decl.params;
                s->param_count = m->as.fn_decl.param_count;
                s->return_type = m->as.fn_decl.return_type;
                s->generic_params = m->as.fn_decl.generic_params;
                s->generic_param_count = m->as.fn_decl.generic_param_count;
                s->is_referenced = true; // don't warn unused

                // Store mangled name back for codegen
                free(m->as.fn_decl.name);
                m->as.fn_decl.name = mname;
            }
        }
    }

    // Pass 1b: resolve type aliases in all type annotations
    // When a TYPE_NAMED references a type alias, replace it with the aliased
    // type
    for (int i = 0; i < global->count; i++) {
        SemaSymbol *s = &global->syms[i];
        if (s->tag == TYPE_SYM_TAG && s->alias_type &&
            s->alias_type->kind == TYPE_NAMED) {
            SemaSymbol *target = scope_lookup(global, s->alias_type->name);
            if (target && target->tag == TYPE_SYM_TAG) {
                s->alias_type = target->alias_type;
                s->type = target->alias_type;
            }
        }
    }

    // Pass 2: check function bodies
    for (int i = 0; i < program->as.program.decl_count; i++) {
        AstNode *d = program->as.program.decls[i];
        if (d->kind == NODE_FN_DECL) {
            SemaScope *fn_scope = scope_new(global);
            ctx.current = fn_scope;
            ctx.current_fn_name = d->as.fn_decl.name;

            // Register generic type parameters in function scope
            for (int g = 0; g < d->as.fn_decl.generic_param_count; g++) {
                SemaSymbol *gs = scope_add(fn_scope,
                    d->as.fn_decl.generic_params[g], d->tok);
                gs->tag = TYPE_SYM_TAG;
                gs->alias_type = ast_type_generic(d->as.fn_decl.generic_params[g]);
                gs->is_referenced = true;
            }

            // Resolve type aliases in return type and params
            d->as.fn_decl.return_type =
                sema_resolve_type(&ctx, d->as.fn_decl.return_type);
            for (int j = 0; j < d->as.fn_decl.param_count; j++)
                d->as.fn_decl.params[j].type =
                    sema_resolve_type(&ctx, d->as.fn_decl.params[j].type);

            ctx.current_fn_return = d->as.fn_decl.return_type;

            for (int j = 0; j < d->as.fn_decl.param_count; j++) {
                Param *p_decl = &d->as.fn_decl.params[j];
                SemaSymbol *p = scope_add(
                    fn_scope, d->as.fn_decl.params[j].name, p_decl->tok);
                p->is_imported = d->is_imported;
                p->type = d->as.fn_decl.params[j].type;
                p->is_mut = p_decl->is_mut;

                if (p_decl->default_value != NULL) {
                    AstType *def_type = check_expr(&ctx, p_decl->default_value);
                    if (!ast_types_equal(def_type, p_decl->type)) {
                        sema_error(&ctx, &p_decl->default_value->tok,
                                   "default value type mismatch for parameter "
                                   "'%s': expected '%s' but got '%s'",
                                   p_decl->name, ast_type_str(p_decl->type),
                                   ast_type_str(def_type));
                    }
                }
            }

            AstNode *body = d->as.fn_decl.body;
            for (int j = 0; j < body->as.block.stmt_count; j++) {
                check_stmt(&ctx, body->as.block.stmts[j]);
            }

            check_unused_symbols(&ctx, fn_scope);
            ctx.current = global;
            scope_free(fn_scope);
            ctx.current_fn_name = "";
        }
        // Check impl block method bodies
        if (d->kind == NODE_IMPL_BLOCK) {
            for (int j = 0; j < d->as.impl_block.method_count; j++) {
                AstNode *m = d->as.impl_block.methods[j];
                SemaScope *fn_scope = scope_new(global);
                ctx.current = fn_scope;
                ctx.current_fn_name = m->as.fn_decl.name;

                for (int g = 0; g < m->as.fn_decl.generic_param_count; g++) {
                    SemaSymbol *gs = scope_add(fn_scope,
                        m->as.fn_decl.generic_params[g], m->tok);
                    gs->tag = TYPE_SYM_TAG;
                    gs->alias_type = ast_type_generic(
                        m->as.fn_decl.generic_params[g]);
                    gs->is_referenced = true;
                }

                m->as.fn_decl.return_type =
                    sema_resolve_type(&ctx, m->as.fn_decl.return_type);
                for (int k = 0; k < m->as.fn_decl.param_count; k++)
                    m->as.fn_decl.params[k].type =
                        sema_resolve_type(&ctx, m->as.fn_decl.params[k].type);

                ctx.current_fn_return = m->as.fn_decl.return_type;

                for (int k = 0; k < m->as.fn_decl.param_count; k++) {
                    Param *p = &m->as.fn_decl.params[k];
                    SemaSymbol *ps = scope_add(fn_scope, p->name, p->tok);
                    ps->is_imported = d->is_imported;
                    ps->type = p->type;
                    ps->is_mut = p->is_mut;
                }

                if (m->as.fn_decl.body) {
                    AstNode *body = m->as.fn_decl.body;
                    for (int k = 0; k < body->as.block.stmt_count; k++) {
                        check_stmt(&ctx, body->as.block.stmts[k]);
                    }
                }

                ctx.current = global;
                scope_free(fn_scope);
                ctx.current_fn_name = "";
            }
        }
    }

    check_unused_symbols(&ctx, global);
    scope_free(global);
    return ctx.errors == 0;
}
