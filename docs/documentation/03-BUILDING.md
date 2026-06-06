# 03 — Building URUS

The URUS compiler is plain C11. You need three things:

1. A C11 toolchain.
2. CMake ≥ 3.16.
3. Disk space (the build is tiny — well under 100 MB).

## Supported host toolchains

| Platform        | Compiler                            | Notes                                                 |
|-----------------|-------------------------------------|-------------------------------------------------------|
| Windows         | **clang** or **clang-cl** (LLVM)    | MSVC `cl.exe` works for the *compiler itself* but **not** for the emitted C — see below |
| macOS           | Apple Clang or Homebrew Clang/GCC   | The default Xcode toolchain is fine                   |
| Linux           | GCC ≥ 9 or Clang ≥ 12               | Most distros ship a modern enough toolchain           |
| FreeBSD / other | Anything C11-conformant             | Untested but should work                              |

The compiler **itself** is portable C11 — it builds with any C11
compiler. The constraint is only on the **emitted** C: that file uses
GCC statement-expressions and `__auto_type`, which native MSVC does not
support. See `02-ARCHITECTURE.md` and `10-DESIGN-DECISIONS.md` for why
this is the deliberate choice.

## Build on Windows (PowerShell)

```powershell
# From the repo root:
cmake -B compiler/build -S .
cmake --build compiler/build
```

The compiler binary lands at `compiler/build/Debug/urusc.exe` (or
`Release` with `--config Release`).

If you don't have a C compiler installed:

```powershell
winget install LLVM.LLVM Kitware.CMake
# restart your shell so PATH picks up clang and cmake
```

## Build on macOS

```bash
cmake -B compiler/build -S .
cmake --build compiler/build -j
```

Binary at `compiler/build/urusc`.

## Build on Linux

```bash
sudo apt install -y build-essential cmake     # or your distro's equivalent
cmake -B compiler/build -S .
cmake --build compiler/build -j
```

Binary at `compiler/build/urusc`.

## Verify the build

```bash
compiler/build/urusc --version
# urusc 0.0.1
```

## Compile your first URUS program

The compiler can emit either a token dump, an AST dump, or C source.
C source is the default.

```bash
compiler/build/urusc examples/hello.urus --emit-c
# writes examples/hello.urus.c
```

Then build the produced C file with your system compiler, passing the
runtime include path:

```bash
gcc examples/hello.urus.c -I stdlib/runtime -o hello
./hello
# → Hello, Aurochs!
```

On Windows:

```powershell
clang examples\hello.urus.c -I stdlib\runtime -o hello.exe
.\hello.exe
```

## Run the test suite

```powershell
# Windows
powershell -ExecutionPolicy Bypass -File scripts\run-tests.ps1
```

```bash
# POSIX
bash scripts/run-tests.sh
```

The script walks `tests/run/` (must compile + run cleanly) and
`tests/fail/` (must produce a diagnostic). See
[`08-TESTING.md`](./08-TESTING.md) for what each test exercises.

## Common build problems

### "MSVC is not supported by URUS"

You tried to compile an URUS-emitted `.c` file with `cl.exe`. The very
first line of the emitted file is a `#error` directive blocking this.
Use `clang` or `clang-cl` on Windows instead. See
[`14-TROUBLESHOOTING.md`](./14-TROUBLESHOOTING.md).

### "Unknown type name `__auto_type`"

Same root cause: you compiled the emitted C with a toolchain that does
not support GCC's `__auto_type`. Switch to GCC or Clang.

### CMake cannot find a C compiler

Install one. On Windows: `winget install LLVM.LLVM`. On macOS:
`xcode-select --install`. On Linux: `sudo apt install build-essential`
or equivalent.

### "fatal error: 'urus_rt.h' file not found"

You forgot the `-I stdlib/runtime` flag when building the *produced* C
file. The compiler does not embed the runtime; it `#include`s it.

## Build flavors

CMake-side:

| Flavor      | How                                                | Use for                  |
|-------------|----------------------------------------------------|--------------------------|
| `Debug`     | `cmake --build compiler/build --config Debug`       | day-to-day               |
| `Release`   | `cmake --build compiler/build --config Release`     | performance benchmarks   |
| `RelWithDebInfo` | `--config RelWithDebInfo`                      | profiling                |
| ASan        | add `-DCMAKE_C_FLAGS="-fsanitize=address"`          | finding compiler bugs    |
| UBSan       | add `-DCMAKE_C_FLAGS="-fsanitize=undefined"`        | finding undefined behaviour |

ASan + UBSan are not in CI yet — they are in the Tier 1 hardening list
of [`docs/security/SECURITY-AUDIT.md`](../security/SECURITY-AUDIT.md).
Run them locally before sending a non-trivial PR.

## Reproducible builds

Not yet. The compiler embeds a build timestamp via `__DATE__` / `__TIME__`
in `--version` output. Removing those is on the v0.1.0 list. Until then,
do not rely on byte-identical compiler binaries.

— *Last updated 2026-06-03.*
