# 15 — Versioning & Change Codes

URUS follows **Semantic Versioning** with an extra **build number** so
that every snapshot of the codebase has a single, unambiguous identity.
This document is the canonical reference for the format.

---

## Version format

```
v MAJOR . MINOR . PATCH - b BUILD
│ │       │       │       │
│ │       │       │       └── monotonic build counter, 3+ digits, zero-padded
│ │       │       └────────── patch (backwards-compatible bug fix)
│ │       └────────────────── minor (backwards-compatible feature)
│ └────────────────────────── major (breaking change)
└──────────────────────────── literal "v"
```

Examples:

```
v0.0.1-b001       — initial scaffold
v0.0.1-b008       — eighth tracked build of the v0.0.1 line
v0.1.0-b025       — first build of v0.1.0
v1.0.0-b1000      — stable release
```

The pre-1.0 rules of SemVer apply: any minor bump (`0.0 → 0.1`,
`0.1 → 0.2`, …) may break compatibility. After v1.0 only major bumps
may break.

## Build number rules

1. **Monotonic across all versions.** Build numbers never reset. After
   `v0.0.1-b020`, the next build — whether it lands on `v0.0.1`,
   `v0.0.2`, or `v0.1.0` — is `b021`.
2. **Zero-padded to at least 3 digits.** `b001`, `b042`, `b317`,
   `b1000`. This keeps lexical sort = numeric sort up to b999, and we
   widen the pad when we cross b999.
3. **One archive entry per build.** Even tiny builds get a number.
   That is the entire point — every state of the codebase is named.
4. **Builds are immutable.** Once an archive entry is written, it is
   not edited. Mistakes get a *new* build that corrects them.

## Status labels

Each archive entry carries a **Status** field:

| Status   | Meaning                                                  |
|----------|----------------------------------------------------------|
| `Alpha`  | Pre-feature-complete; expect breakage every build.        |
| `Beta`   | Feature-complete for the next release, hardening only.    |
| `Stable` | Released. No further work in this version line.           |
| `LTS`    | Stable + long-term support window declared.               |

URUS v0.0.x is all `Alpha` until v0.0 closes.

---

## Change codes

Every archive entry uses a fixed set of one-letter codes so that
scanning a long changelog stays fast. The codes are:

| Code | Meaning (EN)                                              | Arti (ID)                                                |
|------|-----------------------------------------------------------|----------------------------------------------------------|
| `A`  | Added — a brand-new feature                                | Fitur baru                                                |
| `U`  | Updated — an existing feature got better                   | Pembaruan fitur                                           |
| `F`  | Fixed — a bug is gone                                      | Perbaikan bug                                             |
| `R`  | Removed — a feature was deleted                            | Fitur dihapus                                             |
| `D`  | Deprecated — feature still works but will be removed later | Fitur akan dihapus di masa depan                          |
| `S`  | Security — a security issue was addressed                  | Perbaikan keamanan                                        |
| `P`  | Performance — measurable speed or memory win               | Peningkatan performa                                      |
| `B`  | Breaking — incompatible with prior versions                | Perubahan tidak kompatibel                                |
| `O`  | Optimized — code clean-up that improves a metric           | Optimisasi                                                |
| `C`  | Changed — general modification that does not fit elsewhere | Perubahan umum                                            |
| `T`  | Testing — new tests, harness work                          | Pengujian                                                 |
| `X`  | Experimental — opt-in feature, may disappear               | Fitur eksperimen                                          |
| `M`  | Migration — users have to act to upgrade                   | Memerlukan migrasi                                        |
| `N`  | Notes — important context, not a code change               | Catatan penting                                           |
| `I`  | Internal — compiler / runtime refactor                     | Perubahan internal compiler/runtime                       |
| `L`  | Language — syntax / language-feature change                | Perubahan sintaks atau fitur bahasa                       |
| `E`  | Ecosystem — tooling, package manager, IDE, formatter, linter | Tooling, package manager, IDE, formatter, linter        |

**Style rules.**

- One code per line. One topic per line.
- Codes are upper-case followed by `: `.
- Lines are short — full prose belongs in the linked doc, not the entry.
- Multiple lines with the same code is fine; do not collapse them.
- Order within an entry follows the **severity order** below.

**Severity order** (top of the entry → bottom):

```
B  →  S  →  M  →  R  →  D  →  A  →  L  →  U  →  C  →  F  →  P  →  O  →  X  →  T  →  I  →  E  →  N
```

Breaking + security + migration go at the top so a reader skimming
sees the dangerous parts first.

---

## Archive entry format

The archive lives at [`docs/archive/`](../archive/). One Markdown file
per build, named `v<MAJOR>.<MINOR>.<PATCH>-b<BUILD>.md`.

Each file has this shape:

```markdown
# v0.0.1-b008

Version : v0.0.1-b008
Date    : 2026-06-03
Status  : Alpha

## Changes

A : Documentation folder docs/documentation/ with 15 files
A : 15-VERSIONING.md (this convention)
A : docs/archive/ build-numbered archive
I : Moved ANALYSIS / MERGE / REVIEW / SECURITY / SPEC into sub-folders
N : Establishes versioning + change-code conventions for all future builds

## Linked work

- docs/documentation/15-VERSIONING.md
- docs/archive/INDEX.md
```

**Sections.**

- **Header** — version, date (ISO 8601), status.
- **Changes** — change-code list, severity-ordered.
- **Linked work** — files, PRs, issues, audit IDs touched by this build.

**No section may be empty.** If nothing changed in a category, omit it
— but the build itself must touch *something* worth recording, or it
should not get an entry.

---

## When to mint a new build

A new build is minted when **any one** of the following lands on the
default branch:

- a public-API change (`L`, `B`, `R`, `D`, `A` on the language surface),
- a user-visible compiler behavior change (`A`, `U`, `C`, `F`, `P`),
- a security fix (`S`),
- a tooling / ecosystem release (`E`),
- a documentation milestone that warrants a callable name,
- the maintainer says so.

Internal refactors (`I`) without behavior change do **not** require a
new build on their own — they are folded into the next build that
ships for another reason.

## When to bump version

| Bump                                | Triggers                                                       |
|-------------------------------------|----------------------------------------------------------------|
| `MAJOR` (`0.x → 1.0`, `1.x → 2.0`)  | Breaking change after v1.0. Pre-v1.0 we use minor for breaking. |
| `MINOR` (`0.0 → 0.1`)               | New language feature, new stdlib module, new backend.           |
| `PATCH` (`0.0.1 → 0.0.2`)           | Bug fix or security fix without new features.                   |

Pre-v1.0 the line is fuzzy by design — the project is still pre-alpha.
After v1.0 we hold this strictly.

## Tagging & releases

Each archive entry that we *publicly* release also gets a matching git
tag with the same name (`v0.0.1-b008`). Internal builds may or may not
be tagged at the maintainer's discretion. The archive entry is the
source of truth; the git tag is convenience.

## Long-term shape

Eventually URUS will have many hundreds of archive entries. The folder
structure handles this by:

- using zero-padded build numbers so lexical sort works,
- keeping one entry per file (no megafiles),
- maintaining a [`docs/archive/INDEX.md`](../archive/INDEX.md) summary
  that links every entry with a one-line description.

When build numbers cross `b999`, widen the pad globally to four digits
in the next entry (`b1000`); existing files do not get renamed.

---

## Quick reference

```
v0.0.1-b008                    ← version with build number
A:  added                       L:  language change
U:  updated                     S:  security
F:  fixed                       P:  performance
R:  removed                     B:  breaking
D:  deprecated                  M:  migration required
C:  changed                     E:  ecosystem
T:  tests                       I:  internal
X:  experimental                N:  note
O:  optimized                   (severity order at top: B → S → M → R → D → A → L → U → C → F → P → O → X → T → I → E → N)
```

— *Last updated 2026-06-03 — applies from v0.0.1-b008 onward.*
