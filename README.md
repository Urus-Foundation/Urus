<div align="center">

<img src="https://raw.githubusercontent.com/Urus-Foundation/initial-resource/main/assets/logo.jpg" alt="URUS logo" width="240"/>

# URUS

### *Aurochs — Strength · Dominance · Resilience*

*"Rooted strong, wild in execution."*

A small, honest, systems-programming language.
Bootstrapped in **C11**, transpiled to **portable C**, designed to grow into something we can trust.

---

[![Version](https://img.shields.io/badge/version-v0.0.1--b031-1d1f24?style=flat-square)](./docs/archive/INDEX.md)
[![CI](https://img.shields.io/badge/CI-gcc%20%7C%20clang%20%7C%20ASan%2FUBSan-blue?style=flat-square)](./.github/workflows/ci.yml)
[![Status](https://img.shields.io/badge/status-pre--alpha-orange?style=flat-square)](#-status)
[![License](https://img.shields.io/badge/license-Apache--2.0%20OR%20MIT-blue?style=flat-square)](./LICENSE)
[![Build](https://img.shields.io/badge/build-CMake%20%E2%89%A5%203.16-064F8C?style=flat-square)](./docs/documentation/03-BUILDING.md)
[![C Standard](https://img.shields.io/badge/standard-C11-A8B9CC?style=flat-square)](#)
[![Security](https://img.shields.io/badge/security-0%20stop--ship%20remaining-green?style=flat-square)](./docs/security/SECURITY-AUDIT.md)
[![Docs](https://img.shields.io/badge/docs-16%20documents-9B59B6?style=flat-square)](./docs/documentation/00-INDEX.md)

[**Quickstart**](#-quickstart) ·
[**Docs**](./docs/documentation/00-INDEX.md) ·
[**Spec**](./docs/spec/SPEC.md) ·
[**Roadmap**](./ROADMAP.md) ·
[**Security**](./docs/security/SECURITY-AUDIT.md) ·
[**Changelog**](./CHANGELOG.md) ·
[**Archive**](./docs/archive/INDEX.md)

</div>

---

> [!CAUTION]
> **URUS v0.0.1 is pre-alpha.** A red-team audit found 10 stop-ship
> security issues, including a **critical RCE primitive** in the
> f-string codegen path
> ([`F-MEM-1`](./docs/security/SECURITY-AUDIT.md#f-mem-1--codegen-f-string-interpolation-pastes-attacker-text-verbatim-into-emitted-c)).
> **All 10 findings are closed as of `v0.0.1-b015`** (regression-tested
> in `tests/fail/SEC-*`), but the project has not passed an external
> pen-test — continue to treat untrusted `.urus` input with care.
> Full report:
> [`docs/security/SECURITY-AUDIT.md`](./docs/security/SECURITY-AUDIT.md).

> [!IMPORTANT]
> **Native MSVC (`cl.exe`) is not supported.** The C that `urusc` emits
> uses GCC statement-expressions and `__auto_type`. On Windows install
> LLVM (`winget install LLVM.LLVM`) and use **`clang`** or **`clang-cl`**.

> [!NOTE]
> **Lineage.** v0.0.1 is a **clean-room rebuild** of the archived
> [`Urus-Foundation/Urus-archive`](https://github.com/Urus-Foundation/Urus-archive)
> (~12 000 LOC, archived because the codebase had become hard to maintain).
> Kept the good ideas, dropped the bloat, started over at ~3 500 LOC.
> Full kept/dropped/deferred table:
> [`docs/merge/MERGE-DECISIONS.md`](./docs/merge/MERGE-DECISIONS.md).
> Archive programs will **not** compile here unmodified.

---

## Table of contents

- [What is URUS?](#what-is-urus)
- [Why another language?](#why-another-language)
- [📊 Status](#-status)
- [🚀 Quickstart](#-quickstart)
- [🦬 A taste of URUS](#-a-taste-of-urus)
- [🏗 Architecture in 30 seconds](#-architecture-in-30-seconds)
- [📁 Project layout](#-project-layout)
- [📚 Documentation map](#-documentation-map)
- [🛣 Roadmap](#-roadmap)
- [🧪 Testing](#-testing)
- [🤝 Contributing](#-contributing)
- [🛡 Security policy](#-security-policy)
- [⚖️ License](#️-license)
- [🙏 Acknowledgements](#-acknowledgements)

---

## What is URUS?

URUS is a **systems-programming language** in early development.
The end-state goal — around v1.0 — is a small, predictable language
with:

- **C / C++-class performance.** No GC, no hidden allocations,
  direct mapping to the machine.
- **Rust-class safety in the type system.** Ownership and borrowing
  surfaced statically rather than checked at runtime.
- **Globally legible English keywords.** Reads the same in Jakarta,
  Berlin, and São Paulo.
- **Boring deployment.** Transpile to C, compile with the host toolchain,
  ship a static binary.

Today — **v0.0.1** — that vision is the headline; the implementation is
a small, useful slice. A self-contained C11 compiler in ~3 500 LOC
translates a meaningful subset of URUS into portable C11, and a
header-only runtime gives you `println`, `Result`, `Option`, and
`panic`. **Treat this as a working seed, not a product.**

---

## Why another language?

| You currently reach for… | URUS aspires to be…                                                   |
|--------------------------|------------------------------------------------------------------------|
| **C**                    | The same control, with stronger types and `Result`/`Option` baked in.  |
| **C++**                  | A smaller, less surprising language with one obvious way to do things. |
| **Rust**                 | A simpler borrow story, syntax that is easier to skim.                 |
| **Go**                   | Comparable simplicity, but no GC and a real systems story.             |
| **Zig**                  | The same "compile-to-C is a virtue" attitude, with stronger defaults.  |

URUS is **not chasing novelty**. It is trying to be a good citizen in
the systems-programming neighborhood and to make a few opinionated
calls (English keywords, English diagnostics, English spec; transpile
first, native backends later) that make adoption faster.

---

## 📊 Status

| Component           | v0.0.1   | Notes                                                              |
|---------------------|----------|--------------------------------------------------------------------|
| Lexer               | ✅       | Full token set, comments, escapes, numeric prefixes, f-strings.    |
| Parser              | ✅       | Items, statements, Pratt expressions (18 levels), patterns.        |
| AST + arena         | ✅       | Discriminated-union AST, bump arena, no per-node free.             |
| Semantic analysis   | 🟡 basic | Name resolution, struct-field check; **no full type inference**.   |
| Type system         | 🟡 basic | Primitives + named structs + `Result` / `Option`.                  |
| Codegen             | 🟡       | C11 transpile via GCC statement-expressions + `__auto_type`.       |
| Runtime             | 🟡       | `println` with `{}` interpolation, `Result`, `Option`, `panic`.    |
| Standard library    | 🟡 stub  | `urus.io` only.                                                    |
| Parser recursion cap | ✅      | `URUS_MAX_PARSE_DEPTH = 256` (closes F-COMP-1 in `v0.0.1-b011`).    |
| Input / string / arena / strbuf caps | ✅ | 64 MiB / 16 MiB / 256 MiB / 1 GiB respectively (Tier-0 #11–#14, `v0.0.1-b011`). |
| Length-aware `urus_*_fmt_n` API | ✅ | Codegen passes explicit count; sentinel scan bounded (closes F-MEM-7 in `v0.0.1-b012`). |
| Runtime `URUS_FMT_PTR_TAG` removed | ✅ | `char *` routes through length-bounded `urus_str` (closes F-MEM-8 in `v0.0.1-b012`). |
| Disjoint Result/Option tags | ✅ | Result uses 0/1, Option uses 2/3 (closes F-TY-1 in `v0.0.1-b012`). |
| `let mut` enforcement | ✅     | Sema rejects writes to immutable bindings (closes F-COMP-2 in `v0.0.1-b013`). |
| `match` exhaustiveness | ✅    | Result / Option / declared-enum matches require all variants or `_` (closes F-COMP-3 in `v0.0.1-b013`). |
| f-string placeholder validator | ✅ | `{…}` must match `IDENT (.IDENT)*`; closes F-MEM-1 RCE primitive in `v0.0.1-b014`. |
| Widened Result/Option payload | ✅ | 16-byte `urus_payload_t` union — no more str/double truncation (closes F-TY-2 in `v0.0.1-b015`). |
| Integer / float literal overflow detection | ✅ | Lexer surfaces `ERANGE`; out-of-range literals diagnosed (Tier-1, `v0.0.1-b016`). |
| UTF-8 input validation | ✅    | Rejects embedded NULs, overlongs, lone surrogates (Tier-1, `v0.0.1-b016`). |
| `?` operator context check | ✅ | `?` only inside `fn -> Result/Option` (`v0.0.1-b018`). |
| String helpers      | ✅       | `urus_str_eq/cmp/starts_with/ends_with/contains/len/is_empty` (`v0.0.1-b017`). |
| `read_line`         | ✅       | `Option<str>`, 16 MiB cap, CRLF-tolerant (`v0.0.1-b019`). |
| `--max-input-bytes` flag | ✅  | Runtime cap override, 1 GiB hard ceiling (`v0.0.1-b020`). |
| `&mut` borrow gate  | ✅       | Mutable borrow of immutable binding rejected (`v0.0.1-b021`). |
| Unused-`mut` warning | ✅      | First `diag_warn` consumer — style advice, not an error (`v0.0.1-b021`). |
| CI matrix + sanitizers | ✅    | gcc/clang/macOS/MSVC + ASan/UBSan job (`v0.0.1-b022`). |
| libFuzzer target    | ✅       | `urus_compile_buffer` fuzzed 5 min/PR in CI (`v0.0.1-b023`). |
| `while let`         | ✅       | Parser desugar to `loop`+`match` (`v0.0.1-b024`). |
| `for` range loops   | ✅       | `0..n` / `0..=n`; bad iterators diagnosed, not miscompiled (`v0.0.1-b024`). |
| Diagnostics windowing | ✅     | 120-col snippets + 64-error flood cap (`v0.0.1-b025`). |
| `defer` on every exit | ✅     | Early `return` + `?` propagation, value-first ordering (`v0.0.1-b028`). |
| Embedder-safe pipeline | ✅    | No `exit()` path left in `urus_compile_buffer` — fatal + OOM both unwind (`v0.0.1-b026`/`b029`). |
| End-to-end verified | ✅       | 23/23 run-tests + 6/6 examples compile **and run** green; 13 latent codegen bugs fixed (`v0.0.1-b030`). |
| Value-producing `match` | ✅   | `let x = match …` / tail position works; user-enum tags compared correctly (`v0.0.1-b030`). |
| Typed payload extraction | ✅  | `Ok("s")` round-trips — read side picks the matching union arm (`v0.0.1-b030`). |
| Fuzz-hardened lexer/parser | ✅ | First real fuzz runs: EOF guards in char/string literals, unary depth-cap, arena total cap, leak-proof scratch (`v0.0.1-b031`, SEC-18..20). |
| Borrow checker      | ❌       | Planned in v0.3.0.                                                 |
| Generics            | ❌       | Parsed; **not monomorphised**. Built-ins only.                     |
| Traits              | ❌       | Keyword reserved.                                                  |
| `async` / threads   | ❌       | Planned in v0.4.0 (structured concurrency).                        |
| Package manager     | ❌       | Planned as `tanduk` in v0.1.0.                                     |
| LSP / IDE           | ❌       | Planned in v0.2.0.                                                 |
| LLVM backend        | ❌       | Planned in v0.2.0.                                                 |
| Self-hosting        | ❌       | Planned at v1.0.                                                   |
| Reproducible builds | ❌       | Tier-1 hardening item.                                             |

Read the full per-version plan in
[`docs/documentation/11-ROADMAP-DETAILED.md`](./docs/documentation/11-ROADMAP-DETAILED.md).

---

## 🚀 Quickstart

### Prerequisites

- A C11 compiler — **GCC**, **Clang**, or **clang-cl**.
  Native **MSVC `cl.exe` is rejected** by a `#error` at the top of every
  emitted file. On Windows: `winget install LLVM.LLVM`.
- **CMake ≥ 3.16**.

### Build the compiler

<details open>
<summary><b>Windows (PowerShell)</b></summary>

```powershell
cmake -B compiler/build -S .
cmake --build compiler/build
.\compiler\build\Debug\urusc.exe --version
```

</details>

<details>
<summary><b>macOS / Linux</b></summary>

```bash
cmake -B compiler/build -S .
cmake --build compiler/build -j
./compiler/build/urusc --version
```

</details>

### Compile and run your first URUS program

```bash
# 1. Transpile URUS → C
compiler/build/urusc examples/hello.urus --emit-c
# writes examples/hello.urus.c

# 2. Build the C with your system compiler (link the runtime)
gcc examples/hello.urus.c -I stdlib/runtime -o hello
./hello
# → Hello, Aurochs!
```

On Windows:

```powershell
clang examples\hello.urus.c -I stdlib\runtime -o hello.exe
.\hello.exe
```

### Run the test suite

```powershell
# Windows
powershell -ExecutionPolicy Bypass -File scripts\run-tests.ps1
```

```bash
# POSIX
bash scripts/run-tests.sh
```

> [!TIP]
> If you don't have a host C compiler installed, the test runner still
> validates lexer + parser + sema + codegen by checking that
> `urusc --emit-c` exits cleanly.

Full build guide, common errors, sanitizer builds:
[`docs/documentation/03-BUILDING.md`](./docs/documentation/03-BUILDING.md).

---

## 🦬 A taste of URUS

```urus
module main

use urus.io.println

struct Aurochs {
    name:   str,
    weight: f64,
    power:  u32,
}

impl Aurochs {
    fn new(name: str, weight: f64) -> Aurochs {
        return Aurochs { name: name, weight: weight, power: 100 }
    }

    fn rampage(&self) {
        println("{self.name} rampages with power {self.power}!")
    }
}

fn divide(a: i64, b: i64) -> Result<i64, str> {
    if b == 0 {
        return Err("division by zero")
    }
    return Ok(a / b)
}

// `?` propagates the error; `defer` schedules cleanup at block end.
fn average(a: i64, b: i64, total: i64) -> Result<i64, str> {
    defer println("average() exiting")
    let q = divide(a + b, total)?
    return Ok(q)
}

fn main() {
    let bos = Aurochs.new("Bos", 1200.0)
    bos.rampage()

    match average(10, 30, 4) {
        Ok(v)  => println(f"avg = {v}"),
        Err(e) => println(f"err: {e}"),
    }
}
```

What you see above is **all real, all v0.0.1**: structs, `impl` blocks,
methods, `Result` + `Option`, the postfix `?` operator, `defer`, f-strings,
pattern matching, control flow, the works. A few things — full generics,
traits, `async`, the borrow checker — are documented but not yet
implemented; the
[language guide](./docs/documentation/06-LANGUAGE-GUIDE.md) is honest
about every gap.

---

## 🏗 Architecture in 30 seconds

```
.urus → lexer → parser → sema → codegen_c → .c → host gcc/clang → binary
                            ↑                           ↑
                            │                           │
                       FNV-1a scopes               urus_rt.h
                       arena-allocated AST         (header-only)
```

- **Single arena per compilation.** Allocation is a pointer bump; the
  whole AST dies together at the end.
- **One-token lookahead.** Recursive-descent for items and statements,
  Pratt parser for expressions across 18 precedence levels.
- **Position-aware diagnostics.** Every token, every AST node, every
  error carries a `SrcLoc { offset, length, line, col }`. Errors render
  with a snippet and a caret.
- **Single-TU codegen.** One `.c` file per `.urus` file, includes the
  header-only runtime, compiles with any C11 toolchain that supports
  GCC statement-expressions.

Deep dive:
[`docs/documentation/02-ARCHITECTURE.md`](./docs/documentation/02-ARCHITECTURE.md)
· module-by-module:
[`docs/documentation/05-COMPILER-INTERNALS.md`](./docs/documentation/05-COMPILER-INTERNALS.md).

---

## 📁 Project layout

```
Urus/
├── CMakeLists.txt              top-level build
├── compiler/                   reference compiler in portable C11
│   ├── include/                public headers (the compiler's API)
│   └── src/                    arena · strbuf · diag · lexer · parser
│                               ast · sema · codegen_c · main
├── stdlib/
│   └── runtime/urus_rt.h       header-only C runtime
├── examples/                   end-to-end URUS programs
├── tests/
│   ├── run/                    must compile AND run cleanly
│   └── fail/                   must produce a specific diagnostic
├── docs/
│   ├── documentation/          developer & contributor handbook (16 files)
│   ├── analysis/               architecture review + Top 100 roadmap
│   ├── merge/                  archive → v0.0.1 kept/dropped/deferred
│   ├── review/                 14-perspective adoption council review
│   ├── security/               red-team security audit + stop-ship list
│   ├── spec/                   formal language specification
│   └── archive/                build-numbered archive of every release
├── scripts/                    PowerShell + bash test runners
└── CHANGELOG.md · ROADMAP.md · CONTRIBUTING.md · GOVERNANCE.md · SECURITY.md
```

Detailed walk-through:
[`docs/documentation/04-PROJECT-LAYOUT.md`](./docs/documentation/04-PROJECT-LAYOUT.md).

---

## 📚 Documentation map

URUS ships with **16 detailed handbook documents** in `docs/documentation/`,
plus topic folders for analysis, merge decisions, security, spec, and
the build archive. The index is your friend.

| Document | What you learn |
|----------|----------------|
| [`00-INDEX.md`](./docs/documentation/00-INDEX.md) | Navigation across every other doc |
| [`01-OVERVIEW.md`](./docs/documentation/01-OVERVIEW.md) | What URUS is, vision, history |
| [`02-ARCHITECTURE.md`](./docs/documentation/02-ARCHITECTURE.md) | Pipeline + data ownership |
| [`03-BUILDING.md`](./docs/documentation/03-BUILDING.md) | Build on every platform |
| [`04-PROJECT-LAYOUT.md`](./docs/documentation/04-PROJECT-LAYOUT.md) | Every folder explained |
| [`05-COMPILER-INTERNALS.md`](./docs/documentation/05-COMPILER-INTERNALS.md) | Module-by-module deep dive |
| [`06-LANGUAGE-GUIDE.md`](./docs/documentation/06-LANGUAGE-GUIDE.md) | URUS as a language |
| [`07-RUNTIME.md`](./docs/documentation/07-RUNTIME.md) | `urus_rt.h` internals |
| [`08-TESTING.md`](./docs/documentation/08-TESTING.md) | Test harness + how to add tests |
| [`09-CONTRIBUTING-DEEP.md`](./docs/documentation/09-CONTRIBUTING-DEEP.md) | Style, PR flow, hard rules |
| [`10-DESIGN-DECISIONS.md`](./docs/documentation/10-DESIGN-DECISIONS.md) | ADR-style record of every "why" |
| [`11-ROADMAP-DETAILED.md`](./docs/documentation/11-ROADMAP-DETAILED.md) | Per-version milestones to v1.0 |
| [`12-FAQ.md`](./docs/documentation/12-FAQ.md) | Common questions |
| [`13-GLOSSARY.md`](./docs/documentation/13-GLOSSARY.md) | Terminology |
| [`14-TROUBLESHOOTING.md`](./docs/documentation/14-TROUBLESHOOTING.md) | Error → cause → fix |
| [`15-VERSIONING.md`](./docs/documentation/15-VERSIONING.md) | Version format + change-code legend |

And the topic folders:

- 🔬 [`docs/analysis/ANALYSIS.md`](./docs/analysis/ANALYSIS.md) — 15-phase
  architecture review + the Top 100 improvements list.
- 🧬 [`docs/merge/MERGE-DECISIONS.md`](./docs/merge/MERGE-DECISIONS.md) —
  what was kept, dropped, or deferred from `Urus-archive`.
- 🧠 [`docs/review/REVIEW.md`](./docs/review/REVIEW.md) — adoption review
  from 14 different perspectives (user, tester, adopter, devrel, …).
- 🛡 [`docs/security/SECURITY-AUDIT.md`](./docs/security/SECURITY-AUDIT.md) —
  red-team audit + stop-ship list + scorecard.
- 📜 [`docs/spec/SPEC.md`](./docs/spec/SPEC.md) — formal language
  specification.
- 🗂 [`docs/archive/INDEX.md`](./docs/archive/INDEX.md) — every recorded
  build of URUS, with change-code summaries.

---

## 🛣 Roadmap

```
v0.0.1   → ✅ first scaffold + adopted archive's best ideas
v0.0.2   → 🚨 stop-ship security release (close the 10 audit findings)
v0.0.3   → 🛠 polishing: real type inference, exhaustive match, multi-span errors
v0.1.0   → 📦 tanduk package manager + first standard library
v0.2.0   → ⚡ LLVM backend + LSP + urusfmt
v0.3.0   → 🔒 borrow checker (affine ownership, lifetime elision)
v0.4.0   → 🧵 urus.async with structured concurrency, urus.net
v0.5.0   → 🧰 urus.crypto, json, toml, time, rand
v1.0.0   → 🦬 self-hosted, language frozen, external pen-test passed
```

Per-version milestones, gate criteria, and out-of-scope notes live in
[`docs/documentation/11-ROADMAP-DETAILED.md`](./docs/documentation/11-ROADMAP-DETAILED.md).

---

## 🧪 Testing

Two simple categories:

```
tests/
├── run/        programs that must compile AND run without error
└── fail/       programs whose errors are expected (incl. SEC-01 PoC)
```

```bash
# Quick local sweep
bash scripts/run-tests.sh

# Recommended pre-PR: sanitizer build
cmake -B compiler/build -S . -DCMAKE_C_FLAGS="-fsanitize=address,undefined -g -O1"
cmake --build compiler/build
bash scripts/run-tests.sh
```

Full guide:
[`docs/documentation/08-TESTING.md`](./docs/documentation/08-TESTING.md).

> [!NOTE]
> **CI is live** (`v0.0.1-b022`/`b023`): gcc / clang / macOS / MSVC-cmake
> build matrix, an ASan + UBSan job over the full harness, and a 5-minute
> libFuzzer run per PR. See
> [`.github/workflows/ci.yml`](./.github/workflows/ci.yml).

---

## 🤝 Contributing

We welcome help — pre-alpha is exactly when a contributor can shape the
project for years.

> [!IMPORTANT]
> **Hard rule:** commits must **NOT** contain a `Co-Authored-By` line.
> All commits are purely under the author's name.
> See [`docs/documentation/09-CONTRIBUTING-DEEP.md`](./docs/documentation/09-CONTRIBUTING-DEEP.md) → "Commits".

The short version:

1. Open an issue (or draft PR with a question) before large work.
2. Branch from `main`: `feat/…`, `fix/…`, `docs/…`.
3. One topic per commit. Imperative subject ≤ 72 chars.
4. Run `bash scripts/run-tests.sh` with ASan + UBSan locally.
5. Update the matching doc in the same PR.
6. Add a `CHANGELOG.md` entry under the unreleased section.
7. If you ship a user-visible change, append a build entry to
   [`docs/archive/INDEX.md`](./docs/archive/INDEX.md) using the format
   in [`15-VERSIONING.md`](./docs/documentation/15-VERSIONING.md).

Long form:
[`docs/documentation/09-CONTRIBUTING-DEEP.md`](./docs/documentation/09-CONTRIBUTING-DEEP.md)
· short form:
[`CONTRIBUTING.md`](./CONTRIBUTING.md)
· governance:
[`GOVERNANCE.md`](./GOVERNANCE.md).

### Good first issues

- 🟢 Improve diagnostics: multi-span rendering (rustc-style).
- 🟢 More tests in `tests/run/` — one per language feature.
- 🟢 Wire a dedicated libFuzzer harness for the lexer (the
  whole-pipeline `fuzz_compile` target exists; a lexer-only one would
  fuzz deeper per cycle).
- 🟡 Reproducible builds (Tier-1 hardening item).
- 🟡 Real type inference in sema — replace codegen's best-effort
  local-type table (v0.0.3 headline).
- 🟠 F-TY-2 full monomorphisation for payloads > 16 bytes (v0.0.2
  headline).

---

## 🛡 Security policy

> [!CAUTION]
> **Do not file public issues for exploitable security findings.**
> Email **urusfoundation@gmail.com** (placeholder until the policy ships).
> Disclosure window: 90 days.

URUS v0.0.1 had **10 stop-ship findings**, documented openly because we
believe honesty about pre-alpha status protects users better than
marketing. **All 10 are closed as of `v0.0.1-b015`**, each with a
regression test under `tests/fail/SEC-*`. The full report — with
severity, attack scenario, PoC, and fix per finding — lives in
[`docs/security/SECURITY-AUDIT.md`](./docs/security/SECURITY-AUDIT.md).

Headline finding (closed in `b014`):

> `F-MEM-1` — the codegen pasted f-string `{...}` placeholder bytes
> verbatim into emitted C; a malicious `.urus` file could land
> arbitrary C in the produced binary. **Critical.** Fixed by
> `validate_fmt_placeholder` (placeholders must match
> `IDENT (.IDENT)*`), and since `b030` a codegen-stage error can no
> longer write an artifact while exiting 0.

Scorecard (honest, not aspirational):

| Dimension              | Score      |
|------------------------|-----------:|
| Memory safety          | **2 / 10** |
| Compiler security      | **2 / 10** |
| Runtime security       | **3 / 10** |
| Enterprise readiness   | **0 / 10** |
| **Overall**            | **2.5 / 10** |

Until those numbers move, do not ship URUS in a context that compiles
input you did not write yourself.

---

## ⚖️ License

URUS is dual-licensed under either:

- **[Apache License, Version 2.0](./LICENSE)**, or
- **MIT License**

at your option. Pick whichever fits your project — both grants apply
identically to all source in this repository.

`SPDX-License-Identifier: Apache-2.0 OR MIT`

---

## 🙏 Acknowledgements

URUS stands on the shoulders of every systems language that came
before it. Specific debts:

- **C** — for being a target you can trust to exist on every machine.
- **Rust** — for showing that ownership-as-types is the right answer.
- **Zig** — for proving that "transpile to C is a virtue" is not a
  cop-out, and for the structured-concurrency hint URUS will adopt at v0.4.
- **Go** — for showing how to keep a language small.
- **The `Urus-archive` codebase** — for being the *wrong* design that
  taught us the *right* one. v0.0.1 is the second draft of a love letter.

And to every developer who reads the source, runs the tests, files an
honest issue, or sends a tiny PR — **thank you.** That is how a language
becomes a community.

---

<div align="center">

*Rooted strong, wild in execution.*

🦬 — *URUS, 2026-06-07 — v0.0.1-b031.*

[**↑ back to top**](#urus)

</div>
