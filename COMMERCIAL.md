# Commercial Use & Licensing — URUS

This document is the **binding commercial-licensing policy** for the
URUS project. It complements the open-source license (`LICENSE`),
the governance document (`GOVERNANCE.md`), and the contribution policy
(`CONTRIBUTING.md`).

If you are a hobbyist, student, researcher, or open-source developer
using URUS under Apache-2.0 OR MIT and complying with the license
text — **you do not need this document**. It exists for situations
where the open license is *not enough* for what you want to do, and a
separate agreement with the project founder is required.

---

## Table of contents

- [TL;DR](#tldr)
- [What the open licenses already allow](#what-the-open-licenses-already-allow)
- [What requires a commercial license](#what-requires-a-commercial-license)
- [How commercial licensing works](#how-commercial-licensing-works)
- [Pricing model](#pricing-model)
- [Attribution & branding obligations](#attribution--branding-obligations)
- [Royalty & revenue-share framework](#royalty--revenue-share-framework)
- [Compliance & audit](#compliance--audit)
- [Indemnification & SLA](#indemnification--sla)
- [Contributor implications](#contributor-implications)
- [Contact](#contact)

---

## TL;DR

> 💡 **Using URUS in your product = free under Apache-2.0 / MIT.**
> **Using the URUS *name* or *aurochs mark* commercially = requires a
> commercial agreement and may carry a royalty.**
> **Distributing a branded URUS fork = requires a commercial agreement.**
> **Regulated-sector procurement (gov / banking / health) = requires a
> commercial agreement.**

When in doubt, email **urusfoundation@gmail.com** (placeholder) or
**Rasya Andrean — `rasyaandrean@outlook.co.id`**.

---

## What the open licenses already allow

Apache-2.0 OR MIT — at the recipient's option — already allows the
following without payment or notification:

1. **Internal use** of URUS inside any organisation, commercial or
   non-profit.
2. **Embedding URUS-compiled binaries** in commercial products.
3. **Selling services** built on top of URUS (consulting, hosting,
   training, support).
4. **Forking, modifying, and redistributing** the source under the same
   license, *with attribution preserved*.
5. **Combining with proprietary code** as long as the URUS source you
   redistribute remains licensed under Apache-2.0 / MIT (MIT permits
   bundling more freely; Apache-2.0 requires attribution per § 4).

If your use fits one of those buckets and you comply with `LICENSE`
and `NOTICE`, you owe the project **nothing**. Build cool things.

---

## What requires a commercial license

These cases go beyond what Apache-2.0 / MIT grants and require a
separate written agreement:

### 1. Branded commercial distribution

You want to ship a fork, distribution, IDE, cloud build service, or
SDK **under the name "URUS"** or any name confusable with it
(e.g. "URUS Pro", "URUSCloud", "URUSCertified", "Urusc-Enterprise").

Trademark rights are governed by [GOVERNANCE.md § 5](./GOVERNANCE.md#5-trademarks).
Apache-2.0 / MIT explicitly do **not** grant trademark rights.

### 2. OEM / SDK bundling

You want to ship URUS inside a paid console SDK, embedded toolchain,
OEM development environment, or commercial IDE that you sell or
distribute under your own brand.

### 3. Hosted "URUS-as-a-Service"

You want to operate a paid online build service, REPL, sandbox,
educational platform, or hosted package registry **branded as URUS**.

### 4. Regulated-sector procurement

You are a government agency, defense contractor, bank, healthcare
provider, or critical-infrastructure operator that requires:

- a named legal licensor on the agreement,
- indemnification clauses,
- a written SLA,
- export-control compliance attestations,
- FIPS / Common Criteria evaluation paperwork.

Open-source projects cannot provide those by default; a commercial
agreement can.

### 5. Certification & paid training

You want to operate a paid certification program ("URUS Certified
Developer"), paid course ("URUS Bootcamp"), or paid conference
("URUSConf") that uses URUS brand assets.

### 6. White-label resale

You want to remove URUS attribution and resell the technology under a
different brand.

### 7. Patent / IP indemnification

You want a written warranty against patent claims related to URUS code.
The default Apache-2.0 patent grant is offered as-is without warranty;
indemnification is a paid add-on.

### 8. Premium support & priority security access

You want guaranteed response times, advance notice of security
advisories, or a private patch channel.

---

## How commercial licensing works

```
You ────► urusfoundation@gmail.com (or rasyaandrean@outlook.co.id)
              │
              ▼
   ┌─────────────────────────────────────────┐
   │ 1. Scoping call (free, ≤30 min)          │
   │ 2. Project founder reviews your case     │
   │ 3. Written term sheet                    │
   │ 4. Negotiation                           │
   │ 5. Executed agreement                    │
   │ 6. Optional: trademark license bundle    │
   │ 7. Optional: SLA / support bundle        │
   └─────────────────────────────────────────┘
              │
              ▼
        Commercial license active
```

All commercial agreements are:

- **Non-exclusive** — multiple licensees can coexist.
- **Per-jurisdiction** — governed by a law specified in the agreement.
- **Time-bounded** — renew or terminate per the term.
- **Revocable for cause** — non-payment, brand misuse, breach of
  attribution.

---

## Pricing model

Pricing is **case-by-case** during the pre-1.0 phase. Typical structures:

| Structure                  | When it applies                                     | Indicative range *(case by case)*           |
|----------------------------|-----------------------------------------------------|---------------------------------------------|
| One-time fee               | Small fork rename, single product launch            | **USD 1 000 – 10 000**                     |
| Annual flat fee            | Branded distribution, hosted service                | **USD 5 000 – 50 000 / year**              |
| Per-seat / per-instance    | OEM SDK, enterprise IDE                             | **USD 5 – 500 / seat / year**              |
| Revenue share              | Paid platforms, certification programs              | **2 % – 10 %** of attributable revenue      |
| Custom (gov / regulated)   | Procurement contracts with SLAs                     | Quote                                       |

> [!NOTE]
> The numbers above are **indicative** and have **no contractual
> weight** until a signed agreement says otherwise. They exist so you
> can budget. The actual quote depends on scope, volume, jurisdiction,
> and your specific brand usage.

Volume discounts, startup discounts, and educational waivers are
available. Ask.

---

## Attribution & branding obligations

Even with a commercial license, the following typically apply:

- Your packaging must **state that it is built on URUS**, using a
  specific attribution string agreed in the contract (default:
  *"Powered by URUS — © Rasya Andrean and the URUS project
  contributors, used under license."*).
- The aurochs mark, when displayed, must follow the brand guidelines
  (proportions, clear space, approved colors).
- Any modifications you make to URUS code **and redistribute** are
  still governed by Apache-2.0 / MIT (the open license never goes away).
- You must not imply project endorsement of unrelated products or services.
- You must not falsely claim official status beyond what the contract
  grants.

---

## Royalty & revenue-share framework

When a deal includes a revenue-share component, the standard terms are:

- **Attributable revenue** = gross revenue from the licensed product
  or service line, net of refunds and chargebacks.
- **Reporting cadence** = quarterly, in the licensee's local currency.
- **Payment cadence** = within **30 days** of each quarter close.
- **Audit right** = the project (or its representative) may audit
  attributable-revenue calculations once per year on **30 days' notice**.
- **Floor & ceiling** = a minimum annual amount and/or a cap may be
  negotiated.
- **Pass-through** = a licensee may not deduct downstream royalties from
  their own customers from the URUS royalty base.

All royalty payments are received by the **founder** (or the formally
designated project entity in the future). They are **not** distributed
to upstream open-source contributors automatically — see
[Contributor implications](#contributor-implications).

---

## Compliance & audit

Commercial licensees agree to:

- maintain **records sufficient to verify** revenue-share calculations
  for at least **3 years**,
- cooperate with periodic **brand-usage reviews**,
- promptly **notify** the project of any security incident affecting
  URUS-branded surface area,
- promptly **pull** any disputed brand asset on request.

Non-compliance is grounds for termination of the commercial license
without refund.

---

## Indemnification & SLA

Default open-source distribution is **"as-is" with no warranty**, per
the disclaimer in `LICENSE`.

Commercial agreements may add:

- **Indemnification** against third-party IP claims arising from URUS
  source code as distributed (with carve-outs for modifications you
  made).
- **Security SLA** — guaranteed acknowledgment + fix times for issues
  affecting the licensed deployment.
- **Support SLA** — response times for non-security issues.
- **Roadmap influence** — a structured channel to request roadmap
  consideration (no veto, no exclusivity).

Premium tiers are priced separately and require evidence that the
deployment is materially valuable (production traffic, headcount,
revenue), to filter casual asks.

---

## Contributor implications

This section is here so contributors are not surprised.

- **By contributing under the DCO**, you grant the project an
  unconditional license to redistribute your work — including in
  commercial agreements that the founder enters into.
- Commercial revenue flows to the **founder** (or future formal entity),
  not to individual upstream contributors.
- Contributors **retain copyright** in their own contributions and may
  separately license them however they wish (e.g. for their own
  consultancy).
- Future formal **sponsorship**, **paid maintainer**, or **grants**
  programs are separate from upstream contributions and will be
  announced explicitly when they exist.
- Contributors are encouraged to **ask** before doing speculative work
  in the hope of a commercial deal — those conversations should happen
  in advance.

This is standard for OSS projects with a single steward. It is being
said plainly to keep expectations clear.

---

## Contact

| Topic                                | Address                                             |
|--------------------------------------|------------------------------------------------------|
| Commercial license (primary)         | `urusfoundation@gmail.com` (placeholder)             |
| Founder (always reachable)           | **Rasya Andrean — `rasyaandrean@outlook.co.id`**     |
| Brand / trademark permission         | `urusfoundation@gmail.com` (placeholder)                  |
| Security SLA / support               | `urusfoundation@gmail.com` (placeholder)               |
| Governance questions                 | `urusfoundation@gmail.com` (placeholder)             |

When you reach out, please include:

1. **Who you are** — company name, jurisdiction, primary contact.
2. **What you want to do** — specific use case, brand usage,
   distribution channel.
3. **Scale** — expected number of users, seats, instances, or revenue.
4. **Timeline** — when you need a decision.
5. **Constraints** — required SLAs, indemnification, regulatory regime.

A reply typically arrives **within 7 business days**.

---

*Last updated: 2026-06-03 — applies from v0.0.1-b008 onward.*
