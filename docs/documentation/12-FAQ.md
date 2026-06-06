# 12 — FAQ

Short answers to questions that come up often. Long-form answers live
in the linked documents.

---

### Is URUS production-ready?

**No.** v0.0.1 is a pre-alpha preview. The
[security audit](../security/SECURITY-AUDIT.md) lists 10 stop-ship
issues including a critical RCE via f-string codegen (F-MEM-1). Treat
URUS today as "do not compile untrusted source files."

### What can I actually build with v0.0.1?

Small standalone programs: hello-world, fizzbuzz, recursive functions,
struct + method examples, simple parsers, anything that fits in one
file and uses only `println` / `print` / `eprintln` from the runtime.
See `examples/` for the working set.

### Why English keywords if the project is Indonesian?

The *language* is intended for global use; the *project culture* is
Indonesian-friendly. See decision D-004 in
[`10-DESIGN-DECISIONS.md`](./10-DESIGN-DECISIONS.md).

### Why does the compiler transpile to C instead of using LLVM?

Because LLVM is months of work and would block every other feature.
Transpiling to C gets the language usable in weeks instead of months.
LLVM lands in v0.2.0. See decision D-002.

### Why does MSVC not work?

The emitted C uses GCC statement-expressions (`({ … })`) and
`__auto_type`. MSVC supports neither. Use Clang or clang-cl on Windows
(`winget install LLVM.LLVM`). See decision D-003 and
[`03-BUILDING.md`](./03-BUILDING.md).

### Is the syntax stable?

**No.** Until v1.0, URUS is allowed to break syntax. We try to bundle
breaks into release boundaries and document them in `CHANGELOG.md`.

### Is `urus_rt.h` ABI-stable?

**No.** Until v1.0. Do not link a v0.0.1-compiled object against a
future runtime.

### Why is there no borrow checker?

Because writing a borrow checker is months of work and we needed to
ship *something*. References are lowered to raw C pointers in v0.0.1.
The borrow checker lands in v0.3. See
[`11-ROADMAP-DETAILED.md`](./11-ROADMAP-DETAILED.md).

### What about `unsafe`?

The keyword is **not yet reserved**. We will reserve it as a no-op in
v0.0.3 and give it semantics in v0.1.

### Why does my `Result<MyStruct, str>` print garbage?

Because the payload in v0.0.1 is fixed at `int64_t`. Anything larger
than 8 bytes is silently truncated. This is F-TY-2 in the audit. The
fix is per-type monomorphisation in v0.0.2.

### How do I add a new language feature?

See [`09-CONTRIBUTING-DEEP.md`](./09-CONTRIBUTING-DEEP.md), section
"Adding a new language feature". The minimum change-set spans lexer →
ast → parser → sema → codegen → tests → spec → guide → changelog.

### Will URUS run on my microcontroller?

Eventually. The transpile-to-C backend already produces portable C; if
your microcontroller has a C compiler, you can in principle build URUS
output for it. The runtime, however, uses `stdio.h` / `tmpfile()` and
will need to be carved up into a `no_std` subset. That work is on the
v0.1 list.

### Is the package manager done?

No. `tanduk` is v0.1.0 work. The design is in
[`11-ROADMAP-DETAILED.md`](./11-ROADMAP-DETAILED.md).

### Why aren't there CI builds?

CI is not yet configured. It is a Tier-1 hardening item. Send a PR.

### Why do you reject PRs that add `Co-Authored-By` lines?

Project rule from the maintainer (see decision D-012). Every commit is
purely under its human author's name.

### How do I report a security issue?

Email `urusfoundation@gmail.com` (planned — placeholder until the policy
ships in `SECURITY.md`). Do not file a public GitHub issue for
exploitable findings.

### Where do I ask questions that are not answered here?

For now: the project's GitHub Discussions tab (planned). Long-term: a
proper chat channel will be set up around v0.1.

— *Last updated 2026-06-03.*
