# Changelog

## Unreleased (since V0.3.0)

### New Features
- **Extended Pattern Matching**: `match` now works on `int`, `str`, and `bool` values with literal patterns and `_` wildcard (#117, #118)
- **Defer Statements**: `defer { ... }` for scope-based cleanup, LIFO execution order (#112, #116)
- **Standard Library Path**: `import` now searches `URUSCPATH` for library modules (#114)
- **Raw Emit**: `__emit__("...")` to inline C code directly (#113)
- **Type Aliases**: `type ID = int;` for semantic type aliasing (#110, #111)
- **Do-While Loops**: `do { ... } while cond;` (#108, #109)
- **Array Method Syntax**: `arr.len()`, `arr.push(x)`, `arr.pop()` (#106, #107)
- **String Method Syntax**: `s.trim()`, `s.upper()`, `s.contains(sub)`, etc. (#104, #105)
- **Constants**: `const MAX: int = 100;` compile-time constants (#102, #103)
- **Bitwise Operators**: `&`, `|`, `^`, `~`, `<<`, `>>`, `&~`, plus `**` (exponent) and `%%` (floored remainder) (#99)
- **Mutable Function Parameters**: `fn foo(mut x: int)` (#98)

### Bug Fixes
- Fixed multiple security vulnerabilities from audit (#100, #101)
- Fixed empty `URUS_LIB_DIR` in Termux installs (#505de65)
- Fixed garbage unused warnings for imported declarations (#fcd02f1)

### Improvements
- Runtime moved from `include/urus_runtime.h` to `runtime/urus_runtime.h`
- Updated library installation paths and preprocess logic

---

## V0.3.0 (2026-03-21)

### New Features
- **Tuple Types**: `(int, str)` — stack-allocated compound types with `.0`, `.1` field access (#15)
- **Tuple Destructuring**: `let (x, y): (int, str) = get_pair();` and `for (k, v) in pairs { }` (#74)
- **Runes (Macro System)**: `rune square(x) { x * x }` invoked with `square!(5)` — Urus's unique macro system (#66)
- **Statement-level Runes**: Rune bodies with semicolons expand as statement blocks (#75)
- **If-Expressions**: `if cond { a } else { b }` as expressions, compiles to C ternary (#71, #79)
- **Type Inference**: `let x = 42;` — type annotation is now optional, inferred from initializer (#78)
- **HTTP Built-ins**: `http_get(url)` and `http_post(url, body)` via curl (#87)
- **String Escape Sequences**: `\t` (tab) and `\0` (null) now supported (#77)

### Bug Fixes
- Fixed `urus_str_replace()` missing `r->len` assignment — uninitialized length field (#89 VULN-01)
- Fixed `urus_pop()` memory leak — now calls `elem_drop` before discarding element (#89 VULN-02)
- Fixed unchecked `malloc`/`realloc` — added `urus_alloc`/`urus_realloc` wrappers with NULL checks (#89 VULN-03)
- Fixed `urus_read_file()` — added `ftell()` error check and `fread()` return capture (#89 VULN-04)
- Fixed array of tuples/results producing incorrect C codegen — `elem_sizeof`/`elem_ctype` now handle TYPE_TUPLE and TYPE_RESULT (#70)
- Fixed tuples containing heap types (str, array) leaking memory — generate drop functions for tuples with heap fields (#73)
- Fixed rune table overflow silently discarding macros — now emits error (#76)
- Fixed rune argument count mismatch giving unhelpful error message (#72)

### Improvements
- RAII cleanup for tuple types containing heap-allocated fields
- Tuple typedef system with unique C type names per tuple signature
- Compiler version bumped to 0.3.0

### Contributors
- **aimardcr** — security vulnerability report (#89)
- **billalxcode** — HTTP request feature request (#87)
- **RasyaAndrean** — all feature implementations, bug fixes, documentation

---

## V0.2/3(A) "Added" (2026-03-17)

### New Features
- **Struct Spread Syntax**: `Point { x: 10.0, ..p1 }` — create a new struct by copying fields from an existing instance and selectively overriding fields (#47)
- **Numeric Separators**: `1_000_000`, `3.14_159` — underscores as visual separators in integer and float literals (#49)

### Bug Fixes
- Fixed parser infinite loop on nested struct/enum with syntax errors — added error recovery breaks (#27)
- Fixed empty struct literal `Abc{}` producing confusing error — parser now handles it correctly (#42)
- Fixed string `+=` codegen — now generates `urus_str_concat()` instead of invalid C pointer arithmetic (#43)
- Fixed trailing decimal float `20.` not accepted by lexer — changed condition to allow floats without fractional digits (#48)
- Fixed garbage column numbers in sema error messages — `lexer_init()` was not initializing `line_start` field (#50)
- Fixed undefined function call causing segfault (PR #45)
- Fixed more accurate semantic error messages (PR #44)
- Fixed default output name based on platform: `a.out` (Linux), `a.exe` (Windows) (PR #39)
- Fixed version mismatch in Dockerfile
- Removed `inline` from `urus_str_equal` — GCC `-O2` already handles inlining

### Improvements
- Added Termux (Android) build instructions in README
- Editor support separated into dedicated repo: `Urus-Foundation/editor-support`
- Assets and diary moved to `Urus-Foundation/initial-resource`
- GitHub issue templates standardized to English (PR #38)

### Contributors
- **fepfitra** — issue reports (#27, #42, #43, #47, #48, #49, #50), default output name fix (PR #39)
- **John-fried** — segfault fix (PR #45), error accuracy (PR #44), assets removal (PR #40), issue templates (PR #38)
- **RasyaAndrean** — bug fixes, feature implementation, documentation, repo management

---

## V0.2/2(F) "Fixed" (2026-03-09)

### Build System
- Migrated from Makefile/build.bat to **CMake** for cross-platform portability
- Added `cmake/embed-string.cmake` to embed runtime header into the compiler binary

### Bug Fixes
- Fixed missing `stddef.h` include in `urus_runtime.h` (Linux compatibility)
- Fixed implicit declaration of POSIX functions in C11 mode
- Fixed unterminated string in `emit()` codegen function
- Compiler is now **standalone** — `urus_runtime.h` is embedded into the binary, no external runtime file needed
- Fixed `error.c` using POSIX `getline()` — replaced with portable `fgets()` for MSVC/Windows compatibility
- Fixed `_urus_tmp.c` double CRLF corruption on Windows — temp file now written in binary mode
- Fixed `--help`/`--version` flags not recognized (was treated as filename)
- Fixed GCC `cc1` not found on Windows — compiler now injects GCC bin directory into PATH
- Removed obsolete `-I include` flag from GCC invocation (runtime is now embedded)
- Added MSVC compatibility defines (`_CRT_SECURE_NO_WARNINGS`, `_CRT_NONSTDC_NO_DEPRECATE`)

### Improvements
- Added `show_help()` CLI usage with `--help` and `-h` flags
- Added `--version` / `-v` flag to display compiler version
- Rich **error diagnostics**: colored output with filename, line number, column caret (^) pointer
- Error reporting integrated into both **parser** and **semantic analysis**
- Added `install` and `uninstall` targets via CMake
- Updated installation documentation for CMake workflow
- Added parser error test case (`tests/invalid/parser/unclosed_brace.urus`)
- Dockerfile updated to use CMake build and org URL updated

### Contributors
- **John-fried** — Linux fixes, CMake migration, standalone compiler, error logging (PR #2-#7)
- **RasyaAndrean** — Project maintenance, PR reviews

---

## V0.2/1 (2026-03-02)

### New Features
- **Enums / Tagged Unions**: `enum Shape { Circle(r: float); Rect(w: float, h: float); Point; }`
- **Pattern Matching**: `match` statement with variant bindings
- **Modules / Imports**: `import "module.urus";` with circular import detection
- **Error Handling**: `Result<T, E>` type with `Ok(val)` / `Err(msg)`, `is_ok()`, `is_err()`, `unwrap()`, `unwrap_err()`
- **String Interpolation**: `f"Hello {name}, age {age}"` desugars to to_str + concat
- **For-each Loops**: `for item in array { ... }` — iterate over array elements
- **Conversion Functions**: `to_int()` and `to_float()` now fully implemented

### Bug Fixes
- Fixed Makefile missing sema.c and codegen.c
- Fixed `ast_type_str` static buffer clobber (round-robin buffers)
- Fixed `urus_str_replace` unsigned underflow with signed diff
- Fixed array codegen removing GCC statement expressions (standard C11)
- Fixed array element types (now supports `[float]`, `[str]`, `[bool]`, `[MyStruct]`)
- Fixed array index assignment generating invalid C lvalue
- Fixed temp file path dead ternary
- Added bounds checking on array access
- Break/continue now validated to be inside loops

### Improvements
- Reference counting: `retain`/`release` functions for str, array
- Larger emit buffer (4096 from 2048)
- `to_str` now retains the string (proper refcounting)
- Empty function params emit `void` in C for correctness
- Version string in compiler output

---

## V0.1 (2025-03-01)

### Initial Release
- Primitive types: int, float, bool, str, void
- Variables with `let` / `let mut`, mandatory type annotation
- Functions with typed parameters and return types
- Control flow: if/else, while, for (range-based), break, continue
- Operators: arithmetic, comparison, logical, assignment
- Structs (declaration, literal creation, field access)
- Arrays (literal, indexing, len, push)
- String concatenation with `+`
- Comments (single-line `//`, multi-line `/* */`)
- 30+ built-in functions (string ops, math, file I/O, assert)
- Transpiles to C via GCC
