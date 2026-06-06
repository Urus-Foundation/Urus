/*
 * URUS Runtime v0.0.1
 *
 * Tiny C header bundled into every URUS-generated translation unit.
 * Goals: no dependency beyond libc, header-only by default (define
 * URUS_RT_IMPLEMENTATION in one .c file to compile the bodies — single
 * translation unit compiles work as-is via -DURUS_RT_HEADER_ONLY).
 *
 * Provides:
 *   - urus_bool, urus_true, urus_false
 *   - urus_str: fat pointer (ptr + len), built from C string literals
 *   - urus_Result and urus_Option as tagged unions
 *   - println/print family with `{name}`-style interpolation via variadic
 *     `urus_fmt_arg` arrays + the URUS_FMT_* macros emitted by codegen
 *   - urus_panic to abort with a message
 */
#ifndef URUS_RT_H
#define URUS_RT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- booleans ---------- */
typedef int urus_bool;
#define urus_true  1
#define urus_false 0

/* ---------- strings (fat pointer) ---------- */
typedef struct {
    const char *ptr;
    size_t      len;
} urus_str;

static inline urus_str urus_str_from_lit(const char *p, size_t n) {
    urus_str s = { p, n };
    return s;
}

/* ---------- string helpers (v0.0.1-b017) ----------
 *
 * v0.0.1 has no real stdlib yet — these are the half-dozen primitives
 * URUS programs reach for most.  All are `static inline` so they live
 * in the header and add no link cost.  Each accepts the fat-pointer
 * `urus_str` (NUL-tolerant): comparisons are bytewise, length-aware,
 * and do NOT walk past `len`.  Empty `ptr == NULL` is treated as
 * length 0.
 */

static inline size_t urus_str_len(urus_str s) { return s.len; }
static inline urus_bool urus_str_is_empty(urus_str s) { return s.len == 0; }

static inline urus_bool urus_str_eq(urus_str a, urus_str b) {
    if (a.len != b.len) return urus_false;
    if (a.len == 0)     return urus_true;
    return memcmp(a.ptr, b.ptr, a.len) == 0 ? urus_true : urus_false;
}

/* Returns -1 / 0 / +1.  Lexicographic over unsigned bytes, then by length
 * so a proper prefix sorts before the longer string (POSIX strcmp shape). */
static inline int urus_str_cmp(urus_str a, urus_str b) {
    size_t n = a.len < b.len ? a.len : b.len;
    if (n > 0) {
        int c = memcmp(a.ptr, b.ptr, n);
        if (c != 0) return c < 0 ? -1 : 1;
    }
    if (a.len == b.len) return 0;
    return a.len < b.len ? -1 : 1;
}

static inline urus_bool urus_str_starts_with(urus_str s, urus_str prefix) {
    if (prefix.len > s.len) return urus_false;
    if (prefix.len == 0)    return urus_true;
    return memcmp(s.ptr, prefix.ptr, prefix.len) == 0 ? urus_true : urus_false;
}

static inline urus_bool urus_str_ends_with(urus_str s, urus_str suffix) {
    if (suffix.len > s.len) return urus_false;
    if (suffix.len == 0)    return urus_true;
    return memcmp(s.ptr + (s.len - suffix.len), suffix.ptr, suffix.len) == 0
           ? urus_true : urus_false;
}

/* True iff `needle` appears as a substring of `s` at any byte position.
 * Naive O(n*m) — fine at v0.0.1 because URUS strings are bounded by the
 * 16 MiB lex cap (F-MEM-2) and the formatter cap below. */
static inline urus_bool urus_str_contains(urus_str s, urus_str needle) {
    if (needle.len == 0)    return urus_true;
    if (needle.len > s.len) return urus_false;
    size_t end = s.len - needle.len;
    for (size_t i = 0; i <= end; i++) {
        if (memcmp(s.ptr + i, needle.ptr, needle.len) == 0) return urus_true;
    }
    return urus_false;
}

/* ---------- Result and Option (widened payload in v0.0.1-b015) ----------
 *
 * Both are stored as a tagged union plus a 16-byte payload arena.  Until
 * b014 the payload was a single `int64_t`, which silently truncated any
 * value larger than 8 bytes — `Ok(some_str)` lost half of `urus_str` on
 * 64-bit platforms (closes **F-TY-2**, partially).
 *
 * The payload is now a `urus_payload_t` union covering: int64, uint64,
 * double, pointer, and `urus_str` (the largest in-tree URUS value).
 * Total size: 16 bytes on every supported ABI.  Source-level code keeps
 * the same `urus_ok / urus_err / urus_some / urus_payload` names; the
 * constructor macros are now type-dispatched via `_Generic` so the right
 * union arm is filled without the caller doing an explicit cast.
 *
 * For payloads larger than 16 bytes (rare in v0.0.1 — user structs that
 * exceed the budget) URUS still requires boxing.  The codegen emits a
 * size assertion (`_Static_assert(sizeof(T) <= 16, …)`) at the
 * construction site so the failure is a build error with a precise
 * message rather than a silent memcpy of half the value.
 *
 * **F-TY-1** (b012) is preserved: Result and Option tag values are
 * disjoint so a cross-cast (`urus_is_ok(opt)`) reliably returns false.
 */

typedef enum { URUS_RES_OK = 0, URUS_RES_ERR = 1 } urus_res_tag;
typedef enum { URUS_OPT_NONE = 2, URUS_OPT_SOME = 3 } urus_opt_tag;

typedef union {
    int64_t   i;
    uint64_t  u;
    double    f;
    void     *p;
    urus_str  s;          /* the largest in-tree URUS value (ptr + len) */
    uint64_t  raw[2];     /* fixed 16-byte footprint guarantee */
} urus_payload_t;

typedef struct {
    urus_res_tag   tag;
    urus_payload_t payload;
} urus_Result;

typedef struct {
    urus_opt_tag   tag;
    urus_payload_t payload;
} urus_Option;

/* Compile-time size guard: keeps the layout stable so the codegen emit
 * pattern (memcpy into `.raw`) never silently changes underneath us. */
#ifdef __cplusplus
static_assert(sizeof(urus_payload_t) == 16, "urus_payload_t must be 16 bytes");
#else
_Static_assert(sizeof(urus_payload_t) == 16, "urus_payload_t must be 16 bytes");
#endif

/* Type-dispatched constructors — pick the right union arm based on the
 * argument's static type so str payloads no longer truncate, pointers no
 * longer get sign-extended through `int64_t`, and doubles no longer
 * bit-pun through an integer arm. */
/* _Generic gotcha: every association is type-checked even when not
 * selected, so casts like `(void *)(v)` blow up when `v` is a struct
 * (urus_str). Dispatch to per-type constructor *functions* instead —
 * argument conversion is then only checked against the selected one. */
static inline urus_payload_t urus__pl_str(urus_str s)    { urus_payload_t p; p.s = s;          return p; }
static inline urus_payload_t urus__pl_ptr(const void *q) { urus_payload_t p; p.p = (void *)q;  return p; }
static inline urus_payload_t urus__pl_f(double f)        { urus_payload_t p; p.f = f;          return p; }
static inline urus_payload_t urus__pl_i(int64_t i)       { urus_payload_t p; p.i = i;          return p; }
static inline urus_payload_t urus__pl_u(uint64_t u)      { urus_payload_t p; p.u = u;          return p; }

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define URUS_PAYLOAD_OF(v) (_Generic((v),                                      \
        urus_str:        urus__pl_str,                                         \
        char *:          urus__pl_ptr,                                         \
        const char *:    urus__pl_ptr,                                         \
        void *:          urus__pl_ptr,                                         \
        float:           urus__pl_f,                                           \
        double:          urus__pl_f,                                           \
        int8_t:          urus__pl_i,                                           \
        int16_t:         urus__pl_i,                                           \
        int32_t:         urus__pl_i,                                           \
        int64_t:         urus__pl_i,                                           \
        uint8_t:         urus__pl_u,                                           \
        uint16_t:        urus__pl_u,                                           \
        uint32_t:        urus__pl_u,                                           \
        uint64_t:        urus__pl_u,                                           \
        default:         urus__pl_i)(v))
#else
#define URUS_PAYLOAD_OF(v) ((urus_payload_t){ .i = (int64_t)(v) })
#endif

#define urus_ok(v)        ((urus_Result){ URUS_RES_OK,  URUS_PAYLOAD_OF(v) })
#define urus_err(v)       ((urus_Result){ URUS_RES_ERR, URUS_PAYLOAD_OF(v) })
#define urus_some(v)      ((urus_Option){ URUS_OPT_SOME, URUS_PAYLOAD_OF(v) })
#define urus_none()       ((urus_Option){ URUS_OPT_NONE, { .i = 0 } })
#define urus_is_ok(x)     ((x).tag == URUS_RES_OK)
#define urus_is_err(x)    ((x).tag == URUS_RES_ERR)
#define urus_is_some(x)   ((x).tag == URUS_OPT_SOME)
#define urus_is_none(x)   ((x).tag == URUS_OPT_NONE)

/* Back-compat scalar accessor.  Returns the int64 arm — correct for the
 * common int / pointer-on-64-bit cases.  For str payloads use
 * `urus_payload_str(x)`; for typed access from generated code use
 * `urus_payload_as(x, T)`. */
#define urus_payload(x)         ((x).payload.i)
#define urus_payload_str(x)     ((x).payload.s)
#define urus_payload_ptr(x)     ((x).payload.p)
#define urus_payload_f(x)       ((x).payload.f)
#define urus_payload_u(x)       ((x).payload.u)
/* The typed accessor codegen reaches for when it knows the static type. */
#define urus_payload_as(x, T)   (*(T *)&((x).payload))

/* ---------- formatted I/O ----------
 *
 * The `urus_fmt_arg` array is built by codegen for every println call.
 * Each element is a tagged value.
 *
 * v0.0.1-b012 changes the contract: codegen now emits an explicit element
 * count alongside the array and calls `urus_*_fmt_n(args, count)` instead of
 * the sentinel-terminated `urus_*_fmt(args)`.  The sentinel-only loop is
 * preserved as a back-compat wrapper that *internally* caps its scan at
 * `URUS_FMT_MAX_ARGS` so a missing-END codegen bug can no longer walk
 * arbitrarily far past the array (closes F-MEM-7).
 *
 * **URUS_FMT_PTR_TAG is gone in v0.0.1-b012** (F-MEM-8): the runtime no
 * longer treats arbitrary pointers as null-terminated C strings.  The
 * macro `URUS_FMT_ANY` now routes `char*` / `const char*` through
 * `urus_str_from_lit(p, strlen(p))` so the renderer always works on a
 * length-bounded `urus_str`.  Any older translation unit that still
 * names `URUS_FMT_PTR_TAG` will fail to compile — that is the intended
 * behaviour.
 */

typedef enum {
    URUS_FMT_END_TAG = 0,
    URUS_FMT_STR_TAG,
    URUS_FMT_INT_TAG,
    URUS_FMT_UINT_TAG,
    URUS_FMT_FLOAT_TAG,
    URUS_FMT_BOOL_TAG,
} urus_fmt_tag;

/* Hard upper bound on the sentinel-terminated back-compat scan.  Picked so a
 * generous synthetic call (println with hundreds of args) still works, but a
 * missing-END buffer cannot leak more than a few KiB before the loop stops. */
#define URUS_FMT_MAX_ARGS 1024

/* Hard upper bound on the rendered size of a single `urus_fmt_to_str_n`
 * call.  Picked at 64 MiB to match `URUS_MAX_INPUT_BYTES` — a formatter
 * cannot produce more memory pressure than the source it came from.
 * Closes a latent DoS path where a program could request `f"{x}"` with
 * a multi-gigabyte `urus_str` and the runtime would happily try to
 * allocate the buffer (v0.0.1-b017 hardening). */
#define URUS_FMT_MAX_RENDER_BYTES (64ull * 1024ull * 1024ull)

/* Hard upper bound on a single urus_read_line allocation (v0.0.1-b019).
 * 16 MiB matches the string-literal lex cap — input lines and source
 * literals get the same budget.  Longer lines are truncated at the cap
 * (the rest of the line is drained and discarded so the next read starts
 * on a fresh line). */
#define URUS_READLINE_MAX_BYTES (16ull * 1024ull * 1024ull)

typedef struct {
    urus_fmt_tag tag;
    union {
        urus_str    s;
        int64_t     i;
        uint64_t    u;
        double      f;
        urus_bool   b;
    };
} urus_fmt_arg;

#define URUS_FMT_END        ((urus_fmt_arg){ URUS_FMT_END_TAG, { .i = 0 } })
#define URUS_FMT_STR(x)     ((urus_fmt_arg){ URUS_FMT_STR_TAG, { .s = (x) } })

/* Helper: wrap a `const char *` C string into a length-bounded urus_str.
 * Used by URUS_FMT_ANY's char* arms.  strlen-based: caller must ensure the
 * pointer is null-terminated, which is the same precondition as before but
 * now the *renderer* knows the length up front.  Closes F-MEM-8. */
static inline urus_fmt_arg urus__fmt_from_cstr(const char *p) {
    urus_str s = { p ? p : "", p ? strlen(p) : 0 };
    urus_fmt_arg a = { URUS_FMT_STR_TAG, { .s = s } };
    return a;
}

/* C11 _Generic dispatch picks the right tag based on the argument's type.
 * Same gotcha as URUS_PAYLOAD_OF: each association is type-checked even
 * when not selected, so select a *function* and apply it once — argument
 * conversion is then only checked against the chosen constructor.
 * Compilers without _Generic (pre-C11 / MSVC < 2019) fall back to int. */
static inline urus_fmt_arg urus__fmt_str(urus_str s)  { urus_fmt_arg a = { URUS_FMT_STR_TAG,   { .s = s } }; return a; }
static inline urus_fmt_arg urus__fmt_i(int64_t i)     { urus_fmt_arg a = { URUS_FMT_INT_TAG,   { .i = i } }; return a; }
static inline urus_fmt_arg urus__fmt_u(uint64_t u)    { urus_fmt_arg a = { URUS_FMT_UINT_TAG,  { .u = u } }; return a; }
static inline urus_fmt_arg urus__fmt_f(double f)      { urus_fmt_arg a = { URUS_FMT_FLOAT_TAG, { .f = f } }; return a; }

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define URUS_FMT_ANY(x) (_Generic((x),                   \
        urus_str:        urus__fmt_str,                  \
        char *:          urus__fmt_from_cstr,            \
        const char *:    urus__fmt_from_cstr,            \
        int8_t:          urus__fmt_i,                    \
        int16_t:         urus__fmt_i,                    \
        int32_t:         urus__fmt_i,                    \
        int64_t:         urus__fmt_i,                    \
        uint8_t:         urus__fmt_u,                    \
        uint16_t:        urus__fmt_u,                    \
        uint32_t:        urus__fmt_u,                    \
        uint64_t:        urus__fmt_u,                    \
        float:           urus__fmt_f,                    \
        double:          urus__fmt_f,                    \
        default:         urus__fmt_i)(x))
#else
#define URUS_FMT_ANY(x) ((urus_fmt_arg){ URUS_FMT_INT_TAG, { .i = (int64_t)(x) } })
#endif

/* ---------- public API ----------
 *
 * The *_n variants below are the canonical entry points starting in
 * v0.0.1-b012: codegen passes an explicit element count so the runtime
 * never has to scan for `URUS_FMT_END_TAG`.  The non-`_n` variants are
 * kept as back-compat wrappers; they cap their sentinel scan at
 * `URUS_FMT_MAX_ARGS` to defang F-MEM-7 even if a future codegen bug
 * forgets the terminator.
 */

void urus_runtime_init(int argc, char **argv);

/* Length-aware (preferred): */
void urus_println_fmt_n(const urus_fmt_arg *args, size_t count);
void urus_print_fmt_n(const urus_fmt_arg *args, size_t count);
void urus_eprintln_fmt_n(const urus_fmt_arg *args, size_t count);
urus_str urus_fmt_to_str_n(const urus_fmt_arg *args, size_t count);

/* Sentinel-terminated (back-compat thin wrappers): */
void urus_println_fmt(const urus_fmt_arg *args);
void urus_print_fmt(const urus_fmt_arg *args);
void urus_eprintln_fmt(const urus_fmt_arg *args);
urus_str urus_fmt_to_str(const urus_fmt_arg *args);

void urus_println(urus_str s);
void urus_print(urus_str s);
void urus_eprintln(urus_str s);
void urus_panic(urus_str msg) __attribute__((noreturn));

/* Read one line from stdin (v0.0.1-b019).  Returns Some(str) without the
 * trailing newline, or None on EOF / read error.  The returned str owns a
 * heap allocation; v0.0.1 has no ownership tracking, so the buffer leaks
 * by design until the arena story lands in v0.0.2 (same policy as
 * urus_fmt_to_str_n).  Line length is capped at URUS_READLINE_MAX_BYTES. */
urus_Option urus_read_line(void);

#ifdef __cplusplus
}
#endif

/* ---------- implementation (header-only) ----------
 *
 * URUS bundles its tiny runtime as header-only by default to keep the build
 * footprint of v0.0.1 minimal: one C file in, one .exe out, no extra link
 * dependencies. Define URUS_RT_NO_IMPL in some other TU if you want to break
 * this up later.
 */

#ifndef URUS_RT_NO_IMPL

static int g_urus_argc = 0;
static char **g_urus_argv = NULL;

static void urus__write_str(FILE *out, urus_str s) {
    fwrite(s.ptr, 1, s.len, out);
}

static void urus__write_one(FILE *out, const urus_fmt_arg *a) {
    switch (a->tag) {
        case URUS_FMT_END_TAG:   break;
        case URUS_FMT_STR_TAG:   urus__write_str(out, a->s); break;
        case URUS_FMT_INT_TAG:   fprintf(out, "%lld", (long long)a->i); break;
        case URUS_FMT_UINT_TAG:  fprintf(out, "%llu", (unsigned long long)a->u); break;
        case URUS_FMT_FLOAT_TAG: fprintf(out, "%g",  a->f); break;
        case URUS_FMT_BOOL_TAG:  fputs(a->b ? "true" : "false", out); break;
        /* No PTR_TAG arm: char* now arrives via URUS_FMT_STR_TAG, wrapped at
         * the call site through urus__fmt_from_cstr.  Closes F-MEM-8. */
    }
}

void urus_runtime_init(int argc, char **argv) {
    g_urus_argc = argc;
    g_urus_argv = argv;
}

/* ----- length-aware path (preferred) ----- */

void urus_print_fmt_n(const urus_fmt_arg *args, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (args[i].tag == URUS_FMT_END_TAG) break;  /* tolerate trailing END */
        urus__write_one(stdout, &args[i]);
    }
    fflush(stdout);
}

void urus_println_fmt_n(const urus_fmt_arg *args, size_t count) {
    urus_print_fmt_n(args, count);
    fputc('\n', stdout);
    fflush(stdout);
}

void urus_eprintln_fmt_n(const urus_fmt_arg *args, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (args[i].tag == URUS_FMT_END_TAG) break;
        urus__write_one(stderr, &args[i]);
    }
    fputc('\n', stderr);
    fflush(stderr);
}

/* ----- sentinel-terminated back-compat (closes F-MEM-7) ----- */

static size_t urus__bounded_count(const urus_fmt_arg *args) {
    /* Walk at most URUS_FMT_MAX_ARGS slots looking for END.  Past that we
     * stop and return the cap — a missing END can no longer turn into an
     * unbounded out-of-bounds read. */
    size_t i = 0;
    while (i < (size_t)URUS_FMT_MAX_ARGS && args[i].tag != URUS_FMT_END_TAG) i++;
    return i;
}

void urus_print_fmt(const urus_fmt_arg *args) {
    urus_print_fmt_n(args, urus__bounded_count(args));
}

void urus_println_fmt(const urus_fmt_arg *args) {
    urus_println_fmt_n(args, urus__bounded_count(args));
}

void urus_eprintln_fmt(const urus_fmt_arg *args) {
    urus_eprintln_fmt_n(args, urus__bounded_count(args));
}

void urus_println(urus_str s) {
    urus__write_str(stdout, s);
    fputc('\n', stdout);
    fflush(stdout);
}

void urus_print(urus_str s) {
    urus__write_str(stdout, s);
    fflush(stdout);
}

void urus_eprintln(urus_str s) {
    urus__write_str(stderr, s);
    fputc('\n', stderr);
    fflush(stderr);
}

void urus_panic(urus_str msg) {
    fputs("urus: panic: ", stderr);
    /* Defensive: cap the panic message at 4 KiB and tolerate a NULL ptr
     * (e.g. a default-initialised `urus_str`).  A pathological panic
     * payload must not itself become a vector — log what we can and
     * abort regardless (v0.0.1-b017 hardening). */
    if (msg.ptr && msg.len) {
        size_t n = msg.len > 4096 ? 4096 : msg.len;
        fwrite(msg.ptr, 1, n, stderr);
        if (msg.len > n) fputs(" …(truncated)", stderr);
    } else {
        fputs("(no message)", stderr);
    }
    fputc('\n', stderr);
    fflush(stderr);
    abort();
}

urus_str urus_fmt_to_str_n(const urus_fmt_arg *args, size_t count) {
    /* Two-pass: measure into a discard buffer to size the allocation, then
       render. Simpler than chasing realloc growth and acceptable at v0.0.1.
       v0.0.1-b017: refuse rendered sizes above URUS_FMT_MAX_RENDER_BYTES so
       a runaway formatter cannot turn into a multi-GiB allocation. */
    FILE *measure = tmpfile();
    if (!measure) { urus_str z = { "", 0 }; return z; }
    for (size_t i = 0; i < count; i++) {
        if (args[i].tag == URUS_FMT_END_TAG) break;
        urus__write_one(measure, &args[i]);
    }
    long n = ftell(measure);
    if (n < 0) n = 0;
    if ((unsigned long long)n > (unsigned long long)URUS_FMT_MAX_RENDER_BYTES) {
        fclose(measure);
        fputs("urus: runtime: format result exceeds URUS_FMT_MAX_RENDER_BYTES\n",
              stderr);
        urus_str z = { "", 0 };
        return z;
    }
    rewind(measure);
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(measure); urus_str z = { "", 0 }; return z; }
    fread(buf, 1, (size_t)n, measure);
    buf[n] = '\0';
    fclose(measure);
    urus_str s = { buf, (size_t)n };
    return s;
}

urus_str urus_fmt_to_str(const urus_fmt_arg *args) {
    return urus_fmt_to_str_n(args, urus__bounded_count(args));
}

urus_Option urus_read_line(void) {
    size_t cap = 256, len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) return urus_none();

    int c;
    bool got_any = false;
    while ((c = fgetc(stdin)) != EOF) {
        got_any = true;
        if (c == '\n') break;
        if (len + 1 >= cap) {
            if (cap >= (size_t)URUS_READLINE_MAX_BYTES) {
                /* Truncate at the cap; drain the rest of the line so the
                 * next call starts fresh instead of reading our tail. */
                while ((c = fgetc(stdin)) != EOF && c != '\n') {}
                break;
            }
            size_t ncap = cap * 2;
            if (ncap > (size_t)URUS_READLINE_MAX_BYTES)
                ncap = (size_t)URUS_READLINE_MAX_BYTES;
            char *nbuf = (char *)realloc(buf, ncap);
            if (!nbuf) { free(buf); return urus_none(); }
            buf = nbuf; cap = ncap;
        }
        buf[len++] = (char)c;
    }

    if (!got_any) { free(buf); return urus_none(); }   /* immediate EOF */

    /* Tolerate CRLF input: strip one trailing '\r' (Windows pipes). */
    if (len > 0 && buf[len - 1] == '\r') len--;

    buf[len] = '\0';
    urus_str s = { buf, len };
    return urus_some(s);
}

#endif /* URUS_RT_NO_IMPL */
#endif /* URUS_RT_H */
