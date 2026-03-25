# Contributor Guide

This guide is for anyone who wants to work on the URUS compiler, runtime, or tooling. It covers the coding standards, how to build and test, and the workflow for getting changes merged.

## Building the compiler

You need a C11 compiler (GCC, Clang, or MSVC) and CMake 3.10+.

```bash
git clone https://github.com/Urus-Foundation/Urus.git
cd Urus/compiler
cmake -S . -B build
cmake --build build
```

Verify the build:

```bash
./build/urusc --version          # Linux/macOS
./build/Debug/urusc.exe --version  # Windows
```

## Running the test suite

Tests live in `tests/run/`. Each test is a pair of files: a `.urus` source file and a `.expected` file containing the expected output.

```bash
cd tests

# Linux / macOS
bash run_tests.sh ../compiler/build/urusc

# Windows
run_tests.bat ..\compiler\build\Debug\urusc.exe
```

The test runner compiles each `.urus` file, runs the resulting binary, and compares the output against the `.expected` file. Any mismatch is a failure.

### Adding a test

Create the source file and its expected output:

```bash
cat > tests/run/my_feature.urus << 'EOF'
fn main(): void {
    print("it works");
}
EOF

echo "it works" > tests/run/my_feature.expected
```

Run the test suite to confirm it passes.

## Coding standards

### C style

The compiler is written in C11. Follow these conventions:

- **Indentation:** 4 spaces, no tabs.
- **Braces:** K&R style — opening brace on the same line as the statement.
- **Naming:** `snake_case` for everything. Public functions use a module prefix: `lexer_`, `parser_`, `sema_`, `codegen_`, `ast_`, `error_`, `urus_`.
- **Line length:** soft limit of ~100 characters.
- **Compile clean:** the codebase should build without warnings under `-Wall -Wextra`.

```c
void lexer_advance(Lexer *l) {
    if (l->pos >= l->len) {
        return;
    }
    l->pos++;
}
```

### Memory rules

- Heap allocations must have a matching free. String literals stored in AST nodes are duplicated with `strdup()` and freed in `ast_free()`.
- Runtime strings and arrays are ref-counted via `urus_str_retain`/`urus_str_release` and `urus_array_retain`/`urus_array_release`.
- On Windows, generated C files must be opened with `fopen("wb")` to prevent CRLF corruption.

### Error handling in the compiler

Errors are fatal: the compiler prints a diagnostic to stderr and exits with code 1. The diagnostic includes the filename, line number, the source line, and a caret (`^`) pointing to the exact column.

There's no error recovery — the compiler stops at the first error it encounters.

## Branch and PR workflow

All contributions target the `main` branch. The typical workflow:

1. Fork the repository on GitHub.
2. Create a feature branch from `main`:
   ```bash
   git checkout main && git pull
   git checkout -b feat/my-feature
   ```
3. Make your changes, add tests, and verify everything passes.
4. Commit with a descriptive message (see below).
5. Push and open a pull request targeting `main`.

### Commit messages

We use [Conventional Commits](https://www.conventionalcommits.org/):

```
feat: add optional type support
fix: correct bounds check in array codegen
docs: update installation guide for Termux
refactor: extract scope logic into separate file
test: add coverage for tuple destructuring
chore: update CMake minimum version
```

The prefix matters — it tells reviewers at a glance whether the commit adds functionality, fixes a bug, or changes internals.

### Branch naming

| Prefix | Purpose |
|--------|---------|
| `feat/` | New feature |
| `fix/` | Bug fix |
| `docs/` | Documentation changes |
| `refactor/` | Internal restructuring |

## How to add a new language feature

Adding a feature to URUS touches several files in a predictable order. Here's the checklist:

1. **`include/token.h`** — Define a new token if the feature introduces a keyword or operator.
2. **`src/lexer.c`** — Teach the lexer to recognize the new token.
3. **`include/ast.h`** and **`src/ast.c`** — Add an AST node type for the new construct. Add constructors, printing, cloning, and freeing logic.
4. **`src/parser.c`** — Parse the new syntax and build the AST node.
5. **`src/Sema/sema.c`** — Add type-checking rules for the new node.
6. **`src/codegen.c`** — Generate the corresponding C code.
7. **`runtime/urus_runtime.h`** — Add runtime support if the feature needs it (new data structures, built-in functions, etc.).
8. **`tests/run/`** — Add at least one test with a `.urus` source and `.expected` output.
9. **`SPEC.md`** — Update the language specification.
10. **`examples/`** — Add a standalone example program demonstrating the feature.

Not every feature needs all ten steps — a new operator might skip the runtime and AST steps, for instance. But the order is always the same: lexer → parser → sema → codegen → tests → docs.

### Worked example: adding the `**` operator

Here's what it looked like to add the exponentiation operator:

```
1. token.h    → added TOK_POWER
2. lexer.c    → recognize "**" as a two-character operator
3. ast.h      → reused NODE_BINARY with op="**"
4. parser.c   → added ** to the precedence table (right-associative, above multiplication)
5. sema.c     → validated that both operands are numeric
6. codegen.c  → emitted pow(a, b) from <math.h>
7. runtime    → not needed (pow is already in the C standard library)
8. tests/     → added run/power.urus + run/power.expected
9. SPEC.md    → documented ** in the operators section
```

## Debugging tips

- **`--emit-c`** shows the generated C. This is the fastest way to understand what the compiler does with your code and to spot codegen bugs.
- **`--tokens`** dumps the lexer output. Useful when you're adding a new token and want to verify the lexer recognizes it.
- **`--ast`** dumps the parsed tree. Useful for checking that the parser builds the right structure.
- If GCC fails to compile the generated code, the temp file `_urus_tmp.c` is left on disk for inspection.
