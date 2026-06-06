/*
 * URUS compile-pipeline entry point (v0.0.1-b023).
 *
 * Splits the lex → parse → sema → codegen pipeline out of main.c so it can
 * be driven from more than one host:
 *   - urusc's main()           (file in, .c file out)
 *   - the libFuzzer target     (bytes in, exit code out, no file I/O)
 *   - future: LSP / REPL hosts
 */
#ifndef URUS_COMPILE_H
#define URUS_COMPILE_H

#include <stddef.h>
#include "strbuf.h"
#include "diag.h"

/*
 * Validate that `p[0..n)` is well-formed UTF-8 with no embedded NULs.
 * Returns 0 if valid, else the 1-based byte offset of the first bad byte.
 * (Moved here from main.c in b023 so the fuzz target mirrors the exact
 * production input surface.)
 */
size_t urus_utf8_first_bad_offset(const unsigned char *p, size_t n);

/*
 * Compile a source buffer to C.
 *
 *   src      — source bytes; need not be NUL-terminated (a terminated
 *              copy is made internally)
 *   len      — byte length of src
 *   filename — name used in diagnostics (e.g. "fuzz" or the real path)
 *   out_c    — on success, receives the emitted C (caller must sb_free);
 *              may be NULL if the caller only wants the verdict
 *
 * Returns 0 on success, non-zero if any diagnostic error fired (including
 * UTF-8 validation failure).  Never writes files, never calls exit().
 */
int urus_compile_buffer(const char *src, size_t len,
                        const char *filename, StrBuf *out_c);

#endif
