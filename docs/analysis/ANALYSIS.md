# URUS — Deep Architectural Analysis & Long-Range Roadmap

> *A 15-phase exhaustive review of URUS v0.0.1, the strengths and the
> sharp edges, with a prioritised list of the next 100 things to do.*

This document is intentionally critical. URUS v0.0.1 ships as a working
proof-of-concept, but **almost every component still has the seeds of
serious technical debt**. The point of this document is to surface the
debt before it ossifies.

For each finding we give: **Severity** (Low/Med/High/Critical), the
**Root cause**, the **Long-term impact**, and a concrete **Proposed fix**.
At the end we collect everything into a prioritised top-100 list.

---

## Phase 1 — Project understanding

### 1.1 Goals (as shipped)
- Systems language: predictable performance, no GC.
- Memory safety story comparable to Rust but with a gentler learning curve.
- Bootstrap path: C → C, eventually URUS → LLVM, eventually URUS → URUS.
- Global keyword set (English) so the language is accessible to non-Indonesian readers.

### 1.2 Architecture (today, v0.0.1)

```
                ┌───────────┐
.urus  ───►──── │   lexer   │ ──► tokens
                └───────────┘
                       │
                       ▼
                ┌───────────┐
                │  parser   │ ──► AST (arena-allocated)
                └───────────┘
                       │
                       ▼
                ┌───────────┐
                │   sema    │ ──► annotated AST + diagnostics
                └───────────┘
                       │
                       ▼
                ┌───────────┐
                │ codegen_c │ ──► one C99 translation unit
                └───────────┘
                       │
                       ▼
                  external cc
```

### 1.3 What's missing entirely
- **Intermediate Representation (IR)**. Codegen reads the AST directly.
- **Module loader**. Files are compiled in isolation.
- **Optimizer**. Every optimisation is whatever the host C compiler gives us.
- **Package manager**.
- **LSP / IDE integration**.
- **Standard library** beyond `println`.

### 1.4 Risks
- The AST-as-IR shortcut works for v0.0.1 (~1k LOC URUS source). It will be
  the single biggest blocker by v0.2 because every optimisation, every
  borrow check, and every codegen variant duplicates AST-walking logic.

---

## Phase 2 — Language design review

### Finding 2.1 — Ambiguous `T { ... }` parsing
- **Severity:** High
- **Root cause:** `Identifier {` is currently parsed as a struct literal
  only when the identifier starts uppercase. This convention bleeds into
  semantics — `i32 { … }` works, `vec { … }` does not.
- **Impact:** When generics and `impl Trait` arrive, the rule becomes
  pathological: `Vec<T> { … }` vs `Vec < T > {}` ambiguity.
- **Fix:** Adopt Rust's "no struct literal in restricted expression
  contexts" — disallow `S {…}` in `if`, `while`, `match` scrutinees.
  Require parentheses there.

### Finding 2.2 — Path syntax with both `.` and `::`
- **Severity:** Medium
- **Root cause:** `use a.b.c` and `use a::b::c` both parse. Convenient for
  newcomers, but ambiguous when a module path contains a value of the same
  name (e.g. `foo.bar` could be a field access).
- **Fix:** Reserve `::` for paths, `.` for field/method access. Deprecate
  `.` in `use` after v0.1.

### Finding 2.3 — `return` is mandatory; block-tail expressions are awkward
- **Severity:** Low
- **Root cause:** Spec allows tail expressions but most examples still use
  `return`. Mixed style hurts readability.
- **Fix:** Document a single canonical form in the style guide
  (prefer trailing expressions when the function fits in one screen).

### Finding 2.4 — No `?` propagation
- **Severity:** Medium
- **Impact:** Every error-returning function needs verbose `match` blocks.
- **Fix:** Add `?` as a postfix operator that desugars to
  `match { Ok(v) => v, Err(e) => return Err(e) }`. Lands in v0.1.

### Finding 2.5 — Missing visibility levels
- **Severity:** Medium
- **Root cause:** Only `pub` and "private" today. No `pub(crate)`, no friend visibility.
- **Fix:** Adopt Rust's `pub(crate)` / `pub(super)` once we have multi-file modules.

### Finding 2.6 — String literal `{}` interpolation is too magical
- **Severity:** Medium
- **Root cause:** Interpolation works **only** inside `println` / `print` /
  `eprintln`. Anywhere else, `"hello {name}"` is the literal nine characters.
- **Impact:** Surprising. Users will try to use it everywhere.
- **Fix:** Generalise to an `fstr!"..."` macro/literal once the macro system
  exists (v0.1.x). Until then, document the limitation prominently.

### Finding 2.7 — Mutability is a paint-job, not a guarantee
- **Severity:** High (becomes Critical at v1.0)
- **Root cause:** `let mut x` exists but is **not** enforced at sema —
  the compiler will accept assignments to immutable bindings.
- **Fix:** Sema must reject re-assignment to bindings declared without
  `mut`. Currently easy; gets harder once we have aliasing.

### Finding 2.8 — No formal grammar
- **Severity:** Low
- **Root cause:** `SPEC.md` has BNF-ish notation but no machine-verifiable
  grammar.
- **Fix:** Publish a Pest/Lark grammar alongside the spec; CI verifies the
  hand-written parser accepts every test that the grammar accepts.

---

## Phase 3 — Type system analysis

### Finding 3.1 — `Result` / `Option` are *non-generic* tagged unions
- **Severity:** Critical
- **Root cause:** `urus_Result` stores its payload as `int64_t`. Anything
  larger silently corrupts the stack. Anything smaller wastes space but is
  safe.
- **Impact:** As soon as someone returns `Result<MyBigStruct, str>` it will
  crash. We've shipped a footgun.
- **Fix:** **Monomorphise** `Result<T,E>` and `Option<T>` per instantiation
  at codegen time. The codegen knows the concrete `T` and `E` for every
  call site; emit `urus_Result_Aurochs_str` etc. Lands in v0.1.0.

### Finding 3.2 — No type inference
- **Severity:** High
- **Root cause:** Sema doesn't compute types. Codegen relies on C's
  `__auto_type` to fake it. This breaks for MSVC, breaks for any future
  LLVM backend, and prevents type-directed error messages.
- **Fix:** Implement Hindley-Milner-lite for the v0.0.x line:
  bidirectional inference with type variables and unification. Local-only
  (no top-level generic inference) is fine for v0.1.

### Finding 3.3 — No exhaustiveness checking
- **Severity:** High
- **Root cause:** `match` accepts any combination of arms. Missing a case
  is silently UB at runtime (the generated `if/else if` chain just falls through).
- **Fix:** Compute the variant set of the scrutinee; require either a
  wildcard arm or all variants present. Algorithm: rustc's "Witness" approach.

### Finding 3.4 — No nullability discipline
- **Severity:** Critical for stdlib design
- **Root cause:** Raw pointers (`*T`) can be null. There's nothing in the
  type system that says "this can't be null".
- **Fix:** Establish the rule: **references (`&T`/`&mut T`) are never
  null; raw pointers may be**. Sema enforces it on `as`-casts only at first
  (`&T as *T` is fine; `*T as &T` requires `unsafe`).

### Finding 3.5 — No trait coherence
- **Severity:** Low today, Critical by v0.4
- **Root cause:** Traits don't exist yet. When they do, the orphan rule
  (impl is in the crate of the trait or the type) must be decided early.
- **Fix:** Document the orphan rule **before** shipping traits.

### Finding 3.6 — Generic syntax `Vec<T>` collides with comparison
- **Severity:** Medium
- **Root cause:** `let x = a < b > c;` is ambiguous with `let x = a::<b>();`.
- **Fix:** Adopt the Rust "turbofish" `::<>` for type argument disambiguation
  in expressions. Document it in the spec before generics ship.

---

## Phase 4 — Compiler analysis

### Finding 4.1 — Single-pass lexer is correct but inflexible
- **Severity:** Low
- **Strength:** ~600 LOC, handles every v0.0.1 token, position-tracked.
- **Weakness:** No tokens are reused across files. When we add a module
  loader, we'll want tokens to live in a shared arena.
- **Fix:** Move all interned strings to a global `StringInterner`. Token
  text becomes `StringId` (32-bit), not `const char *`.

### Finding 4.2 — Parser is correct but lossy
- **Severity:** Medium
- **Root cause:** The parser drops trivia (whitespace, comments). An LSP
  needs to format code and produce hover info; both require the original
  tokens.
- **Fix:** Adopt a **lossless syntax tree** (rowan/Roslyn style) by v0.2.
  The compiler keeps the AST it has today; the formatter/LSP keeps the
  green tree.

### Finding 4.3 — Sema does name resolution but not type checking
- **Severity:** High
- **Root cause:** see 3.2. This puts a hard ceiling on diagnostic quality —
  you can't say "expected `i64`, found `str`" because we don't know the types.
- **Fix:** see 3.2.

### Finding 4.4 — No IR
- **Severity:** Critical by v0.2
- **Root cause:** Codegen walks the AST. Optimisation passes will have to
  walk it again. Every transformation duplicates the visitor.
- **Fix:** Introduce **URUS-MIR** — a mid-level IR with explicit basic
  blocks, SSA, and explicit `Drop`. Codegen targets MIR; LLVM is a MIR-to-LLVM
  lowering. Lands in v0.1.x as an internal refactor; promoted to public API in v0.2.

### Finding 4.5 — Generated C requires GCC/Clang
- **Severity:** Medium
- **Root cause:** Codegen uses `__auto_type` and statement expressions
  (`({ ... })`). MSVC does not support either.
- **Fix:** Once type inference (3.2) lands, drop both. `let x = expr` becomes
  `T x = expr;` where T is the inferred type. Replace stmt-expr with
  temporary variables and `goto`-free control flow.

### Finding 4.6 — Diagnostics are good but lack suggestions
- **Severity:** Medium
- **Strength:** SrcLoc + snippet + caret + level coloring.
- **Weakness:** No "did you mean ...?" suggestions, no notes attached to
  the same error, no multi-span errors (e.g. "this `let` is immutable / so
  this assignment fails").
- **Fix:** Migrate diagnostics to a structured format (id + primary span +
  secondary spans + suggestions). Render via a pluggable formatter.

### Finding 4.7 — No compile-time evaluation
- **Severity:** Medium
- **Root cause:** `const X: u64 = 1 + 2;` works only because C const-folds
  it. URUS itself has no const evaluator.
- **Fix:** A small interpreter on the AST for the constant subset; required
  for array sizes (`[u8; N]`) and for compile-time `match` exhaustiveness.

### Finding 4.8 — Method resolution is name-only
- **Severity:** High
- **Root cause:** `cg_expr` for `EX_METHOD_CALL` finds the *first* impl
  block defining a method with that name. Two structs with a method called
  `len()` collide.
- **Fix:** During sema, attach the **receiver's type** to every method call
  node. Codegen reads the resolved type, not a name lookup.

---

## Phase 5 — Runtime analysis

### Finding 5.1 — No allocator abstraction
- **Severity:** High
- **Root cause:** Anything that wants memory calls `malloc`. There's no
  way to plug in an arena, a tracking allocator, or a sandboxed allocator.
- **Fix:** Define a `urus_alloc_t` vtable; all stdlib functions take an
  optional allocator (default = the system one). Models: Zig, Odin.

### Finding 5.2 — No concurrency primitives
- **Severity:** Critical for v0.4
- **Root cause:** There is no thread/coroutine model. Adding them
  retroactively while keeping the borrow checker honest is one of the
  hardest design problems in language engineering.
- **Fix:** Commit to **structured concurrency** (a scope owns its tasks)
  *now*, before any async/await design. This constrains the design space
  but produces a saner result.

### Finding 5.3 — `panic` calls `abort()` unconditionally
- **Severity:** Low
- **Impact:** No way for an embedding host (e.g. a unit test runner) to
  recover.
- **Fix:** Replace `abort()` with `longjmp` to a thread-local
  `urus_panic_recovery` if registered, else abort. Standard pattern; one
  page of code.

### Finding 5.4 — `urus_fmt_arg` arrays go on the stack
- **Severity:** Low
- **Strength:** Zero-allocation in the common case.
- **Weakness:** A `println` with 256 interpolations blows the stack.
- **Fix:** Document the limit, switch to a heap path when arg count > 32.

### Finding 5.5 — No backtrace on panic
- **Severity:** Medium
- **Fix:** Vendor a tiny libbacktrace replacement, or rely on system
  libraries. Demand a stack capture on every panic in debug builds.

---

## Phase 6 — Security audit

### Finding 6.1 — String literals are stored as raw `const char *`
- **Severity:** High
- **Root cause:** `urus_str_from_lit` creates a fat pointer over the C
  literal. The literal is null-terminated, but the fat pointer length is
  trusted. Future stdlib code might assume the buffer length is exactly
  the recorded length.
- **Impact:** A miscompile (wrong length) leads to OOB reads.
- **Fix:** Always emit lengths from codegen; never compute via `strlen()` at runtime.

### Finding 6.2 — `match` payload extraction casts via `intptr_t`
- **Severity:** Critical
- **Root cause:** See 3.1. Lossy when sizeof(T) > sizeof(int64_t).
- **Fix:** See 3.1.

### Finding 6.3 — No format-string injection (yet)
- **Severity:** Low
- **Strength:** `urus_println_fmt` takes a typed arg array, not a
  user-supplied format string. Cannot be exploited à la `printf(user_input)`.
- **Fix:** Document this design choice in the spec so a future "fast path"
  PR doesn't reintroduce a format-string vulnerability.

### Finding 6.4 — No supply-chain story
- **Severity:** Critical at v0.1 (package manager)
- **Fix:** Mandatory:
  - Cryptographic checksums (BLAKE3) of every dependency.
  - Lockfile required.
  - Build scripts sandboxed (no network, no filesystem outside `OUT_DIR`).
  - Publishers must sign packages (sigstore-style).

### Finding 6.5 — Compiler trusts the C compiler completely
- **Severity:** Medium
- **Root cause:** "Reflections on Trusting Trust." The C toolchain that
  builds `urusc` could itself be compromised.
- **Fix:** Document a reproducible-build recipe; aim for stage-2 = stage-3
  byte-identical builds. Required for any v1.0 stability promise.

---

## Phase 7 — Performance audit

### 7.1 Compiler throughput

- Bump arena → very fast allocation. Probably already faster than rustc
  per-LOC, simply because we don't do much.
- Lexer is single-pass, no regex. Good.
- Parser is recursive descent with one-token lookahead. Good.
- **Bottleneck (predicted, untested):** the lookup loops in sema are
  `O(scopes × symbols)`. Fine at 1k LOC, painful at 100k. Replace with hash maps.

### 7.2 Generated-code performance

Today: whatever the host C compiler does. That's actually a strong starting
point — `gcc -O3` is competitive with anything Rust does. The risk is that
our `__auto_type`-heavy emission prevents some optimisations (especially
inlining of `urus_println_fmt`'s arg array). Measure before fixing.

### 7.3 Binary size

`urusc.exe` will be ~200KB. Negligible. The runtime is header-only and tiny.

### 7.4 Comparison table (predicted; not yet benchmarked)

| Workload         | URUS v0.0.1 | C   | Rust | Go  |
|------------------|-------------|-----|------|-----|
| `fib(40)`        | == C        | 1×  | 1.0× | 1.1×|
| Hashmap insert   | n/a yet     | 1×  | 1.0× | 1.5×|
| Cold startup     | == C        | 1×  | 1.5× | 4×  |
| Compile speed    | very fast   | n/a | slow | fast|

---

## Phase 8 — Standard library review

There is none. The runtime alone provides `println` and `panic`. Filling
this is a design problem more than a coding problem — every API decision
you make today is one you'll regret later.

### Recommended ordering for v0.1+

1. `urus.collections` — `Vec<T>`, `HashMap<K, V>`, `BTreeMap<K, V>`, `String`.
2. `urus.io` — `File`, `BufReader`, `BufWriter`, `Stdin`, `Stdout`, `Stderr`.
3. `urus.fs` — `Path`, `read`, `write`, `metadata`.
4. `urus.error` — `Error` trait, source-chaining.
5. `urus.time` — monotonic + wall + duration; timezone-aware in v0.3.
6. `urus.sync` — `Mutex`, `RwLock`, atomics.
7. `urus.thread` — OS threads + scoped spawn.
8. `urus.net` — sockets, TLS by v0.5.
9. `urus.crypto` — formally reviewed primitives only; v0.7+.
10. `urus.serde` — JSON, TOML, YAML — third-party at first, blessed by v1.

### Anti-patterns to avoid

- `Vec::new` returning a `Vec` whose backing buffer is allocated on the
  global heap implicitly. Always require an explicit allocator at construction.
- `String` as a `Vec<u8>` with a UTF-8 invariant — fine, but document the
  invariant in the type's docs, not just the title.
- Mutable global state. Anywhere.

---

## Phase 9 — Tooling review

We have:

- The compiler.
- A test runner (`scripts/run-tests.ps1`).

We don't have anything else. Prioritised order:

1. **`urus fmt`** — opinionated, no configuration. Built on the lossless syntax tree (4.2).
2. **`tanduk`** — package manager + build system.
3. **`urus-analyzer`** — LSP, reusing the compiler crates.
4. **`urus lint`** — clippy-style; rules are *separate crates* so they evolve faster than the compiler.
5. **`urus doc`** — a doc-comment renderer.
6. **`urus test`** — first-class test runner (today: `urus.uji` planned, but
   in v0.1 just run anything with `#[test]`).
7. **`urus bench`** — statistical benchmarking, criterion-like.

---

## Phase 10 — Ecosystem review

Nonexistent in v0.0.1. The hard problems are not technical:

- **License clarity.** Apache-2.0 + MIT dual-license is solid; document it
  in every file.
- **Governance.** See `GOVERNANCE.md`. The BDFL phase must end before the
  10th non-trivial contributor lands.
- **Trademark.** Register "URUS" early; otherwise someone will. The
  foundation owns the mark; everyone can use the language.
- **Code of Conduct.** Contributor Covenant 2.1; enforcement is a named
  committee, not the BDFL.

---

## Phase 11 — AI era readiness

This is where URUS can leapfrog older languages — we're designing now,
when AI agents are writing 30-60% of all new code.

### Recommendations

- **Machine-readable diagnostics.** `urusc --json-diagnostics` from v0.0.2.
  Every error has a stable code (`E0001`, …), span, suggestion, and
  category. The LLM can act on it.
- **Stable AST dump.** `urusc --emit-ast=json` for every AST node. Both
  humans and agents read it.
- **Project metadata as a manifest.** `tanduk` lockfile in plain TOML so
  agents don't need to parse a binary.
- **Documentation comments are addressable.** `///` on every public item;
  `urus doc --json` emits a searchable index.
- **Compiler-as-a-library.** `libtanduk` and `liburus_analyzer` — agents
  embed the compiler instead of shelling out. Same code path as the LSP.
- **Build determinism.** Same input → same output bytes. Cache hits matter
  more when an agent is building 1000 variants/hour.

---

## Phase 12 — Future-tech compatibility

| Target                 | URUS-readiness                                  |
|------------------------|-------------------------------------------------|
| WebAssembly + WASI     | Easy — emit-c → emcc, or via LLVM in v0.2.       |
| Cloud-native / serverless | Once the binary size and startup are tight.  |
| Edge / embedded        | C transpile already targets bare-metal toolchains. |
| `no_std`-equivalent    | Plan from day 1: `urus.alloc` is a vtable.       |
| GPU / SIMD             | `urus.simd` as portable intrinsics; v1.x.        |
| Quantum                | N/A — no language design action needed yet.       |
| AI infrastructure      | `urus.tensor` planned as a third-party crate.    |

---

## Phase 13 — Hidden problems

### 13.1 The codegen / AST coupling
URUS codegen reads the AST directly. Every future feature (borrow check,
const eval, MIR) will want a different shape. We will end up rewriting
codegen *twice* if we don't introduce MIR before v0.2.

### 13.2 The mutability lie
We documented `let mut` but don't enforce it. The first user to discover
this will lose trust in the type system. **Fix in v0.0.2 at the latest.**

### 13.3 The `Result` payload trap
`urus_Result` stores payloads in 64 bits. The day someone returns
`Result<Aurochs, str>`, their compiled program will silently corrupt the
stack. **This is a stop-ship for v0.1.**

### 13.4 The "we will add traits later" debt
Trait coherence rules must be designed before *any* trait code ships, or
we'll pick rules to fit existing impls — which is how Ruby got refinements.

### 13.5 The build-system schism
Today: CMake. The package manager will use `tanduk`. Two build systems for
the same language is a recipe for ecosystem fragmentation. Pick one for
end users (`tanduk`); the CMake build of the compiler itself can stay as
"how we bootstrap".

### 13.6 The benchmark-free claim
The README implies performance parity with C. We have no benchmarks. Add
a benchmark suite **before** the first marketing post, or we'll find out
on Hacker News.

---

## Phase 14 — Radical ideas

The point of this phase is to be unreasonable. Some of these are bad. The
exercise is worth doing.

### 14.1 First-class **structured concurrency**
A `task` is a value that *cannot* outlive its scope. There is no
`tokio::spawn` equivalent. This is more restrictive than Rust async but
makes 90% of "where did my task go?" bugs unrepresentable.

### 14.2 Effect system (small)
Functions declare what they touch: `fn read_file(p: Path) ! io::Error`.
The `!` is read "may raise". Cheaper than monads, easier than checked
exceptions. Inspired by Koka and Roc.

### 14.3 Capability-based imports
A file with `use std.fs` cannot accidentally make network calls, because
`std.fs` is a *capability*. Tests can pass a stub. Compilation knows the
maximal blast radius of a module.

### 14.4 **Compile-time-only generics**
No runtime polymorphism for generics. Every `Vec<T>` is a fully separate
type. We get C++ template performance without C++ template error pages —
because we keep the type-class trait bounds Rust-style.

### 14.5 Property-test built into the language
`#[for_all]` annotations on tests run them with shrinking property data.
Inspired by Hypothesis, but native.

### 14.6 Editable compile errors
Every diagnostic ships with a deterministic auto-fix. Pressing `f` in the
LSP applies it. We make this contract explicit: every new diagnostic must
come with either a suggestion or a justification for why none is possible.

### 14.7 **Time-travel debugger** as a first-class build target
`urusc --target=replay` instruments every assignment. The runtime records
to a ring buffer. Replays are reproducible to the byte. Inspired by rr,
PernosCo, and TLA+. Hard to ship; insane DX win when it works.

### 14.8 Built-in observability
Every fn boundary has an OpenTelemetry hook compiled in (zero-cost if
not enabled). Production debugging is a build-flag away.

### 14.9 Native database type
A `query!"SELECT … WHERE id = {id}"` macro that *checks the SQL against a
schema at compile time*. Done in F# (Type Providers), done in Rust (sqlx),
but never made native.

### 14.10 Formal verification subset
A `pure fn` may not perform side effects, and its body is exported in SMT-LIB
for an external prover (Z3, CVC5). Compile errors when the prover can find
a counter-example.

---

## Phase 15 — Roadmap

### Immediate (v0.0.2 — within a month)

- Fix the `let mut` lie (Finding 2.7).
- Fix the `Result` payload trap (Finding 3.1) — at minimum, error out at
  codegen on any payload > 64 bits.
- Drop GCC extensions from emitted C (Finding 4.5) — start with `__auto_type`.
- Exhaustiveness checking (Finding 3.3).
- Numeric type checking (Finding 3.2 — first pass).
- `--json-diagnostics` (Phase 11).
- More tests; especially around `match`.
- Multi-file modules.

### High priority (v0.1.x — 1-3 months)

- `tanduk` package manager (Phase 8 / Finding 6.4).
- `?` operator (Finding 2.4).
- Monomorphic generics.
- Traits (single dispatch).
- `urus fmt`.
- `urus-analyzer` (LSP).
- A small stdlib: `Vec`, `HashMap`, `String`, basic IO.

### Medium priority (v0.2.x — 3-6 months)

- **MIR.** Refactor codegen to lower from MIR, not AST.
- **LLVM backend.**
- Lossless syntax tree (Finding 4.2).
- Compiler-as-a-library.
- Property-based testing native.

### Long-term (v0.3.x — 6-12 months)

- Borrow checker.
- Structured async/await.
- Capabilities / effects.
- Cross-platform target matrix in CI: Linux, macOS, Windows, WASM, bare-metal.

### 5-year

- Self-hosted compiler.
- 1.0 release with language freeze.
- Tier-1 platform support: Linux/macOS/Windows on x86_64 and ARM64.
- Foundation registered.
- 1000+ packages on the tanduk registry.

### 10-year

- Top-10 language by GitHub stars in its category (systems / no-GC).
- At least one major OSS infra project (database / OS / browser engine) has a
  URUS port.
- Formal verification subset in use in safety-critical code.

### 20-year

- A general-purpose language still considered modern: not a museum piece.
- Cross-pollination with whatever-comes-after-Rust without throwing away
  the v1.0 contract.

---

## Scorecard (URUS v0.0.1)

The numbers below are **honest self-assessments**, not aspirations.

| Dimension          | Score | Comment |
|--------------------|------:|---------|
| **Security**       | 4/10  | No supply chain; `Result` payload trap; mutability not enforced. |
| **Performance**    | 7/10  | Compiler is fast; emitted C is good thanks to GCC/Clang. Untested vs Rust. |
| **Scalability**    | 3/10  | AST-as-IR caps us. Module loader missing. |
| **Maintainability**| 6/10  | Small, well-commented, arena-based. Will get worse without MIR. |
| **Enterprise readiness** | 1/10 | Pre-alpha; no stdlib, no LSP, no package manager. |
| **AI readiness**   | 5/10  | AST is structured; needs JSON dump + stable error IDs. |
| **DX**             | 5/10  | Diagnostics already have spans + carets. No suggestions yet. |
| **Ecosystem**      | 0/10  | None exists. |
| **Cross-platform** | 6/10  | C transpile is portable; build system tested only on Windows. |
| **Future-proofing**| 6/10  | The hard decisions (concurrency, ownership) are not yet made — which is good, if we choose well. |
| **Overall**        | **4.3 / 10** | A solid skeleton. Everything still to play for. |

---

## TOP 100 IMPROVEMENTS — RANKED

The numbers correspond to the prioritised work-list. Each entry: **title**
— *severity* — short rationale.

### Critical (1–15)

1. **Monomorphise `Result<T,E>` and `Option<T>`** — Critical — silent corruption today.
2. **Enforce `let mut`** — Critical — restores trust in the type system.
3. **Exhaustiveness check for `match`** — Critical — UB on a missed arm.
4. **Drop GCC extensions from emitted C** — High — unlocks MSVC users.
5. **Real type inference** — High — every diagnostic improves.
6. **Method resolution by receiver type, not name** — High — collisions today.
7. **Lockfile + signed packages from day 1 of `tanduk`** — Critical — supply chain.
8. **Sandbox build scripts** — Critical — same.
9. **Introduce MIR** — High — every future feature depends on it.
10. **`?` operator** — High — error handling ergonomics.
11. **Numeric overflow semantics: panic in debug, wrap in release** — High — predictability.
12. **Reproducible builds for `urusc` itself** — High — trust-bootstrapping story.
13. **Backtrace on `panic`** — High — debuggability.
14. **Multi-file module loader** — High — usability cliff today.
15. **`--json-diagnostics`** — High — AI integration.

### High (16–40)

16. **Const evaluator** — array sizes, exhaustiveness.
17. **`StringInterner`** — perf + memory.
18. **`urus fmt`**.
19. **`urus-analyzer` (LSP)**.
20. **`tanduk new` / `tanduk build` / `tanduk run` / `tanduk test`**.
21. **Monomorphic generics** for user types.
22. **Single-dispatch traits**.
23. **Stdlib `Vec`, `HashMap`, `String`** — opinionated allocators.
24. **`urus.io::File`** with reasonable error types.
25. **`urus.fs::read`, `write`, `metadata`**.
26. **`Drop` trait** — deterministic cleanup.
27. **`unsafe { }` blocks**.
28. **References checked at sema** (no aliasing rules yet).
29. **Borrow checker, scope-based** — affine ownership first.
30. **Type parameter bounds: `fn f<T: Display>(x: T)`**.
31. **`pub(crate)` / `pub(super)`** visibilities.
32. **`urus doc` + `///` doc-comments**.
33. **`urus test` test runner**.
34. **Benchmark suite** — public, on CI.
35. **`urus lint` with rule plugins**.
36. **Lossless syntax tree** for tooling.
37. **Compiler-as-library** crate split.
38. **VS Code extension**.
39. **`urus.collections` enhanced: `BTreeMap`, `VecDeque`, `LinkedList` (the third only for completeness)**.
40. **`urus.error` trait + source chaining**.

### Medium (41–70)

41. **LLVM backend**.
42. **Optimisation: inliner, mem2reg, GVN, dead-code**.
43. **`urus.time` monotonic + wall + timezone**.
44. **`urus.sync::Mutex`, `RwLock`, `Atomic*`**.
45. **`urus.thread::scope` (structured)**.
46. **`urus.channel`**.
47. **Coroutines / state-machine async**.
48. **`urus.net` — TCP, UDP**.
49. **`urus.net::tls`** via OS APIs.
50. **`urus.http` server + client**.
51. **`urus.regex`** (probably wrapping a vetted C library at first).
52. **`urus.json`, `urus.toml`**.
53. **WASM target**.
54. **`#[no_std]` equivalent**.
55. **ARM64 + x86_64 first-class CI**.
56. **`urus.simd`** portable intrinsics.
57. **Macro system** (declarative, hygienic).
58. **Procedural macros** (compile-time code execution, sandboxed).
59. **Property-based testing native**.
60. **Coverage tool**.
61. **Fuzzer integration** (afl, libfuzzer).
62. **Effect / capability system** (mini-version first).
63. **Refinement-type subset**.
64. **`urus.tensor` (likely third-party but blessed)**.
65. **`urus.crypto` — primitives chosen, not invented**.
66. **`urus.crypto::tls` — pure URUS TLS by v1**.
67. **Cross-compilation matrix**.
68. **Sourcemap to URUS from emitted C via `#line`**.
69. **Time-travel debugger prototype**.
70. **Online playground** (`urus-lang.dev`).

### Long tail (71–100)

71. **Trait objects** (`dyn Trait`).
72. **GATs (generic associated types)** — only when concretely needed.
73. **Specialisation** — careful; we'll learn from Rust's pain.
74. **Refinement of `unsafe` semantics** — TBA RFCs.
75. **Foreign-function interface (FFI) review**.
76. **C ABI compatibility annotation**.
77. **Stable ABI for shared libraries**.
78. **Cross-crate inlining** via MIR.
79. **PGO + ThinLTO integration**.
80. **Linker plugin** (own linker? no — use lld).
81. **`urus build --sysroot` for cross**.
82. **Tier-2 platforms: FreeBSD, OpenBSD, illumos**.
83. **Tier-3: NetBSD, Haiku, Redox**.
84. **Embedded HALs** for Cortex-M, RISC-V.
85. **GPU compute** via SPIR-V / CUDA codegen.
86. **Editor: Helix support**.
87. **Editor: JetBrains plugin**.
88. **Editor: Zed support**.
89. **Reference book (the URUS Book)** — separate repo.
90. **The URUS Cookbook** — recipes.
91. **The URUS Rustonomicon equivalent** (advanced/unsafe).
92. **Translation: docs into Indonesian, Japanese, Chinese**.
93. **Conference talks** — RustConf-equivalent partnership.
94. **University course materials** — open syllabus on GitHub.
95. **CRDT crate** — distributed-systems story.
96. **Actor framework** — `urus.actor`.
97. **Raft reference** — `urus.raft`.
98. **OS kernel demo** — small standalone OS in URUS.
99. **Database engine demo** — page cache, B+ tree, WAL.
100. **Compiler self-host, byte-identical reproducibility** — earns "1.0".

---

## A note on the scoring

A 4.3 / 10 today is a feature, not a bug. Pre-alpha languages should score
low — the whole point of a v0.0.1 release is to prove the pipeline exists.
What matters is whether the **plan** survives contact with reality:
fix the critical findings *in order*, do not skip MIR, do not ship
generics without monomorphisation, do not ship a package manager without
signatures. If we hold that line, URUS at v1.0 looks like a serious
contender. If we don't, URUS at v1.0 is a 2030 footnote.

The aurochs survived for ten thousand years on the European plain. The
project that bears its name should aim for the same scale.
