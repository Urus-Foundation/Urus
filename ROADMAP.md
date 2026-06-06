# 🗺️ URUS Roadmap

This is the long-form plan for URUS. It is **aspirational** — dates are
targets, not commitments. Anything before v1.0 may break without ceremony.

The detailed architectural reasoning behind these milestones lives in
[`docs/ANALYSIS.md`](./docs/ANALYSIS.md), including the **Top 100 next
improvements** that feed into each release.

---

## v0.0.x — Foundation (now)

**Theme: prove the pipeline end-to-end.**

- [x] Lexer, parser, AST, sema, C transpiler.
- [x] Result/Option runtime.
- [x] String interpolation in `println`.
- [ ] Multi-file modules (one file = one module today).
- [ ] `--emit-c` produces MSVC-compatible C (no GCC extensions).
- [ ] Source-mapped `#line` directives in emitted C.
- [ ] Exhaustiveness check for `match`.
- [ ] Better diagnostics: "did you mean ...?" suggestions.

## v0.1.x — Tooling & Stdlib seed

**Theme: stop being a toy.**

- [ ] `tanduk` — package manager + build runner (cargo-style).
  - Lockfile format, semver resolution, local + git deps.
  - One-line install: `tanduk new`, `tanduk build`, `tanduk run`, `tanduk test`.
- [ ] `urus.collections` — `Vec<T>`, `HashMap<K,V>`, `String`.
- [ ] `urus.io` expansion — `File`, `BufReader`, stdout/stderr handles.
- [ ] Monomorphisation of generics (one IR per instantiation).
- [ ] Trait declarations + `impl Trait for Type` (single-dispatch).
- [ ] Better error model: `?` operator desugars to `match … { Err(e) => return Err(e), Ok(v) => v }`.
- [ ] A formatter: `urus fmt`.

## v0.2.x — Real backend & LSP

**Theme: production-quality codegen and editor experience.**

- [ ] **LLVM backend** — drop GCC-extension reliance; emit LLVM IR directly.
- [ ] Optimisation pipeline (inliner, mem2reg, GVN, dead-code).
- [ ] **`urus-analyzer`** — full language-server (LSP):
  - Diagnostics, hover, go-to-def, find-references, completion.
  - Workspace-wide rename.
- [ ] VS Code extension and a plain `vim-urus` plugin.
- [ ] Linter: `urus lint` with configurable severity (`warn`, `deny`).

## v0.3.x — Memory safety story

**Theme: a borrow-checker that Rust-users find acceptable but Go-users don't flee.**

- [ ] Affine ownership: every value has a single owning binding.
- [ ] `&T` and `&mut T` checked at compile-time (one-mut-or-many-shared rule).
- [ ] Lifetime elision rules for most function signatures.
- [ ] Escape hatch: `unsafe { … }` blocks for raw pointers and FFI.
- [ ] `Drop` trait for deterministic cleanup.

## v0.4.x — Concurrency

**Theme: structured concurrency with predictable cost.**

- [ ] Native threads via `urus.thread`.
- [ ] **Structured concurrency** primitives — scopes guarantee task termination.
- [ ] Channels (`urus.channel`).
- [ ] Async/await — built on stackless coroutines, lowered to a state-machine.
- [ ] `urus.net` — TCP, UDP, TLS via OS APIs.

## v0.5.x — Production stdlib

- [ ] `urus.crypto` — AES-GCM, ChaCha20-Poly1305, SHA-2/3, BLAKE3, Argon2.
- [ ] `urus.json`, `urus.toml`, `urus.yaml`.
- [ ] `urus.http` — server and client.
- [ ] `urus.regex`.
- [ ] `urus.time` — proper monotonic + wall-clock split, timezone-aware.

## v0.7.x — Self-hosting prep

- [ ] Bootstrap a second compiler implementation in URUS, compiled by the v0.6 C compiler.
- [ ] CI cross-bootstrap: stage 1 (C urusc) builds stage 2 (URUS urusc) builds stage 3 (URUS urusc again) — stages 2 and 3 must be byte-identical.

## v1.0.0 — Self-hosted, stable

- [ ] **Language freeze.** Breaking changes require RFC + a 2-version deprecation cycle.
- [ ] Compiler written in URUS.
- [ ] Stable ABI for libraries.
- [ ] Stability promise: anything in `urus.{io,fs,net,collections,sync}` stays source-compatible across the 1.x line.

## v1.x+ — Ecosystem and reach

- [ ] **WebAssembly target** with WASI support.
- [ ] **Embedded targets** — bare-metal ARM/RISC-V, `#![no_std]` equivalent.
- [ ] **GPU/SIMD intrinsics** through a portable `urus.simd` module.
- [ ] **Database driver protocol** — official driver crates for Postgres, SQLite, ClickHouse.
- [ ] **Distributed systems** — `urus.actor`, `urus.raft` reference implementations.
- [ ] **Formal verification subset** — `pure fn` and refinement types on a stdlib core, ala Dafny/F*.

---

## How priorities are decided

1. **Safety bugs > correctness bugs > UX bugs > features.**
2. A feature ships only when its **diagnostics** are at least as helpful as Rust's. Cryptic errors are debt.
3. Every feature must pass three doors: **specification**, **implementation**, **tests + docs**.

## How you can help

See [`CONTRIBUTING.md`](./CONTRIBUTING.md). The shortest road to having an
impact is filing a "weird program that broke" with the smallest URUS source
that reproduces it.
