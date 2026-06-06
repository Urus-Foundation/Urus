# 01 — Project Overview

## What URUS is

URUS is a **systems-programming language** in early development. The
end-state goal — somewhere around v1.0 — is a small, predictable
language with:

- the **performance characteristics of C / C++** (no GC, no hidden
  allocations, direct mapping to hardware),
- the **memory-safety story of Rust** (ownership + borrowing, surfaced
  in the type system rather than at runtime),
- a **simple, global-keyword syntax** that reads the same to an
  Indonesian, Japanese, German, or American developer,
- and a **boring deployment story** (transpile to C → compile with the
  host toolchain → static binary).

The current release — **v0.0.1** — is much smaller than that. It is a
useful slice: a self-contained C11 compiler (~3 500 LOC) that translates
a meaningful subset of URUS into portable C11, plus a header-only
runtime.

Think of v0.0.1 as the **bootstrap seed**. Everything later grows from
this seed: the LLVM backend, the package manager, the LSP, eventually
the self-hosted compiler. None of those exist yet.

## The aurochs

The mascot is the aurochs (Latin *Bos primigenius*) — the extinct wild
ancestor of cattle. The name was chosen for what it evokes:

- **strength** — the language is intended for systems work where
  performance and predictability matter,
- **dominance** — URUS is not afraid to take a strong opinion on
  ownership, safety, and syntax,
- **resilience** — the compiler must be robust against hostile input;
  the runtime must abort cleanly rather than corrupt,
- **rooted strong, wild in execution** — conservative semantics with
  expressive surface syntax.

The cultural reference is intentionally Indonesian / regional in feel,
even though the language itself uses English keywords. The compiler
itself was bootstrapped at `D:/Urus/` on 2026-06-02.

## Lineage — the `Urus-archive` story

URUS has been written before. The previous repository,
[`Urus-Foundation/Urus-archive`](https://github.com/Urus-Foundation/Urus-archive),
reached ~12 000 lines of C source implementing a richer feature set,
but it became too tangled to maintain. Among other issues:

- a `rune` macro system that expanded textually, evading type checking;
- a `try` / `catch` implementation built on `setjmp` / `longjmp` that
  bypassed `defer`;
- reference counting bolted on at runtime, fighting the rest of the
  type system;
- an `__emit__` keyword that let user code inject raw C — a permanent
  open backdoor;
- exotic operators (`%%`, `**`, `&~`, `do/while`) that complicated the
  parser without delivering proportional value.

In June 2026 the decision was made to **archive that codebase and start
over** with a smaller, more disciplined surface area. v0.0.1 is the
result of that restart.

Many good ideas were carried over deliberately:

- **f-string literals** (`f"x = {name}"`)
- **postfix `?`** on `Result` for error propagation
- **`defer expr;`** for LIFO cleanup at block end
- **FNV-1a hashed scope tables** for O(1) name lookup
- the **`--tokens` / `--ast`** CLI surface
- the **"emit C11, require Clang or clang-cl"** posture

The kept / dropped / deferred list is documented in
[`docs/merge/MERGE-DECISIONS.md`](../merge/MERGE-DECISIONS.md). v0.0.1
is best described as a **clean-room rebuild** of `Urus-archive`'s v0.1.0,
not a fork.

Practical consequence: **archive programs will not compile here
unmodified.** Some syntactic divergences (semicolons optional, `fn -> T`
arrow style, `use a.b.c` instead of `import "file"`) were chosen to
match modern conventions.

## Goals (v0.0.1)

1. **Compile useful programs.** Hello world, arithmetic, control flow,
   structs, `impl`s, `match`, `Result` / `Option`, f-strings, `?`,
   `defer`. The examples in `examples/` are the working set.
2. **Stay readable.** The whole compiler should fit in a contributor's
   head over a weekend. ~3 500 LOC is the budget.
3. **Honest diagnostics.** Every error carries a `SrcLoc` and is rendered
   with a snippet and a caret.
4. **No undocumented behavior.** If the compiler accepts it, the spec
   covers it. If the spec covers it, the tests exercise it.
5. **Be honest about what is missing.** The README and CHANGELOG mark
   every unfinished surface (no inference, no exhaustiveness, no `mut`
   enforcement, no borrow checker, no async, no traits, no package
   manager). No false marketing.

## Non-goals (v0.0.1)

- Self-hosting. The compiler is C; it always will be at v0.0.x.
- Native MSVC support. The emitted C uses GCC statement-expressions and
  `__auto_type`; clang-cl is the Windows path.
- Production use. URUS v0.0.1 is **not safe** to run on untrusted source
  — see [the security audit](../security/SECURITY-AUDIT.md) for the
  stop-ship list.

## Vision (v1.0 and beyond)

The goal at v1.0 is a small, expressive, *boring* systems language that
people reach for when they would otherwise reach for C, Rust, Go, or
Zig — and pick URUS specifically because:

- the syntax is clean and globally legible,
- the safety story is in the type system, not at runtime,
- deployment is a static binary by default,
- the toolchain is one well-maintained binary (`urusc`) plus one
  package manager (`tanduk`),
- the language itself is *small enough* to be specifiable and verifiable.

URUS is not chasing novelty. It is trying to be a good citizen in the
systems-programming neighborhood.

## License

URUS is licensed under either Apache-2.0 or MIT at your option. See
[`LICENSE`](../../LICENSE).

— *Last updated 2026-06-03.*
