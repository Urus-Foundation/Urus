#include "diag.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* b025 hardening:
 *   - Snippet lines are windowed to URUS_DIAG_MAX_SNIPPET columns around
 *     the caret.  Pre-b025 a 16 MiB single-line source printed the whole
 *     line on every diagnostic — output amplification that turned one
 *     malformed file into gigabytes of stderr.
 *   - Total errors are capped at URUS_DIAG_MAX_ERRORS.  Past the cap the
 *     context prints one "too many errors" notice and suppresses further
 *     diagnostics (counting continues so exit codes stay truthful).
 *   - diag_note gained the same snippet + color treatment as warn/error.
 */
#define URUS_DIAG_MAX_SNIPPET 120
#define URUS_DIAG_MAX_ERRORS  64

static const char *level_str(DiagLevel l) {
    switch (l) {
        case DIAG_NOTE:  return "note";
        case DIAG_WARN:  return "warning";
        case DIAG_ERROR: return "error";
        case DIAG_FATAL: return "fatal";
    }
    return "?";
}

static const char *color_for(DiagLevel l, bool color) {
    if (!color) return "";
    switch (l) {
        case DIAG_NOTE:  return "\033[1;36m";   /* cyan */
        case DIAG_WARN:  return "\033[1;33m";   /* yellow */
        case DIAG_ERROR: return "\033[1;31m";   /* red */
        case DIAG_FATAL: return "\033[1;35m";   /* magenta */
    }
    return "";
}

static const char *color_reset(bool color) {
    return color ? "\033[0m" : "";
}

void diag_init(DiagCtx *d, const char *file, const char *source) {
    d->error_count = 0;
    d->warn_count  = 0;
    d->source_text = source;
    d->source_file = file;
    d->can_recover = false;   /* armed explicitly via diag_set_recovery */
#ifdef _WIN32
    d->use_color = false;  /* keep simple on cmd.exe; enable later via flag */
#else
    d->use_color = true;
#endif
}

/* Returns true when the diagnostic should still be printed; emits the
 * one-time "too many errors" notice when the cap is first crossed. */
static bool diag__check_cap(DiagCtx *d, DiagLevel lvl) {
    if (lvl != DIAG_ERROR && lvl != DIAG_FATAL) {
        /* Warnings/notes stop printing too once errors flood — they would
         * only add noise below an avalanche. */
        return d->error_count <= URUS_DIAG_MAX_ERRORS;
    }
    if (d->error_count == URUS_DIAG_MAX_ERRORS + 1) {
        fprintf(stderr,
                "error: too many errors (%d) — further diagnostics suppressed\n",
                URUS_DIAG_MAX_ERRORS);
        return false;
    }
    return d->error_count <= URUS_DIAG_MAX_ERRORS;
}

static void print_snippet(const DiagCtx *d, SrcLoc loc) {
    if (!d->source_text) return;
    const char *src = d->source_text;
    size_t off = loc.offset;
    size_t line_start = off;
    while (line_start > 0 && src[line_start - 1] != '\n') line_start--;
    size_t line_end = off;
    while (src[line_end] && src[line_end] != '\n') line_end++;

    /* Window the line to URUS_DIAG_MAX_SNIPPET columns centred on the
     * caret; long lines get "…" markers on the clipped side(s). */
    size_t win_start = line_start, win_end = line_end;
    bool clip_l = false, clip_r = false;
    if (line_end - line_start > URUS_DIAG_MAX_SNIPPET) {
        size_t half = URUS_DIAG_MAX_SNIPPET / 2;
        win_start = (off > line_start + half) ? off - half : line_start;
        win_end   = win_start + URUS_DIAG_MAX_SNIPPET;
        if (win_end > line_end) {
            win_end = line_end;
            win_start = (win_end > (size_t)URUS_DIAG_MAX_SNIPPET)
                        ? win_end - URUS_DIAG_MAX_SNIPPET : line_start;
        }
        clip_l = win_start > line_start;
        clip_r = win_end   < line_end;
    }

    fprintf(stderr, "   |\n");
    fprintf(stderr, "%2u | %s%.*s%s\n",
            loc.line,
            clip_l ? "…" : "",
            (int)(win_end - win_start), src + win_start,
            clip_r ? "…" : "");
    fprintf(stderr, "   | ");
    if (clip_l) fputc(' ', stderr);  /* account for the leading ellipsis */
    for (size_t i = win_start; i < off && i < win_end; i++)
        fputc(src[i] == '\t' ? '\t' : ' ', stderr);
    uint32_t span = loc.length ? loc.length : 1;
    if (span > URUS_DIAG_MAX_SNIPPET) span = URUS_DIAG_MAX_SNIPPET;
    for (uint32_t i = 0; i < span; i++) fputc('^', stderr);
    fputc('\n', stderr);
}

static void diag__vemit(DiagCtx *d, DiagLevel lvl, SrcLoc loc,
                        const char *fmt, va_list ap) {
    if (!diag__check_cap(d, lvl)) return;

    fprintf(stderr, "%s%s%s: ", color_for(lvl, d->use_color),
            level_str(lvl), color_reset(d->use_color));
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n  --> %s:%u:%u\n",
            loc.file ? loc.file : (d->source_file ? d->source_file : "<input>"),
            loc.line, loc.col);
    print_snippet(d, loc);
}

void diag_emit(DiagCtx *d, DiagLevel lvl, SrcLoc loc, const char *fmt, ...) {
    if (lvl == DIAG_ERROR || lvl == DIAG_FATAL) d->error_count++;
    if (lvl == DIAG_WARN) d->warn_count++;
    va_list ap; va_start(ap, fmt);
    diag__vemit(d, lvl, loc, fmt, ap);
    va_end(ap);
}

void diag_note(DiagCtx *d, SrcLoc loc, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    diag__vemit(d, DIAG_NOTE, loc, fmt, ap);
    va_end(ap);
}

void diag_warn(DiagCtx *d, SrcLoc loc, const char *fmt, ...) {
    d->warn_count++;
    va_list ap; va_start(ap, fmt);
    diag__vemit(d, DIAG_WARN, loc, fmt, ap);
    va_end(ap);
}

void diag_error(DiagCtx *d, SrcLoc loc, const char *fmt, ...) {
    d->error_count++;
    va_list ap; va_start(ap, fmt);
    diag__vemit(d, DIAG_ERROR, loc, fmt, ap);
    va_end(ap);
}

void diag_fatal(DiagCtx *d, SrcLoc loc, const char *fmt, ...) {
    d->error_count++;
    va_list ap; va_start(ap, fmt);
    diag__vemit(d, DIAG_FATAL, loc, fmt, ap);
    va_end(ap);
    /* b026: embedders (urus_compile_buffer, fuzzer, future LSP) arm a
     * recovery point and get control back; the bare CLI keeps exit(2). */
    if (d->can_recover) longjmp(d->recover, 1);
    exit(2);
}
