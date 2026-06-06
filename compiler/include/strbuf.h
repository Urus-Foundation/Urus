/*
 * Growing string buffer with printf-style append.
 * Used by codegen to assemble emitted C source.
 */
#ifndef URUS_STRBUF_H
#define URUS_STRBUF_H

#include "urus_common.h"

typedef struct {
    char  *data;
    size_t len;
    size_t cap;
} StrBuf;

void  sb_init(StrBuf *sb);
void  sb_reserve(StrBuf *sb, size_t additional);
void  sb_putc(StrBuf *sb, char c);
void  sb_puts(StrBuf *sb, const char *s);
void  sb_putn(StrBuf *sb, const char *s, size_t n);
void  sb_printf(StrBuf *sb, const char *fmt, ...);
void  sb_indent(StrBuf *sb, int level);
char *sb_finish(StrBuf *sb);  /* steals; caller frees */
void  sb_free(StrBuf *sb);

#endif
