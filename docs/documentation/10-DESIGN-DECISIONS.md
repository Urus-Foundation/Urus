# 10 — Design Decisions

This document records the *why* behind the choices the rest of the
codebase depends on. When a future contributor asks "why is it this
way?" — this is where the answer lives.

Each decision is structured as **Context → Decision → Consequences →
Alternatives considered**. New decisions go at the bottom with a date.

---

## D-001 — The first compiler is written in C, not URUS

- **Date:** 2026-06-02
- **Context:** A self-hosted compiler is the goal at v1.0, but writing
  the v0.0.1 compiler in URUS is impossible (the language has no
  compiler yet).
- **Decision:** Bootstrap in plain C11. CMake build. Three or four
  thousand lines, max.
- **Consequences.**
  - Anyone with a C toolchain can build URUS. No chicken-and-egg.
  - The compiler is small enough to read in an afternoon.
  - The cost is paid back at v1.0 when the URUS-in-URUS compiler comes
    online and `urusc.c` is retired.
- **Alternatives.** Rust (heavyweight; bootstraps via cargo). Zig
  (good fit; rejected to avoid adding a third toolchain dependency).
  OCaml (excellent for compilers; rare on developer machines).

---

## D-002 — The backend is "transpile to C"

- **Date:** 2026-06-02
- **Context:** A real backend (LLVM, Cranelift) is months of work and
  blocks every other feature behind it.
- **Decision:** Emit one self-contained C translation unit. Let the
  host's `gcc` / `clang` / `clang-cl` finish the job.
- **Consequences.**
  - Portable to every platform that has a C compiler.
  - Debugging the generated code is easy — it is human-readable C.
  - The performance ceiling is whatever `gcc -O3` reaches, which is
    enough for v0.0.x.
  - **Trust boundary moves out.** The host C compiler is now in our TCB.
    This is documented in the security audit.
- **Alternatives.** Direct LLVM IR (deferred to v0.2). QBE (smaller
  than LLVM, but still a hard dependency). Custom backend (years).

---

## D-003 — The emitted C uses GCC statement-expressions and `__auto_type`

- **Date:** 2026-06-02
- **Context:** The `?` operator lowers naturally to a statement
  expression `({ … })`. Block-local type inference rides on
  `__auto_type`.
- **Decision:** Require Clang, GCC, or clang-cl as the post-URUS C
  compiler. Reject native MSVC `cl.exe` with a `#error` on the first
  line of every emitted file.
- **Consequences.**
  - The `?` codegen is simple and obviously correct.
  - The user has to install LLVM on Windows. Documented in `03-BUILDING.md`.
  - `cl.exe` interop is a v0.2.0 problem.
- **Alternatives.** Lower `?` to a helper function (loses inlining,
  changes the return semantics). Implement our own type inference (it
  is in the v0.1 plan anyway, but premature for v0.0.1).

---

## D-004 — Keywords are English, not Indonesian

- **Date:** 2026-06-02
- **Context:** Early URUS drafts (and the archive) experimented with
  Indonesian keywords (`fungsi`, `kawasan`, …). The user explicitly
  asked for the *language* to be globally readable.
- **Decision:** All keywords are English. The *project* (docs, brand,
  community) remains Indonesian-friendly.
- **Consequences.**
  - A developer in Jakarta or Berlin reads the same source.
  - The compiler's own messages remain English (changing this is a
    v0.1 i18n question).
- **Alternatives.** Bilingual keywords (rejected — doubles the parser
  surface). Indonesian keywords with English aliases (rejected — same).

---

## D-005 — Bump arena, one per compilation, no per-node free

- **Date:** 2026-06-02
- **Context:** AST nodes are short-lived. Per-node `malloc`/`free`
  would dominate the profile.
- **Decision:** A single bump-allocated arena owns every AST node and
  every intermediate string. The arena is destroyed at the end of the
  run.
- **Consequences.**
  - Allocation is a pointer bump. The compiler is fast on cold start.
  - Use-after-free inside the AST is impossible because everything
    dies together.
  - Memory usage is "proportional to the largest source we ever
    compiled" until `arena_free`.
- **Alternatives.** Per-node `free` (slow, bug-prone). Reference
  counting (rejected for the same reason it was rejected in the
  archive).

---

## D-006 — FNV-1a hashed scope tables

- **Date:** 2026-06-03 (post-archive merge)
- **Context:** The archive used linked-list scopes. Lookup was O(n) in
  the number of names defined in the enclosing scope. For modules with
  ~thousand names this was already noticeable.
- **Decision:** Open-addressing hash table per `Scope`, FNV-1a as the
  hash, 0.75 load factor, growth by doubling.
- **Consequences.**
  - Lookup is O(1) average regardless of module size.
  - Each scope still chains to its parent via a pointer, so shadowing
    works.
- **Alternatives.** xxHash (faster, but a larger dependency for what is
  a small hot path). Quadratic probing (more code, marginal gain).

---

## D-007 — One-token parser lookahead, recursive descent + Pratt

- **Date:** 2026-06-02
- **Context:** Hand-written parsers are easier to extend, debug, and
  produce good error messages than generated ones, for a language of
  URUS's size.
- **Decision:** Items and statements use recursive descent. Expressions
  use a Pratt parser with 18 precedence levels (matching common systems
  languages).
- **Consequences.**
  - Adding a new operator is one table entry.
  - Error messages can carry full source context.
- **Alternatives.** Tree-sitter (extra dependency, fights C). Generated
  parser (worse errors). PEG (slower without packrat).

---

## D-008 — Result and Option are runtime tagged unions

- **Date:** 2026-06-02
- **Context:** Without generics + monomorphisation, we cannot have a
  zero-cost `Result<T, E>` at v0.0.1.
- **Decision:** Both lowered to a `{ tag, int64_t payload }` struct.
- **Consequences.**
  - Works without generics.
  - **Silently truncates payloads larger than 8 bytes.** This is the
    F-TY-2 stop-ship issue. The v0.0.2 fix is per-type monomorphisation.
- **Alternatives.** Heap-allocate the payload (kills the zero-cost
  story). Macro-expand per type from the runtime (introduces macro hell).

---

## D-009 — Diagnostics carry source locations end-to-end

- **Date:** 2026-06-02
- **Context:** "Error on line 3" with no column or snippet is the worst
  error message format. We refuse to ship that.
- **Decision:** Every `Token`, every `AST` node, and every `Diag`
  carries a `SrcLoc` (offset, length, line, column). The renderer prints
  the offending line and a caret column.
- **Consequences.**
  - Error messages are pleasant to read.
  - Slight memory overhead per node — acceptable.
- **Alternatives.** Only line numbers (no column rendering possible).
  Computing positions lazily (complex, error-prone).

---

## D-010 — No package manager in v0.0.1

- **Date:** 2026-06-02
- **Context:** A bad package manager is a *permanent* security and
  maintenance liability (see the supply-chain section of the audit).
- **Decision:** Defer `tanduk` to v0.1.0. Design it with the supply-chain
  requirements already in mind: lockfile + checksums, signed publishes,
  sandboxed build scripts, transparency log.
- **Consequences.**
  - v0.0.1 is single-file-only. Examples live in `examples/`.
  - No dependency hell.
- **Alternatives.** A naive registry shipped with v0.0.1 (rejected —
  hard to walk back from).

---

## D-011 — License is dual Apache-2.0 / MIT

- **Date:** 2026-06-02
- **Context:** This matches the Rust ecosystem, which is the closest
  community in spirit.
- **Decision:** Dual license. The user picks.
- **Consequences.**
  - Compatible with virtually every downstream license.
  - Patent grant via Apache-2.0 for those who need it.
- **Alternatives.** GPL (incompatible with the systems-language target
  audience). BSD-only (no patent grant).

---

## D-012 — No commits include a `Co-Authored-By` line

- **Date:** 2026-06-02
- **Context:** Project rule from the maintainer. All commits are purely
  under the human author's name.
- **Decision:** Documented in `09-CONTRIBUTING-DEEP.md` as a hard rule.
- **Consequences.** Self-explanatory.

---

## D-013 — The lexer assumes a null-terminated source buffer

- **Date:** 2026-06-02
- **Context:** The lexer reads `*cur` past the supposed end if `cur` is
  not null-terminated. This is fast — no bounds check per token.
- **Decision:** `main.c::read_file` is responsible for placing a `\0`
  after the file contents. The lexer treats `\0` as EOF.
- **Consequences.**
  - The lexer is fast.
  - **`read_file` must validate this assumption** — see F-MEM-5. The
    current code does so for full reads but not for short reads.
- **Alternatives.** Bounds-checked reads (slower; also nice). A length
  field threaded through (more code; on the table for v0.0.2).

---

## D-014 — Tests live in `tests/`, examples live in `examples/`

- **Date:** 2026-06-02
- **Context:** Two different audiences. Tests are for the test runner;
  examples are for humans.
- **Decision:** Keep them separate. `examples/` is allowed to be
  beautiful and over-commented. `tests/` is allowed to be terse and
  weird.

---

When you add a new architectural decision, append it here with the next
ID and date. **Do not delete entries** — even superseded decisions are
useful context.

— *Last updated 2026-06-03.*
