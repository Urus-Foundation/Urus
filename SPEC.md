# URUS Language Specification v0.3.0

## Overview

URUS is a statically-typed, compiled programming language that transpiles to standard C11.
Memory is managed automatically via reference counting.

## Syntax Style

C-like: curly braces for blocks, semicolons as statement terminators.

---

## 1. Primitive Types

| Type    | Description              | C equivalent     |
|---------|--------------------------|-------------------|
| `int`   | 64-bit signed integer    | `int64_t`         |
| `float` | 64-bit floating point    | `double`          |
| `bool`  | Boolean                  | `bool` (stdbool)  |
| `str`   | UTF-8 string (ref-counted) | `urus_str*`     |
| `void`  | No value (return type)   | `void`            |

## 2. Composite Types

### Arrays

```
let nums: [int] = [1, 2, 3];
let names: [str] = ["Alice", "Bob"];
```

Dynamic, growable, ref-counted. Supported element types: `int`, `float`, `bool`, `str`, structs, tuples, results.

### Tuples

```
let t: (int, str) = (42, "hello");
print(t.0);    // 42
print(t.1);    // hello
```

Stack-allocated compound types. Access elements by index: `.0`, `.1`, `.2`, etc. Tuples containing heap types (str, arrays) are properly cleaned up via generated drop functions.

### Result Type

```
Result<T, E>
```

Tagged union with `Ok(T)` and `Err(E)` variants. See [Section 14](#14-error-handling).

### Structs

See [Section 8](#8-structs).

### Enums

See [Section 9](#9-enums--tagged-unions).

## 3. Variables

### Immutable (default)

```
let x: int = 10;
let name: str = "hello";
```

### Mutable

```
let mut count: int = 0;
count = count + 1;
```

### Type Inference

Type annotation is optional when the type can be inferred from the initializer:

```
let x = 42;           // inferred as int
let pi = 3.14;        // inferred as float
let name = "hello";   // inferred as str
let flag = true;      // inferred as bool
```

### Tuple Destructuring

```
let (x, y): (int, str) = get_pair();
let (a, b) = (1, "two");
```

## 4. Constants

```
const MAX_SIZE: int = 100;
const PI: float = 3.14159;
const APP_NAME: str = "MyApp";
const DEBUG: bool = false;
```

Compile-time constants. Types supported: `int`, `float`, `bool`, `str`.

## 5. Type Aliases

```
type ID = int;
type Name = str;
type Numbers = [int];
type Pair = (int, str);
```

Creates a semantic alias for an existing type. The alias is interchangeable with the original type.

## 6. Functions

```
fn add(a: int, b: int): int {
    return a + b;
}

fn greet(name: str): void {
    print(name);
}
```

- Return type after `:` following parameter list.
- `void` can be omitted: `fn greet(name: str) { ... }` is valid.
- No function overloading.

### Entry Point

```
fn main(): void {
    // program starts here
}
```

### Default Parameters

```
fn greet(name: str = "World"): void {
    print(f"Hello {name}!");
}
```

### Mutable Parameters

```
fn increment(mut x: int): int {
    x += 1;
    return x;
}
```

## 7. Control Flow

### If / Else

```
if x > 10 {
    print("big");
} else if x > 5 {
    print("medium");
} else {
    print("small");
}
```

No parentheses required around condition (but allowed).

### If-Expressions

`if` can be used as an expression that returns a value:

```
let label = if x > 5 { "big" } else { "small" };
print(if x > 0 { "positive" } else { "negative" });
```

Compiles to C ternary operator.

### While Loop

```
while x < 100 {
    x = x + 1;
}
```

### Do-While Loop

```
do {
    x += 1;
} while x < 100;
```

### For Loop (Range)

```
for i in 0..10 {       // exclusive: 0 to 9
    print(i);
}

for i in 0..=10 {      // inclusive: 0 to 10
    print(i);
}
```

### For-each Loop

```
let names: [str] = ["Alice", "Bob"];
for name in names {
    print(name);
}
```

### Tuple Destructuring in For-each

```
let pairs: [(int, str)] = [(1, "a"), (2, "b")];
for (k, v) in pairs {
    print(f"{k}: {v}");
}
```

### Break / Continue

```
while true {
    if done {
        break;
    }
    continue;
}
```

Validated to only appear inside loops.

## 8. Structs

```
struct Point {
    x: float;
    y: float;
}

fn main(): void {
    let p: Point = Point { x: 1.0, y: 2.0 };
    print(p.x);
}
```

No methods on structs. Use free functions:

```
fn distance(a: Point, b: Point): float {
    let dx: float = a.x - b.x;
    let dy: float = a.y - b.y;
    return sqrt(dx * dx + dy * dy);
}
```

### Struct Spread Syntax

Create a new struct by copying fields from an existing instance and overriding specific fields:

```
let p1: Point = Point { x: 1.0, y: 2.0 };
let p2: Point = Point { x: 10.0, ..p1 };  // y copied from p1
```

## 9. Enums / Tagged Unions

```
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

- Variants can be unit (no data) or carry fields.
- Construct with `EnumName.Variant` or `EnumName.Variant(args)`.

## 10. Pattern Matching

### On Enums

```
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

```
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

## 11. Defer

```
fn process(): void {
    print("start");
    defer { print("cleanup"); }
    print("working");
    // "cleanup" runs automatically at end of function
}
```

- Defer bodies execute in LIFO (Last-In-First-Out) order.
- Runs before every return path.

## 12. Runes (Macros)

```
rune square(x) { x * x }
rune max(a, b) { if a > b { a } else { b } }
```

Invocation uses the `!` suffix:

```
fn main(): void {
    print(square!(5));       // 25
    print(max!(10, 20));     // 20
}
```

- Runes expand at compile time by textual substitution.
- No type checking during definition — only at expansion site.
- Statement-level runes (bodies with semicolons) expand as statement blocks.

## 13. String Interpolation

```
let name: str = "World";
let count: int = 42;
print(f"Hello {name}! Count is {count}.");
```

- `f"..."` strings can contain `{expr}` blocks.
- Expressions are converted via `to_str()` and concatenated.
- Use `{{` and `}}` for literal braces.

## 14. Error Handling (Result Type)

```
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

- `Result<T, E>` is a tagged union with `Ok(T)` and `Err(E)` variants.
- Built-in functions: `is_ok()`, `is_err()`, `unwrap()`, `unwrap_err()`.
- `unwrap()` on an Err or `unwrap_err()` on an Ok aborts with an error message.

## 15. Modules / Imports

```
import "math_utils.urus";
```

- Imports are resolved relative to the importing file's directory.
- Standard library imports search via `URUSCPATH` environment variable.
- Circular imports are detected and rejected.
- All top-level declarations from imported files are merged into the program.

## 16. Raw Emit

```
__emit__("printf(\"raw C!\\n\");");
```

Inline C code directly into the generated output. Bypasses all type and safety checks.

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

## 18. Numeric Literals

```
42              // integer
3.14            // float
1_000_000       // numeric separators
3.14_159        // float with separators
0xFF            // hexadecimal
0o755           // octal
0b1010          // binary
1.5e-10         // scientific notation
```

## 19. String Methods (Method-call Syntax)

Strings and arrays support method-call syntax as an alternative to free functions:

```
let s: str = "  Hello World  ";
s.trim()            // str_trim(s)
s.upper()           // str_upper(s)
s.lower()           // str_lower(s)
s.contains("World") // str_contains(s, "World")
s.find("World")     // str_find(s, "World")
s.len()             // str_len(s)
s.slice(2, 7)       // str_slice(s, 2, 7)
s.replace("o", "0") // str_replace(s, "o", "0")
s.starts_with("  H") // str_starts_with(s, "  H")
s.ends_with("  ")   // str_ends_with(s, "  ")
s.split(" ")        // str_split(s, " ")
s.char_at(0)        // char_at(s, 0)
```

Arrays:

```
let mut arr: [int] = [1, 2, 3];
arr.len()           // len(arr)
arr.push(4)         // push(arr, 4)
arr.pop()           // pop(arr)
```

## 20. Comments

```
// single line comment

/*
   multi-line
   comment
*/
```

## 21. Scope Rules

- Block scoping: variables live within their `{ }` block.
- No variable shadowing in the same scope.
- Inner scopes can shadow outer variables.

```
let x: int = 1;
{
    let x: int = 2;  // OK: shadows outer x
    print(x);        // prints 2
}
print(x);            // prints 1
```

## 22. Memory Model

All heap-allocated values (strings, arrays, structs) use reference counting.

- Assignment copies the reference, increments refcount.
- When a variable goes out of scope, refcount decrements.
- When refcount reaches 0, memory is freed.
- **No cycle detection.** Programmer must avoid circular references.
- Defer statements provide deterministic cleanup ordering.

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
| `str_upper(s)` | Uppercase string |
| `str_lower(s)` | Lowercase string |
| `str_trim(s)` | Trim whitespace |
| `str_contains(s, sub)` | Check if string contains substring |
| `str_find(s, sub)` | Find index of substring (-1 if not found) |
| `str_slice(s, a, b)` | Substring from index a to b |
| `str_replace(s, a, b)` | Replace occurrences of a with b |
| `str_starts_with(s, p)` | Check if string starts with prefix |
| `str_ends_with(s, p)` | Check if string ends with suffix |
| `str_split(s, delim)` | Split string into array of strings |
| `char_at(s, i)` | Get character at index (as string) |

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
| `pow(x, y)` | Power (x^y) |
| `min(a, b)` | Minimum of two ints |
| `max(a, b)` | Maximum of two ints |
| `fmin(a, b)` | Minimum of two floats |
| `fmax(a, b)` | Maximum of two floats |

### Result

| Function | Description |
|----------|-------------|
| `is_ok(result)` | Check if Result is Ok |
| `is_err(result)` | Check if Result is Err |
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
| `assert(cond, msg)` | Abort with message if condition false |

---

## Grammar (EBNF)

```ebnf
program        = { declaration } ;

declaration    = fn_decl | struct_decl | enum_decl | import_decl
               | const_decl | type_alias | rune_decl | emit_stmt
               | statement ;

fn_decl        = "fn" IDENT "(" param_list ")" [ ":" type ] block ;
param_list     = [ param { "," param } ] ;
param          = [ "mut" ] IDENT ":" type [ "=" expr ] ;

struct_decl    = "struct" IDENT "{" { field } "}" ;
field          = IDENT ":" type ";" ;

enum_decl      = "enum" IDENT "{" { variant } "}" ;
variant        = IDENT [ "(" param_list ")" ] ";" ;

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
postfix        = call { "++" | "--" } ;
call           = primary { "(" arg_list ")" | "." IDENT [ "(" arg_list ")" ]
                 | "[" expr "]" } ;
arg_list       = [ expr { "," expr } ] ;

primary        = INT_LIT | FLOAT_LIT | STR_LIT | FSTR_LIT
               | "true" | "false"
               | "Ok" "(" expr ")"
               | "Err" "(" expr ")"
               | IDENT "!" "(" arg_list ")"
               | IDENT
               | IDENT "{" field_init_list [ "," ".." expr ] "}"
               | IDENT "." IDENT [ "(" arg_list ")" ]
               | "[" [ expr { "," expr } ] "]"
               | "(" expr { "," expr } ")"
               | "(" expr ")" ;

FSTR_LIT       = "f" '"' { any_char | "{" expr "}" } '"' ;

field_init_list = field_init { "," field_init } ;
field_init      = IDENT ":" expr ;

type           = "int" | "float" | "bool" | "str" | "void"
               | "[" type "]"
               | "(" type { "," type } ")"
               | "Result" "<" type "," type ">"
               | IDENT ;

(* Numeric Literals *)
INT_LIT        = DIGIT { DIGIT | "_" }
               | "0x" HEX_DIGIT { HEX_DIGIT | "_" }
               | "0o" OCT_DIGIT { OCT_DIGIT | "_" }
               | "0b" BIN_DIGIT { BIN_DIGIT | "_" } ;
FLOAT_LIT      = DIGIT { DIGIT | "_" } "." { DIGIT | "_" } [ ("e"|"E") ["+"|"-"] DIGIT { DIGIT } ] ;

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

```urus
struct Circle {
    radius: float;
}

fn area(c: Circle): float {
    return 3.14159 * c.radius * c.radius;
}

fn main(): void {
    let c: Circle = Circle { radius: 5.0 };
    print(f"Area: {area(c)}");

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

- Generics / templates
- Closures / lambdas
- Interfaces / traits
- Methods on structs (`impl` blocks)
- Optional type (`Option<T>`)
- Cycle detection in refcount
- Pointer/unsafe operations
- Async/await
- Package manager
