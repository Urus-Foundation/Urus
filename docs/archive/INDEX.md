# URUS Build Archive — Index

Every meaningful state of the URUS codebase gets a build number and a
one-paragraph entry below. The full archive entry per build lives in
its own file (`vX.Y.Z-bNNN.md`); this index is the at-a-glance summary.

For the format itself, see
[`docs/documentation/15-VERSIONING.md`](../documentation/15-VERSIONING.md).

> **Reading guide.** The newest build is at the top. Build numbers are
> monotonic — they never reset across versions. Codes follow the
> legend at the bottom of this page.

---

## v0.0.1 line

| Build                                  | Date       | Status | Highlights                                                                  |
|----------------------------------------|------------|--------|-----------------------------------------------------------------------------|
| [`v0.0.1-b031`](./v0.0.1-b031.md)      | 2026-06-07 | Alpha  | First CI run green (exec bits, ps1 exit code, fuzzer link flags) + first fuzzer findings fixed: lexer EOF overflow (SEC-18/19), arena 512 MiB total cap, NULL-tolerant `urus_memcpy` |
| [`v0.0.1-b030`](./v0.0.1-b030.md)      | 2026-06-06 | Alpha  | First **end-to-end verified** build: 13 latent codegen/parser/runtime bugs fixed (value-producing `match`, F-TY-2 read side, `self` lowering, user enums, optional `;`, `_Generic` rewrite); 23/23 run-tests + 6/6 examples compile **and run** green |
| [`v0.0.1-b029`](./v0.0.1-b029.md)      | 2026-06-04 | Alpha  | arena/strbuf OOM unwind via `urus_abort` hook — `urus_compile_buffer` now fully embedder-safe (no exit path left) |
| [`v0.0.1-b028`](./v0.0.1-b028.md)      | 2026-06-04 | Alpha  | `defer` fires on every exit edge: early `return` (value-first ordering) + `?` propagation; per-fn defer stack |
| [`v0.0.1-b027`](./v0.0.1-b027.md)      | 2026-06-04 | Alpha  | Language guide synced to b013–b025 reality (3 stale danger-notes retired) + `echo` / `grades` examples |
| [`v0.0.1-b026`](./v0.0.1-b026.md)      | 2026-06-04 | Alpha  | `diag_fatal` longjmp unwind — embedders (fuzzer, future LSP) survive fatals; CLI keeps exit(2) |
| [`v0.0.1-b025`](./v0.0.1-b025.md)      | 2026-06-04 | Alpha  | Diagnostics polish: 120-col windowed snippets (anti output-amplification), 64-error flood cap, unified emit path |
| [`v0.0.1-b024`](./v0.0.1-b024.md)      | 2026-06-04 | Alpha  | `while let` sugar (parser desugar to loop+match) + for-range codegen hardening (diagnostics, no broken C) |
| [`v0.0.1-b023`](./v0.0.1-b023.md)      | 2026-06-04 | Alpha  | Fuzzing infra: `urus_compile_buffer` split, libFuzzer target, seed corpus, 5-min CI fuzz job |
| [`v0.0.1-b022`](./v0.0.1-b022.md)      | 2026-06-04 | Alpha  | GitHub Actions CI: gcc/clang/macOS/MSVC build matrix + ASan/UBSan job over the full test sweep |
| [`v0.0.1-b021`](./v0.0.1-b021.md)      | 2026-06-04 | Alpha  | Sema quality pass: `&mut` borrow gate, duplicate-param + stray-`self` errors, unused-`mut` warning |
| [`v0.0.1-b020`](./v0.0.1-b020.md)      | 2026-06-04 | Alpha  | `--max-input-bytes` override flag (1 GiB ceiling) + test harness rewrite: skip/args markers, self-tests, CI-ready exit codes |
| [`v0.0.1-b019`](./v0.0.1-b019.md)      | 2026-06-04 | Alpha  | stdlib expansion: `urus.io.read_line` (`Option<str>`, 16 MiB cap, CRLF-tolerant) + prelude registration of `urus_str_*` helpers |
| [`v0.0.1-b018`](./v0.0.1-b018.md)      | 2026-06-04 | Alpha  | Sema enforces `?` operator context — must be inside a fn returning Result/Option |
| [`v0.0.1-b017`](./v0.0.1-b017.md)      | 2026-06-04 | Alpha  | CHANGELOG backfill (b014/b015/b016) + runtime hardening: formatter render cap, defensive `urus_panic`, `urus_str_*` helpers |
| [`v0.0.1-b016`](./v0.0.1-b016.md)      | 2026-06-04 | Alpha  | Tier-1 hardening pass #1: integer / float literal overflow detection + UTF-8 input validation + suffix-strip defensive parse |
| [`v0.0.1-b015`](./v0.0.1-b015.md)      | 2026-06-04 | Alpha  | Stop-ship fix #5: `F-TY-2` — Result/Option payload widened to 16-byte union (`urus_payload_t`); **all 10 stop-ship findings now closed** |
| [`v0.0.1-b014`](./v0.0.1-b014.md)      | 2026-06-04 | Alpha  | Stop-ship fix #4: `F-MEM-1` — f-string placeholder validator closes the URUS→C code-injection primitive |
| [`v0.0.1-b013`](./v0.0.1-b013.md)      | 2026-06-04 | Alpha  | Stop-ship fixes #3: `F-COMP-2` (`let mut` enforcement), `F-COMP-3` (`match` exhaustiveness on Result/Option/enums) |
| [`v0.0.1-b012`](./v0.0.1-b012.md)      | 2026-06-04 | Alpha  | Stop-ship fixes #2: `F-MEM-7`, `F-MEM-8`, `F-TY-1` (length-aware fmt API, drop `PTR_TAG`, disjoint Result/Option tags) |
| [`v0.0.1-b011`](./v0.0.1-b011.md)      | 2026-06-04 | Alpha  | Stop-ship fixes #1: `F-COMP-1`, `F-MEM-2`, `F-MEM-5` + Tier-0 caps #11–#14 (input / string / arena / strbuf) |
| [`v0.0.1-b010`](./v0.0.1-b010.md)      | 2026-06-03 | Alpha  | Replaced every `*@urus-lang.dev` placeholder with `urusfoundation@gmail.com` |
| [`v0.0.1-b009`](./v0.0.1-b009.md)      | 2026-06-03 | Alpha  | Rewrite of CHANGELOG / CONTRIBUTING / GOVERNANCE / SECURITY; new NOTICE, COMMERCIAL, CODE_OF_CONDUCT |
| [`v0.0.1-b008`](./v0.0.1-b008.md)      | 2026-06-03 | Alpha  | Versioning convention + build archive; `15-VERSIONING.md` written            |
| [`v0.0.1-b007`](./v0.0.1-b007.md)      | 2026-06-03 | Alpha  | `docs/documentation/` 15-file dev/contributor handbook written               |
| [`v0.0.1-b006`](./v0.0.1-b006.md)      | 2026-06-03 | Alpha  | `docs/` reorganized into `analysis/ merge/ review/ security/ spec/`          |
| [`v0.0.1-b005`](./v0.0.1-b005.md)      | 2026-06-03 | Alpha  | Adversarial red-team security audit; 10 stop-ship findings documented        |
| [`v0.0.1-b004`](./v0.0.1-b004.md)      | 2026-06-03 | Alpha  | Adoption review by 14-perspective council (`docs/review/REVIEW.md`)          |
| [`v0.0.1-b003`](./v0.0.1-b003.md)      | 2026-06-03 | Alpha  | Selective merge from `Urus-archive`: f-strings, `?`, `defer`, FNV-1a, …      |
| [`v0.0.1-b002`](./v0.0.1-b002.md)      | 2026-06-02 | Alpha  | Architecture review + Top 100 roadmap (`docs/analysis/ANALYSIS.md`)          |
| [`v0.0.1-b001`](./v0.0.1-b001.md)      | 2026-06-02 | Alpha  | First scaffold: compiler, runtime, examples, tests, base docs                |

---

## Legend

```
A : added           U : updated         F : fixed           R : removed
D : deprecated      S : security        P : performance     B : breaking
O : optimized       C : changed         T : tests           X : experimental
M : migration       N : notes           I : internal        L : language
E : ecosystem
```

Severity order within each entry (top → bottom):

```
B → S → M → R → D → A → L → U → C → F → P → O → X → T → I → E → N
```

---

## How new entries land

1. Land your change on the default branch.
2. Mint the next build number (monotonic; never reuse).
3. Create `vMAJOR.MINOR.PATCH-bNNN.md` in this folder using the template
   in [`docs/documentation/15-VERSIONING.md`](../documentation/15-VERSIONING.md).
4. Add one row at the top of the table above.
5. Commit with subject `archive: vX.Y.Z-bNNN — <highlight>`.

— *Last updated 2026-06-07.*
