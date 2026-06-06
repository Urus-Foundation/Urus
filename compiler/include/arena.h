/*
 * Bump-allocator arena.
 * All AST nodes, symbols, and intermediate strings come from arenas so that
 * cleanup is a single arena_free() call. Eliminates a class of leak bugs and
 * is dramatically faster than per-node malloc/free.
 */
#ifndef URUS_ARENA_H
#define URUS_ARENA_H

#include "urus_common.h"

typedef struct ArenaChunk ArenaChunk;

typedef struct Arena {
    ArenaChunk *head;
    size_t      chunk_size;
    size_t      total_allocated;
} Arena;

void  arena_init(Arena *a, size_t chunk_size);
void *arena_alloc(Arena *a, size_t size);
void *arena_alloc_zero(Arena *a, size_t size);
char *arena_strdup(Arena *a, const char *s);
char *arena_strndup(Arena *a, const char *s, size_t n);
void  arena_free(Arena *a);

#endif
