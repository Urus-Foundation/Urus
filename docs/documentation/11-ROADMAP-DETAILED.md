# 11 — Detailed Roadmap

The top-level [`ROADMAP.md`](../../ROADMAP.md) gives the one-paragraph
version-by-version plan. This file expands each version into the
milestones, the deliverables, and the gates that must be met before
the version ships.

> These plans are *intent*, not commitment. Every milestone slips when
> reality argues with the schedule.

---

## v0.0.2 — Stop-ship security release

**Target window:** mid-2026, immediately after v0.0.1.

The single purpose of v0.0.2 is to close the **10 stop-ship security
findings** documented in
[`docs/security/SECURITY-AUDIT.md`](../security/SECURITY-AUDIT.md).
Nothing else lands until those do.

### Must-fix (gate criteria)

1. **F-MEM-1** — f-string `{...}` placeholder is re-lexed and restricted
   to `IDENT (DOT IDENT)*`. The regression PoC
   `tests/fail/SEC-01_fstr_injection.urus` must fail to compile.
2. **F-TY-2** — `Result<T, E>` and `Option<T>` monomorphised per type
   pair. Payload no longer truncated to int64.
3. **F-COMP-1** — parser depth cap (default 256).
4. **F-COMP-2** — `let mut` enforced in sema.
5. **F-COMP-3** — `match` exhaustiveness checking.
6. **F-MEM-2** — `lex_string` realloc NULL handling.
7. **F-MEM-5** — refuse non-regular input files; zero the tail of the
   input buffer on short reads.
8. **F-MEM-7** — pass explicit length to `urus_print_fmt`.
9. **F-MEM-8** — remove `URUS_FMT_PTR_TAG`.
10. **F-TY-1** — disjoint Result / Option tag namespaces; codegen
    refuses cross-cast.

### Should-fix

- Cap input file size (default 64 MiB).
- Cap string literal length (default 16 MiB).
- Cap `arena_alloc` per call (default 256 MiB).
- StrBuf growth refuses > 1 GiB output.
- Publish `SECURITY.md` policy with disclosure email.

### Nice-to-have

- ASan + UBSan in local dev script.
- libFuzzer harness for lexer.
- CI matrix on Linux + macOS + Windows clang-cl.

### Out of scope

- New language features. v0.0.2 is a security release, full stop.

---

## v0.0.3 — Polishing release

**Target window:** late 2026.

### Goals

- Real type inference (Hindley-Milner lite, block-local).
- `defer` fires on `return` (currently only at block end).
- `?` operator extended to `Option<T>`.
- Multi-line errors with multiple spans.
- `--json-diagnostics` for tooling consumers.
- Diagnostic IDs (`E0001`, …) with stable URLs.
- Source-mapped emitted C (`#line` directives).
- Stable AST JSON dump (`--ast=json`).
- Method resolution by receiver type (F-COMP-4).
- `as` cast whitelist (F-TY-3).
- Visibility enforcement (`pub` actually means something).

### Removed

- The deprecated `--emit-tokens` / `--emit-ast` flags are removed (only
  `--tokens` / `--ast` remain).

---

## v0.1.0 — `tanduk` and stdlib

**Target window:** 2027 (Q1-Q2).

### `tanduk` package manager

The supply-chain design is locked in *before* we ship a registry. From
the audit's Tier-3 section:

- Lockfile with BLAKE3 checksums.
- Signed publishes (sigstore or minisign).
- Sandboxed build scripts.
- Transparency log.
- Typosquatting checks at publish.
- Air-gapped mirror tooling.
- Per-package permissions manifest ("this lib needs net + fs").

### Standard library v0.1

- `urus.collections` — `Vec<T>`, `Map<K, V>`, `Set<T>`.
- `urus.io` — files, stdin/stdout, structured errors.
- `urus.str` — proper UTF-8 string operations.
- `urus.fmt` — debug printing, structured display.
- `urus.test` — assertion macros and the test runner harness.

### Language

- **Monomorphic generics** for user types.
- **Traits**, with coherence rules documented before any trait code
  ships.
- **`unsafe` blocks** with documented semantics (delimit the audit
  boundary; never implicit).
- Const evaluator for array sizes.
- Multi-file modules (1 file = 1 module, like Go).

### Quality gates

- ASan + UBSan + MSan in CI.
- libFuzzer 5 minutes / PR, 24 hours nightly.
- Reproducible compiler builds.

---

## v0.2.0 — LLVM backend + LSP

**Target window:** 2027 Q3-Q4.

### LLVM backend

- New module `compiler/src/codegen_llvm.c`. Existing `codegen_c.c`
  stays.
- `--backend=c|llvm` flag.
- LLVM is added as an optional dependency; the C backend remains the
  default for portability.

### Language server

- Module `tools/urus-ls`.
- Diagnostics, go-to-definition, find-references, completion, hover,
  format-on-save.
- VSCode extension as the first client; emacs / vim configurations
  documented.

### Tooling

- `urusfmt` (code formatter).
- `urusdoc` (markdown documentation generator).

---

## v0.3.0 — Borrow checker

**Target window:** 2028.

### Goals

- Affine ownership.
- Tree-Borrows-style aliasing model (adopt from Rust academia,
  audit, integrate).
- Lifetime elision rules.
- `Drop` trait + deterministic cleanup.
- `defer` fires on every exit edge.

This is the largest single language change planned. It is also the one
that turns URUS from "fancy C" into "actually-safe systems language."

---

## v0.4.0 — Async (structured concurrency)

**Target window:** 2028-2029.

### Goals

- `urus.async` runtime.
- **Structured concurrency** by design. Every `spawn` is scoped to a
  parent block. No "fire and forget."
- Cancellation tokens.
- `urus.net` (TCP, UDP, TLS via libsodium-style wrapper).

### Non-goals

- Threads. Async is enough at v0.4.

---

## v0.5.0 — Filling out the stdlib

- `urus.crypto` (wrapping a vetted library).
- `urus.json` / `urus.toml`.
- `urus.time`, `urus.rand`.

---

## v1.0.0 — Self-hosting and freeze

- The URUS-in-URUS compiler reaches parity with `urusc.c`.
- Language is frozen — backwards compatibility committed to.
- SOC 2 / SLSA-3 story for the registry.
- External pen-test by a named firm.

---

## Cross-version themes

- **Security audit follow-ups.** The Tier 1-4 hardening list in
  `docs/security/SECURITY-AUDIT.md` distributes across v0.0.2 — v1.0.
  We do not declare an audit closed; we ship the highest-impact items
  every release.
- **Documentation.** This `docs/documentation/` folder is kept current
  with each release. A doc PR landing alongside the feature PR is the
  norm.
- **Test corpus.** Every release adds at least one test per shipped
  feature.

— *Last updated 2026-06-03.*
