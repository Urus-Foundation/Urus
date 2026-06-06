# 08 — Testing

URUS v0.0.1 has two categories of test, run by one script per host
shell.

## Layout

```
tests/
├── run/        programs that must compile AND run without error
└── fail/       programs whose errors are expected
```

Each file is named `NN_topic.urus`:

| File                                       | Exercises                                |
|--------------------------------------------|------------------------------------------|
| `tests/run/01_hello.urus`                  | basic `println`                          |
| `tests/run/02_arith.urus`                  | numeric primitives, arithmetic           |
| `tests/run/03_struct.urus`                 | struct decls, field access               |
| `tests/run/04_control_flow.urus`           | if/while/for/loop                        |
| `tests/run/05_try_op.urus`                 | `?` on `Result`                          |
| `tests/run/06_fstring.urus`                | f-string interpolation                   |
| `tests/run/07_defer.urus`                  | `defer` LIFO                             |
| `tests/fail/01_undefined.urus`             | undefined identifier diagnostic          |
| `tests/fail/02_dup_struct.urus`            | duplicate struct definition diagnostic   |
| `tests/fail/SEC-01_fstr_injection.urus`    | F-MEM-1 RCE PoC — must fail in v0.0.2+   |

## Running the suite

```powershell
# Windows
powershell -ExecutionPolicy Bypass -File scripts\run-tests.ps1
```

```bash
# POSIX
bash scripts/run-tests.sh
```

The scripts:

1. Build `urusc` if `compiler/build/` is missing.
2. For each file in `tests/run/`:
   - run `urusc <file> --emit-c` (must exit 0)
   - if a C compiler is available, build the produced `.c` and run it
3. For each file in `tests/fail/`:
   - run `urusc <file>` and require a non-zero exit *and* a diagnostic
     mentioning the expected substring (if a `// expect: ...` comment
     is present at the top of the file)

When no host C compiler is installed (the situation on the original
author's Windows machine), the runners stop after the `--emit-c` check.
That is enough to verify the lexer / parser / sema / codegen path; the
final link step is left to the developer.

## Writing a new positive test

1. Pick the next number in `tests/run/`.
2. Keep the file under ~30 lines if possible. One feature per test.
3. The program should print at least one easy-to-grep string so the
   runner can verify behavior once a host C compiler is in play.
4. Add a one-line comment at the top describing what is being tested.

Example skeleton:

```urus
// Tests: <one-line description>
module main
use urus.io.println

fn main() {
    // setup
    // exercise
    println("OK")
}
```

## Writing a new negative test

1. Pick the next number in `tests/fail/`.
2. Add a `// expect: <substring>` directive at the top with the part of
   the diagnostic you want to match.
3. Keep the offending source minimal — one line of offence, surrounded
   by enough valid code to be parseable.

Example:

```urus
// expect: undefined identifier `nope`
module main
fn main() { nope() }
```

## What we do not have yet

These are tracked Tier-1 work in the security audit:

- **Fuzzing in CI.** No `libFuzzer` or `AFL++` integration. Run them
  locally on the lexer / parser at minimum.
- **Sanitizer builds in CI.** No ASan/UBSan/MSan automation. Build with
  `-fsanitize=address,undefined` locally before sending large PRs.
- **Differential parser tests.** parse → pretty-print → parse → compare
  ASTs. Catches parser non-determinism. Tier-1 item.
- **Property tests for codegen.** Emit C from a random AST and require
  the host C compiler to accept it. Tier-1 item.

If you are looking for a contribution that has obvious value, picking
any one of the above and landing it is high-impact.

## CI

CI is **not configured yet**. The Tier-1 hardening list calls for at
minimum:

- Linux x64, Linux arm64, macOS x64, macOS arm64, Windows clang-cl.
- ASan + UBSan on every PR.
- libFuzzer 5 minutes / PR, 24 hours nightly.
- Test runner in the matrix.

This is on the v0.0.2 critical path.

— *Last updated 2026-06-03.*
