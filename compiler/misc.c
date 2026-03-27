/*
 * Copyright 2026 Urus Foundation (https://github.com/Urus-Foundation)
 *
 * This file is part of the Urus Programming Language.
 * For more about this language check at
 *
 *    https://github.com/Urus-Foundatation/Urus
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "urusc.h"

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
