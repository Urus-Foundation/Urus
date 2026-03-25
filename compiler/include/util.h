#ifndef URUS_UTIL_H
#define URUS_UTIL_H

#include <stddef.h>

char *read_file(const char *path, size_t *out_len);

// --  Memory Management  --
#define xfree(ptr) __xfree((void **)&(ptr))
#define xrealloc(ptr, size) __xrealloc((void **)&(ptr), size)
void *xmalloc(size_t size);
void *__xrealloc(void **ptr, size_t size);
void __xfree(void **ptr);

#endif
