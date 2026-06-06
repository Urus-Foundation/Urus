#include "ast.h"
#include <stdio.h>

Expr *ast_new_expr(Arena *a, ExprKind k, SrcLoc loc) {
    Expr *e = (Expr *)arena_alloc_zero(a, sizeof(Expr));
    e->kind = k;
    e->loc  = loc;
    return e;
}

Stmt *ast_new_stmt(Arena *a, StmtKind k, SrcLoc loc) {
    Stmt *s = (Stmt *)arena_alloc_zero(a, sizeof(Stmt));
    s->kind = k;
    s->loc  = loc;
    return s;
}

Item *ast_new_item(Arena *a, ItemKind k) {
    Item *i = (Item *)arena_alloc_zero(a, sizeof(Item));
    i->kind = k;
    return i;
}

TypeExpr *ast_new_type(Arena *a, TypeExprKind k, SrcLoc loc) {
    TypeExpr *t = (TypeExpr *)arena_alloc_zero(a, sizeof(TypeExpr));
    t->kind = k;
    t->loc  = loc;
    return t;
}

Pattern *ast_new_pat(Arena *a, PatternKind k, SrcLoc loc) {
    Pattern *p = (Pattern *)arena_alloc_zero(a, sizeof(Pattern));
    p->kind = k;
    p->loc  = loc;
    return p;
}

static void indent(int n) { for (int i = 0; i < n; i++) fputs("  ", stdout); }

static void print_type(const TypeExpr *t) {
    if (!t) { fputs("()", stdout); return; }
    switch (t->kind) {
        case TY_NAMED:   printf("%s", t->named.name); break;
        case TY_UNIT:    fputs("()", stdout); break;
        case TY_INFER:   fputs("_", stdout); break;
        case TY_SELF:    fputs("Self", stdout); break;
        case TY_POINTER: fputs(t->pointer.is_mut ? "*mut " : "*", stdout); print_type(t->pointer.inner); break;
        case TY_REF:     fputs(t->ref.is_mut ? "&mut " : "&", stdout); print_type(t->ref.inner); break;
        case TY_SLICE:   fputs("[", stdout); print_type(t->slice.elem); fputs("]", stdout); break;
        case TY_ARRAY:   fputs("[", stdout); print_type(t->array.elem); fputs("; _]", stdout); break;
        case TY_GENERIC:
            printf("%s<", t->generic.name);
            for (int i = 0; i < t->generic.n_args; i++) {
                if (i) fputs(", ", stdout);
                print_type(t->generic.args[i]);
            }
            fputs(">", stdout);
            break;
        case TY_TUPLE:
            fputs("(", stdout);
            for (int i = 0; i < t->tuple.n; i++) {
                if (i) fputs(", ", stdout);
                print_type(t->tuple.elems[i]);
            }
            fputs(")", stdout);
            break;
        case TY_FN:
            fputs("fn(", stdout);
            for (int i = 0; i < t->fn.n_params; i++) {
                if (i) fputs(", ", stdout);
                print_type(t->fn.params[i]);
            }
            fputs(") -> ", stdout);
            print_type(t->fn.ret);
            break;
    }
}

static void print_item(const Item *it, int d) {
    indent(d);
    switch (it->kind) {
        case IT_FN: {
            FnDecl *f = it->fn;
            printf("fn %s(", f->name);
            for (int i = 0; i < f->n_params; i++) {
                if (i) fputs(", ", stdout);
                if (f->params[i].is_self) {
                    fputs(f->params[i].is_ref_self ? (f->params[i].is_mut_self ? "&mut self" : "&self") : "self", stdout);
                } else {
                    fputs("param: ", stdout); print_type(f->params[i].type);
                }
            }
            fputs(") -> ", stdout);
            if (f->ret_type) print_type(f->ret_type); else fputs("()", stdout);
            puts("");
            break;
        }
        case IT_STRUCT:
            printf("struct %s { %d fields }\n", it->strct->name, it->strct->n_fields);
            break;
        case IT_ENUM:
            printf("enum %s { %d variants }\n", it->enm->name, it->enm->n_variants);
            break;
        case IT_IMPL:
            printf("impl %s { %d methods }\n", it->impl->type_name, it->impl->n_methods);
            for (int i = 0; i < it->impl->n_methods; i++) {
                Item dummy = { .kind = IT_FN };
                dummy.fn = it->impl->methods[i];
                print_item(&dummy, d + 1);
            }
            break;
        case IT_USE:
            fputs("use ", stdout);
            for (int i = 0; i < it->use->n_segs; i++) {
                if (i) fputs("::", stdout);
                fputs(it->use->segs[i], stdout);
            }
            puts("");
            break;
        case IT_CONST:
            printf("const %s\n", it->cnst->name);
            break;
        case IT_TYPE_ALIAS:
            printf("type %s\n", it->alias->name);
            break;
    }
}

void ast_print_module(const Module *m) {
    printf("module %s (%d items)\n", m->name ? m->name : "<anon>", m->n_items);
    for (int i = 0; i < m->n_items; i++) print_item(m->items[i], 1);
}
