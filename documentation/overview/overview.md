# URUS Overview

## What is URUS?

URUS is a statically-typed programming language that compiles to standard C11. The compiler reads `.urus` source files, runs them through lexing, parsing, semantic analysis, and code generation, and produces a portable C file. That C file is then compiled to a native binary by GCC or Clang.

The result is a language that feels modern to write — with pattern matching, string interpolation, tuples, and automatic memory management — but produces the same fast, lightweight binaries you'd get from hand-written C.

## Why another language?

There's a gap between languages that are easy to learn and languages that produce efficient native code.

- **C** gives you full control and fast binaries, but managing memory manually is error-prone and the syntax hasn't evolved much in decades.
- **Python** and **JavaScript** are easy to pick up, but they're interpreted and slow for compute-heavy tasks.
- **Rust** solves the safety problem elegantly, but the borrow checker and lifetime annotations create a steep learning curve.
- **Go** is simple and compiles fast, but it relies on garbage collection and doesn't offer the same level of type expressiveness.

URUS sits in the middle: safer than C, simpler than Rust, faster than Python, and more expressive than Go. It's not trying to replace any of these — it's an alternative for people who want something approachable without giving up native performance.

## Who is URUS for?

**Students and educators.** The compiler is small enough (~7,100 lines of C) to read end-to-end, making it a practical case study for compiler courses. The language itself is simple enough to teach in a semester.

**Developers who want native speed with modern ergonomics.** If you're building CLI tools, small servers, or performance-sensitive utilities and don't want the complexity of Rust or the overhead of a runtime, URUS is a good fit.

**Hobbyists and language enthusiasts.** If you enjoy exploring new languages and contributing to early-stage projects, URUS is actively developed and welcomes contributions.

## How the compiler works

The URUS compiler is a single-pass transpiler written in C. It goes through five stages:

```
Source (.urus)
     │
     ▼
 Preprocessor    Resolve imports, expand rune macros
     │
     ▼
   Lexer         Break source text into tokens
     │
     ▼
   Parser        Build an Abstract Syntax Tree (AST)
     │
     ▼
   Sema          Check types, scopes, and correctness
     │
     ▼
  Codegen        Emit standard C11 code
     │
     ▼
 GCC / Clang     Compile C to a native binary
```

Because URUS targets C rather than machine code directly, you get portability for free — if your platform has a C11 compiler, URUS programs will run on it. You also get decades of optimization work from GCC and Clang without having to implement any of it.

The `--emit-c` flag lets you inspect the generated C at any time, which makes debugging straightforward and the compiler's behavior fully transparent.

## Key features at a glance

**Type system.** All types are checked at compile time. Variables are immutable by default and require an explicit `mut` to allow mutation. Type inference is supported for local variables, so you can write `let x = 42;` and the compiler figures out the rest.

**Pattern matching.** The `match` statement works on enums (with field bindings), integers, strings, and booleans. Combined with the `Result<T, E>` type for error handling, this gives you expressive control flow without exceptions.

**Tuples and destructuring.** Tuple types like `(int, str)` are stack-allocated and support destructuring in both `let` statements and `for` loops: `let (x, y) = get_pair();`.

**Runes (macros).** URUS has a lightweight macro system called "runes." A rune like `rune square(x) { x * x }` expands at compile time via textual substitution when invoked as `square!(5)`.

**Defer.** Borrowed from Go, `defer { ... }` schedules a block to run at the end of the enclosing function, regardless of which return path is taken. Multiple defers execute in LIFO order.

**Automatic memory management.** Heap-allocated values (strings, arrays, structs) are reference-counted. The compiler inserts retain and release calls automatically. There's no garbage collector and no manual free.

**Modules.** Multi-file programs use `import "file.urus";` to pull in declarations. The compiler resolves imports relative to the source file or via the `URUSCPATH` library path.

**Standalone compiler.** The runtime library is embedded into the compiler binary at build time. There are no external dependencies beyond GCC/Clang — a single `urusc` binary is all you need.

## Technology stack

| Component | Implementation |
|-----------|---------------|
| Compiler language | C11, ~7,100 lines |
| Runtime | Header-only C library, embedded in the compiler binary |
| Memory model | Reference counting with automatic retain/release |
| Code generation target | Standard C11 (no compiler extensions) |
| Build system | CMake 3.10+ |
| Supported platforms | Windows, Linux, macOS, Termux (Android) |

## Current status

URUS is at version **0.3.x** and under active development. The core language — types, control flow, structs, enums, pattern matching, error handling, tuples, macros, defer, modules — is implemented and tested. The compiler has 33 integration tests and compiles cleanly on all supported platforms.

The next milestones are generics, `Option<T>`, and method syntax (`impl` blocks). See the [Project Roadmap](../roadmap/project-roadmap.md) for the full plan.

## Inspirations

URUS draws ideas from several languages:

- **Rust** — enums with data, pattern matching, Result type, immutability by default
- **Go** — simplicity, fast compilation, defer, clean syntax
- **Zig** — transpile-to-C philosophy, minimal runtime
- **Python** — f-string interpolation, readability as a design goal
