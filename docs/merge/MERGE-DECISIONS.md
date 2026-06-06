# URUS v0.0.1 — Merge Decisions vs URUS-archive v0.1.0

> **Context.** URUS-archive (https://github.com/Urus-Foundation/Urus-archive)
> reached v0.1.0 at ~12k LOC and was archived because the codebase had
> become *hard to maintain and overly complex*. URUS v0.0.1 is a
> **clean-room rebuild**, not a port. This document records *what we kept,
> what we dropped, and why* — so future contributors don't accidentally
> reintroduce the complexity that killed the archive.

The rule of thumb used here:

> **Adopt** any feature whose user-visible value is high and whose
> implementation cost is bounded. **Reject** any feature that scattered
> non-local state across the compiler, even if the feature itself looked
> appealing in a tour. **Defer** anything good-but-large to a later
> milestone with a clear owner.

---

## 1. ADOPTED from archive

| # | Feature                              | Why we want it                                                | Where it lands |
|---|--------------------------------------|---------------------------------------------------------------|----------------|
| 1 | **f-string literals** `f"x = {x}"`   | Works everywhere a string works; explicit; no special-casing `println`. | Lexer + parser  |
| 2 | **`?` operator** on `Result<T,E>`    | Removes 80% of `match` boilerplate in error paths.            | Parser + codegen |
| 3 | **`defer { … }`**                    | RAII-equivalent cleanup without inventing destructors yet.    | Sema + codegen |
| 4 | **FNV-1a hashed symbol table**       | O(1) name lookup instead of O(n) linear walk.                 | Sema           |
| 5 | **CLI flags `--tokens` / `--ast`**   | Matches archive's shorter flag names. `--emit-c` kept; old `--emit-tokens`/`--emit-ast` kept as deprecated aliases for one release. | Driver         |
| 6 | **Posture: target C11, require Clang / GCC / clang-cl, reject native MSVC** | Honest about what the emitted code needs. Aligns with archive's pragmatic stance. | Codegen + README |

All six are isolated changes — none of them adds cross-cutting state.
That's the bar.

## 2. REJECTED — and why each one bloated the archive

| # | Feature                            | Reject reason                                                                 |
|---|-------------------------------------|--------------------------------------------------------------------------------|
| 1 | **`rune` macros** (textual subst.) | Textual macros are the C-preprocessor lesson all over again: hostile to LSPs, hygiene-free, error spans lie. Equivalent value is reachable later via **proper hygienic macros** (v0.2) or **compile-time generic functions** (v0.1). |
| 2 | **`try` / `catch` via setjmp/longjmp** | Cross-cuts every function frame and breaks `defer`, RAII, and async if/when they ship. `Result<T,E>` + `?` covers the use case with clean, local semantics. |
| 3 | **Reference counting**             | Cycles become a runtime gotcha that you can't grep for. RC bleeds into every type's ABI. We will commit to a borrow-checker story in v0.3 instead; until then, ownership is manual + linear-by-convention. |
| 4 | **`__emit__("…raw C…")`**         | Audit nightmare: a single `__emit__` call invalidates every static guarantee the compiler offers. If you need an escape hatch, design `unsafe { }` properly. |
| 5 | **`%%` floored remainder, `**` exponent, `&~` and-not** | Exotic operators with low payoff, non-trivial lexer cost, and they collide with future operator overloads. Replaceable with stdlib functions. |
| 6 | **`do { } while (…)` loop**       | Subsumed by `loop { …; if !cond { break; } }`. One fewer keyword in the grammar; one fewer parser branch. |
| 7 | **`async fn` + thread-per-async-call** | "Spawn an OS thread per await" is the wrong model — it has the cost of threads and the API of green tasks. Wait for **structured concurrency** (v0.4) which constrains the design correctly. |
| 8 | **String concatenation with `+`**  | Overloading `+` on `str` looks friendly but invites the JavaScript "1 + '1' = '11'" footgun. Use f-strings or explicit `str.concat`. |

## 3. DEFERRED — good ideas, wrong release

These are *not* rejected — they are scheduled.

| Feature                          | Target  | Owner needed |
|----------------------------------|---------|--------------|
| Monomorphic user generics        | v0.1.0  | Yes          |
| Single-dispatch traits           | v0.1.0  | Yes          |
| `unsafe { }` blocks              | v0.1.0  | —            |
| Borrow checker (affine model)    | v0.3.0  | Yes          |
| Structured concurrency           | v0.4.0  | Yes          |
| LLVM backend                     | v0.2.0  | Yes          |
| LSP (`urus-analyzer`)            | v0.2.0  | Yes          |
| Package manager (`tanduk`)       | v0.1.0  | Yes          |
| Proper hygienic macros           | v0.2.0  | Yes          |

## 4. Syntax — where URUS v0.0.1 diverges from the archive

These are **intentional** divergences. They are recorded here so nobody
asks "why isn't the archive's syntax accepted?"

| Construct          | Archive (v0.1.0)            | v0.0.1 (this repo)                      |
|--------------------|-----------------------------|------------------------------------------|
| Return type        | `fn name(): RetType`         | `fn name() -> RetType`                   |
| Statement terminator | Mandatory `;`               | Optional `;` for the trailing expression of a block (Rust-style) |
| Struct fields      | `name: type;`                | `name: type,`                            |
| Imports            | `import "math_utils.urus";` | `use urus.io.println;` or `use a::b::c;` |
| Numeric primitives | `int` (i64), `float` (f64)  | Full set: `u8..u64`, `i8..i64`, `f32`, `f64`, `usize`, `isize` |
| Wildcard pattern   | `_`                          | `_`                                      |
| Boolean type       | `bool`                       | `bool`                                   |

**Rationale for divergence:**

1. **Arrow `->` for return types** is the convention every modern systems
   language has converged on (Rust, Swift, Kotlin, Hare). Newcomers expect it.
2. **Optional trailing semicolons** make expression-oriented blocks read
   naturally:
   ```urus
   let parity = if n % 2 == 0 { "even" } else { "odd" };
   ```
3. **Comma struct fields** match the rest of the language (function
   parameters, enum variants, generic args). The archive's mix was
   inconsistent.
4. **`use a.b.c` paths** are visually quieter than literal file paths and
   tools resolve them through the module loader — the archive's
   `import "math_utils.urus";` exposes the filesystem layout in the source.
5. **Full numeric primitive set** is non-negotiable for an honest systems
   language. The archive's `int` + `float` is fine for scripting but loses
   to C the moment you touch hardware registers, network protocols, or
   binary parsers.

We are explicit: **a program from the archive will not compile in v0.0.1
unmodified.** A port guide will be published with v0.1.0 covering the
mechanical translation.

## 5. The principle, restated

The archive grew to 12k LOC because every "small" feature pulled in
non-local state — `__emit__` made codegen tolerant of arbitrary text,
RC made every drop point a maybe-mutate-ABI, `rune` made errors span the
wrong tokens. Each one looked cheap *at the time*.

**v0.0.1's rule:** if adding a feature requires the parser, sema, *and*
codegen to grow more than one screen each, it does not go in 0.0.x.
Period.

---

*Last reviewed: 2026-06-03. Update whenever a feature is moved between
the three lists above.*
