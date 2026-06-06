/*
 * urusc — the URUS reference compiler v0.0.1
 *
 *   urusc <input.urus> [--emit-c|--emit-tokens|--emit-ast] [-o file]
 *
 * Default emit is C source, written to <input>.c.
 */
#include "urus_common.h"
#include "diag.h"
#include "arena.h"
#include "lexer.h"
#include "parser.h"
#include "sema.h"
#include "codegen_c.h"
#include "strbuf.h"
#include "compile.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/*
 * Hard upper bound on a single input source.  Anything larger is almost
 * certainly an attack (or an honest accident — but either way we refuse
 * to feed it to the lexer).  Tier-0 security item #11.
 */
#define URUS_MAX_INPUT_BYTES (64ull * 1024ull * 1024ull)

typedef enum {
    EMIT_C,
    EMIT_TOKENS,
    EMIT_AST,
} EmitKind;

/*
 * Read a regular file fully into a heap-allocated, null-terminated buffer.
 *
 * Hardening compared to the v0.0.1-b001 version:
 *   - stat()s the path first and refuses non-regular files (no /dev/zero,
 *     /proc/self/mem, named pipes, …).  Closes F-MEM-10 partially.
 *   - Enforces URUS_MAX_INPUT_BYTES.  Closes Tier-0 item #11.
 *   - Zeroes the tail of the buffer if fread returned a short count, so
 *     the lexer's "scan until \0" assumption is never fed uninitialised
 *     memory.  Closes F-MEM-5.
 */
/* UTF-8 validation lives in compile.c since b023 so the fuzz target and
 * the CLI share one implementation — see urus_utf8_first_bad_offset. */

/* Runtime-overridable input cap (Tier-0 #11 follow-up, b020).  Defaults to
 * URUS_MAX_INPUT_BYTES; `--max-input-bytes N` (with optional K/M/G suffix)
 * raises or lowers it.  The override is still bounded above at 1 GiB —
 * the flag exists for generated-code users with legitimately large single
 * files, not as an escape hatch to disable the cap entirely. */
#define URUS_INPUT_CAP_CEILING (1024ull * 1024ull * 1024ull)
static unsigned long long g_max_input_bytes = URUS_MAX_INPUT_BYTES;

static bool parse_size_arg(const char *s, unsigned long long *out) {
    if (!s || !*s) return false;
    char *end = NULL;
    errno = 0;
    unsigned long long v = strtoull(s, &end, 10);
    if (errno == ERANGE || end == s) return false;
    if (*end == 'K' || *end == 'k')      { v *= 1024ull; end++; }
    else if (*end == 'M' || *end == 'm') { v *= 1024ull * 1024ull; end++; }
    else if (*end == 'G' || *end == 'g') { v *= 1024ull * 1024ull * 1024ull; end++; }
    if (*end != '\0') return false;
    if (v == 0 || v > URUS_INPUT_CAP_CEILING) return false;
    *out = v;
    return true;
}

static char *read_file(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        fprintf(stderr, "urusc: cannot stat '%s'\n", path);
        return NULL;
    }
    if (!(st.st_mode & S_IFREG)) {
        fprintf(stderr, "urusc: '%s' is not a regular file\n", path);
        return NULL;
    }
    if ((unsigned long long)st.st_size > g_max_input_bytes) {
        fprintf(stderr,
                "urusc: '%s' is %llu bytes; the limit is %llu bytes "
                "(raise with --max-input-bytes, ceiling 1G)\n",
                path,
                (unsigned long long)st.st_size,
                g_max_input_bytes);
        return NULL;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "urusc: cannot open '%s'\n", path);
        return NULL;
    }

    size_t n = (size_t)st.st_size;
    char  *buf = (char *)malloc(n + 1);
    if (!buf) { fclose(f); return NULL; }

    size_t got = fread(buf, 1, n, f);
    fclose(f);

    /* Short read?  Treat the rest as zero so the lexer never reads garbage. */
    if (got < n) {
        memset(buf + got, 0, n - got);
    }
    buf[n] = '\0';

    /* UTF-8 well-formedness check (Tier-1, b016).  Diagnostic prints the
     * 1-based file offset of the first malformed byte so editors can
     * jump there directly. */
    size_t bad = urus_utf8_first_bad_offset((const unsigned char *)buf, got);
    if (bad != 0) {
        fprintf(stderr,
                "urusc: '%s' is not valid UTF-8 — bad byte at offset %llu\n",
                path, (unsigned long long)(bad - 1));
        free(buf);
        return NULL;
    }
    return buf;
}

static void print_usage(FILE *out) {
    fputs(
        "urusc " URUS_VERSION " — URUS reference compiler\n"
        "\n"
        "Usage: urusc <input.urus> [options]\n"
        "\n"
        "Options:\n"
        "  --emit-c        Emit C source (default).\n"
        "  --tokens        Dump the token stream.\n"
        "  --ast           Dump the parsed AST.\n"
        "  -o <file>       Write output to <file> instead of <input>.c.\n"
        "  --max-input-bytes N[K|M|G]\n"
        "                  Override the input size cap (default 64M, ceiling 1G).\n"
        "  --version       Print version and exit.\n"
        "  -h, --help      Print this help and exit.\n"
        "\n"
        "Compatibility aliases (deprecated, removed in v0.0.3):\n"
        "  --emit-tokens   == --tokens\n"
        "  --emit-ast      == --ast\n"
        "\n"
        "Note: emitted C requires a C11 compiler with statement-expression\n"
        "support — GCC, Clang, or clang-cl. Native MSVC (cl.exe) is rejected.\n",
        out);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(stderr);
        return 1;
    }

    const char *input = NULL;
    const char *output = NULL;
    EmitKind emit = EMIT_C;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--emit-c") == 0)              emit = EMIT_C;
        else if (strcmp(a, "--tokens") == 0)         emit = EMIT_TOKENS;
        else if (strcmp(a, "--ast") == 0)            emit = EMIT_AST;
        /* deprecated aliases */
        else if (strcmp(a, "--emit-tokens") == 0) {
            fprintf(stderr, "urusc: warning: '--emit-tokens' is deprecated; use '--tokens'\n");
            emit = EMIT_TOKENS;
        }
        else if (strcmp(a, "--emit-ast") == 0) {
            fprintf(stderr, "urusc: warning: '--emit-ast' is deprecated; use '--ast'\n");
            emit = EMIT_AST;
        }
        else if (strcmp(a, "-o") == 0 && i + 1 < argc) output = argv[++i];
        else if (strcmp(a, "--max-input-bytes") == 0 && i + 1 < argc) {
            if (!parse_size_arg(argv[++i], &g_max_input_bytes)) {
                fprintf(stderr,
                        "urusc: invalid --max-input-bytes value '%s' "
                        "(want N, NK, NM, or NG; 1..1G)\n", argv[i]);
                return 1;
            }
        }
        else if (strcmp(a, "--version") == 0) {
            printf("urusc %s\n", URUS_VERSION);
            return 0;
        } else if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            print_usage(stdout);
            return 0;
        } else if (a[0] == '-') {
            fprintf(stderr, "urusc: unknown option '%s'\n", a);
            return 1;
        } else {
            if (input) {
                fprintf(stderr, "urusc: only one input file supported in v" URUS_VERSION "\n");
                return 1;
            }
            input = a;
        }
    }

    if (!input) {
        print_usage(stderr);
        return 1;
    }

    char *source = read_file(input);
    if (!source) return 1;

    DiagCtx diag;
    diag_init(&diag, input, source);

    Arena arena;
    arena_init(&arena, 256 * 1024);

    /* ---- token-dump path ---- */
    if (emit == EMIT_TOKENS) {
        Lexer lx;
        lexer_init(&lx, source, input, &diag, &arena);
        for (;;) {
            Token t = lexer_next(&lx);
            printf("%4u:%-3u %-12s '%.*s'\n",
                   t.loc.line, t.loc.col, tok_kind_name(t.kind),
                   (int)t.length, t.start);
            if (t.kind == TOK_EOF) break;
        }
        arena_free(&arena);
        free(source);
        return diag_has_errors(&diag) ? 1 : 0;
    }

    Parser p;
    parser_init(&p, source, input, &diag, &arena);
    Module *m = parser_parse_module(&p);

    if (diag_has_errors(&diag)) {
        fprintf(stderr, "urusc: aborting due to previous errors (%d)\n",
                diag.error_count);
        arena_free(&arena);
        free(source);
        return 1;
    }

    if (emit == EMIT_AST) {
        ast_print_module(m);
        arena_free(&arena);
        free(source);
        return 0;
    }

    /* Semantic analysis */
    sema_check(m, &diag);
    if (diag_has_errors(&diag)) {
        fprintf(stderr, "urusc: aborting due to %d error(s)\n", diag.error_count);
        arena_free(&arena);
        free(source);
        return 1;
    }

    /* Codegen */
    StrBuf sb;
    sb_init(&sb);
    codegen_c_emit(&sb, m, &diag);
    if (diag_has_errors(&diag)) {
        /* e.g. f-string placeholder validation (F-MEM-1) — never write a
         * poisoned artifact while returning success. */
        fprintf(stderr, "urusc: aborting due to %d error(s)\n", diag.error_count);
        sb_free(&sb);
        arena_free(&arena);
        free(source);
        return 1;
    }

    /* Determine output path */
    char default_out[1024];
    if (!output) {
        snprintf(default_out, sizeof(default_out), "%s.c", input);
        output = default_out;
    }

    FILE *of = fopen(output, "wb");
    if (!of) {
        fprintf(stderr, "urusc: cannot write '%s'\n", output);
        sb_free(&sb);
        arena_free(&arena);
        free(source);
        return 1;
    }
    fwrite(sb.data, 1, sb.len, of);
    fclose(of);
    fprintf(stderr, "urusc: wrote %s (%zu bytes)\n", output, sb.len);

    sb_free(&sb);
    arena_free(&arena);
    free(source);
    return 0;
}
