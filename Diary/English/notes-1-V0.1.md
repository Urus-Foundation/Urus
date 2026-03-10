# Rasya's Notes #1 — V0.1

**Date:** 2025
**By:** Rasya Andrean

---

## The Beginning

This was the very first version of URUS. The idea was simple — create a programming language that's easy to learn but still compiles to native binaries through C. Not as low-level as C, but not as slow as Python either.

## What Was Built

The basic compiler pipeline was working. Lexer for tokenization, parser for building AST, semantic analyzer for type checking, and codegen for generating C code. The flow: `.urus` → lexer → parser → sema → codegen → `.c` → GCC → binary.

Features included in this version:
- Primitive types: `int`, `float`, `bool`, `str`, `void`
- Immutable variables by default, use `mut` for mutable
- Functions with parameters and return types
- Simple structs
- Arrays (`[int]` only)
- Control flow: `if/else`, `while`, `for..in` range
- Standard operators: arithmetic, comparison, logical
- Built-in functions: `print`, `len`, `push`, `to_str`
- String operations: `str_len`, `str_upper`, `str_lower`, `str_trim`, `str_contains`, `str_slice`, `str_replace`
- File I/O: `read_file`, `write_file`, `append_file`
- Comments: `//` and `/* */`
- Header-only runtime (`urus_runtime.h`)

## State of Things

Still very early. The compiler worked but wasn't stable. No enums, no pattern matching, no string interpolation. Arrays only supported `int`. Minimal documentation. But the foundation was there — and that's what mattered.

---

*V0.1 — the first step.*
