/*
 * URUS Semantic Analyzer v0.0.1
 *
 * Scope:
 *   - Two-pass: (1) collect top-level item names, (2) check bodies.
 *   - Name resolution into a chain of scopes.
 *   - Light type checking: integer/float/bool/string/unit + named structs/enums.
 *     Numeric literals are coerced to the annotated type (let x: u64 = 1).
 *   - Result<T,E> and Option<T> recognised as generic-named types only —
 *     proper monomorphisation lands in v0.0.2.
 *   - Method resolution: foo.bar(args) → struct lookup in impl block.
 *   - Diagnostics go through DiagCtx — no abort on first error.
 *
 * Out of scope for v0.0.1: full generics, lifetimes, traits, borrow checking,
 * pattern exhaustiveness checking.
 */
#ifndef URUS_SEMA_H
#define URUS_SEMA_H

#include "ast.h"
#include "diag.h"

bool sema_check(Module *m, DiagCtx *diag);

#endif
