#include "sema.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- Symbol table (FNV-1a hashed, chained scopes) ----------
 *
 * Each scope owns a small open-addressing hash table for O(1) name lookup.
 * Walking through parent scopes is still linear in scope depth, which is
 * fine because depth is bounded by lexical nesting (rarely > 8).
 *
 * The hash is FNV-1a 32-bit — fast, no allocations, good distribution for
 * short identifiers. When the load factor exceeds 0.75 the table doubles.
 */

typedef enum {
    SYM_VAR,
    SYM_FN,
    SYM_STRUCT,
    SYM_ENUM,
    SYM_CONST,
    SYM_TYPE,
    SYM_MODULE,    /* for `use foo.bar` we register `foo` */
    SYM_BUILTIN,
} SymKind;

typedef struct Symbol {
    const char *name;       /* NULL slot = empty */
    SymKind     kind;
    bool        is_mut;     /* for SYM_VAR: was bound with `let mut` / `mut self` /
                               `&mut T` parameter / `*mut T`. Other kinds: ignored. */
    bool        written;    /* for SYM_VAR: an assignment targeted this binding.
                               Drives the unused-`mut` warning (b021). */
    SrcLoc      loc;        /* binding site, for end-of-scope diagnostics */
    /* payload */
    union {
        FnDecl     *fn;
        StructDecl *strct;
        EnumDecl   *enm;
        ConstDecl  *cnst;
        TypeAlias  *alias;
    };
} Symbol;

typedef struct Scope {
    struct Scope *parent;
    Symbol       *slots;     /* power-of-two sized */
    uint32_t      mask;      /* cap-1 */
    uint32_t      count;
    Arena        *arena;
} Scope;

static URUS_INLINE uint32_t fnv1a(const char *s) {
    uint32_t h = 2166136261u;
    while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; }
    return h;
}

static void scope_init(Scope *s, Arena *a, Scope *parent) {
    s->parent = parent;
    s->arena  = a;
    s->mask   = 7;            /* initial cap 8 */
    s->count  = 0;
    s->slots  = (Symbol *)arena_alloc_zero(a, sizeof(Symbol) * 8);
}

static void scope_grow(Scope *s) {
    uint32_t old_cap = s->mask + 1;
    Symbol  *old     = s->slots;
    uint32_t new_cap = old_cap * 2;
    s->slots = (Symbol *)arena_alloc_zero(s->arena, sizeof(Symbol) * new_cap);
    s->mask  = new_cap - 1;
    s->count = 0;
    for (uint32_t i = 0; i < old_cap; i++) {
        if (!old[i].name) continue;
        /* reinsert */
        uint32_t h = fnv1a(old[i].name);
        uint32_t idx = h & s->mask;
        while (s->slots[idx].name) idx = (idx + 1) & s->mask;
        s->slots[idx] = old[i];
        s->count++;
    }
}

static Symbol *scope_lookup_local(Scope *s, const char *name) {
    if (!s->slots) return NULL;
    uint32_t h = fnv1a(name);
    uint32_t idx = h & s->mask;
    for (;;) {
        Symbol *slot = &s->slots[idx];
        if (!slot->name) return NULL;
        if (strcmp(slot->name, name) == 0) return slot;
        idx = (idx + 1) & s->mask;
    }
}

static Symbol *scope_lookup(Scope *s, const char *name) {
    for (Scope *cur = s; cur; cur = cur->parent) {
        Symbol *r = scope_lookup_local(cur, name);
        if (r) return r;
    }
    return NULL;
}

static Symbol *scope_define(Scope *s, Arena *a, const char *name, SymKind kind) {
    (void)a;  /* arena already captured at init */
    /* grow if load factor would exceed 0.75 */
    if ((s->count + 1) * 4 > (s->mask + 1) * 3) scope_grow(s);
    uint32_t h = fnv1a(name);
    uint32_t idx = h & s->mask;
    for (;;) {
        Symbol *slot = &s->slots[idx];
        if (!slot->name) {
            slot->name = name;
            slot->kind = kind;
            s->count++;
            return slot;
        }
        if (strcmp(slot->name, name) == 0) return slot;   /* already defined */
        idx = (idx + 1) & s->mask;
    }
}

/* ---------- Sema context ---------- */

typedef struct {
    DiagCtx *diag;
    Arena   *arena;
    Module  *mod;
    Scope    globals;
    /* For methods: resolve `Self` and impl-block lookup. */
    StructDecl *cur_self_struct;
    FnDecl     *cur_fn;
} Sema;

static StructDecl *find_struct(Sema *s, const char *name) {
    Symbol *sym = scope_lookup(&s->globals, name);
    if (sym && sym->kind == SYM_STRUCT) return sym->strct;
    return NULL;
}

static EnumDecl *find_enum(Sema *s, const char *name) {
    Symbol *sym = scope_lookup(&s->globals, name);
    if (sym && sym->kind == SYM_ENUM) return sym->enm;
    return NULL;
}

/* ---------- Pass 1: collect global symbols ---------- */

static void collect_globals(Sema *s) {
    Module *m = s->mod;
    for (int i = 0; i < m->n_items; i++) {
        Item *it = m->items[i];
        switch (it->kind) {
            case IT_FN: {
                if (scope_lookup_local(&s->globals, it->fn->name)) {
                    diag_error(s->diag, it->fn->loc,
                               "duplicate definition of '%s'", it->fn->name);
                } else {
                    Symbol *sym = scope_define(&s->globals, s->arena, it->fn->name, SYM_FN);
                    sym->fn = it->fn;
                }
                break;
            }
            case IT_STRUCT: {
                if (scope_lookup_local(&s->globals, it->strct->name)) {
                    diag_error(s->diag, it->strct->loc,
                               "duplicate type '%s'", it->strct->name);
                } else {
                    Symbol *sym = scope_define(&s->globals, s->arena, it->strct->name, SYM_STRUCT);
                    sym->strct = it->strct;
                }
                break;
            }
            case IT_ENUM: {
                if (scope_lookup_local(&s->globals, it->enm->name)) {
                    diag_error(s->diag, it->enm->loc,
                               "duplicate type '%s'", it->enm->name);
                } else {
                    Symbol *sym = scope_define(&s->globals, s->arena, it->enm->name, SYM_ENUM);
                    sym->enm = it->enm;
                }
                /* register variant constructors as nullary functions on first lookup —
                   handled at call-site instead, to keep this simple. */
                break;
            }
            case IT_CONST: {
                if (scope_lookup_local(&s->globals, it->cnst->name)) {
                    diag_error(s->diag, it->cnst->loc,
                               "duplicate const '%s'", it->cnst->name);
                } else {
                    Symbol *sym = scope_define(&s->globals, s->arena, it->cnst->name, SYM_CONST);
                    sym->cnst = it->cnst;
                }
                break;
            }
            case IT_TYPE_ALIAS: {
                Symbol *sym = scope_define(&s->globals, s->arena, it->alias->name, SYM_TYPE);
                sym->alias = it->alias;
                break;
            }
            case IT_USE: {
                /* register first path segment as module name */
                if (it->use->n_segs > 0) {
                    if (!scope_lookup_local(&s->globals, it->use->segs[0])) {
                        scope_define(&s->globals, s->arena, it->use->segs[0], SYM_MODULE);
                    }
                    /* the last segment is brought into scope as well — e.g.
                       `use urus.io.println` lets you call `println(...)`. */
                    const char *last = it->use->segs[it->use->n_segs - 1];
                    if (!scope_lookup_local(&s->globals, last)) {
                        scope_define(&s->globals, s->arena, last, SYM_BUILTIN);
                    }
                }
                break;
            }
            case IT_IMPL:
                /* methods are attached at pass-2 method check time */
                break;
        }
    }

    /* Built-ins — provide a small prelude so simple programs compile. */
    static const char *builtins[] = {
        "println", "print", "eprintln", "panic",
        "u8", "u16", "u32", "u64", "i8", "i16", "i32", "i64",
        "f32", "f64", "bool", "str", "char", "usize", "isize",
        "Result", "Option", "Ok", "Err", "Some", "None",
        /* urus.str — runtime string helpers (v0.0.1-b017/b019).  Registered
           as plain function names so both `use urus.str.eq` style imports
           and the raw `urus_str_*` C names resolve. */
        "urus_str_len", "urus_str_is_empty", "urus_str_eq", "urus_str_cmp",
        "urus_str_starts_with", "urus_str_ends_with", "urus_str_contains",
        /* urus.io — line input (v0.0.1-b019) */
        "read_line", "urus_read_line",
    };
    for (size_t i = 0; i < sizeof(builtins) / sizeof(*builtins); i++) {
        if (!scope_lookup_local(&s->globals, builtins[i])) {
            scope_define(&s->globals, s->arena, builtins[i], SYM_BUILTIN);
        }
    }
}

/* ---------- Pass 2: check bodies ---------- */

static void check_expr(Sema *s, Scope *scope, Expr *e);

/* ---------- Mutability classification of LHS expressions (F-COMP-2)
 *
 * Returns true when `e` denotes a place the language permits to be
 * mutated. Conservative: anything we cannot statically prove mutable is
 * reported as immutable for an assignment, and silently accepted for a
 * `&mut` borrow (which only fires the error when the base is *known*
 * immutable). Field and index inherit mutability from their base —
 * `a.b.c = 1` is legal iff `a` is mutable. Dereferences pass through
 * because mutability of `*p` is governed by the pointer's `*mut` tag,
 * which v0.0.1 cannot recover from `Expr` alone — the C backend catches
 * any remaining writes through a const pointer.
 */
typedef enum {
    PLACE_MUT,        /* legal to assign */
    PLACE_IMMUT,      /* a place, but immutable */
    PLACE_NOT_PLACE,  /* not an l-value at all (literal, call result, …) */
    PLACE_UNKNOWN,    /* sema cannot decide; do not diagnose */
} PlaceMut;

static PlaceMut classify_lhs(Sema *s, Scope *scope, Expr *e) {
    if (!e) return PLACE_NOT_PLACE;
    switch (e->kind) {
        case EX_IDENT: {
            Symbol *sym = scope_lookup(scope, e->ident);
            if (!sym) return PLACE_UNKNOWN; /* undefined — already diagnosed elsewhere */
            if (sym->kind == SYM_VAR) {
                if (sym->is_mut) {
                    sym->written = true;   /* feeds the unused-`mut` warning */
                    return PLACE_MUT;
                }
                return PLACE_IMMUT;
            }
            if (sym->kind == SYM_CONST) return PLACE_IMMUT;
            return PLACE_NOT_PLACE;
        }
        case EX_FIELD: return classify_lhs(s, scope, e->field.obj);
        case EX_INDEX: return classify_lhs(s, scope, e->index.obj);
        case EX_DEREF: return PLACE_UNKNOWN;
        case EX_PATH:  return PLACE_UNKNOWN;
        default:       return PLACE_NOT_PLACE;
    }
}

static const char *describe_lhs(Expr *e) {
    if (!e) return "expression";
    switch (e->kind) {
        case EX_IDENT: return e->ident;
        case EX_FIELD: return "field";
        case EX_INDEX: return "indexed element";
        case EX_DEREF: return "dereferenced value";
        default:       return "expression";
    }
}

static void check_lhs_mutable(Sema *s, Scope *scope, Expr *lhs, const char *verb) {
    PlaceMut m = classify_lhs(s, scope, lhs);
    if (m == PLACE_IMMUT) {
        diag_error(s->diag, lhs->loc,
                   "cannot %s immutable binding '%s' — declare it with `let mut` or `mut`",
                   verb, describe_lhs(lhs));
    } else if (m == PLACE_NOT_PLACE) {
        diag_error(s->diag, lhs->loc,
                   "left-hand side of %s is not assignable", verb);
    }
}

/*
 * `outer_mut` carries the `mut` keyword from the binding site (let mut, mut
 * self, fn param `mut x`). It cascades into tuple / variant sub-patterns so
 * `let mut (a, b) = ...` makes both `a` and `b` mutable. Pattern-level
 * `pat->ident.is_mut` (rare, e.g. `let (mut a, b) = ...`) ORs in on top.
 */
static void declare_pattern(Sema *s, Scope *scope, Pattern *pat, bool outer_mut) {
    if (!pat) return;
    switch (pat->kind) {
        case PAT_WILDCARD: return;
        case PAT_IDENT: {
            Symbol *sym = scope_define(scope, s->arena, pat->ident.name, SYM_VAR);
            sym->is_mut  = outer_mut || pat->ident.is_mut;
            sym->written = false;
            sym->loc     = pat->loc;
            return;
        }
        case PAT_LITERAL: return;
        case PAT_TUPLE:
            for (int i = 0; i < pat->tuple.n; i++) declare_pattern(s, scope, pat->tuple.elems[i], outer_mut);
            return;
        case PAT_ENUM_VARIANT:
            for (int i = 0; i < pat->variant.n; i++) declare_pattern(s, scope, pat->variant.subs[i], outer_mut);
            return;
        case PAT_STRUCT: return;
    }
}

/* End-of-scope sweep (b021): warn on `let mut x` where x was never the
 * target of an assignment or a `&mut` borrow.  Mutability that is never
 * exercised is usually a leftover from a refactor — and every needless
 * `mut` widens the audit surface for the F-COMP-2 checker. */
static void warn_unused_mut(Sema *s, Scope *scope) {
    if (!scope->slots) return;
    for (uint32_t i = 0; i <= scope->mask; i++) {
        Symbol *sym = &scope->slots[i];
        if (!sym->name || sym->kind != SYM_VAR) continue;
        if (sym->is_mut && !sym->written) {
            diag_warn(s->diag, sym->loc,
                      "binding '%s' is declared `mut` but never mutated — "
                      "consider dropping `mut`", sym->name);
        }
    }
}

static void check_block(Sema *s, Scope *parent, Expr *blk) {
    Scope inner; scope_init(&inner, s->arena, parent);
    for (int i = 0; i < blk->block.n_stmts; i++) {
        Stmt *st = blk->block.stmts[i];
        switch (st->kind) {
            case ST_LET:
                if (st->let_.init) check_expr(s, &inner, st->let_.init);
                declare_pattern(s, &inner, st->let_.pat, st->let_.is_mut);
                break;
            case ST_EXPR:
                if (st->expr_.expr) check_expr(s, &inner, st->expr_.expr);
                break;
            case ST_DEFER:
                if (st->defer_.expr) check_expr(s, &inner, st->defer_.expr);
                break;
            case ST_ITEM:
                /* nested items not supported in v0.0.1 */
                break;
        }
    }
    if (blk->block.tail) check_expr(s, &inner, blk->block.tail);
    warn_unused_mut(s, &inner);
}

static void check_expr(Sema *s, Scope *scope, Expr *e) {
    if (!e) return;
    switch (e->kind) {
        case EX_INT_LIT: case EX_FLOAT_LIT: case EX_STR_LIT: case EX_FSTR_LIT:
        case EX_CHAR_LIT: case EX_BOOL_LIT: case EX_UNIT:
        case EX_CONTINUE: case EX_BREAK:
            return;

        case EX_IDENT: {
            Symbol *sym = scope_lookup(scope, e->ident);
            if (!sym) {
                diag_error(s->diag, e->loc, "undefined name '%s'", e->ident);
            }
            return;
        }

        case EX_PATH: {
            /* Resolve only first segment for v0.0.1. Allow EnumName::Variant. */
            if (e->path.n >= 1) {
                Symbol *sym = scope_lookup(scope, e->path.segs[0]);
                if (!sym) {
                    diag_error(s->diag, e->loc, "unresolved path '%s'", e->path.segs[0]);
                }
            }
            return;
        }

        case EX_UNARY:   check_expr(s, scope, e->unary.operand); return;
        case EX_BINARY:
            check_expr(s, scope, e->binary.lhs);
            check_expr(s, scope, e->binary.rhs);
            return;
        case EX_ASSIGN:
            check_expr(s, scope, e->assign.lhs);
            check_expr(s, scope, e->assign.rhs);
            check_lhs_mutable(s, scope, e->assign.lhs,
                              e->assign.op == ASSIGN_EQ ? "assign to" : "modify");
            return;

        case EX_CALL:
            check_expr(s, scope, e->call.callee);
            for (int i = 0; i < e->call.n_args; i++) check_expr(s, scope, e->call.args[i]);
            return;

        case EX_METHOD_CALL:
            check_expr(s, scope, e->method.recv);
            for (int i = 0; i < e->method.n_args; i++) check_expr(s, scope, e->method.args[i]);
            return;

        case EX_FIELD:   check_expr(s, scope, e->field.obj);   return;
        case EX_INDEX:
            check_expr(s, scope, e->index.obj);
            check_expr(s, scope, e->index.idx);
            return;

        case EX_IF:
            check_expr(s, scope, e->if_.cond);
            check_expr(s, scope, e->if_.then_blk);
            if (e->if_.else_blk) check_expr(s, scope, e->if_.else_blk);
            return;

        case EX_MATCH: {
            check_expr(s, scope, e->match_.scrut);

            /* F-COMP-3: exhaustiveness check.
             *
             * A match is exhaustive when (a) any arm is a wildcard / unguarded
             * identifier pattern (catches everything), or (b) the union of
             * variant names across arms covers a known enum.
             *
             * Heuristic for builtins: arms naming {Ok,Err} → Result; {Some,None}
             * → Option. For user enums: if every variant arm name belongs to
             * the same declared enum, we know its full variant set.
             *
             * A *guarded* arm cannot satisfy the obligation (its guard may be
             * false), so it does not count toward coverage.
             */
            bool has_catchall = false;
            const char *seen[64];
            int n_seen = 0;
            for (int i = 0; i < e->match_.n_arms; i++) {
                MatchArm *arm = &e->match_.arms[i];
                Scope inner; scope_init(&inner, s->arena, scope);
                /* match-bound names are by-value copies — immutable. */
                declare_pattern(s, &inner, arm->pat, false);
                if (arm->guard) check_expr(s, &inner, arm->guard);
                check_expr(s, &inner, arm->body);

                if (arm->guard) continue;          /* guarded — doesn't cover */
                if (!arm->pat) { has_catchall = true; continue; }
                switch (arm->pat->kind) {
                    case PAT_WILDCARD: has_catchall = true; break;
                    case PAT_IDENT:    has_catchall = true; break;   /* bare ident binds all */
                    case PAT_ENUM_VARIANT:
                        if (n_seen < 64) seen[n_seen++] = arm->pat->variant.name;
                        break;
                    default: break;
                }
            }
            if (has_catchall) return;

            /* Detect Result / Option by name set. */
            bool saw_ok = false, saw_err = false, saw_some = false, saw_none = false;
            for (int i = 0; i < n_seen; i++) {
                if (strcmp(seen[i], "Ok")   == 0) saw_ok   = true;
                if (strcmp(seen[i], "Err")  == 0) saw_err  = true;
                if (strcmp(seen[i], "Some") == 0) saw_some = true;
                if (strcmp(seen[i], "None") == 0) saw_none = true;
            }
            if ((saw_ok || saw_err) && !(saw_ok && saw_err)) {
                diag_error(s->diag, e->loc,
                           "non-exhaustive match on `Result`: missing `%s` arm "
                           "(or add a `_` wildcard)",
                           saw_ok ? "Err" : "Ok");
                return;
            }
            if ((saw_some || saw_none) && !(saw_some && saw_none)) {
                diag_error(s->diag, e->loc,
                           "non-exhaustive match on `Option`: missing `%s` arm "
                           "(or add a `_` wildcard)",
                           saw_some ? "None" : "Some");
                return;
            }

            /* User-defined enum: if every seen variant belongs to a single
             * declared enum, demand we cover all of its variants. */
            if (n_seen > 0) {
                EnumDecl *owner = NULL;
                bool consistent = true;
                for (int i = 0; i < n_seen && consistent; i++) {
                    EnumDecl *found = NULL;
                    for (int j = 0; j < s->mod->n_items; j++) {
                        Item *it = s->mod->items[j];
                        if (it->kind != IT_ENUM) continue;
                        for (int k = 0; k < it->enm->n_variants; k++) {
                            if (strcmp(it->enm->variants[k].name, seen[i]) == 0) {
                                found = it->enm; break;
                            }
                        }
                        if (found) break;
                    }
                    if (!found)            { consistent = false; break; }
                    if (!owner)              owner = found;
                    else if (owner != found) { consistent = false; break; }
                }
                if (consistent && owner) {
                    for (int k = 0; k < owner->n_variants; k++) {
                        const char *vname = owner->variants[k].name;
                        bool covered = false;
                        for (int i = 0; i < n_seen; i++)
                            if (strcmp(seen[i], vname) == 0) { covered = true; break; }
                        if (!covered) {
                            diag_error(s->diag, e->loc,
                                       "non-exhaustive match on `%s`: missing variant `%s` "
                                       "(or add a `_` wildcard)",
                                       owner->name, vname);
                        }
                    }
                }
            }
            return;
        }

        case EX_BLOCK:   check_block(s, scope, e); return;

        case EX_RETURN:
            if (e->return_.value) check_expr(s, scope, e->return_.value);
            return;

        case EX_WHILE:
            check_expr(s, scope, e->while_.cond);
            check_expr(s, scope, e->while_.body);
            return;

        case EX_FOR: {
            check_expr(s, scope, e->for_.iter);
            Scope inner; scope_init(&inner, s->arena, scope);
            /* loop variable is fresh per iteration — immutable by default. */
            declare_pattern(s, &inner, e->for_.pat, false);
            check_expr(s, &inner, e->for_.body);
            return;
        }

        case EX_LOOP:    check_expr(s, scope, e->loop_.body); return;

        case EX_STRUCT_LIT: {
            StructDecl *sd = find_struct(s, e->struct_lit.name);
            if (!sd) {
                diag_error(s->diag, e->loc, "unknown struct type '%s'", e->struct_lit.name);
            } else {
                /* Check field names exist; missing/extra fields are warnings for now. */
                for (int i = 0; i < e->struct_lit.n_fields; i++) {
                    const char *fname = e->struct_lit.fields[i].name;
                    bool found = false;
                    for (int j = 0; j < sd->n_fields; j++) {
                        if (strcmp(sd->fields[j].name, fname) == 0) { found = true; break; }
                    }
                    if (!found) {
                        diag_error(s->diag, e->loc,
                                   "struct '%s' has no field '%s'", sd->name, fname);
                    }
                    check_expr(s, scope, e->struct_lit.fields[i].value);
                }
            }
            return;
        }

        case EX_TUPLE_LIT:
            for (int i = 0; i < e->tuple_lit.n; i++) check_expr(s, scope, e->tuple_lit.elems[i]);
            return;
        case EX_ARRAY_LIT:
            for (int i = 0; i < e->array_lit.n; i++) check_expr(s, scope, e->array_lit.elems[i]);
            return;

        case EX_CAST:    check_expr(s, scope, e->cast.expr);  return;
        case EX_REF:
            check_expr(s, scope, e->ref_.inner);
            /* `&mut x` requires x itself to be mutable (b021); a plain `&x`
             * places no obligation.  classify_lhs also marks the binding as
             * written so a `&mut` borrow counts as "mut was exercised". */
            if (e->ref_.is_mut) {
                check_lhs_mutable(s, scope, e->ref_.inner, "take a `&mut` borrow of");
            }
            return;
        case EX_DEREF:   check_expr(s, scope, e->deref.inner);return;
        case EX_RANGE:
            if (e->range.start) check_expr(s, scope, e->range.start);
            if (e->range.end)   check_expr(s, scope, e->range.end);
            return;

        case EX_TRY: {
            check_expr(s, scope, e->try_.inner);
            /*
             * `?` only makes sense inside a function that itself returns
             * Result<_,_> or Option<_>.  Without this check (v0.0.1-b018)
             * a stray `expr?` at the top of `main()` would lower to a
             * statement-expression that did an early `return` of an
             * incompatible type, corrupting the call ABI and silently
             * truncating values at the C-emitter layer.
             *
             * Type-flow E ≡ inner.err_type is still v0.0.2 work (it needs
             * the typed IR).  For now we require the *shape* match.
             */
            if (!s->cur_fn) {
                diag_error(s->diag, e->loc,
                           "`?` used outside any function — it can only propagate "
                           "errors out of a function that returns Result or Option");
                return;
            }
            TypeExpr *rt = s->cur_fn->ret_type;
            bool ok = false;
            if (rt) {
                if (rt->kind == TY_GENERIC && rt->generic.name) {
                    if (strcmp(rt->generic.name, "Result") == 0 ||
                        strcmp(rt->generic.name, "Option") == 0) ok = true;
                } else if (rt->kind == TY_NAMED && rt->named.name) {
                    /* tolerate the bare-name form `-> Result` if user omitted args */
                    if (strcmp(rt->named.name, "Result") == 0 ||
                        strcmp(rt->named.name, "Option") == 0) ok = true;
                }
            }
            if (!ok) {
                diag_error(s->diag, e->loc,
                           "`?` cannot propagate here — function '%s' does not "
                           "return Result<_,_> or Option<_>",
                           s->cur_fn->name ? s->cur_fn->name : "<anonymous>");
            }
            return;
        }
    }
}

static void check_fn(Sema *s, FnDecl *f) {
    Scope fn_scope; scope_init(&fn_scope, s->arena, &s->globals);
    for (int i = 0; i < f->n_params; i++) {
        FnParam *fp = &f->params[i];
        if (fp->is_self) {
            /* `self` only makes sense inside an impl block (b021). */
            if (!f->is_method) {
                diag_error(s->diag, f->loc,
                           "function '%s' takes `self` but is not inside an "
                           "`impl` block", f->name ? f->name : "<anonymous>");
            }
            if (i != 0) {
                diag_error(s->diag, f->loc,
                           "`self` must be the first parameter of '%s'",
                           f->name ? f->name : "<anonymous>");
            }
            Symbol *sym = scope_define(&fn_scope, s->arena, "self", SYM_VAR);
            /* `self` is mutable iff written `mut self` or `&mut self`. */
            sym->is_mut  = fp->is_mut_self;
            sym->written = true;   /* methods commonly mutate via fields; don't
                                      noise-warn on `&mut self` receivers */
            sym->loc     = f->loc;
        } else if (fp->pat) {
            /* Duplicate parameter names shadow silently in C — catch them
               here (b021).  Only PAT_IDENT params can collide; tuple
               patterns bind through declare_pattern's own walk. */
            if (fp->pat->kind == PAT_IDENT &&
                scope_lookup_local(&fn_scope, fp->pat->ident.name)) {
                diag_error(s->diag, fp->pat->loc,
                           "duplicate parameter '%s' in function '%s'",
                           fp->pat->ident.name,
                           f->name ? f->name : "<anonymous>");
                continue;
            }
            /* By-value params are immutable unless the binding pattern is `mut x`.
               `&mut T` / `*mut T` params are *pointers* — the binding holding
               the pointer is still immutable; the C backend's const-correctness
               guards writes through it. */
            declare_pattern(s, &fn_scope, fp->pat, false);
        }
    }
    s->cur_fn = f;
    if (f->body) check_expr(s, &fn_scope, f->body);
    warn_unused_mut(s, &fn_scope);
    s->cur_fn = NULL;
}

static void check_items(Sema *s) {
    Module *m = s->mod;
    for (int i = 0; i < m->n_items; i++) {
        Item *it = m->items[i];
        switch (it->kind) {
            case IT_FN: check_fn(s, it->fn); break;
            case IT_IMPL: {
                StructDecl *sd = find_struct(s, it->impl->type_name);
                if (!sd) {
                    /* enums can have impl too — relax */
                    EnumDecl *ed = find_enum(s, it->impl->type_name);
                    if (!ed) {
                        diag_error(s->diag, it->impl->loc,
                                   "impl for unknown type '%s'", it->impl->type_name);
                    }
                }
                s->cur_self_struct = sd;
                for (int j = 0; j < it->impl->n_methods; j++) {
                    check_fn(s, it->impl->methods[j]);
                }
                s->cur_self_struct = NULL;
                break;
            }
            case IT_CONST:
                if (it->cnst->value) {
                    Scope empty; scope_init(&empty, s->arena, &s->globals);
                    check_expr(s, &empty, it->cnst->value);
                }
                break;
            case IT_STRUCT: case IT_ENUM: case IT_USE: case IT_TYPE_ALIAS:
                break;
        }
    }
}

/* ---------- entrypoint ---------- */

bool sema_check(Module *m, DiagCtx *diag) {
    Sema s = { .diag = diag, .arena = m->arena, .mod = m };
    scope_init(&s.globals, m->arena, NULL);
    collect_globals(&s);
    check_items(&s);
    return !diag_has_errors(diag);
}
