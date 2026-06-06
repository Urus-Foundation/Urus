# 13 — Glossary

Terminology used across the URUS codebase and docs. Bookmark this if
you are new to compilers; it shortens every other doc.

---

**Arena.** A bump allocator that owns a *range* of memory and hands out
fragments by advancing a pointer. The whole arena is freed at once; you
never free a single allocation. URUS uses one arena per compilation —
see [`05-COMPILER-INTERNALS.md#arenac--bump-allocator`](./05-COMPILER-INTERNALS.md).

**AST.** Abstract Syntax Tree. The parser's output; the rest of the
compiler walks it. In URUS, every AST node carries a `SrcLoc` so
diagnostics can point at it.

**Backend.** The part of a compiler that produces machine code (or, in
URUS v0.0.1, C source). The opposite is the *frontend* (lex, parse,
sema).

**Borrow checker.** A static analysis that proves references obey
aliasing rules ("only one `&mut` at a time," "no use after free").
URUS v0.0.1 does not have one; it lands in v0.3.

**Bump allocator.** See *Arena*.

**Codegen.** Short for "code generation." The pass that emits the
output (C source, in v0.0.1).

**`defer`.** A statement that schedules an expression to run when the
enclosing block exits. URUS adopts the convention from Go and Zig.

**Diagnostic.** An error, warning, or note emitted by the compiler.
Carries a severity, a `SrcLoc`, and a message.

**Fat pointer.** A pointer that is paired with extra data. URUS's
`urus_str` is `{ const char *ptr; size_t len; }` — a fat pointer to
string bytes.

**FNV-1a.** A simple non-cryptographic hash function. URUS uses it to
hash identifier names in the scope table. O(1) average lookup.

**f-string.** A string literal prefixed with `f`, where `{name}` is
replaced by the value of `name`. URUS adopts the syntax from Python.

**Lexer / tokenizer.** The pass that turns source bytes into tokens
(`IDENT`, `INT`, `KW_FN`, …). The opposite is the *parser*.

**Monomorphisation.** Generating a specialised version of a generic
function or type for each concrete type argument. URUS v0.0.1 does not
monomorphise generics; this is why `Result<MyStruct, str>` truncates.

**Pratt parser.** A technique for parsing expressions with operator
precedence by associating each operator with a "binding power" and
recursing until a lower-power token is hit. Easy to extend, easy to
read. URUS's parser uses 18 precedence levels.

**Recursive descent.** A parsing technique where each grammar rule
corresponds to a function that calls other parse functions. URUS uses
recursive descent for items and statements, Pratt for expressions.

**SrcLoc.** Short for "source location." A `{ offset, length, line,
col }` struct attached to every token, AST node, and diagnostic.

**Sema / semantic analysis.** The pass after parsing that resolves
names, checks structural rules, and (eventually) type-checks.

**Statement-expression.** A GCC extension where a brace-enclosed block
is an expression whose value is the last expression inside. Written as
`({ … })`. URUS lowers the `?` operator to one. Why we require Clang
or GCC.

**StrBuf.** URUS's growing-byte-buffer struct, used by codegen to
assemble the emitted C source. Heap-allocated; one of the two `free()`
sites in the whole compiler.

**Tagged union.** A struct whose first field is a tag (discriminant)
and whose second field is a union of payload alternatives. URUS uses
them for `Result`, `Option`, AST nodes, and `urus_fmt_arg`.

**Token.** The lexer's output unit. A `{ kind, location, payload }`
record consumed by the parser.

**Transpile.** To compile from one source language to another source
language. URUS *transpiles* to C; the host C compiler then produces
the machine code.

**TU.** Translation unit — one preprocessed C source file. The URUS
codegen emits one TU per `.urus` input.

**Two-pass.** A compilation strategy with two distinct phases. URUS
sema is two-pass: collect globals, then check bodies.

**`urus_rt.h`.** The header-only runtime that every URUS-produced
binary includes. Defines `urus_str`, `urus_Result`, `urus_Option`,
`urus_println`, `urus_panic`.

**Visibility.** Whether an item is accessible from outside its module.
Marked with `pub` in URUS source — but **not enforced** in v0.0.1.

---

Concepts specific to URUS:

**Aurochs.** The extinct wild cattle species the language is named
after. Symbol of strength, dominance, resilience.

**`tanduk`.** The planned package manager (Indonesian for "horn,"
matching the aurochs theme). v0.1.0.

**`urusc`.** The compiler binary.

**v0.0.1.** The current preview release. Pre-alpha.

— *Last updated 2026-06-03.*
