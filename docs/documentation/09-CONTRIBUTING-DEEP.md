# 09 — Contributing (Deep)

The short form lives in [`CONTRIBUTING.md`](../../CONTRIBUTING.md). This
file is the long form — read it before sending a PR larger than a typo
fix.

## Mindset

URUS is a small codebase in a hostile-input domain. The cost of a
careless change here is paid by every downstream user. Two habits we
hold ourselves to:

1. **No defensive overconfidence.** "It works on my machine" is not a
   merge criterion. If you cannot point at a test that exercises the
   change, the change is incomplete.
2. **No silent breakage.** A behavior change is also a spec change is
   also a CHANGELOG entry. They land together or not at all.

## Branching

The default branch is `main`. There are no release branches yet; the
v0.0.x cycle moves too quickly. When v0.1.0 ships, we will cut a
`release/v0.1` branch and adopt the standard "fix on main, backport on
the release branch" pattern.

For a feature branch:

```bash
git checkout -b feat/<short-name>
```

For a fix:

```bash
git checkout -b fix/<short-name>
```

For a doc-only change:

```bash
git checkout -b docs/<short-name>
```

## Commits

- **One topic per commit.** Refactor + behavior change in the same
  commit makes review impossible.
- **Imperative subject line, max 72 chars.**
- **Body wraps at 72 chars** and explains *why*, not *what*.
- **NEVER add a `Co-Authored-By` line.** All commits are purely under
  the author's name. (This is a hard project rule.)
- **Sign-off optional.** We do not enforce DCO at this stage.

A good commit message:

```
parser: cap recursion depth at 256

parse_type / parse_expr / parse_pattern previously had no depth
limit, so a pathological "*****T" deep type crashed urusc with a
stack overflow. Add a depth field to Parser and bail with a fatal
diagnostic when it exceeds 256.

Tracked as F-COMP-1 in docs/security/SECURITY-AUDIT.md.
```

## Code style — C

- **C11.** No GNU extensions in the compiler source (the *emitted* C
  uses GCC extensions; the compiler itself does not).
- **Indent: 4 spaces, no tabs.**
- **Brace style: K&R-ish.** Opening brace on the same line for
  functions / control flow.
- **`snake_case` for everything.** Types are `PascalCase` only when they
  are opaque "object" structs (`Arena`, `Lexer`, `Parser`).
- **Headers:** one `#pragma once`, one `#include "urus_common.h"` at the
  top.
- **No global mutable state in the compiler.** The runtime has a few
  globals; the compiler has none.
- **`static` everything that is not exported.**
- **Avoid `malloc` directly.** Use the arena. The only two heap
  allocations are `StrBuf` and the input-file buffer.
- **Prefer early return on error.** No deep nesting.
- **Asserts are for invariants, not user errors.** User errors are
  diagnostics.
- **No `printf` from compiler code.** Use `diag_emit` for messages,
  `strbuf_appendf` for codegen.

## Code style — URUS source in `examples/` and `tests/`

- 4-space indent.
- Trailing comma in multi-line struct literals and `match` arms.
- One feature per example.
- Tests start with a `// Tests: …` or `// expect: …` comment.

## Diagnostic style

When you emit a diagnostic, follow the pattern:

```
parser: expected `)`, found `;`
  --> tests/run/03_struct.urus:7:12
   |
 7 | let p = Point(1.0, 2.0;
   |                       ^
```

- The prefix tells which compiler stage emitted the message.
- The summary is **lowercase**, no trailing period.
- "expected X, found Y" is the canonical phrasing.
- Include a path:line:col block and a snippet with caret.

## Adding a new language feature

The minimum change-set is:

1. **`lexer.h` / `lexer.c`** — new tokens if needed.
2. **`ast.h` / `ast.c`** — new AST node + constructor.
3. **`parser.c`** — parse routine, hook it into the right precedence
   level.
4. **`sema.c`** — resolve names, run any new checks.
5. **`codegen_c.c`** — emit C for the new node.
6. **`stdlib/runtime/urus_rt.h`** — if a new runtime helper is needed.
7. **`tests/run/NN_<feature>.urus`** — at least one positive test.
8. **`tests/fail/NN_<feature>.urus`** — at least one error case.
9. **`docs/spec/SPEC.md`** — normative spec update.
10. **`docs/documentation/06-LANGUAGE-GUIDE.md`** — friendly write-up.
11. **`CHANGELOG.md`** — under `### Added` of the unreleased section.

PRs that skip any of these get sent back. The pipeline is dense
*precisely* so that no change leaks past the harness.

## Sanitizers, fuzzers, locally

Before sending a non-trivial PR, build with sanitizers and run the
suite:

```bash
cmake -B compiler/build -S . -DCMAKE_C_FLAGS="-fsanitize=address,undefined -g -O1"
cmake --build compiler/build
bash scripts/run-tests.sh
```

For fuzzing the lexer:

```bash
clang -fsanitize=fuzzer,address compiler/src/lexer.c compiler/src/arena.c \
      compiler/src/diag.c compiler/src/strbuf.c \
      -I compiler/include -o fuzz_lexer
./fuzz_lexer -max_total_time=300
```

(The parser and codegen are not fuzz-clean yet — see the Tier-1
hardening list in the security audit.)

## Review checklist

A reviewer asks, in order:

1. Does the PR description say *why*?
2. Is there a test that fails without the change?
3. Does the change touch the parser / sema / codegen as a coherent
   set, or only a slice?
4. Does the CHANGELOG entry match what shipped?
5. Does the diagnostic (if any) carry a `SrcLoc`?
6. Are there `TODO`s? Each one needs an issue link.
7. Is any new behavior security-sensitive? If yes, is the security
   audit updated?

## When in doubt

Open a draft PR with a *question* in the description and ping. We
prefer "I am not sure, what do you think?" over a clever patch we have
to revert later.

— *Last updated 2026-06-03.*
