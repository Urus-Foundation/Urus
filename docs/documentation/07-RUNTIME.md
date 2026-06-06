# 07 — The Runtime (`urus_rt.h`)

The runtime is one **header-only** C file. Every URUS-produced binary
`#include`s it; the produced object file embeds the parts it actually
uses.

The runtime is intentionally tiny. It exists for one reason: to give
the emitted C the smallest possible "language standard library" — a
string type, two tagged unions, a typed-print primitive, and `panic`.

---

## Files

- `stdlib/runtime/urus_rt.h` — everything.

There is no `.c` file. The header uses `static URUS_INLINE` for the
small bits and `static` (file-scope) for state. This means every TU
that includes it gets its own copy of `g_urus_argc` etc. The runtime is
**not designed for multi-TU URUS programs in v0.0.1** — that lands once
the package manager / multi-module codegen does.

---

## The `urus_str` fat pointer

```c
typedef struct {
    const char *ptr;
    size_t      len;
} urus_str;
```

Why fat? Because:

- URUS strings are **not null-terminated**. They are bounded by an
  explicit length.
- This lets the compiler slice substrings without copying.
- It is the obvious match for the eventual borrow checker — `&str`
  becomes a "view into bytes owned by someone else."

The runtime provides `urus_str_lit(literal)` to construct an `urus_str`
from a C string literal at compile time:

```c
urus_str s = urus_str_lit("hello");
```

---

## `urus_Result` and `urus_Option`

```c
typedef struct {
    int      tag;             // URUS_RES_OK = 0, URUS_RES_ERR = 1
    int64_t  payload;         // <-- known limitation, see below
} urus_Result;

typedef struct {
    int      tag;             // URUS_OPT_NONE = 0, URUS_OPT_SOME = 1
    int64_t  payload;
} urus_Option;
```

Helpers:

| Helper                  | Purpose                                  |
|-------------------------|------------------------------------------|
| `urus_ok(x)`            | construct an Ok                          |
| `urus_err(e)`           | construct an Err                         |
| `urus_some(x)`          | construct a Some                         |
| `urus_none()`           | construct a None                         |
| `urus_is_ok(r)`         | tag check                                |
| `urus_is_err(r)`        | tag check                                |
| `urus_is_some(o)`       | tag check                                |
| `urus_is_none(o)`       | tag check                                |
| `urus_payload(r)`       | extract the payload (Ok value or Err)    |

### Known limitations

1. **Payload is fixed at `int64_t`.** Anything bigger gets truncated.
   `Ok(my_big_struct)` silently loses bytes. Tracked as F-TY-2 in the
   security audit. The v0.0.2 fix is per-`<T, E>` monomorphisation.

2. **Result and Option share tag values.** `URUS_RES_ERR == URUS_OPT_SOME == 1`.
   A cross-cast between the two types produces type confusion. Tracked
   as F-TY-1. The fix is disjoint tag namespaces and the codegen
   rejecting any cross-cast.

---

## The `urus_fmt_arg` typed-print primitive

The runtime's print path is a flat array of tagged values, terminated
by a sentinel tag:

```c
typedef enum {
    URUS_FMT_END_TAG,
    URUS_FMT_STR_TAG,
    URUS_FMT_I64_TAG,
    URUS_FMT_F64_TAG,
    URUS_FMT_BOOL_TAG,
    URUS_FMT_PTR_TAG,
    URUS_FMT_RAW_TAG,        // raw const char* + length
} urus_fmt_tag;

typedef struct {
    urus_fmt_tag tag;
    union {
        urus_str s;
        int64_t  i;
        double   f;
        bool     b;
        void    *p;
        struct { const char *ptr; size_t len; } raw;
    };
} urus_fmt_arg;
```

Codegen produces an array literal terminated by `{ URUS_FMT_END_TAG }`
and calls `urus_print_fmt(stdout, args)`.

### Why this design?

A tagged array lets the codegen mix arbitrary types in one `println`
without a `printf` format string — no format-string vulnerabilities, no
mismatch between `%d` and the actual argument. Each element knows its
own type.

### Sharp edges

- The "terminate with END" design means a single missing END in
  generated code walks past the array. F-MEM-7. The v0.0.2 fix is to
  pass an explicit length alongside the array.
- `URUS_FMT_PTR_TAG` calls `fputs((const char *)ptr, out)` — type
  confusion if anything but a C string ever lands there. F-MEM-8. The
  fix is to remove `PTR_TAG` and force all string pointers through
  `urus_str`.

---

## `_Generic`-based dispatch in `println`

The expansion of `println("x = {name}")` produces something like:

```c
urus_println_fmt((urus_fmt_arg[]){
    URUS_FMT_RAW("x = "),
    URUS_FMT_ANY(name),
    { URUS_FMT_END_TAG }
});
```

`URUS_FMT_ANY(x)` is a macro that uses C11 `_Generic` to pick the right
arm:

```c
#define URUS_FMT_ANY(x) _Generic((x), \
    int64_t:    URUS_FMT_I64, \
    double:     URUS_FMT_F64, \
    bool:       URUS_FMT_BOOL, \
    urus_str:   URUS_FMT_STR, \
    default:    URUS_FMT_PTR)(x)
```

This is the entire mechanism — no varargs, no `printf`, no format-string
attacks (assuming the codegen does not paste user text into the macro
expansion, which is exactly the F-MEM-1 bug).

---

## `urus_fmt_to_str`

A two-pass conversion that takes a `urus_fmt_arg[]` and returns a
freshly-allocated `urus_str` containing the rendered text. Internally
uses `tmpfile()`:

1. Pass 1: write to `tmpfile`, measure length.
2. Pass 2: rewind, read back into a fresh `malloc`'d buffer.

This is what backs f-string `let s = f"…"`. **Sharp edges:**

- `tmpfile()` can fail under sandboxes / chroots / restricted Windows
  containers. The current code silently returns an empty string in that
  case. The robust v0.0.2 fix is an in-memory writer or `open_memstream`
  on POSIX.

---

## `urus_panic`

```c
URUS_NORETURN void urus_panic(const char *msg);
```

Prints the message to stderr and calls `abort()`. No signal handlers,
no cleanup. This is the only "runtime fault" path in v0.0.1.

---

## `urus_argc` / `urus_argv`

The runtime captures `main`'s `argc`/`argv` into globals so that any
URUS function can read them. v0.0.1 has no thread support, so the
globals are safe; they will need TLS once async lands.

---

## ABI stability

There is **none yet**. Until v1.0, `urus_rt.h` may change between
versions. Do not link a v0.0.1-compiled object file against a future
runtime.

— *Last updated 2026-06-03.*
