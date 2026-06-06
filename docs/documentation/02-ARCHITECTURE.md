# 02 — Architecture

## The pipeline in one picture

```
                                  ┌────────────────────────┐
                                  │   .urus source file     │
                                  └───────────┬────────────┘
                                              │   read_file()
                                              ▼
                              ┌──────────────────────────────────┐
                              │ source buffer (null-terminated)   │
                              └──────────────┬───────────────────┘
                                             │
                                             ▼
                              ┌──────────────────────────────────┐
                              │           lexer.c                 │
                              │  produces a stream of `Token`s    │
                              │  (one-token lookahead in parser)  │
                              └──────────────┬───────────────────┘
                                             │
                                             ▼
                              ┌──────────────────────────────────┐
                              │           parser.c                │
                              │  recursive-descent + Pratt        │
                              │  builds AST nodes from arena      │
                              └──────────────┬───────────────────┘
                                             │
                                             ▼
                              ┌──────────────────────────────────┐
                              │            ast.c                  │
                              │  arena-backed AST factories       │
                              └──────────────┬───────────────────┘
                                             │
                                             ▼
                              ┌──────────────────────────────────┐
                              │            sema.c                 │
                              │  symbol resolution + checks       │
                              │  (FNV-1a hashed scopes)           │
                              └──────────────┬───────────────────┘
                                             │
                                             ▼
                              ┌──────────────────────────────────┐
                              │         codegen_c.c               │
                              │  emit one self-contained C TU     │
                              │  to a `StrBuf`                    │
                              └──────────────┬───────────────────┘
                                             │
                                             ▼
                              ┌──────────────────────────────────┐
                              │       <input>.c written           │
                              └──────────────┬───────────────────┘
                                             │   you run gcc / clang
                                             ▼
                              ┌──────────────────────────────────┐
                              │   stdlib/runtime/urus_rt.h        │
                              │   linked into the produced binary │
                              └──────────────────────────────────┘
```

Cross-cutting helpers used by every stage:

- **`arena.c`** — bump allocator, owns all AST + intermediate string
  storage. Single-pass free: drop the arena, every AST node dies.
- **`strbuf.c`** — growing byte buffer, used by `codegen_c.c` to assemble
  the emitted C source.
- **`diag.c`** — position-aware diagnostics. Renders snippet + caret.

## The driver

`main.c` is the entry point. It does:

1. Parse CLI flags (`--emit-c`, `--tokens`, `--ast`, `-o`, `--version`,
   `--help`, and deprecated aliases).
2. Read the input file into a heap buffer (null-terminated).
3. Create one **`Arena`** that will own everything for this run.
4. Create one **`DiagCtx`** for diagnostics.
5. Lex → Parse → Sema → Codegen, threading `arena`, `diag`, and the
   source pointer through.
6. Write the output (`.c`, token dump, or AST dump).
7. Free the arena. Done.

No persistent state. No globals (other than the runtime's `urus_argc`/
`urus_argv`, which the *produced* binary uses, not the compiler).

## Data ownership

Every long-lived heap allocation is one of:

| Owner            | Lifetime                                  | Used for                              |
|------------------|-------------------------------------------|---------------------------------------|
| `Arena`          | one compilation run                        | every AST node, every interned string |
| `StrBuf`         | until written or `strbuf_free`             | growing the emitted C source          |
| `Lexer` internal | one `lex_string` call                      | a single string literal being built   |
| `DiagCtx`        | one compilation run                        | diagnostic vector                     |
| `main.c` input   | one compilation run                        | the source-file bytes                 |

There are exactly two `free()` sites in the compiler: `strbuf_free` and
the input-buffer free at the end of `main`. Everything else is bump-allocated
into the arena and reaped when the arena is destroyed.

This is the single biggest architectural choice. It is what makes the
compiler small, fast, and (mostly) crash-safe.

## Module dependency graph

```
            main.c
              │
   ┌──────────┼──────────────┬──────────────┐
   ▼          ▼              ▼              ▼
arena.c   diag.c          lexer.c        codegen_c.c
   ▲          ▲              │                │
   │          │              ▼                ▼
   │          └─────────  parser.c ────── sema.c
   │                         │                │
   └─────────────────────  ast.c              │
                              ▲               │
                              └───────────────┘
                       (strbuf.c used by codegen + diag)
```

There are no circular includes. `urus_common.h` is the bottom of the
dependency stack and may be included from anywhere.

## Failure model

The compiler treats *any* internal inconsistency as a programming bug,
not user error. The escalation order is:

1. **User error in the source** → emit a diagnostic via `diag_*`,
   continue parsing if possible (parser resyncs at item boundaries).
2. **Allocation failure** → `arena_alloc` aborts. The compiler does not
   try to recover from OOM.
3. **Internal invariant violation** → `assert()` or
   `fprintf(stderr, "internal: …"); abort()`.
4. **Unrecoverable parse cascade** → emit-then-exit, with the worst
   error first.

The compiler does **not** attempt to compile a program that produced
any diagnostic of severity ≥ error.

## Re-entrancy and threads

The compiler is **single-threaded by design**. There is no global mutex
because there is no global state to mutex. Two `urusc` processes can
run in parallel safely.

The *runtime* (`urus_rt.h`) keeps `g_urus_argc` / `g_urus_argv` as file
globals — single-threaded runtime in v0.0.1 (see
[the security audit](../security/SECURITY-AUDIT.md) F-MEM where this is
called out).

## Where to extend

When you add a new feature, you touch a predictable set of files:

| Feature kind            | Files                                                   |
|-------------------------|---------------------------------------------------------|
| New keyword              | `lexer.h`, `lexer.c`                                    |
| New AST node             | `ast.h` (enum + payload), `ast.c` (constructor)         |
| New statement / expr     | `parser.c` (parse_*), `ast.h`, `sema.c`, `codegen_c.c`  |
| New diagnostic           | `diag.c` (and the spot that triggers it)                |
| New CLI flag             | `main.c`                                                |
| New runtime function     | `stdlib/runtime/urus_rt.h`                              |
| New built-in symbol      | `sema.c` (prelude init)                                 |

The minimum-change set for a new statement is parser + ast + sema +
codegen + at least one test in `tests/run/`. Anything less is
incomplete.

— *Last updated 2026-06-03.*
