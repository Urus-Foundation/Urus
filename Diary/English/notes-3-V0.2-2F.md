# Rasya's Notes #3 — V0.2/2(F) "Fixed"

**Date:** March 9, 2026
**By:** Rasya Andrean

---

## What Happened in This Version

After V0.2/1 was released on March 2, a contributor named **John-fried** was very active sending PRs from March 7 to 9. He sent a total of 5 PRs (#2 to #7), all of which helped stabilize the compiler.

His first PR (#2) was a Linux fix. Turns out our `urus_runtime.h` was missing `#include <stddef.h>`, so compiling on Linux would fail immediately. There were also implicit declaration issues with POSIX functions because we use strict C11.

Then PR #3 added `install` and `uninstall` targets to the Makefile. Simple but useful — users could just `make install` without manually copying files.

PR #4 added a `show_help()` function to `main.c`. Before this, running `urusc` without arguments would just show an error with no guidance.

The biggest change was PR #5 — migration from Makefile to **CMake**. Previously we had a `Makefile` for Linux/macOS and `build.bat` for Windows. Two separate files to maintain. With CMake, one `CMakeLists.txt` handles all platforms. He also created `cmake/embed-string.cmake` to convert `urus_runtime.h` into a C array embedded directly into the compiler binary.

PR #6 continued the embed work — the compiler became **standalone**. Users no longer need to keep `urus_runtime.h` alongside the compiler. The runtime header is embedded in the binary, so just copying `urusc.exe` is enough.

Finally PR #7 improved **error reporting**. Before, errors were just "Error at line 5". Now there's color (red for "Error", green for caret), filename, line number, the problematic source line, and a caret (^) pointing to the exact column.

---

## Issues Found During Testing

After all PRs were merged, testing began for V0.2/2 release. Many things broke, especially on Windows. This was expected since John-fried developed on Linux.

### 1. `error.c` uses `getline()` — doesn't exist on Windows

`getline()` is POSIX-only, MSVC doesn't have it. Build with Visual Studio immediately failed: `unresolved external symbol getline`. Replaced with portable `fgets()` and switched from `malloc`/`free` buffer to stack buffer `char[4096]`.

### 2. Generated C file corrupt on Windows (double CRLF)

`urus_runtime.h` on Windows uses CRLF (`\r\n`). When embedded into binary, those `\r\n` bytes are included. Then when the compiler writes `_urus_tmp.c` with `fopen("w")` (text mode), Windows adds another `\r` before each `\n`. Result: `\r\n` becomes `\r\r\n`. GCC gets confused. Fix: open file with `"wb"` (binary mode).

### 3. `--help` and `--version` treated as filename

Argument parsing in `main.c` assumed `argv[1]` was a file path. So `urusc --help` tried to open a file named `--help`. Fix: added a pre-scan loop for flag arguments before assuming argv[1] is a file.

### 4. GCC can't find `cc1` on Windows

The compiler finds GCC at a hardcoded path like `C:/msys64/mingw64/bin/gcc.exe`. Found it, fine. But when GCC runs, it needs `cc1` from its subdirectory. Since PATH doesn't include the GCC directory, `cc1` isn't found. Fix: inject GCC's bin directory into PATH before calling it.

### 5. `-I include` flag still used but unnecessary

After PR #6 made the compiler standalone (runtime embedded in generated C), the `-I include` flag sent to GCC was useless. The path it pointed to didn't even exist. Removed all `include_dir` logic.

### 6. MSVC warnings about `strdup`

MSVC gives warnings: "strdup deprecated, use _strdup". Added `_CRT_SECURE_NO_WARNINGS` and `_CRT_NONSTDC_NO_DEPRECATE` in CMakeLists.txt for MSVC only.

---

## Final Test Results

After all fixes, rebuilt from scratch and ran all tests:

- **10 test cases** in `tests/run/` — all PASS
- **8 example programs** in `examples/` — all compile and run
- **8 invalid files** in `tests/invalid/` — all error reporting correct
- **CLI flags** (`--help`, `--version`, `--tokens`, `--ast`, `--emit-c`, `-o`) — all working

Zero failures.

---

## Lessons Learned

If there's one thing learned from this release: **always test on all target platforms**. Many bugs above wouldn't be found by testing only on Linux. Windows has different behavior for line endings, PATH resolution, and POSIX functions. Cross-platform isn't just "compiles everywhere" — it's "runs correctly everywhere".

Also, making the compiler standalone (embedding runtime into binary) was a good decision. Users used to manage two files (compiler + runtime header), now it's just one executable.

---

*V0.2/2(F) "Fixed" — because it's literally about fixing everything that broke after the first batch of contributions.*
