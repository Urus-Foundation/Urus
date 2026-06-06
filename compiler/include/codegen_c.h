/*
 * URUS C-transpiler backend v0.0.1
 *
 * Emits a single self-contained C99 translation unit:
 *   - #include of urus_rt.h (the URUS runtime)
 *   - all struct decls (forward + full)
 *   - all enum decls (tag enum + tagged-union struct)
 *   - all fn decls
 *   - main() shim that calls user `main` and translates Result to exit code
 *
 * Identifier mangling is minimal in v0.0.1: methods become `Type__method`,
 * everything else keeps its source name. Reserved C words get an `_` suffix.
 */
#ifndef URUS_CODEGEN_C_H
#define URUS_CODEGEN_C_H

#include "ast.h"
#include "diag.h"
#include "strbuf.h"

/* Emit C source for module into sb. Returns true on success. */
bool codegen_c_emit(StrBuf *sb, Module *m, DiagCtx *diag);

#endif
