/*
 * urus_compile_buffer — the lex → parse → sema → codegen pipeline as a
 * pure(ish) function over bytes (v0.0.1-b023).
 *
 * main.c calls this for the normal `--emit-c` path; the libFuzzer target
 * (fuzz/fuzz_compile.c) calls it with mutated inputs.  Keeping the two on
 * the same entry point means the fuzzer exercises exactly the production
 * surface — including UTF-8 validation, all the resource caps, and every
 * diagnostic path — with no file-system involvement.
 */
#include "compile.h"
#include "arena.h"
#include "lexer.h"
#include "parser.h"
#include "sema.h"
#include "codegen_c.h"
#include "urus_abort.h"
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

size_t urus_utf8_first_bad_offset(const unsigned char *p, size_t n) {
    size_t i = 0;
    while (i < n) {
        unsigned char b0 = p[i];
        if (b0 == 0x00) return i + 1;        /* embedded NUL */
        if (b0 < 0x80) { i++; continue; }    /* ASCII */
        size_t need;
        uint32_t cp;
        uint32_t low, high;
        if ((b0 & 0xE0) == 0xC0) { need = 1; cp = b0 & 0x1F; low = 0x80;    high = 0x7FF; }
        else if ((b0 & 0xF0) == 0xE0) { need = 2; cp = b0 & 0x0F; low = 0x800;   high = 0xFFFF; }
        else if ((b0 & 0xF8) == 0xF0) { need = 3; cp = b0 & 0x07; low = 0x10000; high = 0x10FFFF; }
        else return i + 1;                   /* stray continuation or 5-/6-byte */

        if (i + need >= n) return i + 1;     /* truncated trailer */
        for (size_t k = 1; k <= need; k++) {
            unsigned char bk = p[i + k];
            if ((bk & 0xC0) != 0x80) return i + k + 1;
            cp = (cp << 6) | (bk & 0x3F);
        }
        if (cp < low || cp > high)        return i + 1; /* overlong / out of range */
        if (cp >= 0xD800 && cp <= 0xDFFF) return i + 1; /* lone surrogate */
        i += need + 1;
    }
    return 0;
}

int urus_compile_buffer(const char *src, size_t len,
                        const char *filename, StrBuf *out_c) {
    /* Reject malformed UTF-8 up front — same contract as read_file. */
    if (urus_utf8_first_bad_offset((const unsigned char *)src, len) != 0) {
        return 1;
    }

    /* The lexer expects a NUL-terminated buffer it can scan to '\0'. */
    char *source = (char *)malloc(len + 1);
    if (!source) return 1;
    memcpy(source, src, len);
    source[len] = '\0';

    DiagCtx diag;
    diag_init(&diag, filename, source);

    Arena arena;
    arena_init(&arena, 256 * 1024);

    int rc = 1;

    /* b029: arm the allocation-failure recovery point.  arena/strbuf OOM
     * (or cap breach) longjmps here instead of exit(1).  Must be armed
     * BEFORE any sb_/arena_ call below and disarmed on every path out. */
    jmp_buf oom_jb;
    if (setjmp(oom_jb) != 0) {
        urus_abort_disarm();
        arena_free(&arena);
        free(source);
        return 1;
    }
    urus_abort_arm(&oom_jb);

    /* b026: arm the fatal-recovery point.  A diag_fatal anywhere in the
     * pipeline (e.g. lexer OOM path) longjmps back here instead of
     * exit(2)-ing the whole process — essential for the fuzzer and any
     * library embedder.  Cleanup below runs on both paths. */
    if (setjmp(diag.recover) == 0) {
        diag_set_recovery(&diag);

        Parser p;
        parser_init(&p, source, filename, &diag, &arena);
        Module *m = parser_parse_module(&p);

        if (!diag_has_errors(&diag)) {
            sema_check(m, &diag);
        }
        if (!diag_has_errors(&diag)) {
            StrBuf sb;
            sb_init(&sb);
            codegen_c_emit(&sb, m, &diag);
            if (!diag_has_errors(&diag)) {
                rc = 0;
                if (out_c) *out_c = sb;      /* transfer ownership */
                else       sb_free(&sb);
            } else {
                sb_free(&sb);
            }
        }
    }
    /* else: diag_fatal fired — rc stays 1, counts are truthful. */

    urus_abort_disarm();
    arena_free(&arena);
    free(source);
    return rc;
}
