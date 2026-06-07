# Changelog

All notable changes to **URUS** are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/) and the project adheres
to [Semantic Versioning](https://semver.org/) once it reaches 1.0.
Until then, *any* minor or patch bump may carry breaking changes.

Each release section below is recapped at two levels of zoom:

1. **Build-level recap** — every push, commit batch, or PR-merge that
   produced a tracked build (matching files in
   [`docs/archive/`](./docs/archive/INDEX.md)).
2. **Per-version summary** — `Added` / `Changed` / `Fixed` / `Security`
   in the classic Keep-a-Changelog shape.

The legend of one-letter codes used in build entries:

```
A=Added  U=Updated  F=Fixed  R=Removed  D=Deprecated  S=Security
P=Performance  B=Breaking  O=Optimized  C=Changed  T=Testing
X=Experimental  M=Migration  N=Notes  I=Internal  L=Language  E=Ecosystem
```

Full convention: [`docs/documentation/15-VERSIONING.md`](./docs/documentation/15-VERSIONING.md).

---

## [Unreleased]

> Working toward the **v0.0.2 security release**.  The 10 stop-ship
> findings catalogued in
> [`docs/security/SECURITY-AUDIT.md`](./docs/security/SECURITY-AUDIT.md)
> are being closed incrementally on the `v0.0.1` line so each fix can
> ship, be tested, and bisect cleanly.  Builds in this window remain
> pre-alpha.

### Security

- `F-COMP-1` closed in `v0.0.1-b011` — parser recursion capped at
  `URUS_MAX_PARSE_DEPTH` (256).  `tests/fail/SEC-02_recursion_bomb.urus`
  is the regression test.
- `F-MEM-2` closed in `v0.0.1-b011` — `lex_string` no longer
  NULL-deref's on `realloc` failure, and refuses strings larger than
  `URUS_MAX_STR_LITERAL_BYTES` (16 MiB).
- `F-MEM-5` closed in `v0.0.1-b011` — `read_file` `stat()`s the input,
  refuses non-regular files, caps at `URUS_MAX_INPUT_BYTES` (64 MiB),
  and zeroes the tail on short reads.

### Changed (internal)

- `compiler/src/parser.c` — recursion-cap wrapper pattern. Every public
  `parse_type` / `parse_pattern` / `parse_precedence` / `parse_block`
  call increments a counter, with the body relocated to a `*_inner`
  helper so depth bookkeeping is exception-safe.
- `compiler/src/arena.c` — `arena_alloc` refuses `> URUS_MAX_ARENA_ALLOC`
  (256 MiB) before alignment rounding.  Closes the worst case of
  F-MEM-3.
- `compiler/src/strbuf.c` — `sb_reserve` has explicit overflow-before-add
  detection and bounds growth at `URUS_MAX_STRBUF_BYTES` (1 GiB).
  Closes F-MEM-4.

### Tests

- `tests/fail/SEC-02_recursion_bomb.urus` — F-COMP-1 regression.
- `tests/fail/SEC-03_input_too_large.urus` — file-size cap reproduction marker.
- `tests/run/08_strbuf_growth.urus` — keeps StrBuf-growth path covered after the guard rewrite.

- `F-MEM-7` closed in `v0.0.1-b012` — codegen emits an explicit element
  count; runtime entry points are `urus_*_fmt_n(args, count)`.  The
  back-compat sentinel scan is bounded at `URUS_FMT_MAX_ARGS = 1024`.
- `F-MEM-8` closed in `v0.0.1-b012` — `URUS_FMT_PTR_TAG` removed.
  `URUS_FMT_ANY` routes `char *` through `urus__fmt_from_cstr` which
  wraps the pointer in a `urus_str` via `strlen` at the call site.
- `F-TY-1` closed in `v0.0.1-b012` — `URUS_OPT_NONE / URUS_OPT_SOME`
  reassigned to `2 / 3` so they are disjoint from
  `URUS_RES_OK / URUS_RES_ERR` (`0 / 1`).

### Added

- Runtime: `urus_println_fmt_n` / `urus_print_fmt_n` /
  `urus_eprintln_fmt_n` / `urus_fmt_to_str_n` — length-aware variants.
- Runtime helper: `urus__fmt_from_cstr` for the `_Generic` char* arm.
- `URUS_FMT_MAX_ARGS = 1024` — bound on the back-compat sentinel scan.
- `tests/run/09_disjoint_tags.urus` — verifies Result/Option no longer alias.
- `tests/run/10_println_length.urus` — exercises the length-aware path
  and the new `char *` dispatch.

### Removed

- `URUS_FMT_PTR_TAG` removed from `urus_fmt_tag`; the `.p` member is
  gone from `urus_fmt_arg`'s union.  This is a **breaking change** for
  any code that named `URUS_FMT_PTR_TAG` directly.  No such code exists
  inside the URUS repository; downstream users on b011 or earlier need
  to rebuild.

- `F-COMP-2` closed in `v0.0.1-b013` — sema now tracks `Symbol.is_mut`
  and refuses assignment (or compound assignment) to immutably-bound
  names with `cannot assign to immutable binding '<name>' — declare it
  with 'let mut' or 'mut'`.  Field and index inherit mutability from
  their base place; deref / qualified path fall through to the C
  backend's const-correctness.  `tests/fail/SEC-04` and `SEC-05` are
  the regressions; `tests/run/11_let_mut.urus` is the positive smoke.
- `F-COMP-3` closed in `v0.0.1-b013` — sema rejects non-exhaustive
  `match` on `Result` (must enumerate `Ok` + `Err`), `Option` (must
  enumerate `Some` + `None`), and any user-defined enum whose variant
  set the arms belong to.  A `_` wildcard or bare-ident pattern
  satisfies the obligation.  Guarded arms do **not** count toward
  coverage.  `tests/fail/SEC-06` and `SEC-07` regress the Result and
  Option cases; `tests/run/12_match_wildcard.urus` proves the wildcard
  escape hatch still compiles.

### Added (b013)

- `tests/fail/SEC-04_assign_immut.urus`
- `tests/fail/SEC-05_assign_immut_compound.urus`
- `tests/fail/SEC-06_match_nonexhaustive_result.urus`
- `tests/fail/SEC-07_match_nonexhaustive_option.urus`
- `tests/run/11_let_mut.urus`
- `tests/run/12_match_wildcard.urus`

### Changed (internal, b013)

- `Symbol` gained an `is_mut` bool; `declare_pattern` gained an
  `outer_mut` parameter that cascades through tuple and variant
  sub-patterns.  All 7 call sites updated.
- New helpers in `compiler/src/sema.c`: `classify_lhs`, `describe_lhs`,
  `check_lhs_mutable`, plus the `PlaceMut` enum.

- `F-MEM-1` closed in `v0.0.1-b014` — the URUS→C code-injection
  primitive is gone.  `validate_fmt_placeholder` re-lexes every
  f-string `{…}` and accepts only `IDENT (.IDENT)*` (`[_A-Za-z]`-led
  ident, ascii alnum / underscore continue, dot separators, no leading
  / trailing / consecutive dots, max 256 bytes).  Empty, unterminated,
  or call-syntax placeholders raise a precise diagnostic and never
  splice their bytes into the C output.  Original PoC at
  `tests/fail/SEC-01_fstr_injection.urus` now fails as designed;
  `tests/fail/SEC-08..SEC-10` cover the new rejected shapes;
  `tests/run/13_fstr_dotted.urus` smokes the legal forms.
- `F-TY-2` closed (partial) in `v0.0.1-b015` — Result / Option payload
  widened from a single `int64_t` to a 16-byte `urus_payload_t` union
  (`int64 / uint64 / double / void* / urus_str / uint64[2]`).
  `URUS_PAYLOAD_OF` uses `_Generic` to fill the right arm at the
  construction site, so `Ok(str)`, `Some(double)`, `Err(pointer)`
  round-trip without truncation.  `_Static_assert` keeps the layout
  pinned at 16 B.  Full monomorphisation for payloads > 16 B is
  deferred to v0.0.2.  Regressions:
  `tests/run/14_result_str_payload.urus`,
  `tests/run/15_option_float_payload.urus`.
- *Tier-1 hardening* in `v0.0.1-b016` — integer / float literal
  overflow surfaced via `errno`/`ERANGE`; numeric type-suffix stripped
  before parse; source must be valid UTF-8 (`urus__utf8_first_bad_offset`
  rejects embedded NULs, overlong encodings, lone surrogates,
  out-of-range code points, truncated trailers).
  `tests/fail/SEC-11_int_overflow.urus` and
  `tests/fail/SEC-12_hex_overflow.urus` are the regressions.

### Added (b014–b016)

- `validate_fmt_placeholder` and `URUS_FMT_MAX_PLACEHOLDER = 256` in
  `compiler/src/codegen_c.c`.
- `urus_payload_t` union and `URUS_PAYLOAD_OF(v)` `_Generic` dispatcher
  in `stdlib/runtime/urus_rt.h`.
- Typed payload accessors: `urus_payload_str`, `urus_payload_ptr`,
  `urus_payload_f`, `urus_payload_u`, `urus_payload_as(x, T)`.
- `urus__utf8_first_bad_offset(p, n)` in `compiler/src/main.c`.
- `tests/fail/SEC-08..SEC-12` and `tests/run/13..15`.

### Changed (internal, b014–b016)

- `cg_fmt_arg_array` gained a `SrcLoc loc` parameter; both call sites
  (`try_emit_println_call`, standalone `EX_FSTR_LIT`) pass it through.
  The legal-splice hot path is now `sb_putc`-based, no heap allocation.
- `compiler/src/lexer.c` includes `<errno.h>`, `<limits.h>`,
  `<stdint.h>`, `<diag.h>`; `lex_number` strips type suffix, zeroes
  `errno`, and refuses values that don't fit in `u64`.
- `compiler/src/main.c` calls the UTF-8 scanner right after the short-read
  zero-tail step; validation failure frees the buffer before return.

### Security (b017–b018)

- *Runtime, b017* — `urus_fmt_to_str_n` caps its rendered size at
  `URUS_FMT_MAX_RENDER_BYTES` (64 MiB); a runaway formatter can no
  longer turn into a multi-GiB allocation.  `urus_panic` caps its
  message at 4 KiB and tolerates NULL/empty payloads.
- *Sema, b018* — the `?` operator is rejected unless the enclosing
  function returns `Result<_,_>` or `Option<_>`.  Pre-b018 a stray
  `expr?` inside `fn main() -> ()` lowered to an early `return` of an
  incompatible type — an ABI corruption, not just a type error.
  `tests/fail/SEC-13_question_wrong_ret.urus` regresses it;
  `tests/run/17_question_ok.urus` smokes the legal form.

### Added (b017–b019)

- String helpers in `urus_rt.h` (b017): `urus_str_len`,
  `urus_str_is_empty`, `urus_str_eq`, `urus_str_cmp`,
  `urus_str_starts_with`, `urus_str_ends_with`, `urus_str_contains` —
  all length-aware, NUL-tolerant, `static inline`.
- `urus.io.read_line` (b019): first interactive-input primitive.
  Returns `Option<str>` — `Some(line)` stripped of `\n` (and one
  trailing `\r` for Windows pipes), `None` on EOF.  Line length capped
  at `URUS_READLINE_MAX_BYTES` (16 MiB) with drain-to-newline on
  truncation.  Codegen maps surface `read_line()` →
  `urus_read_line()`; sema registers the name in the prelude.
- `tests/run/16_str_helpers.urus`, `17_question_ok.urus`,
  `18_read_line.urus`; `tests/fail/SEC-13_question_wrong_ret.urus`.

### Security (b021)

- `&mut x` over an immutable binding is rejected — the F-COMP-2 gate
  (b013) now also covers mutable borrows, closing the write-through-
  pointer loophole.  `tests/fail/SEC-16` regresses it.

### Added (b020–b022)

- `--max-input-bytes N[K|M|G]` CLI flag (b020) — overrides the 64 MiB
  input cap up to a hard 1 GiB ceiling.  Closes the "override flag
  still TODO" note from b011.
- Test harness rewrite (b020): `// harness: skip` and
  `// harness: args <flags>` markers, self-tests for the new flag,
  `passed / failed / skipped` reporting, non-zero exit on failure.
  PowerShell and POSIX runners behave identically.
- Sema quality diagnostics (b021): duplicate parameter names error;
  `self` outside an `impl` block (or in non-first position) errors;
  unused `mut` warns at end of scope (first `diag_warn` consumer).
  Tests: `fail/SEC-14`, `fail/SEC-15`, `run/19_unused_mut_warns.urus`.
- GitHub Actions CI (b022): build matrix gcc / clang / macOS-clang /
  MSVC-cmake + an ASan/UBSan job running the full harness with
  `-fno-sanitize-recover=all`.  Read-only workflow token.

### Security (b023, b025)

- Fuzzing infrastructure (b023): `urus_compile_buffer` extracted into
  `compile.c` as a filesystem-free, exit-free pipeline entry; libFuzzer
  target `fuzz/fuzz_compile.c` drives it under
  `-fsanitize=fuzzer,address,undefined`; CI fuzzes 5 min per PR with
  corpus seeded from tests/ + examples/ (`scripts/make-corpus.sh`).
  Closes Tier-1 item #17.
- Diagnostic output amplification fixed (b025): snippets windowed to
  120 columns around the caret (a 16 MiB single-line source no longer
  prints whole on every error); 64-error flood cap with one-time
  suppression notice.

### Added (b024)

- `while let PAT = EXPR { BODY }` — desugars in the parser to
  `loop { match … { PAT => BODY, _ => break } }`; exhaustiveness and
  binding rules apply to the desugared form for free.
- For-range hardening: non-range iterators and half-open ranges in
  `for` are now URUS-level diagnostics instead of broken emitted C.
- Tests: `run/20_while_let.urus`, `run/21_for_range.urus`,
  `fail/SEC-17_for_no_bounds.urus`.

### Changed (internal, b023–b025)

- CMake: compiler core now builds as `urus_core` static library;
  `urusc` links it; the fuzz target opts in via
  `-DURUS_BUILD_FUZZER=ON` (clang only).
- `diag.c`: all five emit paths route through one `diag__vemit`;
  `diag_note` gained snippet + colors, `diag_fatal` gained colors.

### Changed (b026–b027)

- `diag_fatal` unwinds via longjmp when a recovery point is armed
  (`diag_set_recovery`); `urus_compile_buffer` arms it around the whole
  pipeline so fatals return control to embedders (fuzzer, future LSP)
  instead of `exit(2)`-ing the process.  The bare CLI keeps exit
  semantics.  `URUS_NORETURN` dropped from the declaration so embedder
  cleanup is not optimized away.  Known limitation: `arena.c` /
  `strbuf.c` OOM paths still `exit(1)` — v0.0.2.
- `06-LANGUAGE-GUIDE.md` synced to post-b013 reality: three stale
  danger-notes (mut unenforced / exhaustiveness unchecked / f-string
  RCE) replaced with the shipped rules; added `while let`, `?` context,
  stdlib section; new `examples/echo.urus` + `examples/grades.urus`.

### Fixed (b028)

- `defer` now fires on **every** exit edge: normal block end, explicit
  `return expr` (value materialised first, then defers, then return),
  and the `?` operator's error-propagation path.  Tracking moved to a
  per-function defer stack (`URUS_CG_MAX_DEFERS = 64`, exceeding it is
  a diagnostic).  The guide's sharpest footgun note is retired.
  Tests: `run/22_defer_early_return.urus`, `run/23_defer_question.urus`.

### Changed (b029)

- `arena.c` / `strbuf.c` no longer `exit(1)` on OOM or cap breach —
  they call `urus_abort_oom`, which longjmps to the recovery point
  armed by `urus_compile_buffer` (or `exit(1)`s in the bare CLI, as
  before).  Combined with b026, **no path through the compile pipeline
  can terminate an embedding process** (fuzzer, future LSP).

### Security (b030)

- CLI codegen-error gate (b030) — `urusc` checked diagnostics after
  parse and sema but **not** after codegen, so an f-string placeholder
  rejection printed its error yet still wrote the artifact and exited
  0.  Build systems keying on the exit code would consume the poisoned
  output.  Codegen diagnostics now abort before the output file is
  written, matching `urus_compile_buffer`.

### Fixed (b030) — first end-to-end verified build

b030 is the first build verified on a host with a working C toolchain
(zig cc): every `tests/run` program and every example is compiled to C,
**compiled to a binary, and executed** with output checked.  The sweep
surfaced 13 latent bugs that `--emit-c`-only verification had masked:

- Runtime: `URUS_PAYLOAD_OF` / `URUS_FMT_ANY` rewritten to
  function-selecting `_Generic` — the value-selecting form type-checked
  every association even when unselected, so any `urus_str` argument
  was a hard compile error in the emitted C (i.e. **every**
  Result/Option construction failed on real compilers).
- Codegen: block tails in `void` fns no longer emit `return expr;`;
  control-flow tails no longer emit `return return …`.
- Codegen: `match` is value-producing (conditional-expression chain),
  so `let x = match …` and tail-position matches work.
- Codegen: F-TY-2 **read side** — payload extraction picks the right
  union arm (`str`/`f64`/`u64`) via a minimal per-function local-type
  table; `Ok("hello")` no longer prints as a pointer.  (b015 widened
  construction only.)
- Codegen: `self` → `self_` / `(*self_)` lowering in identifiers and
  f-string placeholders; `Type.method(args)` emits a static call with
  no `&self` argument.
- Codegen: user enums — `Enum::Variant` constructs the tag struct;
  match arms compare `_scrut.tag` (previously every arm was
  `1 /*unknown variant*/` and the first arm always won).
- Parser: trailing `;` optional after `let` / `use` / `const` / `type`,
  per the documented rule; match arms with block-like bodies don't
  need a comma; stale `TY_NAME` enumerator fixed.
- Build: `Arena` named-struct fix (header/impl type identity),
  missing `<stdarg.h>` in codegen_c.c.
- Harness: `run-tests.ps1` no longer throws on stderr status lines
  under Windows PowerShell 5.1 (`$ErrorActionPreference = "Continue"`,
  pass/fail rides `$LASTEXITCODE`).
- Tests: `06_fstring.urus` `{4}` → bound `{four}` (literal placeholders
  are rejected by design since b014).

### Status as of v0.0.1-b029

**All 10 Tier-0 stop-ship findings are closed.**  Tier-1 progress:
ASan/UBSan CI ✅ (b022), libFuzzer per-PR ✅ (b023), input-cap override
✅ (b020), numeric overflow ✅ (b016), UTF-8 validation ✅ (b016),
diagnostics anti-amplification ✅ (b025), embedder-safe pipeline ✅
(b026+b029), defer-on-all-exits ✅ (b028).  Remaining notable:
F-TY-2 full monomorphisation (v0.0.2 headline).

### Security (b031) — first fuzzer findings

The v0.0.1 merge was the first time the b022/b023 CI actually executed.
Once the environment issues were cleared (exec bits on the POSIX
runners, an explicit `exit 0` in `run-tests.ps1`, sanitizer link flags
for `urusc` under `URUS_BUILD_FUZZER=ON`), the first genuine fuzz runs
caught two real bugs within minutes — exactly what the harness is for:

- Lexer EOF overflow — a lone `'` (or a trailing `\` in a string
  literal) at end of input advanced past the NUL terminator and read
  out of bounds (ASan heap-buffer-overflow in `peek0`).  EOF guards
  added in `lex_char` and `lex_string`; regressions
  `tests/fail/SEC-18_char_eof.urus` and
  `tests/fail/SEC-19_str_escape_eof.urus`.
- Arena total-allocation cap — `URUS_MAX_ARENA_TOTAL` (512 MiB) bounds
  the *sum* across chunks; the existing per-call cap couldn't stop many
  small AST allocations from OOMing the process (libFuzzer rss-limit
  hit).  Unwinds through `urus_abort_oom` like the other caps.
- `urus_memcpy` — NULL-tolerant wrapper for the parser's grow-buffer
  pattern (`memcpy(dst, NULL, 0)` is UB per the nonnull attribute and
  UBSan flags it); 21 call sites routed through it.
- Parser scratch buffers leak-proofed (LeakSanitizer finding) — the
  `realloc`+`free` scratch leaked on early-return/break paths out of
  collection loops and under the b026/b029 longjmp unwind.  All 19
  scratch buffers now grow via `arena_grow`, so arena teardown frees
  them on every path — the whole leak class is gone, not just the
  reported instance.
- `fuzz_compile.c` input cap 1 MiB → 64 KiB (codegen output is
  super-linear in input size; large inputs bought RSS spikes, not
  coverage).

### Status as of v0.0.1-b030

**The pipeline is end-to-end verified for the first time.**  All 43
harness tests pass; all 23 `tests/run` programs and all 6 examples
compile with a real C toolchain and produce the expected output.
Remaining notable: codegen's local-type table is best-effort (unknown
payload types degrade to the `int64` arm — wrong value, never UB);
full typed sema is the v0.0.3 headline.

---

## [0.0.1] — 2026-06-02 → 2026-06-03

The **first public preview** of URUS. The version line accumulated
eight tracked builds (`b001` → `b008`) over two calendar days. Each is
preserved as an immutable archive entry; the cross-build narrative is
below.

> [!IMPORTANT]
> v0.0.1 is **pre-alpha**. Treat it as the bootstrap seed of the project,
> not as a usable language. Compiling untrusted `.urus` sources is
> **explicitly unsafe** — see the Security section at the bottom of this
> release.

### Build-level recap

#### `v0.0.1-b008` — 2026-06-03 — *Versioning convention + archive*

- `A` `docs/documentation/15-VERSIONING.md` — version format + change-code legend
- `A` `docs/archive/` folder + `INDEX.md` summary table
- `A` Backfilled entries `v0.0.1-b001` … `v0.0.1-b008`
- `L` Change codes (`A U F R D S P B O C T X M N I L E`) now canonical
- `N` Build numbers monotonic across versions, zero-padded ≥3 digits
- `N` Archive entries are immutable — corrections get a new build

Archive: [`v0.0.1-b008.md`](./docs/archive/v0.0.1-b008.md).

#### `v0.0.1-b007` — 2026-06-03 — *Developer & contributor handbook*

- `A` `docs/documentation/00-INDEX.md` — navigation hub
- `A` `01-OVERVIEW.md` — what URUS is, vision, history
- `A` `02-ARCHITECTURE.md` — pipeline + data ownership
- `A` `03-BUILDING.md` — build on Windows / macOS / Linux
- `A` `04-PROJECT-LAYOUT.md` — every folder explained
- `A` `05-COMPILER-INTERNALS.md` — module-by-module deep dive
- `A` `06-LANGUAGE-GUIDE.md` — URUS as a language
- `A` `07-RUNTIME.md` — `urus_rt.h` internals
- `A` `08-TESTING.md` — test harness
- `A` `09-CONTRIBUTING-DEEP.md` — style + PR flow + hard rules
- `A` `10-DESIGN-DECISIONS.md` — 14 ADR-style records
- `A` `11-ROADMAP-DETAILED.md` — per-version milestones to v1.0
- `A` `12-FAQ.md` — common questions
- `A` `13-GLOSSARY.md` — terminology
- `A` `14-TROUBLESHOOTING.md` — error → cause → fix
- `N` ~2 500 lines of new documentation in one push

Archive: [`v0.0.1-b007.md`](./docs/archive/v0.0.1-b007.md).

#### `v0.0.1-b006` — 2026-06-03 — *`docs/` topic reorganization*

- `C` `docs/ANALYSIS.md` → `docs/analysis/ANALYSIS.md`
- `C` `docs/MERGE-DECISIONS.md` → `docs/merge/MERGE-DECISIONS.md`
- `C` `docs/REVIEW.md` → `docs/review/REVIEW.md`
- `C` `docs/SECURITY-AUDIT.md` → `docs/security/SECURITY-AUDIT.md`
- `C` `docs/SPEC.md` → `docs/spec/SPEC.md`
- `A` `docs/documentation/` placeholder folder (populated in b007)
- `N` Internal link paths updated incrementally as docs change

Archive: [`v0.0.1-b006.md`](./docs/archive/v0.0.1-b006.md).

#### `v0.0.1-b005` — 2026-06-03 — *Red-team security audit*

- `S` `F-MEM-1` (Critical) — f-string `{...}` placeholder → C-source RCE primitive
- `S` `F-TY-2` (Critical) — `Result<T,E>` / `Option<T>` payload truncated to `int64`
- `S` `F-COMP-1` (High) — unbounded parser recursion (`*****T` stack-bomb)
- `S` `F-COMP-2` (High) — `let mut` mutability not enforced
- `S` `F-COMP-3` (High) — `match` exhaustiveness not checked
- `S` `F-MEM-2` (High) — `lex_string` realloc NULL-deref
- `S` `F-MEM-5` (High) — `read_file` short-read leaves uninit tail
- `S` `F-MEM-7` (High) — `urus_print_fmt` unbounded sentinel scan
- `S` `F-MEM-8` (High) — `URUS_FMT_PTR_TAG` type confusion
- `S` `F-TY-1` (High) — `Result` / `Option` share tag namespace
- `A` `docs/SECURITY-AUDIT.md` — 14-phase audit + scorecard + TOP 100 hardening list
- `A` `tests/fail/SEC-01_fstr_injection.urus` — regression PoC for F-MEM-1
- `N` Overall security score 2.5/10 — pre-alpha, fails any external pen-test today
- `N` v0.0.2 defined as the release that closes all 10 stop-ship items

Archive: [`v0.0.1-b005.md`](./docs/archive/v0.0.1-b005.md).

#### `v0.0.1-b004` — 2026-06-03 — *14-perspective adoption review*

- `A` `docs/REVIEW.md` — adoption-council review across user, tester, adopter, technical writer, devrel, etc.
- `N` Verdict: pre-alpha, not yet pitchable to teams
- `N` Establishes the "council review" pattern reused for the security audit

Archive: [`v0.0.1-b004.md`](./docs/archive/v0.0.1-b004.md).

#### `v0.0.1-b003` — 2026-06-03 — *Selective merge from `Urus-archive`*

- `L` f-string literals `f"x = {name}"` — lexer `TOK_FSTR`, AST `EX_FSTR_LIT`, runtime `urus_fmt_to_str`
- `L` Postfix `?` operator on `Result` — parser `PREC_CALL`, AST `EX_TRY`, codegen via GCC statement-expression
- `L` `defer expr;` statement — parser `ST_DEFER`, codegen LIFO at block end (does not fire on `return`, documented)
- `A` Tests `tests/run/{05_try_op,06_fstring,07_defer}.urus`
- `A` `docs/MERGE-DECISIONS.md` — explicit kept / dropped / deferred table
- `U` CLI flags `--tokens` / `--ast` replace `--emit-tokens` / `--emit-ast` (old aliases warn, removed in v0.0.3)
- `I` Replaced linked-list `Scope` with FNV-1a hashed open-addressing scope — O(1) name lookup
- `I` Codegen targets C11; native MSVC `cl.exe` rejected via `#error`
- `R` Dropped from archive: rune macros, try/catch via setjmp, reference counting, `__emit__`, `%%` `**` `&~`, do/while, thread-per-async
- `N` v0.0.1 repositioned as a clean-room rebuild of archived `Urus-Foundation/Urus-archive`
- `N` Archive programs do not compile here unmodified

Archive: [`v0.0.1-b003.md`](./docs/archive/v0.0.1-b003.md).

#### `v0.0.1-b002` — 2026-06-02 — *Architecture review + Top 100 roadmap*

- `A` `docs/ANALYSIS.md` — 15-phase architecture review
- `A` Top 100 improvements list inside ANALYSIS.md
- `A` `docs/SPEC.md` — formal language specification draft
- `N` ANALYSIS.md flags `Result` / `Option` int64 payload truncation as future stop-ship
- `N` Establishes the project tone: honest about what is missing

Archive: [`v0.0.1-b002.md`](./docs/archive/v0.0.1-b002.md).

#### `v0.0.1-b001` — 2026-06-02 — *First scaffold*

- `A` C-based `urusc` compiler binary scaffolded (~3 500 LOC)
- `A` Lexer with full token set, escape sequences, numeric prefixes, nested block comments
- `A` Recursive-descent + Pratt expression parser, 18 precedence levels
- `A` Discriminated-union AST with arena-backed lifetimes
- `A` Two-pass semantic analyzer (collect globals → check bodies)
- `A` C99 transpile backend emitting one self-contained TU
- `A` Header-only runtime `stdlib/runtime/urus_rt.h`
- `A` CLI surface (`--emit-c`, `--emit-tokens`, `--emit-ast`, `-o`, `--version`, `--help`)
- `A` Examples — hello, aurochs, fib, result
- `A` Tests — `tests/run/{01..04}.urus`, `tests/fail/{01,02}.urus`
- `A` Test runners — `scripts/run-tests.ps1`, `scripts/run-tests.sh`
- `A` Top-level docs — README, ROADMAP, CONTRIBUTING, GOVERNANCE, SECURITY, LICENSE
- `L` English keywords baseline — overrides earlier Indonesian-keyword sketch
- `N` Pre-alpha. Compiler not yet smoke-tested on host (no C compiler installed on dev box)

Archive: [`v0.0.1-b001.md`](./docs/archive/v0.0.1-b001.md).

---

### Per-version summary

#### Added

##### Compiler
- `urusc` binary in portable C11, built with CMake.
- Bump-allocator (`Arena`) for AST + intermediate strings — no per-node `malloc`/`free`.
- Source-position-aware diagnostics with snippet rendering and carets.

##### Lexer
- All v0.0.1 keywords: `module`, `use`, `fn`, `struct`, `impl`, `enum`, `trait`, `type`, `let`, `mut`, `const`, `pub`, `if`, `else`, `match`, `return`, `while`, `for`, `in`, `loop`, `break`, `continue`, `true`, `false`, `self`, `Self`, `as`, `defer`.
- Numeric literals: decimal, `0x...`, `0b...`, `0o...`, underscore separators, optional type suffix.
- String literals with escape sequences (`\n`, `\r`, `\t`, `\\`, `\"`, `\xNN`).
- `f"..."` string literals as a distinct token kind.
- Character literals.
- Line (`//`) and nested block (`/* /* */ */`) comments.
- All standard punctuation and operators including `::`, `->`, `=>`, `..`, `..=`, `&&`, `||`, `<<`, `>>`, compound assignment, postfix `?`.

##### Parser
- Recursive-descent for top-level items and statements.
- Pratt expression parser with correct precedence for 18 levels.
- Items: `fn`, `struct`, `impl`, `enum`, `use`, `const`, `type`.
- Statements: `let`, expression statements (optional `;`), `defer`.
- Expressions: literals, identifiers, paths, unary, binary, assignment, calls, method calls, field access, indexing, `if`/`else`, `match`, blocks, `return`, `break`, `continue`, `while`, `for…in`, `loop`, struct/tuple/array literals, `as` casts, `&`/`&mut`, `*`, ranges (`..`, `..=`), postfix `?` on `Result`, f-string literals.
- Patterns: identifier, wildcard, literal, enum-variant (`Ok(v)`, `Err(e)`, `Some(v)`, `None`), tuple.
- Error recovery: resync at the next item boundary.

##### Semantic analyzer
- Two-pass: collect globals → check bodies.
- **FNV-1a hashed** scopes (open addressing, 0.75 load factor) — O(1) name lookup.
- Duplicate-definition detection.
- Built-in prelude (numeric primitives, `bool`, `str`, `Result`, `Option`, `Ok`, `Err`, `Some`, `None`, `println`, `print`, `eprintln`, `panic`).
- Struct field existence check on struct literals.
- Pattern-binding declaration into the surrounding scope.

##### Codegen
- Emits one self-contained C11 translation unit.
- Maps URUS primitive types to fixed-width C types.
- Structs → C structs; enums → tag + union.
- `impl T { fn m(&self, ...) }` → mangled `T__m(T *self_, ...)`.
- `Result<T,E>` / `Option<T>` lowered to runtime tagged unions.
- `println("hello, {name}!")` desugars to `urus_println_fmt` with typed arg array.
- `?` lowered to GCC statement-expression.
- `defer` LIFO at block end.
- C99 keywords (incl. `main`) auto-mangled with `_` suffix.

##### Runtime
- Header-only `urus_rt.h` — no extra link dependencies.
- `urus_str` fat pointer.
- `urus_println` / `urus_print` / `urus_eprintln` with `_Generic`-based type dispatch.
- `urus_Result` / `urus_Option` with `urus_ok`, `urus_err`, `urus_some`, `urus_none`, `urus_is_*` helpers.
- `urus_fmt_to_str` for f-string materialization.
- `urus_panic(msg)` (calls `abort()`).

##### Examples
- `examples/hello.urus`
- `examples/aurochs.urus`
- `examples/fib.urus`
- `examples/result.urus`

##### Tests
- `tests/run/01_hello.urus` through `07_defer.urus`.
- `tests/fail/01_undefined.urus`, `02_dup_struct.urus`, `SEC-01_fstr_injection.urus`.
- `scripts/run-tests.ps1` (Windows) and `scripts/run-tests.sh` (POSIX).

##### Documentation
- `README.md` (rewritten in b008 for repo-grade presentation).
- `CHANGELOG.md`, `ROADMAP.md`, `SECURITY.md`, `CONTRIBUTING.md`, `GOVERNANCE.md`, `LICENSE`.
- `docs/spec/SPEC.md` — formal language specification.
- `docs/analysis/ANALYSIS.md` — 15-phase review + Top 100.
- `docs/merge/MERGE-DECISIONS.md` — archive kept/dropped/deferred.
- `docs/review/REVIEW.md` — 14-perspective adoption review.
- `docs/security/SECURITY-AUDIT.md` — full red-team audit.
- `docs/documentation/00-INDEX.md` … `15-VERSIONING.md` — 16-file handbook.
- `docs/archive/INDEX.md` + `v0.0.1-b001.md` … `v0.0.1-b008.md`.

##### Tooling
- `urusc --emit-c` (default) — write `<input>.c`.
- `urusc --tokens` — dump tokens.
- `urusc --ast` — dump the parsed module.
- Deprecated `--emit-tokens` / `--emit-ast` aliases (removed in v0.0.3).
- `urusc -o <file>` — explicit output path.
- `urusc --version` / `urusc --help`.

##### Posture
- Emitted C targets **C11** (was C99 in early drafts) and requires
  statement-expressions plus `__auto_type`. Supported back ends:
  **GCC**, **Clang**, **clang-cl**. Native **MSVC (`cl.exe`) is rejected**
  by a `#error` at the top of every emitted file.

#### Changed

- Scope tables: linked-list → FNV-1a hash (b003).
- CLI: `--emit-tokens`/`--emit-ast` → `--tokens`/`--ast` (b003).
- Documentation reorganized into `docs/analysis/`, `merge/`, `review/`,
  `security/`, `spec/`, `documentation/`, `archive/` (b006).

#### Removed

- Nothing in v0.0.1. Inherited-but-rejected `Urus-archive` features
  (rune macros, try/catch via setjmp, ref counting, `__emit__`, `%%`,
  `**`, `&~`, do/while, thread-per-async) were never reimplemented.

#### Deprecated

- `--emit-tokens` / `--emit-ast` CLI flags. Use `--tokens` / `--ast`.
  Removal scheduled for v0.0.3.

#### Fixed

- N/A for the initial release.

#### Security

> [!CAUTION]
> **v0.0.1 contains 10 stop-ship security findings**, including a
> critical RCE primitive in the f-string codegen path. **Do not compile
> `.urus` files you do not trust.**
>
> All findings are publicly documented at
> [`docs/security/SECURITY-AUDIT.md`](./docs/security/SECURITY-AUDIT.md).
> Fixes ship in **v0.0.2** as the next release.

- `F-MEM-1` (Critical) — f-string brace contents pasted verbatim into emitted C → RCE.
- `F-TY-2` (Critical) — `Result` / `Option` int64 payload truncation.
- `F-COMP-1` (High) — parser recursion unbounded.
- `F-COMP-2` (High) — `let mut` not enforced by sema.
- `F-COMP-3` (High) — no `match` exhaustiveness check.
- `F-MEM-2` (High) — realloc NULL-deref in `lex_string`.
- `F-MEM-5` (High) — uninit tail of input buffer on short read.
- `F-MEM-7` (High) — `urus_print_fmt` unbounded sentinel scan.
- `F-MEM-8` (High) — `URUS_FMT_PTR_TAG` type confusion.
- `F-TY-1` (High) — `Result` / `Option` share tag namespace.

#### Known limitations

- No package manager.
- Generics other than `Result<T,E>` / `Option<T>` parsed but not monomorphised.
- No `match` exhaustiveness checking.
- The emitted C requires GCC/Clang extensions (`__auto_type`, statement-expressions).
- No borrow checker — references are lowered pointers.
- No async, threads, traits, macros.
- `let` without an explicit type relies on C's `__auto_type`; no native URUS-side inference.
- `defer` does not fire on `return`.

#### Migration notes

- Programs from `Urus-Foundation/Urus-archive` do **not** compile here
  unmodified. Syntactic divergences (semicolons optional, `fn -> T`
  arrow style, `use a.b.c` instead of `import "file"`) are intentional.
- The full kept/dropped/deferred decision list lives at
  [`docs/merge/MERGE-DECISIONS.md`](./docs/merge/MERGE-DECISIONS.md).

---

## How this file is maintained

- Every release section is written **once it ships** and then frozen.
  Corrections go in a follow-up release, not as an in-place edit.
- Build entries here link to the immutable archive entry under
  `docs/archive/`. The two stay in sync.
- The `[Unreleased]` section accumulates entries during development.
  At release time, it is renamed to the new version and a fresh
  `[Unreleased]` block opens.

For the full version + change-code convention:
[`docs/documentation/15-VERSIONING.md`](./docs/documentation/15-VERSIONING.md).
