# URUS Language Guide

This is the complete guide to writing programs in URUS. It covers every language feature with explanations and runnable examples.

For the formal grammar and type rules, see the [Language Specification](../../SPEC.md).

---

## Compiling and running programs

The URUS compiler is invoked as `urusc`. The most common usage:

```bash
urusc program.urus -o program
./program
```

Useful flags for development:

| Flag | Purpose |
|------|---------|
| `-o <name>` | Set the output binary name |
| `--emit-c` | Print the generated C code without compiling |
| `--tokens` | Show the lexer output (for debugging) |
| `--ast` | Show the parsed AST (for debugging) |
| `--version` | Print the compiler version |

The `--emit-c` flag is especially useful for understanding what the compiler does with your code, or for diagnosing issues with the generated output.

---

## Types

URUS is statically typed. Every variable, parameter, and return value has a known type at compile time.

### Primitive types

| Type | What it holds | Size | C equivalent |
|------|--------------|------|-------------|
| `int` | Whole numbers | 64-bit signed | `int64_t` |
| `float` | Decimal numbers | 64-bit | `double` |
| `bool` | `true` or `false` | — | `bool` |
| `str` | UTF-8 text | heap-allocated, ref-counted | `urus_str*` |
| `void` | Nothing (for return types) | — | `void` |

### Composite types

| Syntax | Description |
|--------|-------------|
| `[T]` | Dynamic array of `T` |
| `(T1, T2)` | Tuple, stack-allocated |
| `Result<T, E>` | Either `Ok(T)` or `Err(E)` |
| `StructName` | User-defined struct |
| `EnumName` | User-defined enum |

---

## Variables

Variables are declared with `let` and are **immutable by default**:

```urus
let x: int = 10;
let name: str = "Alice";
```

To make a variable mutable, add `mut`:

```urus
let mut count: int = 0;
count += 1;
count = 42;
```

### Type inference

The type annotation is optional when the compiler can infer it from the initializer:

```urus
let x = 42;          // int
let pi = 3.14;       // float
let name = "Alice";  // str
let flag = true;     // bool
```

You can always write the type explicitly for clarity, but it's not required.

### Tuple destructuring

When the right-hand side is a tuple, you can destructure it directly:

```urus
let (x, y) = (10, "hello");
// x is int, y is str
```

---

## Constants

Constants are declared with `const` and must have a type annotation:

```urus
const MAX_SIZE: int = 1024;
const PI: float = 3.14159;
const APP_NAME: str = "MyApp";
const DEBUG: bool = false;
```

Constants are evaluated at compile time and have no runtime cost. They're always immutable.

---

## Type aliases

You can give an existing type a new name with `type`:

```urus
type UserID = int;
type Name = str;
type Scores = [int];
type Pair = (int, str);
```

The alias is interchangeable with the original type — it's a convenience for readability, not a distinct type.

---

## Functions

Functions are declared with `fn` and require type annotations on parameters and the return type:

```urus
fn add(a: int, b: int): int {
    return a + b;
}

fn greet(name: str): void {
    print(f"Hello, {name}!");
}
```

If the return type is `void`, you can omit it:

```urus
fn greet(name: str) {
    print(f"Hello, {name}!");
}
```

### Default parameter values

Parameters can have defaults. When calling the function, you can omit arguments that have defaults:

```urus
fn greet(name: str = "World"): void {
    print(f"Hello, {name}!");
}

fn main(): void {
    greet();         // "Hello, World!"
    greet("URUS");   // "Hello, URUS!"
}
```

### Mutable parameters

By default, function parameters are immutable inside the function body. Use `mut` to allow modification:

```urus
fn double(mut x: int): int {
    x *= 2;
    return x;
}
```

This creates a mutable copy — it does not modify the caller's variable.

### Entry point

Every URUS program needs a `main` function:

```urus
fn main(): void {
    print("Program starts here.");
}
```

---

## Control flow

### If / else

```urus
if score >= 90 {
    print("A");
} else if score >= 80 {
    print("B");
} else {
    print("C");
}
```

Parentheses around the condition are optional.

### If-expressions

`if` can also be used as an expression that produces a value:

```urus
let label = if x > 100 { "high" } else { "low" };
print(if connected { "online" } else { "offline" });
```

Both branches must produce the same type. The compiler translates this to a C ternary operator.

### While loops

```urus
let mut i: int = 0;
while i < 10 {
    print(i);
    i += 1;
}
```

### Do-while loops

The body runs at least once before the condition is checked:

```urus
let mut attempts: int = 0;
do {
    attempts += 1;
} while attempts < 3;
```

### For loops (range)

Iterate over a range of integers. The `..` operator is exclusive (doesn't include the end), and `..=` is inclusive:

```urus
// Prints 0 through 9
for i in 0..10 {
    print(i);
}

// Prints 0 through 10
for i in 0..=10 {
    print(i);
}
```

### For-each loops

Iterate over array elements:

```urus
let names: [str] = ["Alice", "Bob", "Charlie"];
for name in names {
    print(f"Hello, {name}!");
}
```

With tuple destructuring:

```urus
let pairs: [(int, str)] = [(1, "one"), (2, "two"), (3, "three")];
for (num, word) in pairs {
    print(f"{num} = {word}");
}
```

### Break and continue

`break` exits a loop early. `continue` skips to the next iteration. Both are validated to only appear inside loops.

```urus
for i in 0..100 {
    if i == 50 {
        break;
    }
    if i % 2 == 0 {
        continue;
    }
    print(i);  // odd numbers from 1 to 49
}
```

---

## Structs

Structs are named collections of typed fields:

```urus
struct Point {
    x: float;
    y: float;
}

fn main(): void {
    let p: Point = Point { x: 3.0, y: 4.0 };
    print(f"({p.x}, {p.y})");
}
```

URUS doesn't have methods on structs yet. Use free functions instead:

```urus
fn distance(a: Point, b: Point): float {
    let dx = a.x - b.x;
    let dy = a.y - b.y;
    return sqrt(dx * dx + dy * dy);
}
```

### Struct spread syntax

You can create a new struct by copying most fields from an existing one and overriding specific fields:

```urus
let p1: Point = Point { x: 1.0, y: 2.0 };
let p2: Point = Point { x: 10.0, ..p1 };  // y is copied from p1
```

This is useful for configuration objects or any situation where you want to change one field while keeping the rest the same.

---

## Enums

Enums in URUS are tagged unions — each variant can optionally carry data:

```urus
enum Shape {
    Circle(radius: float);
    Rectangle(width: float, height: float);
    Point;
}
```

Construct enum values with the `EnumName.Variant(args)` syntax:

```urus
let s1: Shape = Shape.Circle(5.0);
let s2: Shape = Shape.Rectangle(10.0, 20.0);
let s3: Shape = Shape.Point;
```

---

## Pattern matching

### Matching on enums

The `match` statement lets you branch based on which variant an enum value holds, and bind the variant's data to local variables:

```urus
fn area(s: Shape): float {
    match s {
        Shape.Circle(r) => {
            return 3.14159 * r * r;
        }
        Shape.Rectangle(w, h) => {
            return w * h;
        }
        Shape.Point => {
            return 0.0;
        }
    }
    return 0.0;
}
```

### Matching on primitives

`match` also works on `int`, `str`, and `bool` values. Use `_` as a wildcard to catch anything not explicitly listed:

```urus
fn day_type(day: str): str {
    match day {
        "Saturday" => { return "weekend"; }
        "Sunday" => { return "weekend"; }
        _ => { return "weekday"; }
    }
    return "unknown";
}

fn describe(n: int): void {
    match n {
        0 => { print("zero"); }
        1 => { print("one"); }
        _ => { print("something else"); }
    }
}
```

---

## Tuples

Tuples are anonymous, fixed-size collections of values with potentially different types. They're stack-allocated and accessed by index:

```urus
let pair: (int, str) = (42, "hello");
print(pair.0);  // 42
print(pair.1);  // hello
```

Tuples work well as lightweight return types when you need to return multiple values:

```urus
fn min_max(arr: [int]): (int, int) {
    let mut lo = arr[0];
    let mut hi = arr[0];
    for v in arr {
        if v < lo { lo = v; }
        if v > hi { hi = v; }
    }
    return (lo, hi);
}

fn main(): void {
    let (lo, hi) = min_max([3, 1, 4, 1, 5, 9]);
    print(f"min={lo}, max={hi}");
}
```

---

## Runes (macros)

Runes are URUS's macro system. They define reusable code patterns that expand at compile time through textual substitution:

```urus
rune square(x) { x * x }
rune clamp(val, lo, hi) { if val < lo { lo } else if val > hi { hi } else { val } }
```

Invoke a rune with the `!` suffix:

```urus
fn main(): void {
    print(square!(7));           // 49
    print(clamp!(15, 0, 10));    // 10
}
```

Runes have no type checking at definition time — the types are checked when the rune is expanded at the call site. Think of them as smart copy-paste.

---

## Defer

`defer` schedules a block of code to run when the enclosing function exits, regardless of which `return` statement is taken:

```urus
fn process_file(path: str): void {
    print("opening file");
    defer { print("closing file"); }

    print("processing...");

    if path == "" {
        return;  // defer still runs
    }

    print("done");
    // defer runs here too
}
```

When you have multiple defers, they execute in reverse order (LIFO — last in, first out):

```urus
fn main(): void {
    defer { print("third"); }
    defer { print("second"); }
    defer { print("first"); }
}
// Output: first, second, third
```

This is useful for cleanup logic — closing files, releasing resources, printing diagnostics — where you want to guarantee the cleanup happens no matter how the function exits.

---

## String interpolation

Prefix a string with `f` to embed expressions inside `{}`:

```urus
let name = "URUS";
let version = 3;
print(f"Welcome to {name} v{version}!");
print(f"2 + 3 = {2 + 3}");
```

Expressions inside `{}` are automatically converted to strings. Use `{{` and `}}` if you need literal braces in the output.

---

## Arrays

Arrays are dynamically-sized, growable collections of a single type:

```urus
let nums: [int] = [1, 2, 3, 4, 5];
print(nums[0]);       // 1
print(len(nums));     // 5
```

Mutation requires `mut`:

```urus
let mut items: [int] = [];
push(items, 10);
push(items, 20);
items[0] = 99;
pop(items);
```

### Method-call syntax

Arrays support method-call syntax as an alternative to the free-function style:

```urus
let mut arr: [int] = [1, 2, 3];
arr.push(4);
print(arr.len());   // 4
arr.pop();
```

Both styles do exactly the same thing — use whichever reads better in context.

---

## String operations

URUS provides a comprehensive set of string functions, available both as free functions and as method calls:

```urus
let s = "  Hello, World!  ";

// These two are equivalent:
print(str_trim(s));
print(s.trim());
```

The full list:

| Method | Free function | Returns |
|--------|--------------|---------|
| `s.len()` | `str_len(s)` | `int` — string length |
| `s.upper()` | `str_upper(s)` | `str` — uppercase copy |
| `s.lower()` | `str_lower(s)` | `str` — lowercase copy |
| `s.trim()` | `str_trim(s)` | `str` — whitespace stripped |
| `s.contains(sub)` | `str_contains(s, sub)` | `bool` |
| `s.find(sub)` | `str_find(s, sub)` | `int` — index, or -1 |
| `s.slice(a, b)` | `str_slice(s, a, b)` | `str` — substring |
| `s.replace(old, new)` | `str_replace(s, old, new)` | `str` |
| `s.starts_with(p)` | `str_starts_with(s, p)` | `bool` |
| `s.ends_with(p)` | `str_ends_with(s, p)` | `bool` |
| `s.split(delim)` | `str_split(s, delim)` | `[str]` |
| `s.char_at(i)` | `char_at(s, i)` | `str` — single character |

---

## Error handling

URUS uses the `Result<T, E>` type for explicit error handling, inspired by Rust. A Result is either `Ok(value)` or `Err(message)`:

```urus
fn parse_port(s: str): Result<int, str> {
    let n = to_int(s);
    if n < 1 {
        return Err("port must be positive");
    }
    if n > 65535 {
        return Err("port out of range");
    }
    return Ok(n);
}

fn main(): void {
    let r = parse_port("8080");
    if is_ok(r) {
        print(f"Port: {unwrap(r)}");
    } else {
        print(f"Error: {unwrap_err(r)}");
    }
}
```

The built-in functions for working with Results:

| Function | Description |
|----------|-------------|
| `is_ok(r)` | Returns `true` if `r` is `Ok` |
| `is_err(r)` | Returns `true` if `r` is `Err` |
| `unwrap(r)` | Extracts the `Ok` value. Aborts if `r` is `Err`. |
| `unwrap_err(r)` | Extracts the `Err` message. Aborts if `r` is `Ok`. |

---

## Operators

### Arithmetic and comparison

| Operators | Description |
|-----------|-------------|
| `+` `-` `*` `/` `%` | Arithmetic (also `+` for string concatenation) |
| `**` | Exponentiation |
| `%%` | Floored remainder (always non-negative) |
| `==` `!=` `<` `>` `<=` `>=` | Comparison |
| `&&` `\|\|` `!` | Logical |

### Bitwise

| Operator | Description |
|----------|-------------|
| `&` | AND |
| `\|` | OR |
| `^` | XOR |
| `~` | NOT (bitwise complement) |
| `<<` | Left shift |
| `>>` | Right shift |
| `&~` | AND-NOT |

### Assignment

All arithmetic and bitwise operators have compound assignment forms: `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `\|=`, `^=`, `<<=`, `>>=`.

### Increment and decrement

`++` and `--` work as both prefix and postfix operators:

```urus
let mut x: int = 5;
x++;   // x is now 6
x--;   // x is now 5
```

---

## Numeric literals

URUS supports several numeric literal formats:

```urus
let decimal = 42;
let with_separators = 1_000_000;   // underscores for readability
let hex = 0xFF;
let octal = 0o755;
let binary = 0b1010;
let scientific = 1.5e-10;
let float_sep = 3.14_159;
```

Underscores can appear anywhere in a numeric literal and are purely visual — the compiler ignores them.

---

## Modules and imports

Split your code across multiple files with `import`:

```urus
// math.urus
fn square(x: int): int {
    return x * x;
}
```

```urus
// main.urus
import "math.urus";

fn main(): void {
    print(f"5^2 = {square(5)}");
}
```

Imports are resolved relative to the importing file. If the file isn't found there, the compiler searches the `URUSCPATH` directory (if set). Circular imports are detected and rejected.

---

## HTTP built-ins

URUS includes two functions for making HTTP requests:

```urus
let body = http_get("https://httpbin.org/get");
print(body);

let response = http_post("https://httpbin.org/post", "hello=world");
print(response);
```

Both return the response body as a string. They require `curl` to be installed on the system.

---

## File I/O

```urus
// Write to a file
write_file("output.txt", "Hello from URUS!\n");

// Read the whole file as a string
let content = read_file("output.txt");
print(content.trim());

// Append to a file
append_file("output.txt", "Another line.\n");
```

There's no sandboxing — URUS programs have the same filesystem access as the user running them.

---

## Raw emit

For cases where you need to drop down to C, the `__emit__` statement inserts raw C code into the generated output:

```urus
fn main(): void {
    __emit__("printf(\"Direct from C!\\n\");");
}
```

This bypasses all of URUS's type checking and safety guarantees. Use it sparingly and only when there's no other way to accomplish what you need.

---

## Conversion functions

| Function | Description |
|----------|-------------|
| `to_str(value)` | Convert any value to a string |
| `to_int(value)` | Convert a string or float to int |
| `to_float(value)` | Convert a string or int to float |

---

## Math functions

| Function | Description |
|----------|-------------|
| `abs(x)` | Absolute value (int) |
| `fabs(x)` | Absolute value (float) |
| `sqrt(x)` | Square root |
| `pow(x, y)` | Power |
| `min(a, b)` / `max(a, b)` | Min/max for ints |
| `fmin(a, b)` / `fmax(a, b)` | Min/max for floats |

---

## Other built-ins

| Function | Description |
|----------|-------------|
| `print(value)` | Print any value to stdout, followed by a newline |
| `input()` | Read one line from stdin as a string |
| `exit(code)` | Terminate the program with an exit code |
| `assert(condition, message)` | Abort with a message if the condition is false |

---

## Comments

```urus
// Single-line comment

/*
   Multi-line
   comment
*/
```

---

## Putting it all together

Here's a more complete example that uses several features together:

```urus
struct Task {
    name: str;
    priority: int;
    done: bool;
}

fn summarize(tasks: [Task]): (int, int) {
    let mut completed: int = 0;
    let mut pending: int = 0;
    for t in tasks {
        if t.done {
            completed++;
        } else {
            pending++;
        }
    }
    return (completed, pending);
}

fn main(): void {
    let tasks: [Task] = [
        Task { name: "Write docs", priority: 1, done: true },
        Task { name: "Fix bug #42", priority: 2, done: false },
        Task { name: "Add tests", priority: 1, done: true },
        Task { name: "Code review", priority: 3, done: false }
    ];

    let (done, todo) = summarize(tasks);
    print(f"Tasks: {done} completed, {todo} remaining");

    for t in tasks {
        let status = if t.done { "done" } else { "todo" };
        print(f"  [{status}] {t.name} (priority {t.priority})");
    }
}
```
