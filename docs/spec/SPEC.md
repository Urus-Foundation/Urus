# URUS Language Reference Specification — v0.0.1

> **Status:** preview. Anything in this document may change without notice
> until v1.0. Sections marked **[PLANNED]** are not yet implemented in
> `urusc` v0.0.1; they appear here so that early code can target the eventual
> shape of the language without breaking later.

---

## 1. Lexical structure

### 1.1 Encoding

URUS source files are UTF-8. The compiler **does not** read a BOM; if your
editor inserts one, strip it. Line endings may be `\n` or `\r\n` — both
count as the same logical line boundary.

### 1.2 Tokens

```
token  := keyword | ident | literal | punct | EOF
```

#### 1.2.1 Identifiers

```
ident      := (letter | "_") (letter | digit | "_")*
letter     := "a"…"z" | "A"…"Z"
digit      := "0"…"9"
```

Reserved words (cannot be identifiers):

`module use fn struct impl enum trait type let mut const pub
if else match return while for in loop break continue
true false self Self as defer`

Type names by convention start with an uppercase letter; values use
lowercase. The compiler does not enforce this in v0.0.1.

#### 1.2.2 Numeric literals

```
int_lit    := dec_lit | hex_lit | oct_lit | bin_lit
dec_lit    := digit (digit | "_")* suffix?
hex_lit    := "0x" (hex_digit | "_")+ suffix?
oct_lit    := "0o" (oct_digit | "_")+ suffix?
bin_lit    := "0b" ("0" | "1" | "_")+ suffix?
float_lit  := digit (digit | "_")* "." digit (digit | "_")* exp? suffix?
            | digit (digit | "_")* exp suffix?
exp        := ("e" | "E") ("+" | "-")? digit+
suffix     := "i8" | "i16" | "i32" | "i64"
            | "u8" | "u16" | "u32" | "u64"
            | "f32" | "f64" | "usize" | "isize"
```

Underscores in numeric literals are ignored. Type suffixes are
syntactically accepted in v0.0.1 but **not yet checked** by sema.

#### 1.2.3 String and char literals

```
str_lit    := '"' (char | esc)* '"'
fstr_lit   := "f" '"' (char | esc | placeholder)* '"'
placeholder:= "{" ident ("." ident)* "}"
char_lit   := "'" (char | esc) "'"
esc        := "\\" ("n" | "r" | "t" | "0" | "\\" | "'" | '"' | "{" | "}"
                  | "x" hex_digit hex_digit)
```

**f-string literals.** Prefix a string with `f` to enable `{name}` and
`{obj.field}` interpolation anywhere a string is wanted:

```urus
let greeting: str = f"Hello, {name}!"
println(f"x = {x}, y = {y}")
```

Regular `"..."` literals are *not* interpolated. The implicit interpolation
that `println` performed in earlier drafts is retired — use `f"..."`
explicitly.

### 1.3 Comments

```
//          line comment until end of line
/* ... */   block comment, may nest
```

### 1.4 Whitespace

Space, tab, CR, LF separate tokens but are otherwise insignificant. URUS is
*not* whitespace-sensitive.

---

## 2. Modules

```
file       := module_decl? item*
module_decl:= "module" ident ";"?
```

A file declares **at most one** module. In v0.0.1 the module name is
informational; multi-file modules arrive in v0.0.2.

`use` brings names into scope:

```
use_decl   := "use" path ";"
path       := ident ("." ident | "::" ident)*
```

Both `urus.io.println` and `urus::io::println` parse identically. The
last path segment is what's introduced into the current scope.

---

## 3. Items

```
item       := vis? (fn_decl | struct_decl | enum_decl | impl_block
                  | use_decl | const_decl | type_alias)
vis        := "pub"
```

### 3.1 Functions

```
fn_decl    := "fn" ident "(" params? ")" ret? block
params     := param ("," param)* ","?
param      := self_param | ident ":" type
self_param := "&" "mut"? "self" | "self"
ret        := "->" type
```

### 3.2 Structs

```
struct_decl:= "struct" ident "{" (field ("," field)* ","?)? "}"
field      := ident ":" type
```

### 3.3 Enums

```
enum_decl  := "enum" ident "{" (variant ("," variant)* ","?)? "}"
variant    := ident ("(" type ("," type)* ","? ")")?
```

### 3.4 Impl blocks

```
impl_block := "impl" ident "{" method* "}"
method     := vis? fn_decl
```

In v0.0.1 only **inherent** impls are supported (no `impl Trait for T`).

### 3.5 Constants and type aliases

```
const_decl := "const" ident ":" type "=" expr ";"
type_alias := "type" ident "=" type ";"
```

---

## 4. Types

```
type       := primitive
            | ident                       # named type
            | ident "<" type ("," type)* ">"  # generic
            | "*" "mut"? type             # raw pointer
            | "&" "mut"? type             # reference
            | "[" type "]"                # slice
            | "[" type ";" expr "]"       # array
            | "(" type ("," type)+ ","? ")"   # tuple
            | "(" ")"                     # unit
            | "Self"
            | "fn" "(" type ("," type)* ")" "->" type
primitive  := "u8"|"u16"|"u32"|"u64"
            | "i8"|"i16"|"i32"|"i64"
            | "f32"|"f64" | "usize" | "isize"
            | "bool" | "str" | "char"
```

Built-in generics recognised by the runtime:

```
Result<T, E>      // either Ok(T) or Err(E)
Option<T>         // either Some(T) or None
```

---

## 5. Statements

```
stmt       := let_stmt | expr_stmt | defer_stmt
let_stmt   := "let" "mut"? pattern (":" type)? ("=" expr)? ";"
expr_stmt  := expr ";"?
defer_stmt := "defer" expr ";"?
```

**`defer`** schedules an expression to run when its **enclosing block**
exits normally (reaching the closing `}`). Multiple defers in the same
block run in **LIFO** order:

```urus
fn main() {
    defer println("c")   // runs last
    defer println("b")
    println("a")         // runs first
    defer println("c0")  // runs after println("a") but before "b" and "c"
}
// Output: a, c0, b, c
```

**Limitation (v0.0.1):** `return`, `break`, and `continue` inside the
block **bypass** pending defers. This matches the simple Zig-like model
and is sufficient for cleanup-at-end patterns. The fully-sound "run on
every exit edge" semantics ships in v0.1.0.

A block's **trailing expression** (without `;`) is the block's value:

```urus
let x = { let a = 1; a + 2 };   // x == 3
```

---

## 6. Expressions

### 6.1 Precedence (lowest → highest)

| #  | Operators                                  | Associativity |
|----|--------------------------------------------|---------------|
| 1  | `= += -= *= /= %=`                         | right         |
| 2  | `..` `..=`                                 | non-assoc     |
| 3  | `||`                                       | left          |
| 4  | `&&`                                       | left          |
| 5  | `|`                                        | left          |
| 6  | `^`                                        | left          |
| 7  | `&`                                        | left          |
| 8  | `== !=`                                    | left          |
| 9  | `< <= > >=`                                | left          |
| 10 | `<< >>`                                    | left          |
| 11 | `+ -`                                      | left          |
| 12 | `* / %`                                    | left          |
| 13 | `as`                                       | left          |
| 14 | unary `-`, `!`, `~`, `&`, `&mut`, `*`      | right         |
| 15 | call `f(x)`, dot `.f`, index `[]`          | left          |
| 16 | primary                                    | —             |

### 6.2 Control flow

`if`, `while`, `loop`, `for…in`, `match`, `return`, `break`, `continue`
are expressions. `if` may produce a value if every arm yields the same type.

```urus
let parity = if n % 2 == 0 { "even" } else { "odd" };
```

### 6.3 Pattern matching

```
match_expr := "match" expr "{" (arm ",")* arm? "}"
arm        := pattern "=>" expr
pattern    := "_"
            | ident                       # binds
            | literal
            | ident "(" pattern ("," pattern)* ","? ")"   # variant
```

Currently supported pattern arms: wildcard `_`, identifier (binding),
literal, and one-payload enum variants from `Result`/`Option`. Exhaustiveness
is **[PLANNED]** for v0.0.2.

### 6.4 String interpolation

Inside the first argument of `println` / `print` / `eprintln`, `{name}`
is replaced with the value of `name`, and `{expr.field}` with the value of
that field. The placeholder syntax accepts simple identifier paths only —
no full expressions in v0.0.1.

---

## 7. Memory and references

- `*T` is a raw pointer (no aliasing or lifetime guarantees).
- `*mut T` is a mutable raw pointer.
- `&T` is a shared reference.
- `&mut T` is an exclusive reference.

In v0.0.1 references **lower to raw C pointers** — there is no borrow check.
The full ownership model lands in v0.3.x. Using references unsafely today
is "your problem"; treat them as a forward-looking notation.

---

## 8. Error handling

URUS standardises on `Result<T, E>`:

```urus
fn parse_age(s: str) -> Result<u32, str> {
    // ...
    return Ok(42)
}

match parse_age("42") {
    Ok(n)  => println("got {n}"),
    Err(e) => println("err: {e}"),
}
```

The `?` postfix operator propagates errors. `expr?` evaluates `expr`;
if it is `Err(e)`, the enclosing function returns `Err(e)`; otherwise the
expression yields the unwrapped value:

```urus
fn parse_pair(a: str, b: str) -> Result<i64, str> {
    let x = parse_int(a)?
    let y = parse_int(b)?
    return Ok(x + y)
}
```

**Limitation (v0.0.1):** `?` is supported on `Result<T,E>` only. On
`Option<T>` it remains **[PLANNED]** for v0.0.2, when the type system
can distinguish the two.

`panic(msg: str)` aborts the process and is meant for unrecoverable
programmer errors only.

---

## 9. Standard prelude

Implicitly in scope in every URUS file (v0.0.1):

- Types: `u8 u16 u32 u64 i8 i16 i32 i64 f32 f64 usize isize bool str char`
- Generics: `Result Option`
- Constructors: `Ok Err Some None`
- Functions: `println print eprintln panic`

---

## 10. Conformance test programs

The reference compiler must accept every program in `tests/run/` and reject
every program in `tests/fail/`. Implementers of alternative URUS compilers
**must** pass the same test corpus to claim v0.0.1 conformance.

---

## 11. Versioning

Pre-1.0, `0.X.Y` releases use semver loosely:

- A bump in `X` may break the language.
- A bump in `Y` is purely additive and bug-fixes.

Post-1.0, the same rules as Cargo/Rust apply: only `1.X.Y` major bumps
break source compatibility, and major bumps require an RFC + a two-version
deprecation cycle.
