#include "arena.h"
#include "urus_abort.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Per-call upper bound on arena_alloc.  256 MiB is far past anything the
 * v0.0.1 compiler legitimately needs (the largest AST node is a few hundred
 * bytes; intermediate string copies stay under a few KiB).  Anything larger
 * is almost certainly an integer-overflow attack pushing size near SIZE_MAX.
 *
 * Closes Tier-0 item #13 and the worst case of F-MEM-3 (size+7 wraparound).
 */
#define URUS_MAX_ARENA_ALLOC (256ull * 1024ull * 1024ull)

/*
 * Total-allocation cap across the whole arena (all chunks).  The per-call
 * cap above bounds one allocation; this bounds the *sum*, so a fuzzer (or
 * a malicious source file) cannot OOM the process through many small
 * AST-node allocations.  512 MiB is ~150× the largest legitimate v0.0.1
 * compilation we have measured.
 */
#define URUS_MAX_ARENA_TOTAL (512ull * 1024ull * 1024ull)

struct ArenaChunk {
    ArenaChunk *next;
    size_t      used;
    size_t      cap;
    /* data follows */
};

static ArenaChunk *new_chunk(size_t cap) {
    ArenaChunk *c = (ArenaChunk *)malloc(sizeof(ArenaChunk) + cap);
    if (!c) {
        /* b029: unwinds to the embedder's recovery point if armed;
         * exit(1) in the bare CLI as before. */
        urus_abort_oom("out of memory (arena chunk)");
    }
    c->next = NULL;
    c->used = 0;
    c->cap  = cap;
    return c;
}

void arena_init(Arena *a, size_t chunk_size) {
    a->chunk_size      = chunk_size ? chunk_size : 64 * 1024;
    a->total_allocated = 0;
    a->head            = new_chunk(a->chunk_size);
}

void *arena_alloc(Arena *a, size_t size) {
    /*
     * Reject obviously-bogus sizes before alignment rounds them.
     * Without this, `size = SIZE_MAX - 3` becomes `0` after `(size+7)&~7`
     * and we hand back a tiny buffer that callers happily overrun.
     */
    if (size > (size_t)URUS_MAX_ARENA_ALLOC) {
        urus_abort_oom("refusing arena_alloc — exceeds 256 MiB per-call cap");
    }
    if (a->total_allocated > (size_t)URUS_MAX_ARENA_TOTAL) {
        urus_abort_oom("refusing arena_alloc — arena exceeds 512 MiB total cap");
    }
    /* 8-byte align */
    size = (size + 7u) & ~(size_t)7u;
    if (!a->head || a->head->used + size > a->head->cap) {
        size_t need = size > a->chunk_size ? size * 2 : a->chunk_size;
        ArenaChunk *c = new_chunk(need);
        c->next = a->head;
        a->head = c;
    }
    void *p = (char *)(a->head + 1) + a->head->used;
    a->head->used      += size;
    a->total_allocated += size;
    return p;
}

void *arena_alloc_zero(Arena *a, size_t size) {
    void *p = arena_alloc(a, size);
    memset(p, 0, size);
    return p;
}

char *arena_strdup(Arena *a, const char *s) {
    size_t n = strlen(s);
    char  *c = (char *)arena_alloc(a, n + 1);
    memcpy(c, s, n);
    c[n] = '\0';
    return c;
}

char *arena_strndup(Arena *a, const char *s, size_t n) {
    char *c = (char *)arena_alloc(a, n + 1);
    memcpy(c, s, n);
    c[n] = '\0';
    return c;
}

void arena_free(Arena *a) {
    ArenaChunk *c = a->head;
    while (c) {
        ArenaChunk *next = c->next;
        free(c);
        c = next;
    }
    a->head            = NULL;
    a->total_allocated = 0;
}
