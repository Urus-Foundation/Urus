# Security Model

This document describes how URUS handles memory safety, what the compiler's attack surface looks like, and what limitations users and developers should be aware of.

## Threat model

URUS has five areas where security matters:

1. **The compiler itself.** It processes untrusted input (source code) and must not crash, leak memory, or execute arbitrary code.
2. **The generated C code.** If the codegen produces incorrect C, the resulting binary could have memory safety issues that the source-level language was supposed to prevent.
3. **The runtime library.** It handles heap allocation, ref-counting, and array bounds checking. Bugs here undermine the safety guarantees.
4. **HTTP built-ins.** The `http_get` and `http_post` functions make network requests via curl. They pass URLs directly without sanitization.
5. **Raw emit.** The `__emit__` statement inserts arbitrary C code into the generated output, bypassing all safety checks.

## Memory safety

### Reference counting

All heap-allocated values in URUS — strings, arrays, and structs — use reference counting for automatic memory management. The compiler inserts `retain` (increment refcount) and `release` (decrement refcount, free when zero) calls at the right points:

- When a value is assigned to a new variable, `retain` is called.
- When a variable goes out of scope or is reassigned, `release` is called on the old value.
- When the refcount reaches zero, the memory is freed immediately.

This gives URUS deterministic destruction: you always know when memory is freed, and there are no GC pauses.

**Known limitation: no cycle detection.** If two objects reference each other, their refcounts never reach zero and the memory leaks. URUS currently has no mechanism to detect or break reference cycles. In practice this rarely comes up because the language doesn't have mutable references between heap objects, but it's theoretically possible with arrays of structs.

### Bounds checking

Every array access goes through a checked getter that validates the index at runtime:

```c
if (index < 0 || index >= a->len) {
    fprintf(stderr, "Array index out of bounds: %lld (len=%lld)\n", index, a->len);
    exit(1);
}
```

There is no way to perform an unchecked array access in URUS (unless you use `__emit__` to write raw C).

### Type safety

The semantic analysis pass ensures:

- All variables have a known type at compile time.
- No implicit type coercion occurs (you must use `to_str`, `to_int`, or `to_float` explicitly).
- Variables are immutable unless declared with `mut`.
- There are no null pointers — every variable must be initialized.
- There is no pointer arithmetic or manual memory management in user code.

## Compiler security

### Input handling

The compiler is designed to handle malformed input gracefully:

- The lexer rejects invalid byte sequences.
- The parser rejects malformed syntax and reports the error with the source location.
- The semantic analyzer rejects type mismatches, undefined variables, and other semantic errors.
- Error messages include filenames and line numbers but do not leak internal compiler state.

### Known risks

| Risk | Severity | Notes |
|------|----------|-------|
| Buffer overflow in compiler | Low | Static buffers are sized conservatively. Round-robin buffers used for `ast_type_str`. |
| Stack overflow from deep recursion | Low | Extremely nested code could overflow the parser's call stack. |
| Path traversal via `import` | Medium | Imports can reference `../../` paths. However, imported files are parsed as URUS source, not executed. |
| Request injection via HTTP | Medium | `http_get` and `http_post` pass the URL string directly to curl without sanitization. |
| Arbitrary code via `__emit__` | High | By design. Users who use `__emit__` accept full responsibility. |

### Import security

Import statements resolve relative paths, so something like `import "../../etc/passwd"` would attempt to parse `/etc/passwd` as URUS source code — which would fail because it's not valid URUS syntax. The import system does not execute files, only parses them.

- All imports are resolved at compile time (no dynamic imports).
- Circular imports are detected.
- Maximum 64 imported files per program.

## Network access

By default, URUS programs have no network access. The only way to make network requests is through the `http_get` and `http_post` built-in functions, which shell out to `curl`. If `curl` isn't installed, these functions fail.

There is no sandboxing on the URLs passed to curl. If you're compiling untrusted code, be aware that it could make arbitrary HTTP requests.

## File system access

URUS programs can read and write files through `read_file`, `write_file`, and `append_file`. These functions have the same permissions as the user running the program — there's no sandboxing or permission model.

## The `__emit__` escape hatch

The `__emit__` statement inserts raw C code into the generated output. It bypasses all of URUS's type checking, bounds checking, and memory safety. An `__emit__` call can do anything C can do, including calling `system()`, writing to arbitrary memory addresses, or invoking undefined behavior.

Treat `__emit__` the way you'd treat `unsafe` in Rust: necessary sometimes, but review every use carefully.

## Recommendations

### For users

- Don't compile and run URUS programs from untrusted sources without reviewing them first.
- Use `--emit-c` to inspect the generated C before running third-party code.
- Be aware that `__emit__` can do anything — look for it when reviewing code.
- Avoid circular data structures (they leak memory due to the ref-counting model).

### For compiler developers

- Use `snprintf` instead of `sprintf` for all buffer writes.
- Use `fopen("wb")` for generated files on Windows.
- Always free AST nodes to avoid compiler memory leaks.
- Never embed user-provided strings into generated C without proper escaping.
- Test the compiler with malformed input. Fuzz testing is recommended.

## Reporting vulnerabilities

If you discover a security issue in the URUS compiler or runtime, please report it privately:

- **GitHub Security Advisories:** https://github.com/Urus-Foundation/Urus/security/advisories
- **Email:** rasyaandrean@outlook.co.id

Do not open a public issue for security vulnerabilities. See the [Security Policy](../../SECURITY.md) for the full reporting process and response timeline.
