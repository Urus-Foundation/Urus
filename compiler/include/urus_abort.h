/*
 * Recoverable-abort hook for allocation-layer failures (v0.0.1-b029).
 *
 * arena.c and strbuf.c sit below DiagCtx (no source location, no diag
 * context threaded through their call sites), but exit(1) from inside a
 * library is unacceptable for embedders — it kills the fuzzer, an LSP
 * host, anything.  This hook is the minimal fix: the embedder arms a
 * jmp_buf; the utility layers call urus_abort_oom(), which longjmps if
 * armed and exit(1)s otherwise (bare CLI behaviour, unchanged).
 *
 * Single-threaded by design — v0.0.1 has no threads.  The hook is
 * process-global state; nesting compile calls re-arms it (the inner
 * call's disarm restores nothing).  Both are acceptable at this stage
 * and documented for the v0.0.2 revisit.
 */
#ifndef URUS_ABORT_H
#define URUS_ABORT_H

#include <setjmp.h>
#include <stdbool.h>

/* Arm / disarm the recovery point.  Pattern:
 *
 *     jmp_buf jb;
 *     if (setjmp(jb) == 0) {
 *         urus_abort_arm(&jb);
 *         ... pipeline ...
 *         urus_abort_disarm();
 *     } else {
 *         // OOM fired somewhere below; process still alive
 *     }
 */
void urus_abort_arm(jmp_buf *jb);
void urus_abort_disarm(void);

/* Report an unrecoverable allocation failure.  Prints `msg` to stderr,
 * then longjmps to the armed point — or exit(1)s if none is armed.
 * Never returns to the caller via either path, hence noreturn (the
 * longjmp transfers control, it does not "return"). */
#if defined(_MSC_VER) && !defined(__clang__)
__declspec(noreturn) void urus_abort_oom(const char *msg);
#else
void urus_abort_oom(const char *msg) __attribute__((noreturn));
#endif

#endif
