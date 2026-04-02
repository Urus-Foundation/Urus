/*
 * Copyright 2026 Urus Foundation (https://github.com/Urus-Foundation)
 *
 * Urus Package Manager — handles urus.toml, dependency resolution,
 * and project scaffolding.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir_p(path) _mkdir(path)
#else
#include <unistd.h>
#define mkdir_p(path) mkdir(path, 0755)
#endif

// ---- TOML-like parser (minimal subset for urus.toml) ----

typedef struct {
    char *name;
    char *value;
} TomlEntry;

typedef struct {
    char *section;
    TomlEntry *entries;
    int count;
    int cap;
} TomlSection;

typedef struct {
    TomlSection *sections;
    int count;
    int cap;
} TomlFile;

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r'))
        *end-- = '\0';
    return s;
}

static char *strip_quotes(char *s) {
    size_t len = strlen(s);
    if (len >= 2 && s[0] == '"' && s[len-1] == '"') {
        s[len-1] = '\0';
        return s + 1;
    }
    return s;
}

static TomlFile *toml_parse(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    TomlFile *toml = calloc(1, sizeof(TomlFile));
    toml->cap = 4;
    toml->sections = calloc((size_t)toml->cap, sizeof(TomlSection));

    // Add default section
    toml->sections[0].section = strdup("");
    toml->sections[0].cap = 8;
    toml->sections[0].entries = calloc(8, sizeof(TomlEntry));
    toml->count = 1;

    int current = 0;
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        char *t = trim(line);
        if (*t == '\0' || *t == '#') continue;

        // Section header: [name]
        if (*t == '[') {
            char *end = strchr(t, ']');
            if (!end) continue;
            *end = '\0';
            char *name = t + 1;

            if (toml->count >= toml->cap) {
                toml->cap *= 2;
                toml->sections = realloc(toml->sections,
                    sizeof(TomlSection) * (size_t)toml->cap);
            }
            current = toml->count++;
            toml->sections[current].section = strdup(name);
            toml->sections[current].cap = 8;
            toml->sections[current].entries = calloc(8, sizeof(TomlEntry));
            toml->sections[current].count = 0;
            continue;
        }

        // Key = value
        char *eq = strchr(t, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = trim(t);
        char *val = trim(eq + 1);
        val = strip_quotes(val);

        TomlSection *sec = &toml->sections[current];
        if (sec->count >= sec->cap) {
            sec->cap *= 2;
            sec->entries = realloc(sec->entries,
                sizeof(TomlEntry) * (size_t)sec->cap);
        }
        sec->entries[sec->count].name = strdup(key);
        sec->entries[sec->count].value = strdup(val);
        sec->count++;
    }

    fclose(f);
    return toml;
}

static const char *toml_get(TomlFile *toml, const char *section, const char *key) {
    for (int i = 0; i < toml->count; i++) {
        if (strcmp(toml->sections[i].section, section) == 0) {
            for (int j = 0; j < toml->sections[i].count; j++) {
                if (strcmp(toml->sections[i].entries[j].name, key) == 0)
                    return toml->sections[i].entries[j].value;
            }
        }
    }
    return NULL;
}

static TomlSection *toml_get_section(TomlFile *toml, const char *section) {
    for (int i = 0; i < toml->count; i++) {
        if (strcmp(toml->sections[i].section, section) == 0)
            return &toml->sections[i];
    }
    return NULL;
}

static void toml_free(TomlFile *toml) {
    if (!toml) return;
    for (int i = 0; i < toml->count; i++) {
        free(toml->sections[i].section);
        for (int j = 0; j < toml->sections[i].count; j++) {
            free(toml->sections[i].entries[j].name);
            free(toml->sections[i].entries[j].value);
        }
        free(toml->sections[i].entries);
    }
    free(toml->sections);
    free(toml);
}

// ---- Package commands ----

static bool file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static int pkg_init(const char *name) {
    if (file_exists("urus.toml")) {
        fprintf(stderr, "Error: urus.toml already exists in this directory\n");
        return 1;
    }

    const char *pkg_name = name ? name : "my-project";

    FILE *f = fopen("urus.toml", "w");
    if (!f) {
        fprintf(stderr, "Error: cannot create urus.toml\n");
        return 1;
    }
    fprintf(f,
        "[package]\n"
        "name = \"%s\"\n"
        "version = \"0.1.0\"\n"
        "authors = []\n"
        "\n"
        "[dependencies]\n",
        pkg_name);
    fclose(f);

    // Create src directory and main.urus
    mkdir_p("src");
    if (!file_exists("src/main.urus")) {
        f = fopen("src/main.urus", "w");
        if (f) {
            fprintf(f,
                "fn main(): void {\n"
                "    print(\"Hello from %s!\");\n"
                "}\n",
                pkg_name);
            fclose(f);
        }
    }

    printf("Created new Urus project '%s'\n", pkg_name);
    printf("  urus.toml\n");
    printf("  src/main.urus\n");
    return 0;
}

static int pkg_add(const char *dep_name, const char *version) {
    if (!file_exists("urus.toml")) {
        fprintf(stderr, "Error: no urus.toml found. Run 'urusc pkg init' first\n");
        return 1;
    }

    if (!dep_name) {
        fprintf(stderr, "Error: package name required. Usage: urusc pkg add <name> [version]\n");
        return 1;
    }

    const char *ver = version ? version : "*";

    // Read existing file
    FILE *f = fopen("urus.toml", "r");
    if (!f) {
        fprintf(stderr, "Error: cannot read urus.toml\n");
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *content = malloc((size_t)size + 1);
    size_t read_bytes = fread(content, 1, (size_t)size, f);
    content[read_bytes] = '\0';
    fclose(f);

    // Check if dependency already exists
    char search[256];
    snprintf(search, sizeof(search), "%s =", dep_name);
    if (strstr(content, search)) {
        fprintf(stderr, "Error: '%s' is already in dependencies\n", dep_name);
        free(content);
        return 1;
    }

    // Find [dependencies] section and append
    char *dep_section = strstr(content, "[dependencies]");
    if (!dep_section) {
        // Add section
        f = fopen("urus.toml", "a");
        if (f) {
            fprintf(f, "\n[dependencies]\n%s = \"%s\"\n", dep_name, ver);
            fclose(f);
        }
    } else {
        // Find end of [dependencies] section
        char *next_section = strstr(dep_section + 14, "\n[");
        f = fopen("urus.toml", "w");
        if (f) {
            if (next_section) {
                fwrite(content, 1, (size_t)(next_section - content), f);
                fprintf(f, "%s = \"%s\"\n", dep_name, ver);
                fputs(next_section, f);
            } else {
                fputs(content, f);
                fprintf(f, "%s = \"%s\"\n", dep_name, ver);
            }
            fclose(f);
        }
    }

    free(content);
    printf("Added %s = \"%s\" to dependencies\n", dep_name, ver);
    return 0;
}

static int pkg_install(void) {
    if (!file_exists("urus.toml")) {
        fprintf(stderr, "Error: no urus.toml found. Run 'urusc pkg init' first\n");
        return 1;
    }

    TomlFile *toml = toml_parse("urus.toml");
    if (!toml) {
        fprintf(stderr, "Error: cannot parse urus.toml\n");
        return 1;
    }

    const char *name = toml_get(toml, "package", "name");
    const char *version = toml_get(toml, "package", "version");
    printf("Installing dependencies for %s@%s\n",
           name ? name : "unknown",
           version ? version : "0.0.0");

    // Create urus_modules directory
    mkdir_p("urus_modules");

    TomlSection *deps = toml_get_section(toml, "dependencies");
    if (!deps || deps->count == 0) {
        printf("No dependencies to install.\n");
        toml_free(toml);
        return 0;
    }

    int installed = 0;
    for (int i = 0; i < deps->count; i++) {
        const char *dep = deps->entries[i].name;
        const char *ver = deps->entries[i].value;

        printf("  Installing %s@%s...", dep, ver);

        // Check if it's a git dependency (starts with http/git)
        if (strncmp(ver, "http", 4) == 0 || strncmp(ver, "git@", 4) == 0) {
            // Git clone
            char cmd[1024];
            char dest[256];
            snprintf(dest, sizeof(dest), "urus_modules/%s", dep);
            if (file_exists(dest)) {
                printf(" already installed\n");
                installed++;
                continue;
            }
            snprintf(cmd, sizeof(cmd), "git clone --depth 1 %s %s 2>&1", ver, dest);
            int ret = system(cmd);
            if (ret != 0) {
                printf(" FAILED\n");
                fprintf(stderr, "  Error: git clone failed for %s\n", dep);
            } else {
                printf(" ok\n");
                installed++;
            }
        } else {
            // Registry package — check stdlib first
            char stdlib_path[256];
            snprintf(stdlib_path, sizeof(stdlib_path), "urus_modules/%s.urus", dep);
            if (file_exists(stdlib_path)) {
                printf(" already installed\n");
                installed++;
                continue;
            }

            // Check if it's a builtin stdlib module
            char builtin_check[256];
            snprintf(builtin_check, sizeof(builtin_check), "compiler/stdlib/%s.urus", dep);
            if (file_exists(builtin_check)) {
                // Copy from stdlib
                char dest[256];
                snprintf(dest, sizeof(dest), "urus_modules/%s.urus", dep);
                FILE *src = fopen(builtin_check, "r");
                FILE *dst = fopen(dest, "w");
                if (src && dst) {
                    char buf[4096];
                    size_t n;
                    while ((n = fread(buf, 1, sizeof(buf), src)) > 0)
                        fwrite(buf, 1, n, dst);
                    printf(" ok (stdlib)\n");
                    installed++;
                } else {
                    printf(" FAILED\n");
                }
                if (src) fclose(src);
                if (dst) fclose(dst);
            } else {
                printf(" not found (registry not available yet)\n");
            }
        }
    }

    printf("\nInstalled %d/%d dependencies\n", installed, deps->count);

    // Generate urus_modules.lock
    FILE *lock = fopen("urus.lock", "w");
    if (lock) {
        fprintf(lock, "# This file is auto-generated by urusc pkg install\n");
        fprintf(lock, "# Do not edit manually\n\n");
        for (int i = 0; i < deps->count; i++) {
            fprintf(lock, "%s = \"%s\"\n",
                    deps->entries[i].name, deps->entries[i].value);
        }
        fclose(lock);
    }

    toml_free(toml);
    return 0;
}

static int pkg_list(void) {
    if (!file_exists("urus.toml")) {
        fprintf(stderr, "Error: no urus.toml found\n");
        return 1;
    }

    TomlFile *toml = toml_parse("urus.toml");
    if (!toml) {
        fprintf(stderr, "Error: cannot parse urus.toml\n");
        return 1;
    }

    const char *name = toml_get(toml, "package", "name");
    const char *version = toml_get(toml, "package", "version");
    printf("%s@%s\n", name ? name : "unknown", version ? version : "0.0.0");

    TomlSection *deps = toml_get_section(toml, "dependencies");
    if (deps && deps->count > 0) {
        printf("\nDependencies:\n");
        for (int i = 0; i < deps->count; i++) {
            printf("  %s = \"%s\"\n", deps->entries[i].name, deps->entries[i].value);
        }
    } else {
        printf("\nNo dependencies\n");
    }

    toml_free(toml);
    return 0;
}

// ---- Public entry point ----

int pkg_main(int argc, char **argv) {
    if (argc < 3) {
        printf(
            "Urus Package Manager\n\n"
            "Usage: urusc pkg <command> [args]\n\n"
            "Commands:\n"
            "  init [name]      Create a new Urus project\n"
            "  add <pkg> [ver]  Add a dependency\n"
            "  install          Install all dependencies\n"
            "  list             List project dependencies\n"
        );
        return 0;
    }

    const char *cmd = argv[2];

    if (strcmp(cmd, "init") == 0) {
        return pkg_init(argc > 3 ? argv[3] : NULL);
    }
    if (strcmp(cmd, "add") == 0) {
        return pkg_add(argc > 3 ? argv[3] : NULL,
                       argc > 4 ? argv[4] : NULL);
    }
    if (strcmp(cmd, "install") == 0) {
        return pkg_install();
    }
    if (strcmp(cmd, "list") == 0) {
        return pkg_list();
    }

    fprintf(stderr, "Unknown package command: %s\n", cmd);
    return 1;
}
