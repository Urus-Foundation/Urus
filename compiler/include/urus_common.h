/*
 * URUS Compiler — common definitions
 * Apache-2.0 OR MIT
 */
#ifndef URUS_COMMON_H
#define URUS_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define URUS_VERSION "0.0.1"
#define URUS_VERSION_MAJOR 0
#define URUS_VERSION_MINOR 0
#define URUS_VERSION_PATCH 1

#ifdef _MSC_VER
#  define URUS_NORETURN __declspec(noreturn)
#  define URUS_INLINE   __forceinline
#else
#  define URUS_NORETURN __attribute__((noreturn))
#  define URUS_INLINE   inline __attribute__((always_inline))
#endif

#define URUS_UNUSED(x) ((void)(x))

/* NULL-tolerant memcpy.  The parser's grow-buffer pattern leaves `src`
 * NULL when zero elements were collected; memcpy(dst, NULL, 0) is UB
 * (nonnull attribute) even though every libc accepts it.  UBSan flags
 * it, so guard the n==0 case once, here. */
static URUS_INLINE void urus_memcpy(void *dst, const void *src, size_t n) {
    if (n > 0) memcpy(dst, src, n);
}

#endif /* URUS_COMMON_H */
