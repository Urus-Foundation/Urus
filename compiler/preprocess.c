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

// --- Import resolution ---

// Track imported files to detect circular imports
#define MAX_IMPORTS 64
static const char *imported_files[MAX_IMPORTS];
static int import_count = 0;

static bool already_imported(const char *path)
{
    for (int i = 0; i < import_count; i++) {
        if (strcmp(path, imported_files[i]) == 0)
            return true;
    }
    return false;
}

// TODO: Add check if path is same with URUSCPATH even though is using "../"
static bool is_path_allowed(char *path)
{
    char *p = path;
    while (*p) {
        while (*p == '/' || *p == '\\')
            p++;
        const char *start = p;
        while (*p && *p != '/' && *p != '\\')
            p++;
        size_t len = (size_t)(p - start);
        if (len == 2 && start[0] == '.' && start[1] == '.')
            return false;
    }
    return true;
}

void get_local_libpath(char *out, size_t size)
{

#if defined(_WIN32)
    const char *prefix = getenv("URUSCPATH");
    if (prefix) {
        snprintf(out, size, "%s", prefix);
    } else {
        const char *drive = getenv("SystemDrive");
        if (!drive)
            drive = "C:";
        snprintf(out, size, "%s\\Program Files\\Urusc\\Lib", drive);
    }

#elif defined(__ANDROID__)
    // Android / Termux
    const char *termux = getenv("PREFIX");
    if (termux) {
        snprintf(out, size, "%s/lib/urusc", termux);
    } else {
        // Android native (not Termux)
        snprintf(out, size, "/system/lib/urusc");
    }

#elif defined(__linux__)
    const char *prefix = getenv("URUSCPATH");
    if (prefix) {
        snprintf(out, size, "%s", prefix);
    } else {
        char local_path[512];
        snprintf(local_path, sizeof(local_path), "/usr/local/lib/urusc");

        // if local exists = the user is building the compiler itself
        FILE *f = fopen(local_path, "r");
        if (f) {
            fclose(f);
            snprintf(out, size, "%s", local_path);
        } else {
            snprintf(out, size, "/usr/lib/urusc");
        }
    }

#else
    // fallback POSIX generic
    snprintf(out, size, "/usr/local/lib/urusc");
#endif
}

// resolve library import path
static char *resolve_stdlib_path(const char *module_name)
{
    char urus_path[PATH_MAX];
    get_local_libpath(urus_path, sizeof(urus_path));

    size_t base_len = strlen(urus_path);
    size_t name_len = strlen(module_name);
    // +1 for sep, +5 for ".urus", +1 for '\0'
    size_t total = base_len + 1 + name_len + 5 + 1;
    char *full = xmalloc(total);
    snprintf(full, total, "%s%c%s.urus", urus_path, URUSC_PATHSEP, module_name);

    return full;
}

// resolve relative import path
static char *resolve_import_path(const char *base_file, const char *import_path)
{
    // Find last / or backslash in base_file
    const char *last_sep = NULL;
    for (const char *p = base_file; *p; p++) {
        if (*p == '/' || *p == '\\')
            last_sep = p;
    }

    if (!last_sep) {
        return strdup(import_path);
    }

    size_t dir_len = (size_t)(last_sep - base_file + 1);
    size_t imp_len = strlen(import_path);
    char *full = xmalloc(dir_len + imp_len + 1);
    memcpy(full, base_file, dir_len);
    memcpy(full + dir_len, import_path, imp_len);
    full[dir_len + imp_len] = '\0';
    return full;
}

bool preprocess_imports(AstNode *program, const char *base_file)
{
    if (import_count >= MAX_IMPORTS) {
        fprintf(stderr, "Error: too many imports (max %d)\n", MAX_IMPORTS);
        return false;
    }

    // Mark base file as imported (to prevent circular self-import)
    imported_files[import_count++] = base_file;

    for (int i = 0; i < program->as.program.decl_count; i++) {
        AstNode *d = program->as.program.decls[i];
        if (d->kind != NODE_IMPORT)
            continue;

        char *path;

        if (d->as.import_decl.is_stdlib) {
            path = resolve_stdlib_path(d->as.import_decl.path);
        } else {
            path = resolve_import_path(base_file, d->as.import_decl.path);
            if (!is_path_allowed(path)) {
                fprintf(stderr,
                        "Error: import path '%s' resolves outside allowed "
                        "directories\n",
                        d->as.import_decl.path);
                xfree(path);
                return false;
            }
        }

        if (already_imported(path)) {
            xfree(path);
            continue;
        }

        if (import_count + 1 >= MAX_IMPORTS) {
            fprintf(stderr, "Error: too many imports (max %d)\n", MAX_IMPORTS);
            xfree(path);
            return false;
        }
        imported_files[import_count++] = path;

        size_t len;
        char *source = read_file(path, &len);
        if (!source) {
            fprintf(stderr, "Error: cannot import '%s'\n",
                    d->as.import_decl.path);
            if (d->as.import_decl.is_stdlib)
                fprintf(stderr, "Tip: make sure you've installed urus stdlib "
                                "correctly in your environment\n");
            return false;
        }

        Lexer lexer;
        lexer_init(&lexer, source, len);
        int token_count;
        Token *tokens = lexer_tokenize(&lexer, &token_count);
        if (!tokens) {
            xfree(source);
            return false;
        }

        Parser parser;
        parser.filename = path;
        parser_init(&parser, tokens, token_count);
        AstNode *imported = parser_parse(&parser);

        if (parser.had_error) {
            fprintf(stderr, "Error parsing imported file '%s'\n", path);
            ast_free(imported);
            xfree(tokens);
            xfree(source);
            return false;
        }

        // Recursively process imports in the imported file
        if (!preprocess_imports(imported, path)) {
            ast_free(imported);
            xfree(tokens);
            xfree(source);
            return false;
        }

        // Merge imported declarations into program (insert before current
        // position)
        int new_count = program->as.program.decl_count +
                        imported->as.program.decl_count - 1;
        AstNode **new_decls =
            xmalloc(sizeof(AstNode *) * (size_t)(new_count + 1));

        int pos = 0;
        // Copy declarations before the import statement
        for (int j = 0; j < i; j++) {
            new_decls[pos++] = program->as.program.decls[j];
        }
        // Insert imported declarations (skip imports from imported file)
        for (int j = 0; j < imported->as.program.decl_count; j++) {
            if (imported->as.program.decls[j]->kind != NODE_IMPORT) {
                imported->as.program.decls[j]->is_imported = true;
                new_decls[pos++] = imported->as.program.decls[j];
                imported->as.program.decls[j] = NULL; // transfer ownership
            }
        }
        // Skip the import statement itself
        // Copy declarations after the import
        for (int j = i + 1; j < program->as.program.decl_count; j++) {
            new_decls[pos++] = program->as.program.decls[j];
        }

        xfree(program->as.program.decls);
        program->as.program.decls = new_decls;
        program->as.program.decl_count = pos;

        // Don't free imported->decls since we transferred ownership
        xfree(imported->as.program.decls);
        imported->as.program.decls = NULL;
        imported->as.program.decl_count = 0;
        ast_free(imported);
        xfree(tokens);
        // Note: source memory is borrowed by tokens, don't free yet

        // Re-scan from beginning since we modified the array
        i = -1;
    }
    return true;
}
