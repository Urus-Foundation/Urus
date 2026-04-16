# URUS Language Specification v0.3.0

URUS is a statically-typed, compiled programming language that transpiles to standard C11. Memory is managed automatically via reference counting.

**Syntax style:** C-like — curly braces for blocks, semicolons as statement terminators.

---

## 1. Primitive Types

| Type | Description | C Equivalent |
|------|-------------|--------------|
| `int` | 64-bit signed integer | `int64_t` |
| `float` | 64-bit floating point | `double` |
| `bool` | Boolean (`true` / `false`) | `bool` |
| `str` | UTF-8 string (ref-counted) | `urus_str*` |
| `void` | No value (return type only) | `void` |

---

## 2. Composite Types

### Arrays

```rust
let nums: [int] = [1, 2, 3];
let names: [str] = ["Alice", "Bob"];
```

Dynamic, growable, ref-counted. Supported element types: `int`, `float`, `bool`, `str`, structs, tuples, results.

### Tuples

```rust
let t: (int, str) = (42, "hello");
print(t.0);    // 42
print(t.1);    // hello
```

Stack-allocated compound types. Access elements by index: `.0`, `.1`, `.2`, etc. Tuples containing heap types (`str`, arrays) are properly cleaned up via generated drop functions.

### Result Type

```rust
Result<T, E>
```

Tagged union with `Ok(T)` and `Err(E)` variants. See [Section 14](#14-error-handling).

### Structs

See [Section 8](#8-structs).

### Enums

See [Section 9](#9-enums--tagged-unions).

---

## 3. Variables

### Immutable (default)

```rust
let x: int = 10;
let name: str = "hello";
```

### Mutable

```rust
let mut count: int = 0;
count = count + 1;
```

### Type Inference

Type annotation is optional when the type can be inferred from the initializer:

```rust
let x = 42;           // inferred as int
let pi = 3.14;        // inferred as float
let name = "hello";   // inferred as str
let flag = true;      // inferred as bool
```

### Tuple Destructuring

```rust
let (x, y): (int, str) = get_pair();
let (a, b) = (1, "two");
```

---

## 4. Constants

```rust
const MAX_SIZE: int = 100;
const PI: float = 3.14159;
const APP_NAME: str = "MyApp";
const DEBUG: bool = false;
```

Compile-time constants. Supported types: `int`, `float`, `bool`, `str`.

---

## 5. Type Aliases

```rust
type ID = int;
type Name = str;
type Numbers = [int];
type Pair = (int, str);
```

Creates a semantic alias for an existing type. The alias is fully interchangeable with the original.

---

## 6. Functions

```rust
fn add(a: int, b: int): int {
    return a + b;
}

fn greet(name: str): void {
    print(name);
}
```

- Return type follows `:` after the parameter list
- `void` can be omitted: `fn greet(name: str) { ... }` is valid
- No function overloading

### Entry Point

```rust
fn main(): void {
    // program starts here
}
```

### Default Parameters

```rust
fn greet(name: str = "World"): void {
    print(f"Hello {name}!");
}
```

### Mutable Parameters

```rust
fn increment(mut x: int): int {
    x += 1;
    return x;
}
```

---

## 7. Control Flow

### If / Else

```rust
if x > 10 {
    print("big");
} else if x > 5 {
    print("medium");
} else {
    print("small");
}
```

No parentheses required around the condition (but allowed).

### If-Expressions

`if` can be used as an expression that returns a value:

```rust
let label = if x > 5 { "big" } else { "small" };
print(if x > 0 { "positive" } else { "negative" });
```

Compiles to C ternary operator.

### While Loop

```rust
while x < 100 {
    x = x + 1;
}
```

### Do-While Loop

```rust
do {
    x += 1;
} while x < 100;
```

### For Loop (Range)

```rust
for i in 0..10 {       // exclusive: 0 to 9
    print(i);
}

for i in 0..=10 {      // inclusive: 0 to 10
    print(i);
}
```

### For-each Loop

```rust
let names: [str] = ["Alice", "Bob"];
for name in names {
    print(name);
}
```

### Tuple Destructuring in For-each

```rust
let pairs: [(int, str)] = [(1, "a"), (2, "b")];
for (k, v) in pairs {
    print(f"{k}: {v}");
}
```

### Break / Continue

```rust
while true {
    if done {
        break;
    }
    continue;
}
```

Validated to only appear inside loops.

---

## 8. Structs

```rust
struct Point {
    x: float;
    y: float;
}

fn main(): void {
    let p: Point = Point { x: 1.0, y: 2.0 };
    print(p.x);
}
```

### Struct Spread Syntax

Create a new struct by copying fields from an existing instance and overriding specific fields:

```rust
let p1: Point = Point { x: 1.0, y: 2.0 };
let p2: Point = Point { x: 10.0, ..p1 };  // y copied from p1
```

### Methods on Structs

See [Section 25](#25-impl-blocks--methods).

---

## 9. Enums / Tagged Unions

```rust
enum Color {
    Red;
    Green;
    Blue;
    Custom(r: int, g: int, b: int);
}

enum Shape {
    Circle(r: float);
    Rect(w: float, h: float);
    Point;
}
```

- Variants can be unit (no data) or carry fields
- Construct with `EnumName.Variant` or `EnumName.Variant(args)`

---

## 10. Pattern Matching

### On Enums

```rust
fn describe(s: Shape): str {
    match s {
        Shape.Circle(r) => {
            return f"Circle with radius {r}";
        }
        Shape.Rect(w, h) => {
            return f"Rectangle {w} x {h}";
        }
        Shape.Point => {
            return "A point";
        }
    }
    return "Unknown";
}
```

Arms bind variant fields as local variables.

### On Primitive Types

Match also works with `int`, `str`, and `bool`:

```rust
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

fn check(b: bool): void {
    match b {
        true => { print("yes"); }
        false => { print("no"); }
    }
}
```

The `_` wildcard matches any unmatched value.

---

## 11. Defer

```rust
fn process(): void {
    print("start");
    defer { print("cleanup"); }
    print("working");
    // "cleanup" runs automatically at end of function
}
```

- Defer bodies execute in LIFO (Last-In-First-Out) order
- Runs before every return path

---

## 12. Runes (Macros)

```rust
rune square(x) { x * x }
rune max(a, b) { if a > b { a } else { b } }
```

Invocation uses the `!` suffix:

```rust
fn main(): void {
    print(square!(5));       // 25
    print(max!(10, 20));     // 20
}
```

- Runes expand at compile time by textual substitution
- No type checking during definition — only at expansion site
- Statement-level runes (bodies with semicolons) expand as statement blocks

---

## 13. String Interpolation

```rust
let name: str = "World";
let count: int = 42;
print(f"Hello {name}! Count is {count}.");
```

- `f"..."` strings can contain `{expr}` blocks
- Expressions are converted via `to_str()` and concatenated
- Use `{{` and `}}` for literal braces

---

## 14. Error Handling

### Result Type

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

- `Result<T, E>` is a tagged union with `Ok(T)` and `Err(E)` variants
- Built-in functions: `is_ok()`, `is_err()`, `unwrap()`, `unwrap_err()`
- `unwrap()` on an `Err` or `unwrap_err()` on an `Ok` aborts with an error message

### Error Propagation (`?` operator)

The `?` operator propagates errors from `Result` values. If the value is `Err`, it returns early with the error; if `Ok`, it unwraps the value:

```rust
fn compute(x: int): Result<int, str> {
    let val: int = divide(x, 2)?;
    let val2: int = divide(val, 0)?;
    return Ok(val2);
}
```

### Try-Catch

```rust
try {
    let result: int = compute(10)?;
    print(result);
} catch (e: str) {
    print("Caught: " + e);
}
```

- Inside a `try` block, `?` throws to the nearest `catch` block
- Outside a `try` block, `?` returns `Err` from the enclosing function
- Implementation uses `setjmp`/`longjmp` with thread-local try stacks

---

## 15. Modules / Imports

```rust
import "math_utils.urus";
```

- Imports are resolved relative to the importing file's directory
- Standard library imports search via `URUSCPATH` environment variable
- Circular imports are detected and rejected
- All top-level declarations from imported files are merged into the program

---

## 16. Raw Emit

```rust
__emit__("printf(\"raw C!\\n\");");
```

Inline C code directly into the generated output. Bypasses all type and safety checks.

---

## 17. Operators

### Arithmetic

`+`, `-`, `*`, `/`, `%`

### Exponentiation

`**` — power operator

### Floored Remainder

`%%` — always-positive remainder

### Comparison

`==`, `!=`, `<`, `>`, `<=`, `>=`

### Logical

`&&`, `||`, `!`

### Bitwise

`&`, `|`, `^`, `~`, `<<`, `>>`, `&~` (AND-NOT)

### Assignment

`=`, `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=`

### Increment / Decrement

`++`, `--` (prefix and postfix)

### String Concatenation

`+` for strings: `"hello" + " world"`

---

## 18. Numeric Literals

```rust
42              // integer
3.14            // float
1_000_000       // numeric separators
3.14_159        // float with separators
0xFF            // hexadecimal
0o755           // octal
0b1010          // binary
1.5e-10         // scientific notation
```

---

## 19. Method-call Syntax

Strings and arrays support method-call syntax as an alternative to free functions.

### String Methods

```rust
let s: str = "  Hello World  ";
s.trim()              // str_trim(s)
s.upper()             // str_upper(s)
s.lower()             // str_lower(s)
s.contains("World")   // str_contains(s, "World")
s.find("World")       // str_find(s, "World")
s.len()               // str_len(s)
s.slice(2, 7)         // str_slice(s, 2, 7)
s.replace("o", "0")   // str_replace(s, "o", "0")
s.starts_with("  H")  // str_starts_with(s, "  H")
s.ends_with("  ")     // str_ends_with(s, "  ")
s.split(" ")          // str_split(s, " ")
s.char_at(0)          // char_at(s, 0)
```

### Array Methods

```rust
let mut arr: [int] = [1, 2, 3];
arr.len()             // len(arr)
arr.push(4)           // push(arr, 4)
arr.pop()             // pop(arr)
```

---

## 20. Comments

```rust
// single line comment

/*
   multi-line
   comment
*/
```

---

## 21. Scope Rules

- Block scoping: variables live within their `{ }` block
- No variable shadowing in the same scope
- Inner scopes can shadow outer variables

```rust
let x: int = 1;
{
    let x: int = 2;  // OK: shadows outer x
    print(x);        // prints 2
}
print(x);            // prints 1
```

---

## 22. Memory Model

All heap-allocated values (strings, arrays, structs) use reference counting.

| Rule | Detail |
|------|--------|
| **Assignment** | Copies the reference, increments refcount |
| **Scope exit** | Decrements refcount |
| **Refcount = 0** | Memory is freed |
| **Cycles** | No cycle detection — programmer must avoid circular references |
| **Cleanup** | Defer statements provide deterministic cleanup ordering |

---

## 23. Built-in Functions

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
| `str_contains(s, sub)` | Check substring |
| `str_find(s, sub)` | Find index of substring (-1 if not found) |
| `str_slice(s, a, b)` | Substring from index `a` to `b` |
| `str_replace(s, a, b)` | Replace occurrences of `a` with `b` |
| `str_starts_with(s, p)` | Check prefix |
| `str_ends_with(s, p)` | Check suffix |
| `str_split(s, delim)` | Split into array of strings |
| `char_at(s, i)` | Character at index (as string) |

### Conversion

| Function | Description |
|----------|-------------|
| `to_str(value)` | Convert to string |
| `to_int(value)` | Convert to int |
| `to_float(value)` | Convert to float |

### Math

| Function | Description |
|----------|-------------|
| `abs(x)` | Absolute value (int) |
| `fabs(x)` | Absolute value (float) |
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

### HTTP

| Function | Description |
|----------|-------------|
| `http_get(url)` | HTTP GET request, returns response body |
| `http_post(url, body)` | HTTP POST request, returns response body |

> Requires `curl` to be available on the system.

### Misc

| Function | Description |
|----------|-------------|
| `exit(code)` | Exit program with code |
| `assert(cond, msg)` | Abort with message if condition is false |

---

## 24. Generics

Functions can be parameterized with type parameters:

```rust
fn identity<T>(x: T): T {
    return x;
}

fn max_val<T>(a: T, b: T): T {
    if a > b { return a; }
    return b;
}
```

### Calling Generic Functions

Type arguments are specified explicitly at the call site:

```rust
let a: int = max_val<int>(10, 20);
let b: float = max_val<float>(3.14, 2.71);
let c: str = identity<str>("hello generics");
```

### Implementation

Generic functions are monomorphized at compile time — a specialized C function is generated for each unique type instantiation.

---

## 25. Impl Blocks / Methods

Methods can be defined on structs using `impl` blocks:

```rust
struct Point {
    x: float;
    y: float;
}

impl Point {
    fn length(self): float {
        return sqrt(self.x * self.x + self.y * self.y);
    }
}
```

### Method Calls

```rust
let p: Point = Point { x: 3.0, y: 4.0 };
let len: float = p.length();
print(f"Length: {len}");
```

- Methods use `self` as the first parameter (no explicit type annotation needed)
- Compiled as `TypeName_method_name(self, ...)` in the generated C code

---

## 26. Traits

Traits define shared behavior as a set of method signatures:

```rust
trait Display {
    fn to_string(self): str;
}
```

### Implementing Traits

```rust
impl Display for Point {
    fn to_string(self): str {
        return f"Point({self.x}, {self.y})";
    }
}
```

- Trait methods use `self` as the first parameter
- A type can implement multiple traits
- Trait implementations are checked for completeness — all methods must be defined

---

## 27. Closures / Lambdas

Anonymous functions using pipe `|` syntax:

```rust
let twice: fn(int): int = |n: int|: int {
    return n * 2;
};
```

### Function Types

Closure and function types are written as `fn(param_types): return_type`:

```rust
let add_one: fn(int): int = |x: int|: int {
    return x + 1;
};
```

### Higher-Order Functions

Functions can accept and return function values:

```rust
fn apply(f: fn(int): int, x: int): int {
    return f(x);
}

let result: int = apply(add_one, 10);  // 11
```

Named functions can also be passed as values:

```rust
fn double(x: int): int {
    return x * 2;
}

let r: int = apply(double, 5);  // 10
```

---

## 28. Async / Await

Functions can be declared `async` to run concurrently:

```rust
async fn compute(x: int): int {
    return x * x;
}

async fn greet(name: str): str {
    return "Hello, " + name + "!";
}
```

### Using Futures

Calling an async function returns a future. Use `await` to block and retrieve the result:

```rust
fn main(): void {
    let fut1 = compute(7);
    let fut2 = compute(3);

    let r1: int = await fut1;
    let r2: int = await fut2;

    print(f"{r1}, {r2}");  // 49, 9
}
```

- Async functions run on separate threads
- `await` blocks until the future completes and returns its value
- Platform-aware: uses Windows threads (`_beginthreadex`) or pthreads

---

## 29. Package Manager

URUS includes a built-in package manager using `urus.toml` manifests.

### Manifest (`urus.toml`)

```toml
[package]
name = "my_project"
version = "0.1.0"

[dependencies]
math = "*"
http = "https://github.com/user/http-lib.git"
json = "0.2.0"
```

### CLI Commands

| Command | Description |
|---------|-------------|
| `urusc pkg init` | Create new project with `urus.toml` and `src/main.urus` |
| `urusc pkg add <name> [version]` | Add dependency (version defaults to `"*"`) |
| `urusc pkg install` | Resolve and install dependencies |
| `urusc pkg list` | Show project info and dependencies |

### Dependency Resolution

- Dependencies matching stdlib module names (e.g., `math`, `json`) resolve to the built-in standard library
- Git URLs are cloned into the `urus_modules/` directory
- A lock file (`urus.lock`) is generated after install

---

## 30. Standard Library

The standard library provides importable modules in `compiler/stdlib/`. Import with:

```rust
import "math.urus";
import "json.urus";
```

### `math.urus`

**Constants:**

| Constant | Value |
|----------|-------|
| `MATH_PI` | 3.14159265358979323846 |
| `MATH_E` | 2.71828182845904523536 |
| `MATH_TAU` | 6.28318530717958647692 |

**Trigonometry:**

| Function | Description |
|----------|-------------|
| `math_sin(x)` | Sine |
| `math_cos(x)` | Cosine |
| `math_tan(x)` | Tangent |
| `math_asin(x)` | Arc sine |
| `math_acos(x)` | Arc cosine |
| `math_atan(x)` | Arc tangent |
| `math_atan2(y, x)` | Two-argument arc tangent |

**Logarithms:**

| Function | Description |
|----------|-------------|
| `math_log(x)` | Natural logarithm |
| `math_log2(x)` | Base-2 logarithm |
| `math_log10(x)` | Base-10 logarithm |

**Rounding:**

| Function | Description |
|----------|-------------|
| `math_ceil(x)` | Ceiling |
| `math_floor(x)` | Floor |
| `math_round(x)` | Round to nearest |

**Utility:**

| Function | Description |
|----------|-------------|
| `math_clamp(val, lo, hi)` | Clamp value to range |
| `math_sign(x)` | Sign (-1, 0, or 1) |
| `math_lerp(a, b, t)` | Linear interpolation |
| `math_random_int(min, max)` | Random integer in range |
| `math_random_float()` | Random float in [0, 1] |

**Number Theory:**

| Function | Description |
|----------|-------------|
| `math_factorial(n)` | Factorial (iterative) |
| `math_gcd(a, b)` | Greatest common divisor |
| `math_lcm(a, b)` | Least common multiple |
| `math_fibonacci(n)` | N-th Fibonacci number |
| `math_is_prime(n)` | Primality test |
| `math_comb(n, k)` | Combination C(n, k) |
| `math_perm(n, k)` | Permutation P(n, k) |

**Statistics:**

| Function | Description |
|----------|-------------|
| `math_sum(values)` | Sum of float array |
| `math_mean(values)` | Mean of float array |

**Numerical Calculus:**

| Function | Description |
|----------|-------------|
| `math_derivative(f_plus, f_minus, h)` | First derivative (central difference) |
| `math_derivative2(f_plus, f_center, f_minus, h)` | Second derivative |
| `math_integrate_trapezoid(values, h)` | Trapezoidal integration |
| `math_integrate_simpson(values, h)` | Simpson's 1/3 rule integration |

### `string.urus`

Extended string utilities beyond the built-in string functions.

### `fs.urus`

File system operations beyond the built-in `read_file`/`write_file`.

### `json.urus`

JSON parsing and serialization.

### `os.urus`

Operating system utilities (environment variables, process execution).

### `http.urus`

Extended HTTP client functionality beyond the built-in `http_get`/`http_post`.

---

## Grammar (EBNF)

```ebnf
program        = { declaration } ;

declaration    = fn_decl | struct_decl | enum_decl | import_decl
               | const_decl | type_alias | rune_decl | emit_stmt
               | trait_decl | impl_block
               | statement ;

fn_decl        = [ "async" ] "fn" IDENT [ generic_params ] "(" param_list ")"
                 [ ":" type ] block ;
generic_params = "<" IDENT { "," IDENT } ">" ;
param_list     = [ param { "," param } ] ;
param          = [ "mut" ] IDENT ":" type [ "=" expr ] ;

struct_decl    = "struct" IDENT "{" { field } "}" ;
field          = IDENT ":" type ";" ;

enum_decl      = "enum" IDENT "{" { variant } "}" ;
variant        = IDENT [ "(" param_list ")" ] ";" ;

trait_decl     = "trait" IDENT "{" { trait_method } "}" ;
trait_method   = "fn" IDENT "(" param_list ")" [ ":" type ] ";" ;

impl_block     = "impl" IDENT [ "for" IDENT ] "{" { fn_decl } "}" ;

import_decl    = "import" STR_LIT ";" ;

const_decl     = "const" IDENT ":" type "=" expr ";" ;

type_alias     = "type" IDENT "=" type ";" ;

rune_decl      = "rune" IDENT "(" ident_list ")" "{" rune_body "}" ;

emit_stmt      = "__emit__" "(" STR_LIT ")" ";" ;

block          = "{" { statement } "}" ;

statement      = let_stmt
               | assign_stmt
               | if_stmt
               | while_stmt
               | do_while_stmt
               | for_stmt
               | return_stmt
               | break_stmt
               | continue_stmt
               | match_stmt
               | defer_stmt
               | try_catch_stmt
               | expr_stmt ;

let_stmt       = "let" [ "mut" ] ( IDENT | "(" ident_list ")" )
                 [ ":" type ] "=" expr ";" ;
assign_stmt    = lvalue assign_op expr ";" ;
assign_op      = "=" | "+=" | "-=" | "*=" | "/=" | "%="
               | "&=" | "|=" | "^=" | "<<=" | ">>=" ;
lvalue         = IDENT { "." IDENT | "[" expr "]" } ;

if_stmt        = "if" expr block { "else" "if" expr block } [ "else" block ] ;
while_stmt     = "while" expr block ;
do_while_stmt  = "do" block "while" expr ";" ;
for_stmt       = "for" ( IDENT | "(" ident_list ")" ) "in" range_or_expr block ;
range_or_expr  = expr ".." ["="] expr | expr ;
return_stmt    = "return" [ expr ] ";" ;
break_stmt     = "break" ";" ;
continue_stmt  = "continue" ";" ;
defer_stmt     = "defer" block ;
try_catch_stmt = "try" block "catch" "(" IDENT ":" type ")" block ;

match_stmt     = "match" expr "{" { match_arm } "}" ;
match_arm      = pattern "=>" block ;
pattern        = IDENT "." IDENT [ "(" ident_list ")" ]
               | INT_LIT | STR_LIT | "true" | "false"
               | "_" ;
ident_list     = [ IDENT { "," IDENT } ] ;

expr_stmt      = expr ";" ;

expr           = if_expr | logic_or ;
if_expr        = "if" expr block "else" block ;
logic_or       = logic_and { "||" logic_and } ;
logic_and      = bitwise_or { "&&" bitwise_or } ;
bitwise_or     = bitwise_xor { "|" bitwise_xor } ;
bitwise_xor    = bitwise_and { "^" bitwise_and } ;
bitwise_and    = equality { "&" equality } ;
equality       = comparison { ( "==" | "!=" ) comparison } ;
comparison     = shift { ( "<" | ">" | "<=" | ">=" ) shift } ;
shift          = addition { ( "<<" | ">>" ) addition } ;
addition       = multiplication { ( "+" | "-" ) multiplication } ;
multiplication = exponent { ( "*" | "/" | "%" | "%%" ) exponent } ;
exponent       = unary { "**" unary } ;
unary          = ( "!" | "-" | "~" | "++" | "--" ) unary
               | postfix ;
postfix        = call { "++" | "--" | "?" } ;
call           = primary { "(" arg_list ")" | "." IDENT [ "(" arg_list ")" ]
                 | "[" expr "]" } ;
arg_list       = [ expr { "," expr } ] ;

primary        = INT_LIT | FLOAT_LIT | STR_LIT | FSTR_LIT
               | "true" | "false"
               | "Ok" "(" expr ")"
               | "Err" "(" expr ")"
               | "await" expr
               | IDENT "!" "(" arg_list ")"
               | IDENT
               | IDENT "{" field_init_list [ "," ".." expr ] "}"
               | IDENT "." IDENT [ "(" arg_list ")" ]
               | "[" [ expr { "," expr } ] "]"
               | "(" expr { "," expr } ")"
               | "(" expr ")"
               | lambda_expr ;

lambda_expr    = "|" [ lambda_param { "," lambda_param } ] "|"
                 [ ":" type ] block ;
lambda_param   = IDENT ":" type ;

FSTR_LIT       = "f" '"' { any_char | "{" expr "}" } '"' ;

field_init_list = field_init { "," field_init } ;
field_init      = IDENT ":" expr ;

type           = "int" | "float" | "bool" | "str" | "void"
               | "[" type "]"
               | "(" type { "," type } ")"
               | "Result" "<" type "," type ">"
               | "fn" "(" [ type { "," type } ] ")" [ ":" type ]
               | IDENT [ "<" type { "," type } ">" ] ;

(* Numeric Literals *)
INT_LIT        = DIGIT { DIGIT | "_" }
               | "0x" HEX_DIGIT { HEX_DIGIT | "_" }
               | "0o" OCT_DIGIT { OCT_DIGIT | "_" }
               | "0b" BIN_DIGIT { BIN_DIGIT | "_" } ;
FLOAT_LIT      = DIGIT { DIGIT | "_" } "." { DIGIT | "_" }
                 [ ("e"|"E") ["+"|"-"] DIGIT { DIGIT } ] ;

(* Tokens *)
IDENT          = ALPHA { ALPHA | DIGIT | "_" } ;
STR_LIT        = '"' { any_char - '"' | '\"' } '"' ;
ALPHA          = "a".."z" | "A".."Z" | "_" ;
DIGIT          = "0".."9" ;
HEX_DIGIT      = "0".."9" | "a".."f" | "A".."F" ;
OCT_DIGIT      = "0".."7" ;
BIN_DIGIT      = "0" | "1" ;
```

---

## Example Program

```rust
struct Circle {
    radius: float;
}

impl Circle {
    fn area(self): float {
        return 3.14159 * self.radius * self.radius;
    }
}

trait Display {
    fn to_string(self): str;
}

impl Display for Circle {
    fn to_string(self): str {
        return f"Circle(radius={self.radius})";
    }
}

fn main(): void {
    let c: Circle = Circle { radius: 5.0 };
    print(f"Area: {c.area()}");
    print(c.to_string());

    let mut sum: int = 0;
    for i in 1..=100 {
        sum += i;
    }
    print(f"Sum 1-100: {sum}");

    defer { print("goodbye"); }

    match sum {
        5050 => { print("correct!"); }
        _ => { print("wrong"); }
    }
}
```

---

## Not Yet Implemented

These are deferred to future versions:

- Optional type (`Option<T>`)
- Cycle detection in refcount
- Pointer/unsafe operations
- Generic structs / generic traits
- Trait bounds on generic parameters
