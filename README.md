<p align="center">
  <img src="assets/logo.jpg" alt="URUS Logo" width="150" />
  <h1 align="center">URUS Programming Language</h1>
  <p align="center">
    <strong>A statically-typed, compiled programming language that transpiles to portable C11.</strong>
  </p>
  <p align="center">Safe. Simple. Fast.</p>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/version-V0.2/2(F)-blue" alt="Version" />
  <img src="https://img.shields.io/badge/build-passing-brightgreen" alt="Build" />
  <img src="https://img.shields.io/badge/license-Apache%202.0-green" alt="License" />
  <img src="https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey" alt="Platform" />
  <img src="https://img.shields.io/badge/C-C11-orange" alt="C Standard" />
  <img src="https://img.shields.io/badge/status-stable-brightgreen" alt="Status" />
</p>

<p align="center">
  <a href="#quick-start">Quick Start</a> &nbsp;&bull;&nbsp;
  <a href="#features">Features</a> &nbsp;&bull;&nbsp;
  <a href="./documentation/">Documentation</a> &nbsp;&bull;&nbsp;
  <a href="./examples/">Examples</a> &nbsp;&bull;&nbsp;
  <a href="#roadmap">Roadmap</a>
</p>

---

## Language Status

URUS **V0.2/2(F) "Fixed"** is in active development. The core language syntax and features are functional. The project follows a custom versioning scheme: `V{major}.{minor}/{patch}`.

---

## Why URUS?

| Goal | How |
|------|-----|
| **Safer than C** | Reference counting, bounds checking, immutable by default |
| **Simpler than Rust** | No borrow checker, no lifetimes — just write code |
| **Faster than Python** | Compiles to native binary via C11 |
| **More portable than Go** | Transpiles to standard C11 — runs anywhere GCC runs |
| **Modern syntax** | Enums, pattern matching, string interpolation, Result type |

---

## Quick Start

### Requirements

- **GCC** 8+ (MinGW-w64 / MSYS2 on Windows)
- **CMake** 3.10+

### Build from Source

```bash
git clone https://github.com/Urus-Foundation/Urus.git
cd Urus/compiler

cmake -S . -B build
cmake --build build
```

Install to system:

```bash
# Linux / macOS
sudo cmake --install build

# Windows (Run As Administrator)
cmake --install build
```

### Prebuilt Binary

> Coming soon — check [Releases](https://github.com/Urus-Foundation/Urus/releases) page.

### Hello World

```rust
fn main(): void {
    print("Hello, World!");
}
```

```bash
urusc hello.urus -o hello
./hello
# Hello, World!
```

---

## Features

### Primitive Types

| Type | Description | C Equivalent |
|------|-------------|--------------|
| `int` | 64-bit signed integer | `int64_t` |
| `float` | 64-bit floating point | `double` |
| `bool` | Boolean (`true` / `false`) | `bool` |
| `str` | UTF-8 string (ref-counted) | `urus_str*` |
| `void` | No value | `void` |
| `[T]` | Dynamic array of T | `urus_array*` |
| `Result<T, E>` | Ok or Err value | `urus_result` |

### Variables

```rust
let x: int = 10;           // immutable
let mut count: int = 0;    // mutable
count += 1;
```

### Functions

```rust
fn add(a: int, b: int): int {
    return a + b;
}

fn greet(name: str = "Anonymous") {
    print(f"Hello {name}!");
}
```

### Control Flow

```rust
// If / Else
if x > 10 {
    print("big");
} else {
    print("small");
}

// While
while x < 100 {
    x += 1;
}

// For (range)
for i in 0..10 {       // exclusive: 0 to 9
    print(f"{i}");
}

for i in 0..=10 {      // inclusive: 0 to 10
    print(f"{i}");
}

// For-each (array)
let names: [str] = ["Alice", "Bob"];
for name in names {
    print(name);
}
```
> The parentheses in the condition are optional. Example: `if (condition) { ... }`

### Structs

```rust
struct Point {
    x: float;
    y: float;
}

fn distance(a: Point, b: Point): float {
    let dx: float = a.x - b.x;
    let dy: float = a.y - b.y;
    return sqrt(dx * dx + dy * dy);
}
```

### Enums & Pattern Matching

```rust
enum Shape {
    Circle(r: float);
    Rect(w: float, h: float);
    Point;
}

fn area(s: Shape): float {
    match s {
        Shape.Circle(r) => {
            return 3.14159 * r * r;
        }
        Shape.Rect(w, h) => {
            return w * h;
        }
        Shape.Point => {
            return 0.0;
        }
    }
    return 0.0;
}
```

### Arrays

```rust
let nums: [int] = [1, 2, 3, 4, 5];
let first: int = nums[0];

let mut items: [int] = [];
push(items, 42);
print(f"Length: {len(items)}");
```

### String Interpolation

```rust
let name: str = "World";
let count: int = 42;
print(f"Hello {name}! Answer: {count}");
```

### Default parameters value
```rust
fn greet(name: str = "John-fried") {
    print(f"Hello there {name}");
}
```

### Modules / Imports

```rust
// math_utils.urus
fn square(x: int): int {
    return x * x;
}

// main.urus
import "math_utils.urus";

fn main(): void {
    print(f"5^2 = {square(5)}");
}
```

### Error Handling (Result Type)

```rust
fn divide(a: int, b: int): Result<int, str> {
    if b == 0 {
        return Err("division by zero");
    }
    return Ok(a / b);
}

fn main(): void {
    let r: Result<int, str> = divide(10, 0);
    if is_err(r) {
        print(f"Error: {unwrap_err(r)}");
    } else {
        print(f"Result: {unwrap(r)}");
    }
}
```

---

## Built-in Functions

### I/O

| Function | Description |
|----------|-------------|
| `print(value)` | Print to stdout with newline |
| `input()` | Read one line from stdin |
| `read_file(path)` | Read file contents as string |
| `write_file(path, s)` | Write string to file |
| `append_file(path, s)` | Append string to file |

### Array

| Function | Description |
|----------|-------------|
| `len(array)` | Array length |
| `push(array, v)` | Append to array |
| `pop(array)` | Remove last element |

### String

| Function | Description |
|----------|-------------|
| `str_len(s)` | String length |
| `str_upper(s)` | Uppercase |
| `str_lower(s)` | Lowercase |
| `str_trim(s)` | Trim whitespace |
| `str_contains(s, sub)` | Check if contains substring |
| `str_find(s, sub)` | Find index of substring |
| `str_slice(s, a, b)` | Substring from a to b |
| `str_replace(s, a, b)` | Replace occurrences |
| `str_starts_with(s, p)` | Check prefix |
| `str_ends_with(s, p)` | Check suffix |
| `str_split(s, delim)` | Split into array |
| `char_at(s, i)` | Character at index |

### Conversion

| Function | Description |
|----------|-------------|
| `to_str(value)` | Convert to string |
| `to_int(value)` | Convert to int |
| `to_float(value)` | Convert to float |

### Math

| Function | Description |
|----------|-------------|
| `abs(x)` / `fabs(x)` | Absolute value |
| `sqrt(x)` | Square root |
| `pow(x, y)` | Power |
| `min(a, b)` / `max(a, b)` | Min/max (int) |
| `fmin(a, b)` / `fmax(a, b)` | Min/max (float) |

### Result

| Function | Description |
|----------|-------------|
| `is_ok(result)` | Check if Ok |
| `is_err(result)` | Check if Err |
| `unwrap(result)` | Extract Ok value (aborts on Err) |
| `unwrap_err(result)` | Extract Err value (aborts on Ok) |

### Misc

| Function | Description |
|----------|-------------|
| `exit(code)` | Exit program |
| `assert(cond, msg)` | Abort if false |

---

## CLI Usage

```
URUS Compiler V0.2/2(F)

usage: urusc <file.urus> [options]

Options:
  --help      Show help message
  --version   Show compiler version
  --tokens    Display lexer tokens
  --ast       Display the AST
  --emit-c    Print generated C code to stdout
  -o <file>   Output executable name (default: a.exe)

Example:
  urusc main.urus -o app
```

---

## Architecture

```mermaid
graph TD
    %% Nodes definition
    A([📄 Source .urus])
    B[Lexer]
    C[Parser]
    D[Sema]
    E[Codegen]
    F[GCC]
    G([🚀 Executable])

    %% Flow
    A --> B --> C --> D --> E --> F --> G

    %% Styling for Dark Mode Contrast
    style A fill:#238636,stroke:#2ea043,color:#fff
    style B fill:#161b22,stroke:#58a6ff,color:#58a6ff
    style C fill:#161b22,stroke:#58a6ff,color:#58a6ff
    style D fill:#161b22,stroke:#58a6ff,color:#58a6ff
    style E fill:#161b22,stroke:#58a6ff,color:#58a6ff
    style F fill:#161b22,stroke:#d2a8ff,color:#d2a8ff
    style G fill:#238636,stroke:#2ea043,color:#fff

    %% Sub-text inside nodes (Ramping)
    subgraph Legend [Process Flow]
    direction LR
    B --- B1[Tokenize]
    C --- C1[AST]
    D --- D1[Type Check]
    E --- E1[C11 Code]
    end
    
    style Legend fill:none,stroke:none,color:#8b949e
```

---

## Project Stats

| Metric | Value |
|--------|-------|
| Version | V0.2/2(F) "Fixed" |
| Compiler Size | ~464 KB (standalone, runtime embedded) |
| Runtime | ~16 KB header-only (embedded in binary) |
| Compiler LOC | ~4,700+ |
| Runtime LOC | ~467 |
| Output | C11 compliant |
| Platforms | Windows, Linux, macOS |
| Build System | CMake 3.10+ |
| Dependencies | GCC 8+ only |

---

## Running Tests

```bash
cd tests/

# Linux / macOS
bash run_tests.sh ../compiler/build/urusc

# Windows
run_tests.bat ..\compiler\build\Release\urusc.exe
```

---

## Repository Structure

```yaml
Urus/
├── compiler/                  # Compiler source code
│   ├── src/                   # Implementation (.c)
│   │   ├── main.c             # CLI entry point
│   │   ├── lexer.c            # Tokenizer
│   │   ├── parser.c           # Recursive descent parser
│   │   ├── sema.c             # Semantic analysis
│   │   ├── codegen.c          # C11 code generator
│   │   ├── ast.c              # AST node constructors
│   │   ├── error.c            # Error reporting
│   │   └── util.c             # File/string utilities
│   ├── include/               # Headers (.h)
│   │   ├── ast.h              # AST definitions
│   │   ├── token.h            # Token types
│   │   ├── lexer.h            # Lexer interface
│   │   ├── parser.h           # Parser interface
│   │   ├── sema.h             # Sema interface
│   │   ├── codegen.h          # Codegen interface
│   │   ├── error.h            # Error interface
│   │   ├── util.h             # Utility functions
│   │   └── urus_runtime.h     # Embedded runtime library
│   ├── cmake/                 # Build automation scripts
│   └── CMakeLists.txt         # Main build configuration
├── examples/                  # Sample programs (.urus)
├── tests/                     # Test suite & runners
├── documentation/             # Detailed project docs
├── SPEC.md                    # Language spec & grammar
├── CHANGELOG.md               # Version history
├── Diary/                     # Dev notes & complaints
├── Dockerfile                 # Containerized build
└── LICENSE                    # Apache 2.0
```

---

## Comparison

| Feature | URUS | C | Rust | Go | Python |
|---------|:----:|:-:|:----:|:--:|:------:|
| Static typing | Yes | Yes | Yes | Yes | No |
| Memory safety | RC + bounds | Manual | Ownership | GC | GC |
| Pattern matching | Yes | No | Yes | No | Limited |
| String interpolation | Yes | No | No | No | Yes |
| Result type | Yes | No | Yes | No | No |
| Null safety | Yes | No | Yes | No | No |
| Compiles to native | Yes | Yes | Yes | Yes | No |
| Learning curve | Low | Medium | High | Low | Low |
| Zero dependencies | Yes | Yes | No | No | No |

---

## Roadmap

### V0.3/1 — Quality of Life
- ~~Default parameter values~~
- ~~Better error messages with source context~~ *(Done in V0.2/2)*
- ~~Warning system (unused variables, etc.)~~
- Multi-line string literals

### V0.4/1 — Type System
- Type aliases (`type ID = int;`)
- Optional type (`Option<T>`)
- Tuple types (`(int, str)`)
- Type inference (`let x = 42;`)

### V0.5/1 — Methods & Traits
- Methods (`impl Point { fn distance() }`)
- Traits / Interfaces
- Generics (`fn max<T>(a: T, b: T): T`)
- Closures

### V1.0/1 — Stable Release
- Standard library
- Package manager
- Full documentation
- Production-ready

### V2.0/1 — Advanced
- Async/await
- Concurrency
- WASM target
- Self-hosting compiler
- LSP server for IDE support

Full roadmap: [documentation/roadmap](./documentation/roadmap/)

---

## Inspiration

URUS draws inspiration from:

- **Rust** — enums, pattern matching, Result type, immutability by default
- **Go** — simplicity, fast compilation, clean syntax
- **Zig** — transpile-to-C philosophy, minimal runtime
- **Python** — f-string interpolation, readability

---

## Contributing

```
1. Fork the repo
2. Create a feature branch
3. Follow coding style (see documentation/development-guide)
4. Add tests
5. Update documentation
6. Submit PR
```

See [CONTRIBUTING.md](./CONTRIBUTING.md) for details.

---

## License

Apache License 2.0 — see [LICENSE](./LICENSE)

---

<p align="center">
  <img src="assets/urus-quote.jpg" alt="Urus Foundation" width="400" />
  <br>
  <em>"Urus nggak harus besar, yang penting konsisten."</em>
  <br>
  <em>"Urus doesn't have to be big, what matters is consistency."</em>
</p>

---

## Contributors

Thanks to everyone who has contributed to URUS!

### Urus Foundation Developer

<table>
  <tr>
    <td align="center"><a href="https://github.com/RasyaAndrean"><img src="https://github.com/RasyaAndrean.png" width="80" /><br /><sub><b>Rasya Andrean</b></sub></a><br /><sub>Founder & Lead</sub></td>
    <td align="center"><a href="https://github.com/John-fried"><img src="https://github.com/John-fried.png" width="80" /><br /><sub><b>John-fried</b></sub></a><br /><sub>Co-Lead</sub></td>
    <td align="center"><a href="https://github.com/Mulyawan-ts"><img src="https://github.com/Mulyawan-ts.png" width="80" /><br /><sub><b>Mulyawan-ts</b></sub></a><br /><sub>Developer</sub></td>
  </tr>
</table>

### Urus Foundation Contributors

<table>
  <tr>
    <td align="center"><a href="https://github.com/kkkfasya"><img src="https://github.com/kkkfasya.png" width="80" /><br /><sub><b>kkkfasya</b></sub></a></td>
    <td align="center"><a href="https://github.com/fmway"><img src="https://github.com/fmway.png" width="80" /><br /><sub><b>fmway</b></sub></a></td>
    <td align="center"><a href="https://github.com/fepfitra"><img src="https://github.com/fepfitra.png" width="80" /><br /><sub><b>fepfitra</b></sub></a></td>
    <td align="center"><a href="https://github.com/lordpaijo"><img src="https://github.com/lordpaijo.png" width="80" /><br /><sub><b>lordpaijo</b></sub></a></td>
    <td align="center"><a href="https://github.com/XBotzLauncher"><img src="https://github.com/XBotzLauncher.png" width="80" /><br /><sub><b>XBotzLauncher</b></sub></a></td>
  </tr>
</table>

---

<p align="center">
  <strong>Built with care. Designed for clarity.</strong>
  <br><br>
  <a href="./documentation/">Documentation</a> &nbsp;&bull;&nbsp;
  <a href="./examples/">Examples</a> &nbsp;&bull;&nbsp;
  <a href="./SPEC.md">Specification</a> &nbsp;&bull;&nbsp;
  <a href="./CHANGELOG.md">Changelog</a>
</p>
