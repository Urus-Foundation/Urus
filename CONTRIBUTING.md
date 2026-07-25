# Contributing to URUS

Thank you for being interested in URUS. Pre-alpha is when a contributor
can shape the language for years, and we take incoming work seriously.

This document is the **binding** contribution policy. If you submit a
patch, file an issue, or open a discussion in this repository, you are
agreeing to the rules below.

The deep contributor handbook is at
[`docs/documentation/09-CONTRIBUTING-DEEP.md`](./docs/documentation/09-CONTRIBUTING-DEEP.md).
**This file (CONTRIBUTING.md) is the policy; that file is the practice.**

---

## Table of contents

- [Code of Conduct](#code-of-conduct)
- [Project ownership & decision authority](#project-ownership--decision-authority)
- [Licensing of your contributions](#licensing-of-your-contributions-the-dco--inbound--outbound)
- [Hard rules](#hard-rules)
- [How to contribute](#how-to-contribute)
- [Issues](#issues)
- [Pull requests](#pull-requests)
- [Commit policy](#commit-policy)
- [Coding style](#coding-style)
- [Tests](#tests)
- [Documentation](#documentation)
- [Security findings](#security-findings)
- [Trademark & brand](#trademark--brand)
- [Commercial use & royalties](#commercial-use--royalties)
- [Disagreements & appeals](#disagreements--appeals)

---

## Code of Conduct

This project is governed by the
[Contributor Code of Conduct](./CODE_OF_CONDUCT.md). All participation
— code, issues, discussions, social interactions in project venues — is
subject to it. Violations may result in a temporary or permanent ban
from the project at the maintainer's sole discretion.

Report conduct issues confidentially to **urusfoundation@gmail.com**
(placeholder).

---

## Project ownership & decision authority

URUS is currently a **single-maintainer project** under a BDFL
(Benevolent Dictator For Life) model. The full structure lives in
[`GOVERNANCE.md`](./GOVERNANCE.md). Short version:

- **Project Founder, Lead Maintainer, and BDFL:**
  **Rasya Andrean** — `rasyaandrean@outlook.co.id`.
- All decisions about scope, design, and release are the founder's to
  make. Disagreements are welcome; the final call is not yours.
- Roadmap changes, scope additions, and breaking changes require
  founder approval **before** code is written.

If you are unsure whether a change will be accepted, **open an issue
first**. We would much rather discuss a design than reject a finished
PR.

---

## Licensing of your contributions (the DCO — inbound = outbound)

URUS is distributed under **Apache-2.0 OR MIT** (see [`LICENSE`](./LICENSE)).
To keep this clean, contributions follow the **inbound = outbound**
rule, formalised through the
[Developer Certificate of Origin v1.1](https://developercertificate.org/).

By submitting a contribution (code, docs, tests, examples, anything in
this repository) you certify that:

> 1. You wrote it or otherwise have the right to submit it under the
>    project's open-source license.
> 2. You are licensing it to the project under **both** Apache-2.0
>    and MIT, so we can re-license downstream releases under either.
> 3. You understand that the project may use, modify, and redistribute
>    your contribution publicly, including commercially.
> 4. You have permission from your employer / school / collaborators
>    where applicable.

**You retain copyright in your contribution.** You are granting a broad
license, not transferring ownership. Specifically:

- Your name stays on the commit. There is no copyright assignment.
- We do **not** ask for a Contributor License Agreement (CLA) at this
  stage. The DCO above is the entire agreement.
- Your contribution may appear in commercial builds of URUS (see
  [Commercial use & royalties](#commercial-use--royalties) and
  [`COMMERCIAL.md`](./COMMERCIAL.md)). You are not entitled to royalties
  for accepted upstream contributions.

> [!IMPORTANT]
> If your employer claims rights over the work you produce on
> company time or hardware, get explicit written permission from them
> before you contribute. We do not police this, but a downstream
> dispute is **your** problem, not ours.

---

## Hard rules

The following are **non-negotiable**. PRs that violate any of these are
closed without review.

1. **No `Co-Authored-By:` lines in commits.**
   Every commit is purely under the human author's name. This rule
   predates the project's first release and applies to every branch,
   every fork that targets `main`, and every release.

2. **No AI-generated content presented as your own.** You may use AI
   tools to assist; you are responsible for the result. Mark
   experimental AI-assisted patches in the PR description.

3. **No proprietary, copyrighted, or trade-secret code copied in.**
   Including: snippets from corporate codebases, leaked source, paywalled
   articles, or anything you do not own and cannot license under
   Apache-2.0 OR MIT.

4. **No commits that change the LICENSE, NOTICE, COMMERCIAL.md, or
   GOVERNANCE.md without prior written approval from the founder.**

5. **No commits that disable security tests, sanitizers, or the MSVC
   `#error` directive** without an explicit founder-approved issue.

6. **No bundling of third-party dependencies** into the compiler or
   runtime without prior approval and license-compatibility review.

7. **No publishing of pre-release binaries** under the URUS name or
   any confusable variant. The trademark policy in
   [Trademark & brand](#trademark--brand) is enforced.

8. **No commercial redistribution under a different brand** that
   removes attribution. See
   [Commercial use & royalties](#commercial-use--royalties).

9. **Respect the security disclosure process.** Security issues go to
   **urusfoundation@gmail.com**, not public issues. See
   [`SECURITY.md`](./SECURITY.md).

10. **One topic per PR.** Refactor + behavior change + style fix in one
    PR makes review impossible; it will be sent back.

---

## How to contribute

1. **Read the relevant docs.**
   - Architecture: [`docs/documentation/02-ARCHITECTURE.md`](./docs/documentation/02-ARCHITECTURE.md)
   - Internals: [`docs/documentation/05-COMPILER-INTERNALS.md`](./docs/documentation/05-COMPILER-INTERNALS.md)
   - Deep contributor guide: [`docs/documentation/09-CONTRIBUTING-DEEP.md`](./docs/documentation/09-CONTRIBUTING-DEEP.md)
2. **Open an issue** for anything non-trivial (≥ ~20 LOC, or any
   user-visible behavior change). Discuss the plan first.
3. **Fork → branch → PR** the usual way (or via GitHub CLI: `gh repo fork Urus-Foundation/Urus --clone`).
4. **Run the test suite + sanitizers locally** before pushing.
5. **Update the matching doc + CHANGELOG + archive entry** if your
   change ships in a build.

---

## Issues

Open an issue when you want to:

- report a bug,
- propose a feature,
- ask whether a change would be accepted,
- discuss design,
- volunteer for a roadmap item.

Use the templates if any exist. Include:

- `urusc --version`
- host OS + C toolchain
- the minimal `.urus` snippet that reproduces the behavior
- expected vs actual output
- any sanitizer / fuzzer output

Do **not** file a security issue publicly. See
[Security findings](#security-findings).

---

## Pull requests

A PR is ready for review when:

- [ ] One topic per PR. (Hard rule.)
- [ ] All hard rules satisfied.
- [ ] Builds with the default CMake config.
- [ ] Builds with ASan + UBSan locally (`-fsanitize=address,undefined -g -O1`).
- [ ] `bash scripts/run-tests.sh` passes.
- [ ] Matching test added (positive *and* negative if applicable).
- [ ] Matching documentation updated.
- [ ] CHANGELOG entry added under `[Unreleased]`.
- [ ] If a user-visible build ships, an archive entry is added too.
- [ ] PR description explains **why**, not just what.
- [ ] Linked issue, if one exists.

A maintainer reviews. We aim to triage within 7 days. Large or
sensitive PRs may take longer.

---

## Commit policy

- **One topic per commit.** Refactor + behavior change in the same
  commit makes review impossible.
- **Subject line in the imperative mood, ≤ 72 characters.**
  Good: `parser: cap recursion depth at 256`.
  Bad: `Fixed some bug in parser stuff`.
- **Body wraps at 72 chars** and explains *why*, not *what*.
- **No `Co-Authored-By:` lines** under any circumstances. *(Hard rule.)*
- **No DCO sign-off line is required.** Submitting a PR is your
  attestation.
- **No marketing in commit messages.** Stick to facts.

Example:

```
parser: cap recursion depth at 256

parse_type / parse_expr / parse_pattern previously had no depth
limit, so a pathological "*****T" deep type crashed urusc with a
stack overflow. Add a depth field to Parser and bail with a fatal
diagnostic when it exceeds 256.

Tracked as F-COMP-1 in docs/security/SECURITY-AUDIT.md.
```

---

## Coding style

Full style guide lives in
[`docs/documentation/09-CONTRIBUTING-DEEP.md`](./docs/documentation/09-CONTRIBUTING-DEEP.md).
TL;DR for compiler source (the host C):

- C11. No GNU extensions in the compiler.
- 4-space indent, no tabs.
- K&R-ish braces. Opening brace on the same line.
- `snake_case` for everything. `PascalCase` only for opaque structs (`Arena`, `Lexer`).
- One `#pragma once` + one `#include "urus_common.h"` at the top of every header.
- No global mutable state in the compiler.
- `static` everything that is not exported.
- Allocate via the arena; do not call `malloc` directly except in `StrBuf` and `main.c::read_file`.
- Asserts for invariants, diagnostics for user errors.

For URUS source in `examples/` and `tests/`:

- 4-space indent.
- Trailing comma in multi-line struct literals and `match` arms.
- One feature per example.
- Tests start with `// Tests: …` or `// expect: …`.

---

## Tests

A PR that changes behavior **must** add at least one test.

- Positive tests go in `tests/run/NN_topic.urus` (must compile + run cleanly).
- Negative tests go in `tests/fail/NN_topic.urus` (must produce a diagnostic).
- Security regression tests go in `tests/fail/SEC-NN_topic.urus`.

Run with `scripts/run-tests.ps1` (Windows) or `scripts/run-tests.sh`
(POSIX). See [`docs/documentation/08-TESTING.md`](./docs/documentation/08-TESTING.md).

---

## Documentation

A PR that changes user-visible behavior **must** update the matching
documentation in the same PR. The pairing is mechanical:

| You changed…                           | Update…                                                         |
|----------------------------------------|-----------------------------------------------------------------|
| Lexer / parser / sema / codegen        | `docs/documentation/05-COMPILER-INTERNALS.md`                   |
| Language surface                       | `docs/spec/SPEC.md` *and* `docs/documentation/06-LANGUAGE-GUIDE.md` |
| Runtime                                | `docs/documentation/07-RUNTIME.md`                              |
| Build / CLI                            | `docs/documentation/03-BUILDING.md`                             |
| Architecture                           | `docs/documentation/02-ARCHITECTURE.md`                         |
| Design decision                        | `docs/documentation/10-DESIGN-DECISIONS.md` (append, never edit) |
| Security                               | `docs/security/SECURITY-AUDIT.md` + `SECURITY.md` if policy changes |
| Anything that ships in a build         | `CHANGELOG.md` + `docs/archive/<vX.Y.Z-bNNN>.md` + `docs/archive/INDEX.md` |

---

## Security findings

Do **not** open a public issue.

- Report to **urusfoundation@gmail.com** (placeholder until the policy
  ships in [`SECURITY.md`](./SECURITY.md)).
- Disclosure window: **90 days** from acknowledgment.
- Acknowledgment SLA: **7 days**.
- For pre-alpha, our current honest scorecard is in
  [`docs/security/SECURITY-AUDIT.md`](./docs/security/SECURITY-AUDIT.md).

Safe-harbor: if you act in good faith, follow the disclosure window,
and do not exfiltrate data beyond the minimum needed to demonstrate the
issue, **we will not pursue legal action against you**. We do not yet
operate a paid bug bounty.

---

## Trademark & brand

> [!IMPORTANT]
> "URUS", the aurochs mark, and the wordmark are **trademarks held by
> the project founder, Rasya Andrean.** Apache-2.0 / MIT grant rights
> to the code, **not** to the brand.

You may:

- Refer to the project as "URUS" in articles, talks, and tutorials.
- Mention URUS compatibility in your own product as a factual statement
  ("works with URUS", "compiles URUS code").

You may **not**, without prior written permission:

- Ship a build of URUS, or a fork that meaningfully diverges, under the
  name "URUS" or a confusable variant.
- Use the aurochs logo on a commercial product, service, or
  paid-distribution channel.
- Imply project endorsement of your fork, service, or company.
- Register domains, package-registry names, or social handles using
  "URUS" + a suffix that suggests official status (`urus-official`,
  `urus-lang`, `urus-foundation-*`, etc.) without permission.

Permissions go to **urusfoundation@gmail.com** (placeholder).

---

## Commercial use & royalties

URUS the **code** is free to use under Apache-2.0 OR MIT, **including
commercially**. No royalty is owed for using URUS, embedding it in a
product, or building paid services on top of it, *provided you comply
with the license text* (preserve copyright notice, include `LICENSE`
and `NOTICE`, etc.).

Specific commercial scenarios that **do** require contact with the
project founder — for licensing terms, attribution rules, and (in
some cases) revenue-share or royalty arrangements:

- **Commercial fork or distribution under a different brand** that
  removes or alters URUS attribution.
- **Use of the URUS name, aurochs mark, or wordmark** in a
  commercial product, service, paid course, certification, or
  conference (see [Trademark & brand](#trademark--brand)).
- **Hosted "URUS-as-a-Service" offerings** (cloud builds, managed
  registries, hosted IDE) that use URUS branding.
- **Embedded resale of URUS** as part of a larger licensed product
  (SDK, console SDK, OEM toolchain).
- **Government, defense, banking, healthcare, or critical-infrastructure
  procurement** that requires a named license-holder, indemnification,
  or SLA.
- **Bundling URUS with proprietary stdlib extensions** that you wish to
  sell.

To open a commercial conversation:

> **Commercial licensing & partnerships:**
> **urusfoundation@gmail.com** (placeholder) /
> **Rasya Andrean — `rasyaandrean@outlook.co.id`.**

Include: company, intended use, distribution channel, expected
volume, timeline. We will respond with terms (which may include
attribution, a one-time fee, a recurring license, or a revenue share,
depending on the case).

Full commercial terms: [`COMMERCIAL.md`](./COMMERCIAL.md).

> [!NOTE]
> **Royalty / revenue-share arrangements apply only to the founder and
> any future entity formally designated to hold project IP.** Contributors
> do not automatically receive a share of commercial revenue — the open
> licenses (Apache-2.0 / MIT) you grant via the DCO are unconditional.
> Future formal sponsorship programs, paid maintainer roles, or
> grant-funded work are entirely separate from upstream contributions
> and will be announced explicitly.

---

## Disagreements & appeals

We try to be reasonable. If a PR is rejected and you believe the
reason is wrong:

1. Reply on the PR with the specific objection. Calmly.
2. If the disagreement persists, open an issue with the `governance`
   label.
3. Final decisions rest with the project founder per
   [`GOVERNANCE.md`](./GOVERNANCE.md).

"I disagree with the maintainer" is not grounds for ignoring the
project's rules. The fork option always exists if your goals diverge
from URUS's.

---

*Thank you for reading this far. Now go write something useful — and
file an issue first.*

— *The URUS project, last updated 2026-06-03.*
