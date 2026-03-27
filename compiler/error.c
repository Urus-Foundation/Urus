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

static void _diag(const char *filename, Token *t)
{
    FILE *f = fopen(filename, "r");
    if (f) {
        char buffer[4096];
        for (int i = 1; i <= t->line; i++) {
            if (!fgets(buffer, sizeof(buffer), f))
                break;
            if (i == t->line) {
                /* remove trailing newline if present */
                size_t len = strlen(buffer);
                if (len > 0 && buffer[len - 1] == '\n')
                    buffer[len - 1] = '\0';

                fprintf(stderr, "  %-5d | %s\n", t->line, buffer);
                fprintf(stderr, "        | ");
                for (int j = 1; j < t->col; j++) {
                    // Print as the real colwidth
                    if (buffer[j - 1] != '\t') {
                        fprintf(stderr, " ");
                    } else {
                        fprintf(stderr, "\t");
                    }
                }
                fprintf(stderr, "\033[1;32m");

                for (size_t k = 0; k < t->length; k++)
                    fprintf(stderr, "^");
                fprintf(stderr, "\033[0m\n");
            }
        }
        fclose(f);
    }
}

void report_error(const char *filename, Token *t, const char *msg)
{
    fprintf(stderr, "\x1b[1m%s:", filename);
    if (t)
        fprintf(stderr, "%d:%d", t->line, t->col);
    fprintf(stderr, " \x1b[31mError\x1b[0m: %s\n", msg);
    if (t)
        _diag(filename, t);
}

void report_warn(const char *filename, Token *t, const char *msg)
{
    fprintf(stderr, "\x1b[1m%s:", filename);
    if (t)
        fprintf(stderr, "%d:%d", t->line, t->col);
    fprintf(stderr, " \x1b[35mWarning\x1b[0m: %s\n", msg);
    if (t)
        _diag(filename, t);
}

void report(const char *filename, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "\x1b[1m%s: \x1b[0m", filename);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
}
