# 06 — Language Guide

A friendly tour of URUS the *language*. This is a companion to the
formal [Specification](../spec/SPEC.md) — the spec is normative; this
guide is readable.

If you are a working developer who knows Rust, Go, or Zig, you will
recognise nearly everything. URUS deliberately avoids novelty.

---

## A first program

```urus
module main

use urus.io.println

fn main() {
    println("Hello, Aurochs!")
}
```

Compile and run:

```bash
urusc hello.urus --emit-c
gcc hello.urus.c -I stdlib/runtime -o hello
./hello
# Hello, Aurochs!
```

Every URUS source file:

- starts with `module <name>`
- may import names with `use a.b.c`
- defines `fn`s, `struct`s, `impl`s, `enum`s, `const`s, `type`s

---

## Variables

```urus
let x = 10          // immutable — assignment to x is a compile error
let mut y = 20      // mutable (enforced by sema since v0.0.1-b013)
let z: i64 = 30     // explicit type
let s = "hello"     // str
```

Mutability is real, not decoration:

- `x = 5` on an immutable binding → `cannot assign to immutable binding 'x'`
- compound assignment (`+=`, `-=`, …) is gated identically
- `&mut x` over an immutable binding is rejected too (since b021)
- a `mut` that is never exercised draws a *warning* —
  `binding 'y' is declared `mut` but never mutated`

URUS's type inference is currently *block-local* and rides on top of
C's `__auto_type`. A real Hindley-Milner-lite inferrer is planned for
v0.1.

---

## Primitive types

| Family       | Members                                               |
|--------------|-------------------------------------------------------|
| Signed int   | `i8`, `i16`, `i32`, `i64`                             |
| Unsigned int | `u8`, `u16`, `u32`, `u64`                             |
| Float        | `f32`, `f64`                                          |
| Boolean      | `bool`                                                |
| Char         | `char` (a Unicode scalar, lowered to `int32_t`)       |
| String       | `str` (fat pointer: `const char*` + length, immutable)|

Strings are not null-terminated and not C strings. Interop with C goes
through `urus_str` (see [`07-RUNTIME.md`](./07-RUNTIME.md)).

---

## Functions

```urus
fn add(a: i64, b: i64) -> i64 {
    return a + b
}

fn greet(name: str) {
    println("hello, {name}")     // f-string-like interpolation in plain "" too
}
```

- Parameters are positional.
- The return type is after `->`.
- A function without `->` returns nothing (lowered to C `void`).
- The final `return` is optional if the last expression of the block
  matches the return type.

---

## Structs

```urus
struct Point {
    x: f64,
    y: f64,
}

let p = Point { x: 1.0, y: 2.0 }
println("({p.x}, {p.y})")
```

Fields are comma-separated. Trailing commas allowed.

---

## Impl blocks and methods

```urus
impl Point {
    fn new(x: f64, y: f64) -> Point {
        return Point { x: x, y: y }
    }

    fn dist_sq(&self) -> f64 {
        return self.x * self.x + self.y * self.y
    }
}

let p = Point.new(3.0, 4.0)
let d2 = p.dist_sq()
```

- `Point.new(...)` calls an associated function (no receiver).
- `p.dist_sq()` calls a method (`&self`, `&mut self`, or `self`).
- `&self` lowers to `Point *self_` in C. `self` (by value) is supported
  but performs a struct copy.

---

## Enums

```urus
enum Shape {
    Circle(f64),
    Rect(f64, f64),
}

let s = Shape.Rect(3.0, 4.0)
```

Each variant has a tag and an optional tuple payload. `match` is the
intended way to take them apart.

---

## Pattern matching

```urus
match s {
    Circle(r)   => println("circle r={r}"),
    Rect(w, h)  => println("rect {w}x{h}"),
}
```

Supported patterns in v0.0.1:

- identifier (`x` — binds)
- wildcard (`_`)
- literal (`42`, `"hi"`, `true`)
- enum variant (`Ok(v)`, `Err(e)`, `Some(v)`, `None`)
- tuple (`(a, b)`)

**Exhaustiveness is checked** (since v0.0.1-b013, closing F-COMP-3):

- a `match` on `Result` must cover `Ok` **and** `Err`
- a `match` on `Option` must cover `Some` **and** `None`
- a `match` whose arms name variants of one declared enum must cover
  *all* of that enum's variants
- a `_` wildcard (or bare identifier) arm satisfies everything
- guarded arms (`pat if cond =>`) do **not** count toward coverage —
  the guard can be false at runtime

---

## Control flow

```urus
if a > b {
    println("a wins")
} else if a == b {
    println("tie")
} else {
    println("b wins")
}

while x < 100 { x = x + 1 }

// `while let` (since v0.0.1-b024) — loop as long as the pattern matches.
// Desugars to `loop { match … { pat => body, _ => break } }`.
while let Some(line) = read_line() {
    println(f"got {line}")
}

for i in 0..10 { println("{i}") }   // 0..10 is exclusive
for i in 0..=9 { println("{i}") }   // 0..=9 is inclusive
// `for` requires both bounds: `for i in 0..` is a compile error in v0.0.1.

loop {
    if done { break }
}
```

`if`, `match`, and `loop` are expressions — they evaluate to the value
of the chosen arm (or `break value` for `loop`).

---

## Result and Option

```urus
fn divide(a: i64, b: i64) -> Result<i64, str> {
    if b == 0 { return Err("division by zero") }
    return Ok(a / b)
}

match divide(10, 0) {
    Ok(v)  => println("got {v}"),
    Err(e) => println("err: {e}"),
}
```

Both are runtime tagged unions. **Known limitation:** the payload in
v0.0.1 is a fixed `int64_t`. Any payload larger than 8 bytes is silently
truncated. Tracked as F-TY-2 in the security audit; the fix is per-type
monomorphisation in v0.0.2.

---

## The `?` operator

```urus
fn safe(a: i64, b: i64) -> Result<i64, str> {
    let q = divide(a, b)?      // returns Err(...) if divide errored
    return Ok(q * 2)
}
```

`?` is postfix. It only works on `Result` in v0.0.1 (extending to
`Option` is on the v0.0.2 list).

Since v0.0.1-b018, sema verifies the *context*: `?` is only legal
inside a function that itself returns `Result<_,_>` or `Option<_>`.
A stray `expr?` in `fn main() -> ()` is a compile error, not a silent
ABI corruption.

---

## defer

```urus
fn use_resource() {
    let r = acquire()
    defer release(r)         // runs at block end, LIFO
    work(r)
}
```

Defers run in reverse order of registration (LIFO), on **every** exit
edge (since v0.0.1-b028):

- normal block end
- explicit `return expr` — the value is evaluated *first*, then defers
  run, then the function returns
- the `?` operator's error-propagation path

A function may hold at most 64 pending defers across nested blocks
(compile error past that).  Defers must not themselves `return`.

---

## f-strings

```urus
let name = "Aurochs"
let s = f"hello, {name}!"
println(s)
```

`f"..."` allows `{name}` interpolation. Placeholders are **validated**
(since v0.0.1-b014, closing the F-MEM-1 RCE primitive): the contents of
`{…}` must be an identifier or a dotted field chain —

- ✅ `{name}`, `{point.x}`, `{a.b.c}`
- ❌ `{f()}`, `{x + 1}`, `{}`, `{x..}` — all hard compile errors

Anything else is rejected with a precise diagnostic, and the offending
bytes are never spliced into the emitted C. Arbitrary expressions in
placeholders may return later behind a proper sub-parser; v0.0.x keeps
the surface deliberately narrow.

---

## `as` casts

```urus
let i: i64 = 10
let f: f64 = i as f64
```

Casts are a thin wrapper over C's `(T)(expr)`. There is no whitelist
yet (F-TY-3) — that means you can mint pointers from integers, which is
not what you want once `unsafe` lands. For now, restrict yourself to
numeric ↔ numeric.

---

## References

```urus
fn first(xs: &[i64]) -> i64 {
    return xs[0]
}
```

`&T` and `&mut T` are lowered to C pointers. There is no borrow checker
in v0.0.1 — references are *unsafe* in the technical sense, even though
they are not annotated `unsafe`. This is the largest single thing that
v0.3 will fix.

---

## Comments

```urus
// line
/* block */
/* /* nested */ */
```

---

## Standard library (v0.0.1-b019+)

Small but real:

```urus
use urus.io.println
use urus.io.read_line

fn main() -> () {
    while let Some(line) = read_line() {
        if urus_str_eq(line, "quit") { break }
        println(f"echo: {line}")
    }
}
```

- `println` / `print` / `eprintln` — f-string aware output
- `panic(msg)` — abort with a message (capped at 4 KiB)
- `read_line()` → `Option<str>` — one stdin line, no trailing newline,
  CRLF-tolerant, 16 MiB cap
- String helpers (callable as plain functions):
  `urus_str_len`, `urus_str_is_empty`, `urus_str_eq`, `urus_str_cmp`,
  `urus_str_starts_with`, `urus_str_ends_with`, `urus_str_contains`

---

## What is missing

- **Generics** — syntactically parsed, semantically ignored.
  (`Result`/`Option` payloads are capped at 16 bytes until
  monomorphisation lands in v0.0.2.)
- **Traits** — keyword reserved, no implementation.
- **async** — not in any v0.0.x.
- **Macros** — never, in the v0.0.x cycle.
- **Module-to-file mapping** — single-file only for now.

See [`11-ROADMAP-DETAILED.md`](./11-ROADMAP-DETAILED.md) for what shows
up when.

— *Last updated 2026-06-04 (v0.0.1-b027).*
