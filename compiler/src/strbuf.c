#include "strbuf.h"
#include "urus_abort.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Hard upper bound on emitted output.  1 GiB is far past anything a
 * legitimate URUS translation unit emits — even the v0.1 standard
 * library is expected to produce tens of MiB at most.
 *
 * Closes Tier-0 item #14 and the F-MEM-4 capacity-overflow spin.
 */
#define URUS_MAX_STRBUF_BYTES (1024ull * 1024ull * 1024ull)

void sb_init(StrBuf *sb) {
    sb->cap  = 256;
    sb->len  = 0;
    sb->data = (char *)malloc(sb->cap);
    if (!sb->data) urus_abort_oom("OOM (sb_init)");
    sb->data[0] = '\0';
}

void sb_reserve(StrBuf *sb, size_t additional) {
    /* Detect addition overflow before it wraps into a smaller number. */
    if (additional > (size_t)URUS_MAX_STRBUF_BYTES ||
        sb->len > (size_t)URUS_MAX_STRBUF_BYTES - additional - 1) {
        urus_abort_oom("emitted output would exceed the 1 GiB StrBuf cap");
    }
    size_t need = sb->len + additional + 1;
    if (need <= sb->cap) return;
    /*
     * Grow by doubling, but never past the cap and never via `cap *= 2`
     * wrapping past SIZE_MAX/2.  Closes F-MEM-4.
     */
    while (sb->cap < need) {
        if (sb->cap > (size_t)URUS_MAX_STRBUF_BYTES / 2) {
            sb->cap = (size_t)URUS_MAX_STRBUF_BYTES;
            break;
        }
        sb->cap *= 2;
    }
    if (sb->cap < need) {
        urus_abort_oom("emitted output would exceed the 1 GiB StrBuf cap");
    }
    char *grown = (char *)realloc(sb->data, sb->cap);
    if (!grown) {
        free(sb->data);
        sb->data = NULL;
        urus_abort_oom("OOM (sb_reserve)");
    }
    sb->data = grown;
}

void sb_putc(StrBuf *sb, char c) {
    sb_reserve(sb, 1);
    sb->data[sb->len++] = c;
    sb->data[sb->len]   = '\0';
}

void sb_puts(StrBuf *sb, const char *s) {
    sb_putn(sb, s, strlen(s));
}

void sb_putn(StrBuf *sb, const char *s, size_t n) {
    sb_reserve(sb, n);
    urus_memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}

void sb_printf(StrBuf *sb, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    va_list aq;
    va_copy(aq, ap);
    int n = vsnprintf(NULL, 0, fmt, aq);
    va_end(aq);
    if (n < 0) { va_end(ap); return; }
    sb_reserve(sb, (size_t)n);
    vsnprintf(sb->data + sb->len, sb->cap - sb->len, fmt, ap);
    sb->len += (size_t)n;
    va_end(ap);
}

void sb_indent(StrBuf *sb, int level) {
    for (int i = 0; i < level; i++) sb_puts(sb, "    ");
}

char *sb_finish(StrBuf *sb) {
    char *p = sb->data;
    sb->data = NULL;
    sb->len = sb->cap = 0;
    return p;
}

void sb_free(StrBuf *sb) {
    free(sb->data);
    sb->data = NULL;
    sb->len = sb->cap = 0;
}
