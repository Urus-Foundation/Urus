# Security Policy

The URUS project takes security seriously. This document is the
**binding** policy for reporting, handling, and disclosing
vulnerabilities in any URUS-owned codebase (compiler, runtime,
standard library, tooling).

If you are looking for a *technical* audit of the current code, see
[`docs/security/SECURITY-AUDIT.md`](./docs/security/SECURITY-AUDIT.md).
That document lists 10 publicly-known stop-ship findings in v0.0.1.

---

## Table of contents

- [Status & supported versions](#status--supported-versions)
- [Reporting a vulnerability](#reporting-a-vulnerability)
- [Our commitments](#our-commitments)
- [Disclosure timeline](#disclosure-timeline)
- [Safe harbor](#safe-harbor)
- [Out of scope](#out-of-scope)
- [Bug bounty](#bug-bounty)
- [Hall of fame](#hall-of-fame)
- [Cryptographic verification of advisories](#cryptographic-verification-of-advisories)
- [Pre-alpha trust posture](#pre-alpha-trust-posture)
- [Contact](#contact)

---

## Status & supported versions

| Version  | Status     | Receives security fixes | Notes                                                      |
|----------|------------|--------------------------|------------------------------------------------------------|
| `v0.0.1` | Pre-alpha  | ❌ — superseded by v0.0.2 when shipped | Use only on input you fully trust.                          |
| `v0.0.2` | Planned    | ✅ — stop-ship release   | Closes the 10 findings in the v0.0.1 audit.                 |
| Older    | n/a        | ❌                       | No releases predate v0.0.1.                                 |

> [!CAUTION]
> **URUS v0.0.1 ships with 10 publicly-documented critical / high
> severity issues.** This is intentional and honest: the project is
> pre-alpha, the issues are catalogued openly so users can make informed
> decisions, and v0.0.2 will close them. Do not run `urusc` on `.urus`
> files you did not write or do not fully trust.

---

## Reporting a vulnerability

> [!IMPORTANT]
> **Do not file public GitHub issues or discussions for security
> vulnerabilities.** Public disclosure before a patch lands harms users.

### Where to send reports

- **Primary:** `urusfoundation@gmail.com` *(placeholder — forwards to the
  founder until project infrastructure is set up)*.
- **Backup:** **Rasya Andrean — `rasyaandrean@outlook.co.id`**.
- **PGP / age:** a public key will be published at
  `https://urus-lang.dev/.well-known/security.txt` once the domain is
  live. Until then, you may request a key out-of-band before sending
  sensitive details.

### What to include

1. **Affected version + build** (e.g. `urusc --version` → `v0.0.1-b009`).
2. **Affected component** (lexer, parser, sema, codegen, runtime,
   tooling, registry).
3. **Reproducer.** Smallest possible `.urus` file, command line, host
   OS, host C compiler.
4. **Impact.** What an attacker can do. Be specific (RCE, info disclosure,
   DoS, sandbox escape, supply-chain).
5. **Suggested fix.** Optional but appreciated.
6. **Your preferred attribution** (name, handle, anonymous, "do not
   acknowledge").

### What you should *not* do

- Do not exploit a vulnerability against systems you do not own.
- Do not exfiltrate data beyond the minimum needed to demonstrate the
  finding.
- Do not publicly disclose the issue before the agreed window closes.
- Do not blackmail, threaten, or demand payment (this voids safe
  harbor).
- Do not test on production infrastructure of any third party hosting
  URUS-based services.

---

## Our commitments

When you report a vulnerability following this policy, **we commit to:**

| Commitment                                       | Target time      |
|--------------------------------------------------|------------------|
| Acknowledge receipt                              | within **7 days**  |
| Provide a triage decision (accept / reject / dup)| within **14 days** |
| Provide a fix ETA for accepted reports           | within **30 days** |
| Coordinate a disclosure date                     | by **day 60**      |
| Public disclosure window                         | by **day 90**      |

For findings of severity **Critical**, we will treat the case as
out-of-band: faster acknowledgment, faster fix, possible emergency
release.

---

## Disclosure timeline

We follow a **90-day coordinated disclosure** model.

```
Day 0    — Report received and acknowledged
Day 7    — Triage complete: accepted / rejected / duplicate
Day 14   — Reporter is informed of accepted status and severity
Day 30   — Fix prepared, tested, gated for release
Day 60   — Reporter and project agree on disclosure date
Day 90   — Public advisory + patch released simultaneously
```

Exceptions:

- **Active exploitation in the wild** → expedited fix and earlier
  public advisory.
- **Coordination with third-party libraries** we depend on → extension
  may be requested, never silently taken.
- **Reporter's preference** for early or late disclosure is considered
  but does not override the 90-day cap.

Once disclosed, the advisory is published at:

- `docs/security/advisories/URUS-<YYYY>-<NNNN>.md`
- The release `CHANGELOG.md` entry tagged `Security`
- The relevant build entry in `docs/archive/`

---

## Safe harbor

If you report a vulnerability in good faith, follow this policy, and
abide by the rules above, **we will not pursue legal action against
you**, including but not limited to:

- the Computer Fraud and Abuse Act (US),
- the Computer Misuse Act (UK),
- analogous statutes in any jurisdiction where the project asserts
  rights,
- any contractual claim that would otherwise apply.

We expressly authorise the following research activities on our own
infrastructure and codebases:

- Reverse engineering of `urusc`.
- Fuzzing the compiler with random or grammar-aware input.
- Building proof-of-concept exploits in a sandboxed environment.
- Compiling and running attacker-controlled `.urus` files **on
  systems you own or have permission to test**.

What this does **not** cover:

- Attacks on third-party services that host URUS code.
- Attacks on individual project members.
- Data exfiltration beyond the minimum required to demonstrate impact.
- Public disclosure that breaks the disclosure window.

When in doubt, ask before you act. We respond fast to good-faith
research.

---

## Out of scope

The following are not considered vulnerabilities for the purposes of
this policy:

- Issues already documented in
  [`docs/security/SECURITY-AUDIT.md`](./docs/security/SECURITY-AUDIT.md)
  (you can still report them — they are tracked but already known).
- Theoretical issues without a demonstrable impact.
- Crashes in the compiler caused by deliberately malformed input
  **that produces a diagnostic**. (A compiler crash without a
  diagnostic — yes, that's a vuln.)
- Findings that require the user to bypass an existing security
  warning the compiler emits.
- Best-practice / hardening suggestions that are not exploitable.
  Open a regular issue.
- Vulnerabilities in third-party C compilers, OSes, or hardware that
  URUS uses or runs on. Report those to their respective projects.
- Social-engineering attacks against maintainers.

---

## Bug bounty

URUS does **not** currently operate a paid bug bounty program.

We may stand one up around the v0.1.0 timeframe, alongside the
package-manager rollout. Until then, accepted critical findings will
receive:

- a written **acknowledgment** in the next release's advisory,
- a **listing in the Hall of Fame** below (your name or handle, opt-in),
- a **personal thank you** from the founder.

If you require a paid disclosure channel, please contact the founder
directly to discuss; ad-hoc arrangements are possible for
exceptionally severe findings.

---

## Hall of fame

Researchers who have responsibly disclosed vulnerabilities to URUS:

> *No external researchers yet. The 10 v0.0.1 findings were authored
> by the project's internal red-team audit. Contributions to this list
> are very welcome.*

---

## Cryptographic verification of advisories

Once the project domain is live, security advisories will be:

- signed with the project's **age** or **minisign** public key,
- mirrored on the project's transparency log,
- announced via the security mailing list.

Until then, advisories will be published as commits to the `main`
branch of this repository, signed by the founder's GitHub-verified key
when possible.

---

## Pre-alpha trust posture

This is the **most important section of this document for current
users**.

URUS v0.0.1 is **pre-alpha software**. It contains known stop-ship
issues, no CI, no fuzz harness, no sandboxing, and no formal
verification. The trust posture you should adopt:

1. **Do not compile untrusted `.urus` files.** A malicious file can land
   arbitrary C in the produced binary (F-MEM-1) and your host C compiler
   will compile it.
2. **Do not run `urusc` against pipe input from network sources** (CI
   artifacts, untrusted PR payloads, …) until v0.0.2.
3. **Do not embed URUS in a multi-tenant build service** until v0.1.
4. **Sandbox aggressively** if you must run on untrusted input today
   (Linux namespaces / Bubblewrap, macOS sandbox-exec, Windows
   AppContainer, ephemeral VMs).

We will lift these warnings step by step as the Tier-0, Tier-1, and
Tier-2 hardening items from the security audit ship.

---

## Contact

| Topic                              | Address                                             |
|------------------------------------|------------------------------------------------------|
| Security report (primary)          | `urusfoundation@gmail.com` (placeholder)               |
| Security report (backup / founder) | **Rasya Andrean — `rasyaandrean@outlook.co.id`**     |
| General policy questions           | `urusfoundation@gmail.com` (placeholder)             |
| Commercial security support / SLA  | `urusfoundation@gmail.com` (placeholder)             |

---

*Last updated: 2026-06-03 — applies from v0.0.1-b009 onward.*
