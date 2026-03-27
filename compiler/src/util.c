#include "util.h"
#include <stdio.h>
#include <stdlib.h>

char *read_file(const char *path, size_t *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Error: cannot open file '%s'\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    size_t len = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = xmalloc(len + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);

    if (out_len)
        *out_len = len;
    return buf;
}

// --  Memory Management  --
void *xmalloc(size_t size)
{
    void *ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "Memory allocation failed; out of memory.\n");
        abort();
    }
    return ptr;
}

void *__xrealloc(void **ptr, size_t size)
{
    void *new_ptr = realloc(*ptr, size);
    if (!new_ptr) {
        fprintf(stderr, "Memory re-allocation failed; out of memory.\n");
        abort();
    }
    return new_ptr;
}

void __xfree(void **ptr)
{
    if (ptr != NULL && *ptr != NULL) {
        free(*ptr);
        *ptr = NULL;
    }
}
