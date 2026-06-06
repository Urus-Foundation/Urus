# 14 — Troubleshooting

Common errors with their causes and fixes. Use this as a reference, not
a tutorial.

---

## Build errors (building the *compiler itself*)

### `cmake: command not found`

CMake is not installed.

- Windows: `winget install Kitware.CMake`
- macOS: `brew install cmake`
- Linux: `sudo apt install cmake` (or equivalent)

### `No CMAKE_C_COMPILER could be found`

No C compiler in PATH.

- Windows: `winget install LLVM.LLVM`
- macOS: `xcode-select --install`
- Linux: `sudo apt install build-essential`

### `unknown type name 'uint32_t'` while building the compiler

Your C compiler is older than C11. Use a modern GCC or Clang.

### Build succeeds but `urusc` does not run

On Windows, `urusc.exe` lands at `compiler/build/Debug/urusc.exe` (or
`Release` if you built `--config Release`). Use the full path.

On POSIX, `urusc` lands at `compiler/build/urusc`. Either use the full
path or add `compiler/build` to your `PATH`.

---

## URUS compilation errors (`urusc` ran but reported a diagnostic)

### `lexer: unterminated string literal`

You opened a `"` but never closed it. Strings cannot span lines without
explicit escapes. Add the missing `"`.

### `lexer: invalid escape sequence`

You wrote `\q` or similar. Allowed escapes: `\n \r \t \\ \" \xNN`.

### `parser: expected X, found Y`

A grammar mismatch. The diagnostic carries the source location — look
at that line. Common cases:

- Missing comma in a struct field list.
- Extra semicolon at the end of an expression block.
- `return` without a value in a function that returns something.

### `parser: cannot parse this expression`

You hit an expression form the parser does not (yet) handle. Open an
issue; include the minimal source that triggers it.

### `sema: undefined identifier 'foo'`

The name is not in scope. Common causes:

- Typo.
- Forgot to `use a.b.foo`.
- Defined later in the file in a context that requires forward declaration.
- Used a generic type argument that the compiler ignores (generics are
  parsed but not yet instantiated in v0.0.1).

### `sema: duplicate definition of 'Foo'`

Two top-level items share the same name. Rename one.

### `sema: struct 'Foo' has no field 'bar'`

The field name in a struct literal does not exist on the struct. Check
spelling and definition.

---

## Build errors compiling the **produced** `.c` file

### `URUS does not support native MSVC. Use Clang, GCC, or clang-cl.`

You compiled the emitted `.c` with `cl.exe`. URUS's emitted C uses GCC
statement-expressions and `__auto_type`. Use `clang` or `clang-cl`.

```powershell
clang hello.urus.c -I stdlib/runtime -o hello.exe
```

### `fatal error: 'urus_rt.h' file not found`

You forgot the runtime include path. Add `-I stdlib/runtime`.

### `'__auto_type' undeclared`

Same root cause as the MSVC error. Switch to GCC or Clang.

### Linker error mentioning `urus_println_fmt` or similar

The runtime is header-only; if a function appears unresolved, you are
mixing two TUs that included different versions of `urus_rt.h`. Rebuild
everything with the same runtime.

---

## Runtime crashes (the produced binary crashes)

### Prints garbage when reading a `Result<MyStruct, str>`

Known limitation. `Result` / `Option` payloads are truncated to
`int64_t` in v0.0.1. Use small payloads (≤ 8 bytes) or wait for v0.0.2.
See F-TY-2 in [`docs/security/SECURITY-AUDIT.md`](../security/SECURITY-AUDIT.md).

### `match` falls through and reads garbage

Known limitation. `match` does not check exhaustiveness in v0.0.1. Add
a wildcard arm:

```urus
match thing {
    Ok(v)  => …,
    Err(e) => …,
    _      => panic("unreachable"),
}
```

### `defer` did not run before my `return`

Known limitation. `defer` fires at block end only, not on `return`.
Restructure to use the final expression of the block as your return
value, or wait for v0.0.3.

### "Segfault on f-string interpolation of a large struct"

Probably F-TY-2 in disguise (a struct snuck through where an `int64_t`
was expected). Verify the type of the value being interpolated.

### `tmpfile() returned NULL` from `urus_fmt_to_str`

You are in a sandboxed environment where `tmpfile()` cannot find a
writable temp directory. Set `TMPDIR` (POSIX) or `TMP` (Windows) to a
writable location. Long-term fix is in v0.0.2 (switch to an
in-memory writer).

---

## Test harness issues

### "0 tests run"

The runner could not find `urusc`. Build it first:

```bash
cmake -B compiler/build -S .
cmake --build compiler/build
```

### A test in `tests/run/` is "skipped"

Probably means no host C compiler was detected. The runner verifies
emit-c only in that case. Install Clang to get full end-to-end testing.

### A test in `tests/fail/` "did not produce the expected error"

Either the test was added with a wrong `// expect:` directive, or a
recent change regressed the diagnostic. Look at the actual output.

---

## Security incident response

If you believe you have found a security issue:

1. **Do not file a public issue.** Email `urusfoundation@gmail.com` (placeholder
   until the disclosure policy ships in `SECURITY.md`).
2. Include: reproducer, urusc version, host OS and C compiler, the
   expected vs actual behavior.
3. Expect a response within 7 days. The disclosure SLA is 90 days
   (Tier-3 hardening item).

Known stop-ship issues in v0.0.1 are tracked in
[`docs/security/SECURITY-AUDIT.md`](../security/SECURITY-AUDIT.md);
fixes ship in v0.0.2.

---

## When this doc does not help

Open a GitHub Discussion (planned), or send a draft PR with a
*question* and someone will pick it up. We would rather answer a
question early than fix a misunderstanding late.

— *Last updated 2026-06-03.*
