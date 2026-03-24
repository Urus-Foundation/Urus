#include "preprocess.h"
#include "lexer.h"
#include "parser.h"
#include "util.h"
#include "ast.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Import resolution ---

// Track imported files to detect circular imports
#define MAX_IMPORTS 64
static const char *imported_files[MAX_IMPORTS];
static int import_count = 0;

static bool already_imported(const char *path) {
    for (int i = 0; i < import_count; i++) {
        if (strcmp(path, imported_files[i]) == 0) return true;
    }
    return false;
}

void get_local_libpath(char *out, size_t size) {

#if defined(_WIN32)
    // Windows, check registry or fallback to default
    const char *drive = getenv("SystemDrive");
    if (!drive) drive = "C:";
    snprintf(out, size, "%s\\Urusc\\Lib", drive);

#elif defined(__ANDROID__)
    // Android / Termux
    const char *termux = getenv("PREFIX");
    if (termux) {
        snprintf(out, size, "%s/lib/urusc", termux);
    } else {
        // Android native (not Termux)
        snprintf(out, size, "/system/lib/urusc");
    }

#elif defined(__APPLE__)
    // macOS check Homebrew ARM, Homebrew Intel, and then fallback
    const char *homebrew = getenv("HOMEBREW_PREFIX");
    if (homebrew) {
        snprintf(out, size, "%s/lib/urusc", homebrew);
    } else {
        // fallback to Apple system
        snprintf(out, size, "/usr/lib/urusc");
    }

#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    // BSD
    snprintf(out, size, "/usr/local/lib/urusc");

#elif defined(__linux__)
    const char *prefix = getenv("URUSCPATH");
    if (prefix) {
        snprintf(out, size, "%s", prefix);
    } else {
        char local_path[512];
        snprintf(local_path, sizeof(local_path),
                 "/usr/local/lib/urusc");

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
static char *resolve_stdlib_path(const char *module_name) {
    char urus_path[PATH_MAX];
    get_local_libpath(urus_path, sizeof(urus_path));

    size_t base_len = strlen(urus_path);
    size_t name_len = strlen(module_name);
    // +1 for sep, +5 for ".urus", +1 for '\0'
    char *full = malloc(base_len + 1 + name_len + 5 + 1);
    sprintf(full, "%s%c%s.urus", urus_path, URUSC_PATHSEP, module_name);

    return full;
}

// resolve relative import path
static char *resolve_import_path(const char *base_file, const char *import_path) {
    // Find last / or backslash in base_file
    const char *last_sep = NULL;
    for (const char *p = base_file; *p; p++) {
        if (*p == '/' || *p == '\\') last_sep = p;
    }

    if (!last_sep) {
        return strdup(import_path);
    }

    size_t dir_len = (size_t)(last_sep - base_file + 1);
    size_t imp_len = strlen(import_path);
    char *full = malloc(dir_len + imp_len + 1);
    memcpy(full, base_file, dir_len);
    memcpy(full + dir_len, import_path, imp_len);
    full[dir_len + imp_len] = '\0';
    return full;
}

bool preprocess_imports(AstNode *program, const char *base_file) {
    // Mark base file as imported (to prevent circular self-import)
    imported_files[import_count++] = base_file;

    for (int i = 0; i < program->as.program.decl_count; i++) {
        AstNode *d = program->as.program.decls[i];
        if (d->kind != NODE_IMPORT) continue;

        char *path;

        if (d->as.import_decl.is_stdlib) {
            path = resolve_stdlib_path(d->as.import_decl.path);
        } else {
            path = resolve_import_path(base_file, d->as.import_decl.path);
        }

        if (already_imported(path)) {
            free(path);
            continue;
        }

        if (import_count >= MAX_IMPORTS) {
            fprintf(stderr, "Error: too many imports (max %d)\n", MAX_IMPORTS);
            free(path);
            return false;
        }
        imported_files[import_count++] = path;

        size_t len;
        char *source = read_file(path, &len);
        if (!source) {
            fprintf(stderr, "Error: cannot import '%s'\n", d->as.import_decl.path);
            return false;
        }

        Lexer lexer;
        lexer_init(&lexer, source, len);
        int token_count;
        Token *tokens = lexer_tokenize(&lexer, &token_count);
        if (!tokens) {
            free(source);
            return false;
        }

        Parser parser;
        parser.filename = path;
        parser_init(&parser, tokens, token_count);
        AstNode *imported = parser_parse(&parser);

        if (parser.had_error) {
            fprintf(stderr, "Error parsing imported file '%s'\n", path);
            ast_free(imported);
            free(tokens);
            free(source);
            return false;
        }

        // Recursively process imports in the imported file
        if (!preprocess_imports(imported, path)) {
            ast_free(imported);
            free(tokens);
            free(source);
            return false;
        }

        // Merge imported declarations into program (insert before current position)
        int new_count = program->as.program.decl_count + imported->as.program.decl_count - 1;
        AstNode **new_decls = malloc(sizeof(AstNode *) * (size_t)(new_count + 1));

        int pos = 0;
        // Copy declarations before the import statement
        for (int j = 0; j < i; j++) {
            new_decls[pos++] = program->as.program.decls[j];
        }
        // Insert imported declarations (skip imports from imported file)
        for (int j = 0; j < imported->as.program.decl_count; j++) {
            if (imported->as.program.decls[j]->kind != NODE_IMPORT) {
                new_decls[pos++] = imported->as.program.decls[j];
                imported->as.program.decls[j] = NULL; // transfer ownership
            }
        }
        // Skip the import statement itself
        // Copy declarations after the import
        for (int j = i + 1; j < program->as.program.decl_count; j++) {
            new_decls[pos++] = program->as.program.decls[j];
        }

        free(program->as.program.decls);
        program->as.program.decls = new_decls;
        program->as.program.decl_count = pos;

        // Don't free imported->decls since we transferred ownership
        free(imported->as.program.decls);
        imported->as.program.decls = NULL;
        imported->as.program.decl_count = 0;
        ast_free(imported);
        free(tokens);
        // Note: source memory is borrowed by tokens, don't free yet

        // Re-scan from beginning since we modified the array
        i = -1;
    }
    return true;
}
