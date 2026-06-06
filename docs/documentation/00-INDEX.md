# URUS Documentation — Index

> **For developers and contributors.** This folder is the canonical
> entry point for understanding *how* URUS works internally, *why* it
> was built the way it is, and *how to extend it without breaking the
> invariants the rest of the compiler depends on*.
>
> If you only want to *use* URUS as a language, read the top-level
> [`README.md`](../../README.md) and the
> [Language Specification](../spec/SPEC.md) instead. This folder is for
> people who want to read or change the compiler itself.

---

## Reading order

You don't have to read these in order — pick what you need. But if you
are brand new to the codebase, the natural sequence is:

| #   | Document                                          | What you learn                                          |
|-----|---------------------------------------------------|----------------------------------------------------------|
| 01  | [Overview](./01-OVERVIEW.md)                      | What URUS is, the vision, the lineage from `Urus-archive` |
| 02  | [Architecture](./02-ARCHITECTURE.md)              | The compiler pipeline, end-to-end, with a diagram         |
| 03  | [Building](./03-BUILDING.md)                      | Build the compiler on Windows / macOS / Linux             |
| 04  | [Project layout](./04-PROJECT-LAYOUT.md)          | Every directory and what lives in it                      |
| 05  | [Compiler internals](./05-COMPILER-INTERNALS.md)  | Each compiler module, deep                                |
| 06  | [Language guide](./06-LANGUAGE-GUIDE.md)          | URUS *as a language* (companion to the formal spec)       |
| 07  | [Runtime](./07-RUNTIME.md)                        | How `urus_rt.h` works at the C level                      |
| 08  | [Testing](./08-TESTING.md)                        | The test harness, how to add tests                        |
| 09  | [Contributing (deep)](./09-CONTRIBUTING-DEEP.md)  | Style, PR flow, code conventions for the C compiler       |
| 10  | [Design decisions](./10-DESIGN-DECISIONS.md)      | Why we made the choices we did                            |
| 11  | [Detailed roadmap](./11-ROADMAP-DETAILED.md)      | Per-version milestones beyond `ROADMAP.md`                |
| 12  | [FAQ](./12-FAQ.md)                                | Common questions                                          |
| 13  | [Glossary](./13-GLOSSARY.md)                      | Terminology used throughout the codebase                  |
| 14  | [Troubleshooting](./14-TROUBLESHOOTING.md)        | "I see this error — what now?"                            |
| 15  | [Versioning](./15-VERSIONING.md)                  | Version format, build numbers, change-code legend         |

---

## Related documents (outside this folder)

- [`README.md`](../../README.md) — user-facing landing page.
- [`docs/spec/SPEC.md`](../spec/SPEC.md) — formal language specification.
- [`docs/analysis/ANALYSIS.md`](../analysis/ANALYSIS.md) — architecture review + Top 100 roadmap from the language-design team.
- [`docs/review/REVIEW.md`](../review/REVIEW.md) — council-based adoption review.
- [`docs/merge/MERGE-DECISIONS.md`](../merge/MERGE-DECISIONS.md) — what was kept/dropped/deferred from `Urus-archive`.
- [`docs/security/SECURITY-AUDIT.md`](../security/SECURITY-AUDIT.md) — red-team security audit with the **stop-ship** list.
- [`docs/archive/INDEX.md`](../archive/INDEX.md) — build-by-build archive of every recorded state of the codebase.
- [`CHANGELOG.md`](../../CHANGELOG.md) — release notes.
- [`ROADMAP.md`](../../ROADMAP.md) — high-level version targets.
- [`CONTRIBUTING.md`](../../CONTRIBUTING.md) — short-form contributor guide.
- [`GOVERNANCE.md`](../../GOVERNANCE.md) — project decision-making.
- [`SECURITY.md`](../../SECURITY.md) — security disclosure policy.

---

## Conventions used in this folder

- **File paths** are written relative to the repository root, e.g.
  `compiler/src/parser.c` — clickable in IDEs and on GitHub.
- **`file:line` references** are stable as of the v0.0.1 tag.
  If you are reading a much later version of the docs, line numbers will
  have drifted; trust the function name over the number.
- **Code blocks** use the `urus` tag for URUS source and `c` for the
  emitted/host C code.
- **Diagrams** are ASCII so they render identically on GitHub, in
  terminals, and in printed form.
- **Tone.** These docs are written to be useful first, polished second.
  If something is wrong, ugly, or under-explained, please open a PR —
  documentation patches are as welcome as compiler patches.

---

## Status of this folder

URUS is **pre-alpha**. The compiler, the language, and these docs all
move quickly. Each document carries a "Last updated" date at the bottom.
When you change a behavior, update the matching doc in the same PR.

— *URUS docs, last updated 2026-06-03.*
