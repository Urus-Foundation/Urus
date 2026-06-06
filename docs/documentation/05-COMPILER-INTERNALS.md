# 05 — Compiler Internals

A module-by-module deep dive. Each section describes the data
structures, the public entry points, the key invariants, and the
gotchas. Read this in front of the source file — they are short
enough to read together.

---

## arena.c — bump allocator

**Purpose.** Own every long-lived allocation made during one
compilation. The compiler frees nothing piecewise; it drops the whole
arena at the end.

**Data.**

```c
typedef struct ArenaChunk {
    struct ArenaChunk *next;
    size_t             used;
    size_t             cap;
    unsigned char     *buf;
} ArenaChunk;

typedef struct Arena {
    ArenaChunk *head;       // current chunk
    ArenaChunk *all;        // chunk list for free
    size_t      chunk_size; // suggested new-chunk size
} Arena;
```

**API.**

| Function                                 | Behavior                                      |
|------------------------------------------|-----------------------------------------------|
| `arena_init(a, chunk_size)`              | initialize empty arena                        |
| `arena_alloc(a, size)`                   | bump-aligned to 8 bytes, may grow             |
| `arena_alloc_zero(a, size)`              | as above + zeroed                             |
| `arena_strdup(a, str)`                   | copy a C string into the arena                |
| `arena_strndup(a, ptr, n)`               | copy `n` bytes + add `\0`                     |
| `arena_free(a)`                          | free every chunk; resets to empty             |

**Invariants.**

- Every returned pointer is 8-byte aligned.
- Returned memory lives until `arena_free`.
- Reallocation is **not supported**. Callers that need growable storage
  use `StrBuf`.

**Known sharp edges.**

- `size` is rounded up via `(size + 7u) & ~7u`. Very large `size` can
  overflow this. Tracked in
  [`docs/security/SECURITY-AUDIT.md`](../security/SECURITY-AUDIT.md)
  as F-MEM-3.

---

## strbuf.c — growing byte buffer

**Purpose.** Assemble the emitted C source one chunk at a time.

**Data.**

```c
typedef struct StrBuf {
    char  *data;
    size_t len;
    size_t cap;
} StrBuf;
```

**API.**

| Function                                  | Behavior                                       |
|-------------------------------------------|------------------------------------------------|
| `strbuf_init(sb, initial_cap)`            | malloc the buffer                              |
| `strbuf_append(sb, s, n)`                 | append `n` bytes, grow on demand               |
| `strbuf_appendf(sb, fmt, ...)`            | printf-style append                            |
| `strbuf_free(sb)`                         | release the heap allocation                    |

**Invariants.**

- `data` is null-terminated after every successful operation.
- Capacity grows by doubling — never linear, never less than `len + add + 1`.

**Sharp edges.**

- `while (sb->cap < need) sb->cap *= 2;` can spin forever on overflow
  (F-MEM-4 in the audit).

---

## diag.c — diagnostics

**Purpose.** Produce human-friendly compiler errors and warnings with a
snippet and a caret.

**Data.**

```c
typedef enum { DIAG_NOTE, DIAG_WARN, DIAG_ERROR } DiagSeverity;

typedef struct Diag {
    DiagSeverity sev;
    SrcLoc       loc;
    const char  *msg;
} Diag;

typedef struct DiagCtx {
    Arena       *arena;
    const char  *source;
    const char  *path;
    Diag        *items;
    size_t       count, cap;
    int          error_count;
} DiagCtx;
```

**API.**

| Function                                          | Behavior                          |
|---------------------------------------------------|-----------------------------------|
| `diag_init(dx, arena, source, path)`              | wire up the context               |
| `diag_emit(dx, sev, loc, fmt, ...)`               | record + render to stderr         |
| `diag_has_errors(dx)`                             | non-zero if any error             |

**Invariants.**

- Format strings are compile-time constants. There is no user-text
  format substitution path.
- Every diagnostic carries a valid `SrcLoc`.

**Rendering.** The snippet shows the line containing the error and a
caret column aligned to the offset. Multi-line errors are flattened;
multi-span errors are on the v0.1.0 roadmap.

---

## lexer.c — tokenizer

**Purpose.** Convert a null-terminated source buffer into a stream of
`Token`s.

**Data.**

```c
typedef enum {
    TOK_EOF, TOK_IDENT, TOK_INT, TOK_FLOAT, TOK_STRING, TOK_CHAR,
    TOK_FSTR, /* f"..." */
    /* keywords */ TOK_KW_FN, TOK_KW_STRUCT, …, TOK_KW_DEFER,
    /* punctuation */ TOK_LPAREN, TOK_RPAREN, …,
    /* operators */ TOK_PLUS, TOK_PLUS_EQ, …
} TokKind;

typedef struct Token {
    TokKind     kind;
    SrcLoc      loc;
    const char *start;       // pointer into source
    size_t      length;
    /* literal payload */
    int64_t  int_val;
    double   float_val;
    char    *str_val;        // owned by arena
} Token;

typedef struct Lexer {
    Arena       *arena;
    DiagCtx     *diag;
    const char  *cur;
    const char  *line_start;
    int          line;
    Token        peeked;
    int          has_peeked;
} Lexer;
```

**Entry point.** `lexer_next(lx) -> Token`. There is one-token
lookahead: `lexer_peek(lx)` populates `peeked` if empty.

**Numeric handling.**

- Decimal: `123`, `123_456`
- Hex: `0x...`, `0X...`
- Octal: `0o...`
- Binary: `0b...`
- Float: `1.0`, `1e3`, `1.5e-2`
- Suffix: `i32`, `u64`, `f64`, etc. — parsed but not yet type-checked.

**String handling.**

- Standard `"..."` with `\n \r \t \\ \" \xNN` escapes.
- `f"..."` is its own token kind so the parser can build an
  `EX_FSTR_LIT`.
- Multi-byte characters in literals are passed through unchanged.

**Sharp edges.**

- `lex_string` uses `realloc` without a NULL check (F-MEM-2).
- The whole source buffer is assumed null-terminated (F-MEM-5).

---

## ast.h / ast.c — abstract syntax tree

**Purpose.** Define every node kind in a discriminated union and
provide arena-backed constructors.

**Three families.**

- **Items** (top-level): `IT_FN`, `IT_STRUCT`, `IT_IMPL`, `IT_ENUM`,
  `IT_USE`, `IT_CONST`, `IT_TYPE`.
- **Statements**: `ST_LET`, `ST_EXPR`, `ST_RETURN`, `ST_BREAK`,
  `ST_CONTINUE`, `ST_DEFER`.
- **Expressions**: `EX_INT_LIT`, `EX_FLOAT_LIT`, `EX_STR_LIT`,
  `EX_BOOL_LIT`, `EX_CHAR_LIT`, `EX_FSTR_LIT`, `EX_IDENT`, `EX_PATH`,
  `EX_UNARY`, `EX_BINARY`, `EX_ASSIGN`, `EX_CALL`, `EX_METHOD_CALL`,
  `EX_FIELD`, `EX_INDEX`, `EX_IF`, `EX_MATCH`, `EX_BLOCK`, `EX_RETURN`,
  `EX_BREAK`, `EX_CONTINUE`, `EX_WHILE`, `EX_FOR`, `EX_LOOP`,
  `EX_STRUCT_LIT`, `EX_TUPLE_LIT`, `EX_ARRAY_LIT`, `EX_CAST`,
  `EX_REF`, `EX_DEREF`, `EX_RANGE`, `EX_TRY`.

Each node carries a `SrcLoc` so diagnostics can point at it.

**Why a tagged union?** A flat `Expr` struct holds one
`enum`-discriminated `union` of payloads. This trades a small amount of
size waste for cache-friendly traversal — the codegen and sema both
visit the AST in tight switches.

---

## parser.c — recursive-descent + Pratt

**Purpose.** Convert tokens to an AST.

**Strategy.**

- **Items and statements** use plain recursive descent (`parse_item`,
  `parse_stmt`).
- **Expressions** use a **Pratt parser** with 18 precedence levels —
  binary operators dispatched by precedence, unary and primary handled
  separately.

**Key functions.**

| Function                  | Returns        | Notes                                    |
|---------------------------|----------------|------------------------------------------|
| `parser_parse_module`     | `Module *`     | top-level driver                          |
| `parse_item`              | `Item *`       | `fn`/`struct`/`impl`/`enum`/`use`/`const` |
| `parse_block`             | `Block *`      | `{ stmts; trailing-expr? }`              |
| `parse_stmt`              | `Stmt *`       | `let`, `defer`, expression stmt          |
| `parse_expr_prec(p, min)` | `Expr *`       | the Pratt engine                          |
| `parse_pattern`           | `Pattern *`    | match arm + let binding patterns          |
| `parse_type`              | `Type *`       | primitives, names, `*T`, `&T`, `[T; N]`  |

**Error recovery.** On error, the parser emits a diagnostic and
resynchronises at the next `fn`/`struct`/`impl`/`enum`/`use`/`const` —
the next item boundary. This catches multiple errors per run.

**Sharp edges.**

- All `parse_*` are unbounded-recursive (F-COMP-1). A pathological
  `*****T` deep type bombs the stack. Depth-cap is on the v0.0.2 list.

---

## sema.c — semantic analysis

**Purpose.** Resolve names; check easy-to-check structural rules.

**Symbol table.**

```c
typedef struct Symbol { … } Symbol;

typedef struct Scope {
    struct Scope *parent;
    Symbol      **slots;     // FNV-1a hashed, open addressing
    size_t        count, cap;
} Scope;
```

The hashed scope replaces the original linked-list scope (an
`Urus-archive` carryover). Lookup is O(1) average for any program size.

**Two passes.**

1. **Collect globals.** Top-level fns, structs, enums, consts go into a
   module-level scope. Duplicate definitions error here.
2. **Check bodies.** Walk each function body; build a stack of `Scope`s;
   resolve every `EX_IDENT` and `EX_PATH`; check struct-literal field
   names against the struct decl; declare pattern bindings into the
   surrounding scope.

**Prelude.** Built-in types (`i64`, `f64`, `str`, `bool`, …) and built-in
identifiers (`Ok`, `Err`, `Some`, `None`, `println`, `print`, `eprintln`,
`panic`) are seeded into a global scope before either pass runs.

**What sema does NOT do (yet).**

- No type inference.
- No type checking past struct-literal field existence.
- No `let mut` enforcement (F-COMP-2).
- No `match` exhaustiveness (F-COMP-3).
- No visibility (`pub`) enforcement (F-TY-4).
- No generic instantiation (F-TY-5).

These are tracked.

---

## codegen_c.c — emit C

**Purpose.** Turn the AST into one self-contained C11 translation unit.

**Strategy.**

- One walk per module.
- Emit a fixed prologue: `#error` for MSVC, then `#include "urus_rt.h"`.
- Emit struct decls, then enum decls, then fn forward decls, then fn
  bodies — in that order, so any in-body reference is already declared.

**Key functions.**

| Function                | Emits                                             |
|-------------------------|---------------------------------------------------|
| `codegen_module`        | the whole TU                                      |
| `cg_type`               | a C type for a URUS type                          |
| `cg_expr`               | a C expression for any URUS expression            |
| `cg_stmt`               | a C statement for any URUS statement              |
| `cg_emit_defers`        | end-of-block LIFO defers                          |
| `cg_fmt_arg_array`      | the `urus_fmt_arg[]` literal for f-strings + println |
| `try_emit_println_call` | special-cases `println("..")` / `print("..")`     |

**Name mangling.** Methods on a struct `T` named `m` become `T__m`.
Identifiers that collide with C keywords (e.g. `main`, `do`, `while`)
get an `_` suffix. This is done in `emit_ident`.

**The `?` operator.** Lowered to a GCC statement-expression:

```c
({
    urus_Result _t = (<inner>);
    if (urus_is_err(_t)) return _t;
    urus_payload(_t);
})
```

Because we use statement-expressions, we cannot compile with native MSVC
— this is the deliberate trade-off (see `10-DESIGN-DECISIONS.md`).

**Defer.** Each block keeps a stack of defer expressions. On the *block-end*
exit edge, `cg_emit_defers` walks the stack in reverse and emits them.
`return` currently bypasses pending defers — documented limitation, on
the list to fix in v0.0.2.

**Sharp edges.**

- `cg_fmt_arg_array` pastes f-string `{...}` bracket contents verbatim
  into emitted C. This is the **critical** F-MEM-1 finding from the
  security audit. Fix: re-lex the placeholder and reject anything other
  than `IDENT (DOT IDENT)*`.

---

## main.c — driver

**Purpose.** CLI parsing + the top-level orchestration loop.

**Flags.**

| Flag                    | Action                                                |
|-------------------------|-------------------------------------------------------|
| (default) / `--emit-c`  | write `<input>.c`                                     |
| `--tokens`              | dump the token stream                                 |
| `--ast`                 | dump the parsed module                                |
| `--emit-tokens` (deprecated) | alias of `--tokens` with a deprecation warning  |
| `--emit-ast` (deprecated)    | alias of `--ast` with a deprecation warning      |
| `-o <path>`             | override the output path                              |
| `--version`             | print version info                                    |
| `--help`                | print usage                                           |

**Flow.** `read_file → arena_init → diag_init → lex → parse → sema →
codegen → write → arena_free`.

**Sharp edges.**

- `read_file` has no upper size bound and no symlink/file-type check
  (F-MEM-5, F-MEM-10).

---

## urus_common.h — shared primitives

Holds:

- `SrcLoc` struct (offset, length, line, col).
- `URUS_INLINE` / `URUS_NORETURN` portable attributes.
- Boilerplate `#include`s used everywhere.

This header is the bottom of the dependency stack. Nothing it includes
includes anything from `urus_common.h` — break that and you get a
circular-include cascade.

— *Last updated 2026-06-03.*
