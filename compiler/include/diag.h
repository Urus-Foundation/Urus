/*
 * Diagnostics — source positions, errors, warnings.
 * v0.0.1: simple but stable shape, designed to grow into rustc-style
 * multi-span diagnostics without breaking the call sites.
 */
#ifndef URUS_DIAG_H
#define URUS_DIAG_H

#include "urus_common.h"
#include <setjmp.h>

typedef struct {
    const char *file;
    uint32_t    line;
    uint32_t    col;
    uint32_t    offset;    /* byte offset into source */
    uint32_t    length;    /* span length in bytes */
} SrcLoc;

typedef enum {
    DIAG_NOTE,
    DIAG_WARN,
    DIAG_ERROR,
    DIAG_FATAL,
} DiagLevel;

typedef struct {
    int         error_count;
    int         warn_count;
    const char *source_text;  /* for snippet rendering */
    const char *source_file;
    bool        use_color;
    /* b026: optional recovery point.  When `can_recover` is set (via
     * diag_set_recovery), diag_fatal longjmps here instead of exit(2) —
     * library embedders (urus_compile_buffer, future LSP) get control
     * back; the standalone CLI keeps exit semantics by not arming it. */
    jmp_buf     recover;
    bool        can_recover;
} DiagCtx;

void diag_init(DiagCtx *d, const char *file, const char *source);
void diag_emit(DiagCtx *d, DiagLevel lvl, SrcLoc loc, const char *fmt, ...);
void diag_note(DiagCtx *d, SrcLoc loc, const char *fmt, ...);
void diag_warn(DiagCtx *d, SrcLoc loc, const char *fmt, ...);
void diag_error(DiagCtx *d, SrcLoc loc, const char *fmt, ...);
/* No longer URUS_NORETURN at the *declaration* level: with an armed
 * recovery point it transfers control via longjmp; without one it still
 * never returns (exit).  Either way it does not return to the caller —
 * but the attribute would let the optimizer delete the longjmp path's
 * cleanup in embedders, so it is intentionally dropped (b026). */
void diag_fatal(DiagCtx *d, SrcLoc loc, const char *fmt, ...);

/* Arm the recovery point.  Use as:
 *     if (setjmp(d->recover) == 0) { diag_set_recovery(d); ...pipeline... }
 *     else { ...fatal fired, ctx still valid, counts truthful... }
 */
static URUS_INLINE void diag_set_recovery(DiagCtx *d) { d->can_recover = true; }

static URUS_INLINE bool diag_has_errors(const DiagCtx *d) {
    return d->error_count > 0;
}

#endif
