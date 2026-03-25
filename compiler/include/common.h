#ifndef URUS_COMMON_H
#define URUS_COMMON_H

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#ifdef _WIN32
    #define URUSC_PATHSEP '\\'
#else
    #define URUSC_PATHSEP '/'
#endif

#ifndef PATH_MAX
    #ifdef MAX_PATH
        #define PATH_MAX MAX_PATH
    #else
        #define PATH_MAX 4096 /* safe default value */
    #endif
#endif

static void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "Memory allocation failed; out of memory.\n");
        abort();
    }
    return ptr;
}

#define xfree(ptr) __xfree((void **)&(ptr))
static void __xfree(void **ptr) {
    if (ptr != NULL && *ptr != NULL) {
        free(*ptr);
        *ptr = NULL;
    }
}

#endif
