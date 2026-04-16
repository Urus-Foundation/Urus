# Contributing to URUS

Thank you for your interest in contributing to URUS! Whether it's a bug fix, new feature, documentation improvement, or test case — every contribution matters.

---

## Getting Started

### 1. Fork and Clone

```bash
git clone https://github.com/<your-username>/Urus.git
cd Urus
```

### 2. Build the Compiler

```bash
cd compiler
cmake -S . -B build
cmake --build build
```

On **Termux**:
```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=$PREFIX
cmake --build build
```

### 3. Run Tests

```bash
cd compiler/build
ctest
```

> [!TIP]
> Each test is a `.urus` file paired with a `.expected` output file in `tests/run/`.

---

## Ways to Contribute

### Report a Bug

1. Search [existing issues](https://github.com/Urus-Foundation/Urus/issues) first
2. Open a new issue with:
   - URUS version (`urusc --version`)
   - OS and C compiler version
   - Minimal `.urus` file that reproduces the bug
   - Expected vs actual behavior
   - Error message (if any)

### Suggest a Feature

1. Open an issue with the `enhancement` label
2. Describe the feature and its use case
3. Show example URUS syntax if possible

### Submit Code

1. **Branch** from `main`:
   ```bash
   git checkout main && git pull origin main
   git checkout -b feat/your-feature-name
   ```

2. **Implement** your changes following the [coding standards](#coding-standards)

3. **Add tests** in `tests/run/` — create a `.urus` source and matching `.expected` file

4. **Verify** everything passes:
   ```bash
   cd compiler/build && ctest
   ```

5. **Commit** with a clear message:
   ```bash
   git commit -m "feat: short description of changes"
   ```

6. **Push** and open a Pull Request:
   ```bash
   git push -u origin feat/your-feature-name
   ```

---

## Coding Standards

### C Code (Compiler)

| Rule | Standard |
|------|----------|
| **Language** | C11 (`-std=c11`) |
| **Indentation** | 4 spaces, no tabs |
| **Line length** | 80 characters max |
| **Naming** | `snake_case` everywhere, public functions use module prefix (`lexer_`, `sema_`) |
| **Warnings** | Must compile clean under `-Wall -Wextra` |

**Brace style** — custom Mozilla/K&R hybrid:

```c
/* Function: brace on a new line */
void
lexer_advance(Lexer *l)
{
    /* Control flow: brace on the same line */
    if (l->pos >= l->len) {
        return;
    }
    l->pos++;
}

/* Struct/Enum: brace on the same line */
typedef struct {
    int pos;
    int len;
} Lexer;
```

> No single-line blocks. Functions, enums, and `if` statements must always use multiple lines.

### URUS Code

```rust
fn main(): void {
    let name: str = "World";
    print(f"Hello {name}!");
}
```

- Semicolons terminate statements
- `let` for immutable, `let mut` for mutable
- Explicit return types on functions

### Commit Messages

[Conventional Commits](https://www.conventionalcommits.org/) format:

| Type | Usage |
|------|-------|
| `feat` | New feature |
| `fix` | Bug fix |
| `refactor` | Internal change, no behavior change |
| `docs` | Documentation only |
| `test` | New or updated tests |
| `chore` | Build, CI, or tooling changes |

---

## Adding a New Language Feature

Follow this checklist when adding a new language feature:

| Step | File(s) |
|------|---------|
| 1. Add token | `compiler/include/token.h` |
| 2. Recognize token | `compiler/src/lexer.c` |
| 3. Add AST node | `compiler/include/ast.h` + `compiler/src/ast.c` |
| 4. Parse syntax | `compiler/src/parser.c` |
| 5. Type-check | `compiler/src/Sema/sema.c` |
| 6. Generate C code | `compiler/src/codegen.c` |
| 7. Runtime support | `compiler/runtime/urus_runtime.h` (if needed) |
| 8. Add tests | `tests/run/` (`.urus` + `.expected`) |
| 9. Update spec | `SPEC.md` |
| 10. Add example | `examples/` |

---

## Adding a Standard Library Module

Standard library modules live in `compiler/stdlib/` as `.urus` files:

1. Create `compiler/stdlib/your_module.urus`
2. Use `__emit__()` for C-level bindings when needed
3. Prefix all exported functions with the module name (e.g., `math_sin`, `json_parse`)
4. Add test cases in `tests/run/`
5. Document in `SPEC.md` under the Standard Library section

---

## Pull Request Guidelines

- Target the `main` branch
- Keep PRs focused — one feature or fix per PR
- Include tests for new functionality
- Update documentation if the change affects user-facing behavior
- All existing tests must pass before merging
- Describe **what** your PR does and **why** in the PR description

---

## Project Structure

```
Urus/
├── compiler/
│   ├── src/                   # Compiler source code
│   │   ├── main.c             # CLI entry point
│   │   ├── lexer.c            # Tokenizer
│   │   ├── parser.c           # Recursive descent parser
│   │   ├── codegen.c          # C11 code generator
│   │   ├── ast.c              # AST constructors
│   │   ├── preprocess.c       # Import resolution + rune expansion
│   │   ├── error.c            # Error reporting
│   │   ├── util.c             # File and string utilities
│   │   └── Sema/              # Semantic analysis
│   │       ├── sema.c         # Type checking
│   │       ├── builtins.c     # Built-in function signatures
│   │       └── scope.c        # Scope and symbol table
│   ├── include/               # Header files
│   ├── runtime/               # Header-only runtime (urus_runtime.h)
│   ├── stdlib/                # Standard library modules (.urus)
│   └── CMakeLists.txt         # Build system
├── examples/                  # Sample programs
├── tests/run/                 # Integration tests (.urus + .expected)
└── documentation/             # Extended docs
```

---

## Need Help?

- [Documentation](./documentation/index.md)
- [Language Spec](./SPEC.md)
- [Examples](./examples/)
- [Open an issue](https://github.com/Urus-Foundation/Urus/issues)

---

## License

By contributing, you agree that your contributions will be licensed under the [Apache License 2.0](./LICENSE).
