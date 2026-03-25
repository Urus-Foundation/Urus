<p align="center">
  <img src="https://raw.githubusercontent.com/Urus-Foundation/initial-resource/main/assets/logo.jpg" alt="URUS Logo" width="150" />
  <h1 align="center">URUS Programming Language</h1>
  <p align="center">
    <strong>A statically-typed, compiled programming language that transpiles to portable C11.</strong>
  </p>
  <p align="center">Safe. Simple. Fast.</p>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/version-0.3.0-blue" alt="Version" />
  <img src="https://img.shields.io/badge/license-Apache%202.0-green" alt="License" />
  <img src="https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS%20%7C%20Termux-lightgrey" alt="Platform" />
  <img src="https://img.shields.io/badge/C-C11-orange" alt="C Standard" />
</p>

<p align="center">
  <a href="#quick-start">Quick Start</a> &nbsp;&bull;&nbsp;
  <a href="#features">Features</a> &nbsp;&bull;&nbsp;
  <a href="./documentation/">Documentation</a> &nbsp;&bull;&nbsp;
  <a href="./examples/">Examples</a> &nbsp;&bull;&nbsp;
  <a href="#roadmap">Roadmap</a>
</p>

---

## What is URUS?

URUS is a statically-typed programming language that compiles to standard C11. It aims to provide the safety guarantees of modern languages while producing code that runs anywhere a C compiler runs.

The compiler is a single-pass transpiler written in C. It reads `.urus` source files, performs lexing, parsing, semantic analysis, and emits portable C11 code. The generated C is then compiled to a native binary using GCC, Clang, or any C11-compliant compiler.

---

## Why URUS?

| Goal | How |
|------|-----|
| **Safer than C** | RAII memory management, bounds checking, immutable by default |
| **Simpler than Rust** | No borrow checker, no lifetimes — straightforward ownership model |
| **Faster than Python** | Compiles to native binary via C11 |
| **Portable** | Transpiles to standard C11 — runs anywhere GCC/Clang runs |
| **Modern syntax** | Enums, pattern matching, string interpolation, Result types, tuples, macros |

---

## Quick Start

### Requirements

- **CMake** 3.10+
- **C compiler**: GCC 8+, Clang, or MSVC (for building the compiler)
- **GCC** 8+ or compatible C11 compiler (for compiling generated code)

### Build from Source

```bash
git clone https://github.com/Urus-Foundation/Urus.git
cd Urus/compiler
cmake -S . -B build
cmake --build build
```

On **Termux**:
```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=$PREFIX
cmake --build build
```

### Install to System

```bash
# Linux / macOS / Termux
sudo cmake --install build

# Windows (Run as Administrator)
cmake --install build
```

### Hello World

```urus
fn main(): void {
    print("Hello, World!");
}
```

```bash
urusc hello.urus -o hello
./hello
# Hello, World!
```

> Having trouble? Open an issue on the [issue tracker](https://github.com/Urus-Foundation/Urus/issues/new?template=complaint.md).

---

## Features

### Type System

| Type | Description | C Equivalent |
|------|-------------|--------------|
| `int` | 64-bit signed integer | `int64_t` |
| `float` | 64-bit floating point | `double` |
| `bool` | Boolean (`true` / `false`) | `bool` |
| `str` | UTF-8 string (heap-allocated) | `urus_str*` |
| `void` | No value | `void` |
| `[T]` | Dynamic array of T | `urus_array*` |
| `(T1, T2)` | Tuple (stack-allocated) | `struct { T1 f0; T2 f1; }` |
| `Result<T, E>` | Ok or Err value | `urus_result*` |

### Variables

```urus
let x: int = 10;           // immutable
let mut count: int = 0;    // mutable
count += 1;

// Type inference
let name = "hello";        // inferred as str
let pi = 3.14;             // inferred as float
```

### Constants

```urus
const MAX_SIZE: int = 100;
const PI: float = 3.14159;
const APP_NAME: str = "MyApp";
```

### Type Aliases

```urus
type ID = int;
type Name = str;
type Numbers = [int];

fn greet(id: ID, name: Name): void {
    print(f"User {id}: {name}");
}
```

### Functions

```urus
fn add(a: int, b: int): int {
    return a + b;
}

// Default parameter values
fn greet(name: str = "World"): void {
    print(f"Hello {name}!");
}

// Mutable parameters
fn increment(mut x: int): int {
    x += 1;
    return x;
}
```

### Tuples

```urus
let t: (int, str) = (42, "hello");
print(t.0);    // 42
print(t.1);    // hello

// Destructuring
let (x, y) = get_pair();

// In for-each loops
let pairs: [(int, str)] = [(1, "a"), (2, "b")];
for (k, v) in pairs {
    print(f"{k}: {v}");
}
```

### Runes (Macros)

```urus
rune square(x) { x * x }
rune max(a, b) { if a > b { a } else { b } }

fn main(): void {
    print(square!(5));       // 25
    print(max!(10, 20));     // 20
}
```

### If-Expressions

```urus
let label = if x > 5 { "big" } else { "small" };
print(if x > 0 { "positive" } else { "negative" });
```

### Control Flow

```urus
// If / Else
if x > 10 {
    print("big");
} else if x > 5 {
    print("medium");
} else {
    print("small");
}

// While
while x < 100 {
    x += 1;
}

// Do-While
do {
    x += 1;
} while x < 100;

// For (range, exclusive)
for i in 0..10 {
    print(i);
}

// For (range, inclusive)
for i in 0..=10 {
    print(i);
}

// For-each
let names: [str] = ["Alice", "Bob"];
for name in names {
    print(name);
}
```

### Structs

```urus
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

### Enums and Pattern Matching

```urus
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

Match also works with primitive types:

```urus
fn describe(n: int): void {
    match n {
        0 => { print("zero"); }
        1 => { print("one"); }
        _ => { print("other"); }
    }
}

fn greet(lang: str): void {
    match lang {
        "en" => { print("Hello!"); }
        "id" => { print("Halo!"); }
        _ => { print("..."); }
    }
}
```

### Defer

```urus
fn process(): void {
    print("start");
    defer { print("cleanup"); }
    print("working");
    // "cleanup" runs automatically at end of function
}
```

Defer bodies execute in LIFO order and run before every return path.

### String Interpolation

```urus
let name: str = "World";
let count: int = 42;
print(f"Hello {name}! Answer: {count}");
```

### Arrays

```urus
let nums: [int] = [1, 2, 3, 4, 5];
let first: int = nums[0];

let mut items: [int] = [];
push(items, 42);
print(f"Length: {len(items)}");

// Method-call syntax
items.push(10);
print(items.len());
```

### String Methods

```urus
let s: str = "  Hello World  ";
print(s.trim());            // "Hello World"
print(s.upper());           // "  HELLO WORLD  "
print(s.lower());           // "  hello world  "
print(s.contains("World")); // true
print(s.len());             // 15
```

### Modules

```urus
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

### Error Handling

```urus
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

### Operators

| Category | Operators |
|----------|-----------|
| Arithmetic | `+`, `-`, `*`, `/`, `%` |
| Exponent | `**` |
| Comparison | `==`, `!=`, `<`, `>`, `<=`, `>=` |
| Logical | `&&`, `\|\|`, `!` |
| Bitwise | `&`, `\|`, `^`, `~`, `<<`, `>>`, `&~` |
| Assignment | `=`, `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `\|=`, `^=`, `<<=`, `>>=` |
| Increment/Decrement | `++`, `--` |
| String concat | `+` |
| Floored remainder | `%%` |

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
| `str_upper(s)` / `s.upper()` | Uppercase |
| `str_lower(s)` / `s.lower()` | Lowercase |
| `str_trim(s)` / `s.trim()` | Trim whitespace |
| `str_contains(s, sub)` / `s.contains(sub)` | Check substring |
| `str_find(s, sub)` / `s.find(sub)` | Find index of substring |
| `str_slice(s, a, b)` / `s.slice(a, b)` | Substring |
| `str_replace(s, old, new)` / `s.replace(old, new)` | Replace occurrences |
| `str_starts_with(s, p)` / `s.starts_with(p)` | Check prefix |
| `str_ends_with(s, p)` / `s.ends_with(p)` | Check suffix |
| `str_split(s, d)` / `s.split(d)` | Split into array |
| `char_at(s, i)` / `s.char_at(i)` | Character at index |

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
| `is_ok(r)` | Check if Ok |
| `is_err(r)` | Check if Err |
| `unwrap(r)` | Extract Ok value (aborts on Err) |
| `unwrap_err(r)` | Extract Err message (aborts on Ok) |

### HTTP

| Function | Description |
|----------|-------------|
| `http_get(url)` | HTTP GET request, returns response body |
| `http_post(url, body)` | HTTP POST request, returns response body |

> Requires `curl` to be available on the system.

### Misc

| Function | Description |
|----------|-------------|
| `exit(code)` | Exit program |
| `assert(cond, msg)` | Abort if condition is false |

---

## CLI Usage

```
URUS Compiler, version 0.3.0

Usage: urusc <file.urus> [options]

Options:
  --help      Show help message
  --version   Show compiler version
  --tokens    Display lexer tokens
  --ast       Display the AST
  --emit-c    Print generated C code to stdout
  -o <file>   Output executable name (default: a.exe / a.out)

Example:
  urusc main.urus -o app
```

---

## Architecture

```
Source (.urus)
     |
     v
  [ Lexer ]       Tokenize source code
     |
     v
  [ Preprocessor ] Expand imports and rune macros
     |
     v
  [ Parser ]      Build Abstract Syntax Tree
     |
     v
  [ Sema ]        Type checking and semantic analysis
     |
     v
  [ Codegen ]     Generate standard C11 code
     |
     v
  [ GCC/Clang ]   Compile to native binary
     |
     v
  Executable
```

---

## Project Structure

```
Urus/
├── compiler/
│   ├── src/                   # Compiler implementation
│   │   ├── main.c             # CLI entry point
│   │   ├── lexer.c            # Tokenizer
│   │   ├── parser.c           # Recursive descent parser
│   │   ├── codegen.c          # C11 code generator
│   │   ├── ast.c              # AST constructors and utilities
│   │   ├── preprocess.c       # Import resolution and rune expansion
│   │   ├── error.c            # Error/warning reporting
│   │   ├── util.c             # File and string utilities
│   │   └── Sema/              # Semantic analysis
│   │       ├── sema.c         # Type checking and validation
│   │       ├── builtins.c     # Built-in function signatures
│   │       └── scope.c        # Scope and symbol table
│   ├── include/               # Public headers
│   ├── runtime/               # Embedded runtime library
│   │   └── urus_runtime.h     # Header-only runtime (strings, arrays, etc.)
│   ├── stdlib/                # Standard library modules (.urus)
│   └── CMakeLists.txt         # Build configuration
├── examples/                  # Sample programs
├── tests/                     # Test suite
│   └── run/                   # Integration tests (.urus + .expected)
├── documentation/             # Extended documentation
├── SPEC.md                    # Language specification
├── CONTRIBUTING.md            # Contribution guide
├── SECURITY.md                # Security policy
├── CODE_OF_CONDUCT.md         # Community guidelines
├── CHANGELOG.md               # Version history
├── Dockerfile                 # Containerized build
└── LICENSE                    # Apache 2.0
```

---

## Project Stats

| Metric | Value |
|--------|-------|
| Version | 0.3.0 |
| Compiler LOC | ~7,100+ |
| Runtime LOC | ~540 |
| Test files | 33 |
| Output | C11 compliant |
| Platforms | Windows, Linux, macOS, Termux |
| Build system | CMake 3.10+ |
| Dependencies | C11 compiler only |

---

## Running Tests

```bash
cd tests/

# Linux / macOS
bash run_tests.sh ../compiler/build/urusc

# Windows
run_tests.bat ..\compiler\build\Debug\urusc.exe
```

Each test consists of a `.urus` source file and a `.expected` file. The test runner compiles and runs the program, then compares the output against the expected file.

---

## Comparison

| Feature | URUS | C | Rust | Go | Python |
|---------|:----:|:-:|:----:|:--:|:------:|
| Static typing | Yes | Yes | Yes | Yes | No |
| Memory safety | RAII + bounds | Manual | Ownership | GC | GC |
| Pattern matching | Yes | No | Yes | No | Limited |
| String interpolation | Yes | No | No | No | Yes |
| Result type | Yes | No | Yes | No | No |
| Null safety | Yes | No | Yes | No | No |
| Compiles to native | Yes | Yes | Yes | Yes | No |
| Learning curve | Low | Medium | High | Low | Low |

---

## Roadmap

### 0.3.x (current)

- [x] Tuple types and destructuring
- [x] Runes (macro system)
- [x] If-expressions
- [x] Type inference
- [x] HTTP built-ins
- [x] Constants (`const`)
- [x] Type aliases (`type`)
- [x] String and array method-call syntax
- [x] Do-while loops
- [x] Defer statements
- [x] Extended pattern matching (int, str, bool)
- [x] Bitwise and exponent operators
- [x] Mutable function parameters
- [x] Raw emit blocks

### 0.4.0 — Type System

- Optional type (`Option<T>`)
- Generics (`fn max<T>(a: T, b: T): T`)
- Portable RAII (explicit drop insertion)

### 0.5.0 — Methods and Traits

- Methods (`impl Point { fn distance() }`)
- Traits / Interfaces
- Closures
- Bundled TCC as default C backend

### 1.0.0 — Stable Release

- Standard library
- Package manager
- Full documentation
- Production-ready

### 2.0.0 — Advanced

- Async/await
- Concurrency primitives
- WebAssembly target
- Self-hosting compiler
- LSP server for IDE support

---

## Inspiration

URUS draws inspiration from:

- **Rust** — enums, pattern matching, Result type, immutability by default
- **Go** — simplicity, fast compilation, clean syntax
- **Zig** — transpile-to-C philosophy, minimal runtime
- **Python** — f-string interpolation, readability

---

## Contributing

We welcome contributions of all kinds. See [CONTRIBUTING.md](./CONTRIBUTING.md) for the full guide.

```
1. Fork the repository
2. Create a feature branch
3. Make your changes and add tests
4. Submit a pull request
```

---

## License

Licensed under the Apache License, Version 2.0. See [LICENSE](./LICENSE) for the full text.

---

## Contributors

### Urus Foundation

<table>
  <tr>
    <td align="center"><a href="https://github.com/RasyaAndrean"><img src="https://github.com/RasyaAndrean.png" width="80" /><br /><sub><b>Rasya Andrean</b></sub></a><br /><sub>Founder & Lead</sub></td>
    <td align="center"><a href="https://github.com/John-fried"><img src="https://github.com/John-fried.png" width="80" /><br /><sub><b>John-fried</b></sub></a><br /><sub>Co-Lead</sub></td>
    <td align="center"><a href="https://github.com/Mulyawan-ts"><img src="https://github.com/Mulyawan-ts.png" width="80" /><br /><sub><b>Mulyawan-ts</b></sub></a><br /><sub>Developer</sub></td>
  </tr>
</table>

### Contributors

<table>
  <tr>
    <td align="center"><a href="https://github.com/kkkfasya"><img src="https://github.com/kkkfasya.png" width="80" /><br /><sub><b>kkkfasya</b></sub></a></td>
    <td align="center"><a href="https://github.com/fmway"><img src="https://github.com/fmway.png" width="80" /><br /><sub><b>fmway</b></sub></a></td>
    <td align="center"><a href="https://github.com/fepfitra"><img src="https://github.com/fepfitra.png" width="80" /><br /><sub><b>fepfitra</b></sub></a></td>
    <td align="center"><a href="https://github.com/lordpaijo"><img src="https://github.com/lordpaijo.png" width="80" /><br /><sub><b>lordpaijo</b></sub></a></td>
    <td align="center"><a href="https://github.com/XBotzLauncher"><img src="https://github.com/XBotzLauncher.png" width="80" /><br /><sub><b>XBotzLauncher</b></sub></a></td>
    <td align="center"><a href="https://github.com/aimardcr"><img src="https://github.com/aimardcr.png" width="80" /><br /><sub><b>aimardcr</b></sub></a></td>
    <td align="center"><a href="https://github.com/billalxcode"><img src="https://github.com/billalxcode.png" width="80" /><br /><sub><b>billalxcode</b></sub></a></td>
  </tr>
</table>

---

<p align="center">
  <a href="./documentation/">Documentation</a> &nbsp;&bull;&nbsp;
  <a href="./examples/">Examples</a> &nbsp;&bull;&nbsp;
  <a href="./SPEC.md">Specification</a> &nbsp;&bull;&nbsp;
  <a href="./CHANGELOG.md">Changelog</a>
</p>
