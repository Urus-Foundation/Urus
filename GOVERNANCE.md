# URUS Governance

This document is the **binding governance policy** of the URUS project.
By participating — code, issues, discussions, conference talks under the
URUS name, anything — you agree to operate inside the structure below.

The policy may change over time as the project grows. Changes are
themselves governed by [section 9](#9-amending-this-document).

---

## Table of contents

1. [Project identity](#1-project-identity)
2. [Roles](#2-roles)
3. [Decision authority](#3-decision-authority)
4. [Intellectual property ownership](#4-intellectual-property-ownership)
5. [Trademarks](#5-trademarks)
6. [Commercial licensing & revenue](#6-commercial-licensing--revenue)
7. [Contributor rights & obligations](#7-contributor-rights--obligations)
8. [Conflict resolution](#8-conflict-resolution)
9. [Amending this document](#9-amending-this-document)
10. [Contact](#10-contact)

---

## 1. Project identity

- **Project name:** URUS.
- **Project Founder, Original Author, and BDFL:**
  **Rasya Andrean** (`rasyaandrean@outlook.co.id`).
- **Established:** 2026, with v0.0.1 publicly released 2026-06-03.
- **License (code):** Apache-2.0 OR MIT, contributor's choice.
- **License (brand):** see [section 5](#5-trademarks). The brand is
  **not** covered by the open-source license of the code.
- **Future legal entity:** The founder may, at his sole discretion,
  transfer governance and IP holdings to a foundation, company, or
  trust formed for this purpose. Any such transfer will be announced
  publicly in advance.

---

## 2. Roles

| Role                | Holder(s)                                  | Filled by                   | Power summary                                              |
|---------------------|--------------------------------------------|------------------------------|------------------------------------------------------------|
| **Founder / BDFL**  | Rasya Andrean                              | N/A (cannot be replaced)     | All powers; final say on everything                        |
| **Core maintainer** | Currently: Rasya Andrean (sole)            | Appointed by the founder     | Can merge to `main`, cut releases, moderate                |
| **Module maintainer** | None yet                                 | Appointed by the founder     | Can merge to a specific subsystem                          |
| **Contributor**     | Anyone who has had a PR merged             | Self-selected via PRs        | All rights granted under DCO + this document               |
| **User**            | Anyone using URUS                          | Self-selected                | Bound by licenses; protected by safe-harbor in `SECURITY.md` |

Roles are not honorary titles. Each carries specific permissions and
specific obligations. Holders can be removed for cause by the founder.

---

## 3. Decision authority

URUS operates under a **BDFL** (Benevolent Dictator For Life) model
during the pre-1.0 cycle. After v1.0 the founder may convert this to a
committee or foundation model; until then:

- **All scope, design, and release decisions** rest with the founder.
- **All hires, partnerships, and external collaborations** under the
  URUS brand are decided by the founder.
- **All disputes between contributors** that cannot be resolved on the
  PR / issue are escalated to the founder, whose decision is final.

Core maintainers may merge PRs that:

- have at least one passing review,
- match the project's coding and documentation standards,
- do not change `LICENSE`, `NOTICE`, `COMMERCIAL.md`, `GOVERNANCE.md`,
  or `SECURITY.md`,
- do not change `docs/spec/SPEC.md` in a backwards-incompatible way,
- do not introduce a new third-party dependency.

Any of the above require **explicit founder approval** in the PR.

---

## 4. Intellectual property ownership

### 4.1 Source code

- **Each contributor retains copyright in their own contributions.**
- Contributions are licensed inbound under **Apache-2.0 OR MIT** via
  the DCO described in [`CONTRIBUTING.md`](./CONTRIBUTING.md).
- The project may redistribute, sublicense, and combine contributions
  under either license at its sole discretion.
- There is **no Contributor License Agreement (CLA)** at this stage.
- There is **no copyright assignment**. Your name stays on your commits.

### 4.2 Aggregate work

- The aggregate URUS codebase — the *selection, arrangement, and
  curation* of contributions — is the project's aggregate copyright,
  exercised by the founder (and any future legal entity).
- This means the project as a whole may be distributed, branded,
  versioned, and commercially packaged as a single work.

### 4.3 Documentation, specifications, and brand assets

- Documentation under `docs/` is licensed under the same dual
  Apache-2.0 OR MIT terms unless an individual file says otherwise.
- The **language specification** (`docs/spec/SPEC.md`) is published
  under the same dual license to maximise downstream interoperability.
- **Brand assets** — the URUS wordmark, the aurochs mark, the logotype,
  color tokens, and any official illustrations — are **not** covered by
  Apache-2.0 / MIT. See [section 5](#5-trademarks).

### 4.4 Patents

- Apache-2.0 includes a patent grant. Contributors who hold patents
  potentially infringed by their contribution grant the project (and
  downstream users) a perpetual, worldwide, royalty-free license to
  practice those claims, per Apache-2.0 § 3.
- The MIT option does not include an explicit patent grant; downstream
  users selecting MIT do so with that risk acknowledged.

### 4.5 No moral-rights waiver implied

Where local law recognises moral rights (e.g. attribution rights under
some EU jurisdictions), nothing in the open-source license or this
governance document waives those rights for you. You retain them.

---

## 5. Trademarks

> [!IMPORTANT]
> The names **URUS**, **urusc**, **tanduk**, the **aurochs mark**, and
> the project's **wordmark / logotype** are **trademarks of the project
> founder, Rasya Andrean.** The open-source license of the code grants
> no rights in these marks.

### 5.1 Permitted nominative use

You may, without permission:

- Use "URUS" to refer to this project in articles, books, talks,
  tutorials, and academic work.
- State factual compatibility ("supports URUS", "compiles URUS code",
  "URUS-compatible").
- Reproduce the wordmark in a screenshot for editorial purposes.

### 5.2 Restricted uses

You may **not**, without prior written permission:

- Distribute a build (binary or source) under the name **"URUS"** if it
  diverges materially from the upstream codebase.
- Use the aurochs mark, the wordmark, or the logotype on a commercial
  product, service, paid course, paid certification, paid conference,
  or paid distribution channel.
- Register domains, package-registry names, or social-media handles
  that suggest official status (`urus-official`, `urus-foundation-*`,
  `officialurus`, etc.).
- Imply project endorsement of your fork, service, company, course, or
  certification.

### 5.3 Enforcement

Trademark violations may be addressed by:

- A request to stop and rename (the default first step).
- DMCA / registrar takedown of squatted assets.
- Legal action in the founder's jurisdiction of choice.

Brand-permission requests: **urusfoundation@gmail.com** (placeholder) /
**Rasya Andrean — `rasyaandrean@outlook.co.id`.**

---

## 6. Commercial licensing & revenue

### 6.1 What the open licenses already allow (free, no contact needed)

Apache-2.0 OR MIT already permits, **without any payment or notification
to the project**:

- Using URUS internally in any organisation, including for-profit.
- Embedding URUS-compiled binaries in commercial products.
- Selling services that use URUS under the hood.
- Forking, modifying, and redistributing the code under the same
  license, with proper attribution.

If your use fits the open-source license, you owe the project nothing
beyond compliance with `LICENSE` and `NOTICE`.

### 6.2 Scenarios that **require** commercial discussion

The following situations are **outside** the scope of the open licenses
and require a separate commercial conversation:

- **Branded commercial distribution.** Shipping a paid product as
  "URUS" or a confusable variant.
- **Trademark use** in any commercial offering ([section 5](#52-restricted-uses)).
- **OEM / SDK bundling** of URUS in a paid console SDK, embedded
  toolchain, or enterprise IDE.
- **Hosted "URUS-as-a-Service"** offerings with URUS branding.
- **Procurement contracts** in government, defense, banking, healthcare,
  or critical-infrastructure sectors that require a named licensor,
  indemnification, or an SLA.
- **Certification programs** ("URUS Certified Developer").

### 6.3 Royalty & revenue-share framework

Where a commercial license is granted, terms may include:

- a **flat license fee** (one-time or annual),
- a **per-seat / per-instance fee**,
- a **revenue share** (typical range 2 – 10 % of attributable revenue,
  case by case),
- an **attribution requirement** (your packaging must mention URUS in a
  named way),
- a **support / SLA bundle** (separately priced),
- **non-exclusive** rights only — multiple commercial licensees can
  coexist.

All commercial royalties flow to the **founder** (or, in the future, to
the formally designated project entity). They do **not** flow to
upstream open-source contributors, because contributors granted their
work under the unconditional open licenses via the DCO. This is the
trade-off open-source projects make in exchange for being open in the
first place.

If the project later adopts a formal sponsorship or grants program for
contributors, that will be announced in writing and bound by its own
terms.

### 6.4 How to start a commercial conversation

Contact:

> **Commercial licensing & partnerships**
> **urusfoundation@gmail.com** (placeholder) /
> **Rasya Andrean — `rasyaandrean@outlook.co.id`**
>
> Include:
> 1. Company / individual + jurisdiction.
> 2. Intended use, distribution channel, expected volume.
> 3. Timeline.
> 4. Specific brand or trademark uses requested.
> 5. Any indemnification / SLA / compliance needs.

The detailed commercial terms live in [`COMMERCIAL.md`](./COMMERCIAL.md).

---

## 7. Contributor rights & obligations

### 7.1 Your rights

- Retain copyright in your contribution.
- Attribution in the commit log.
- Recognition in `THANKS` / `ACKNOWLEDGEMENTS` sections (as added).
- Safe-harbor under [`SECURITY.md`](./SECURITY.md) when reporting
  vulnerabilities.
- Apply for any future paid maintainer or grant role (no guarantee).

### 7.2 Your obligations

- Comply with [`CONTRIBUTING.md`](./CONTRIBUTING.md), the
  [Code of Conduct](./CODE_OF_CONDUCT.md), and this document.
- Follow the [DCO](./CONTRIBUTING.md#licensing-of-your-contributions-the-dco--inbound--outbound).
- Disclose conflicts of interest (employer ownership claims, paid work
  for competitors, etc.) before submitting non-trivial PRs.
- Respect the security disclosure process.
- Not present yourself as a project representative without authority.

### 7.3 What you do **not** get

- A share of commercial revenue or royalties.
- A right to demand the founder accept a PR.
- A right to vote on roadmap, scope, or releases.
- A right to use the brand commercially.

This is not unusual for OSS projects; it is being said plainly so
nobody is surprised later.

---

## 8. Conflict resolution

1. **Self-resolve on the PR / issue.** Most disagreements end here.
2. **Escalate to a core maintainer.** They will moderate.
3. **Escalate to the founder.** Final.

Behavioural disputes go to **urusfoundation@gmail.com** under the
[Code of Conduct](./CODE_OF_CONDUCT.md).

The founder may suspend, ban, or remove any participant for cause,
including: harassment, sustained bad faith, attempts to circumvent
governance, undisclosed conflicts of interest, or violations of the
hard rules in [`CONTRIBUTING.md`](./CONTRIBUTING.md).

---

## 9. Amending this document

- **Cosmetic / clarifying changes** (typos, link fixes, section
  re-ordering) may be merged by a core maintainer.
- **Substantive changes** (anything affecting decision authority, IP
  ownership, royalty terms, trademarks, or contributor rights) require
  the **founder's explicit written approval** in the PR.
- **Notice.** Substantive amendments will be announced in the
  CHANGELOG under the affected release, and the prior version will be
  preserved in `docs/archive/`.

---

## 10. Contact

| Topic                       | Address                                       |
|-----------------------------|------------------------------------------------|
| General governance          | `urusfoundation@gmail.com` (placeholder)       |
| Commercial licensing        | `urusfoundation@gmail.com` (placeholder)       |
| Brand / trademark           | `urusfoundation@gmail.com` (placeholder)            |
| Security issues             | `urusfoundation@gmail.com` (placeholder)         |
| Conduct                     | `urusfoundation@gmail.com` (placeholder)          |
| Founder (always reachable)  | **Rasya Andrean — `rasyaandrean@outlook.co.id`** |

Placeholder addresses point to the founder's inbox until project domain
infrastructure is set up.

---

*Last updated: 2026-06-03 — applies from v0.0.1-b009 onward.*
