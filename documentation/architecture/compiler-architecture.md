# Compiler Architecture

This document describes the internal structure of the URUS compiler — how source code flows through the system, what each module does, and how they connect to each other.

## Pipeline overview

The compiler processes source code in six stages. Each stage takes the output of the previous one and transforms it further:

```
Source (.urus)
     │
     ▼
 Preprocessor ── resolve imports, expand rune macros
     │
     ▼
   Lexer ──────── break source text into tokens
     │
     ▼
   Parser ─────── build an Abstract Syntax Tree
     │
     ▼
   Sema ──────── type-check and validate
     │
     ▼
  Codegen ─────── emit standard C11 code
     │
     ▼
 GCC / Clang ──── compile C to native binary
```

The key design decision is that URUS targets C rather than machine code. This keeps the compiler small (roughly 7,100 lines of C) and gives us decades of optimization from GCC and Clang for free. The tradeoff is a two-step compilation: URUS-to-C, then C-to-binary.

## Stage details

### Preprocessor

**Files:** `src/preprocess.c`

Before any compilation begins, the preprocessor handles two tasks:

1. **Import resolution.** When the compiler encounters `import "file.urus"`, it reads and parses the imported file, then merges its declarations into the main program's AST. Imports are resolved relative to the importing file first, then via the `URUSCPATH` environment variable. Circular imports are detected and rejected. The current limit is 64 imported files per program.

2. **Rune expansion.** Rune definitions (`rune square(x) { x * x }`) are stored in a lookup table. When the parser later encounters an invocation like `square!(5)`, the preprocessor substitutes the arguments into the rune body.

### Lexer

**Files:** `src/lexer.c`, `include/token.h`, `include/lexer.h`

The lexer converts source text into a flat array of tokens. Each token carries its type (keyword, identifier, literal, operator, etc.), its text content, and its position in the source file (line and column).

Notable implementation details:

- F-string literals like `f"Hello {name}"` are tokenized by creating a sub-lexer that processes the embedded expressions. The result is a sequence of string parts and expression tokens that the parser reassembles.
- Numeric literals support hexadecimal (`0xFF`), octal (`0o755`), binary (`0b1010`), scientific notation (`1.5e-10`), and underscore separators (`1_000_000`).
- String escape sequences include `\n`, `\t`, `\0`, `\\`, and `\"`.

### Parser

**Files:** `src/parser.c`, `include/parser.h`, `include/ast.h`, `src/ast.c`

The parser is a recursive descent parser with Pratt parsing for operator precedence. It reads the token array and produces an Abstract Syntax Tree (AST).

The AST node types cover everything the language supports: function declarations, struct and enum definitions, let bindings (including tuple destructuring), all control flow statements (if, while, do-while, for-range, for-each, match, defer), expressions (binary, unary, calls, field access, index access, if-expressions), and top-level constructs (const, type alias, emit, import).

F-string literals are desugared during parsing into a chain of `to_str()` calls and string concatenations.

### Semantic analysis

**Files:** `src/Sema/sema.c`, `src/Sema/builtins.c`, `src/Sema/scope.c`

Semantic analysis (sema) runs two passes over the AST:

1. **Registration pass.** Walks top-level declarations and registers all functions, structs, enums, constants, and type aliases into the global scope. This allows forward references — you can call a function that's defined later in the file.

2. **Checking pass.** Walks every function body and validates types, scopes, mutability, and correctness. Specific checks include:
   - Type compatibility for assignments, comparisons, and function arguments
   - Immutability enforcement (assigning to a non-`mut` variable is an error)
   - Break/continue only inside loops
   - Unused variable warnings
   - Match arm exhaustiveness for enum patterns
   - Const expressions must be compile-time evaluable

Built-in function signatures (print, len, push, str_trim, etc.) are registered in `builtins.c`. Scope management (entering/leaving blocks, variable lookup) lives in `scope.c`.

### Code generation

**Files:** `src/codegen.c`, `include/codegen.h`

The code generator walks the type-checked AST and emits standard C11 code into a string buffer. It produces only portable C — no GCC or Clang extensions.

Key responsibilities:

- **RAII.** The codegen inserts `retain` calls when a ref-counted value is assigned, and `release` calls when a variable goes out of scope. This is how automatic memory management works without a garbage collector.
- **Defer.** Before every `return` statement and at the end of each function, the codegen inserts the defer blocks in LIFO order.
- **Tuples.** For each unique tuple signature (like `(int, str)`), the codegen creates a C typedef and, if any element is heap-allocated, a drop function to release those fields.
- **Enums.** Each enum becomes a C tag enum plus a struct with a union of variant payloads.
- **Match.** Enum match compiles to a switch on the tag. Primitive match compiles to an if-else chain.
- **Runtime embedding.** The runtime header (`urus_runtime.h`) is embedded into the compiler binary as a C array at build time. During compilation, it's written into the generated C file.

### Compilation

**File:** `src/main.c`

After codegen, `main.c` orchestrates the final steps:

1. Write the generated C code to a temporary file (`_urus_tmp.c`).
2. Invoke GCC with `-std=c11 -O2 -lm` to compile it into a native binary.
3. Delete the temporary file on success.

On Windows, the compiler auto-detects GCC in common installation paths (MSYS2, MinGW, WinLibs) and injects the directory into PATH so that GCC's internal tools (`cc1`, etc.) are accessible.

## Source tree

```
compiler/
├── src/
│   ├── main.c             CLI entry point, orchestration
│   ├── lexer.c            Tokenizer
│   ├── parser.c           Recursive descent parser
│   ├── ast.c              AST constructors, printing, cloning, freeing
│   ├── preprocess.c       Import resolution and rune expansion
│   ├── codegen.c          C11 code generation
│   ├── error.c            Error reporting with source context
│   ├── util.c             File reading, string helpers
│   └── Sema/
│       ├── sema.c         Type checking and validation
│       ├── builtins.c     Built-in function signatures
│       └── scope.c        Scope and symbol table management
├── include/
│   ├── token.h            Token type definitions
│   ├── lexer.h            Lexer API
│   ├── parser.h           Parser API
│   ├── ast.h              AST node types, AstType definitions
│   ├── sema.h             Semantic analysis API
│   ├── codegen.h          Codegen API, CodeBuf struct
│   ├── error.h            Error reporting API
│   └── util.h             Utility API
├── runtime/
│   └── urus_runtime.h     Header-only runtime library
├── stdlib/                Standard library modules (.urus)
├── cmake/
│   └── embed-string.cmake Embeds runtime into compiler binary
├── CMakeLists.txt         Build configuration
└── config.h.in            Version string template
```

## Module dependencies

Each module has a narrow set of dependencies:

```
main.c ── lexer.h, parser.h, sema.h, codegen.h, error.h, ast.h, util.h
lexer.c ── lexer.h, token.h
preprocess.c ── lexer.h, parser.h, ast.h
parser.c ── parser.h, lexer.h (f-string sub-lexing), ast.h, token.h
Sema/sema.c ── sema.h, ast.h
Sema/builtins.c ── sema.h
Sema/scope.c ── sema.h
codegen.c ── codegen.h, ast.h
error.c ── error.h, token.h
```

## Data flow summary

```
Source text → Token[] → AstNode* → AstNode* (typed) → char* (C code) → binary
              Lexer     Parser      Sema                Codegen          GCC
```

Every intermediate representation is inspectable:
- `--tokens` dumps the token array
- `--ast` dumps the AST
- `--emit-c` dumps the generated C code
