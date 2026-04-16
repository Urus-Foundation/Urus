# Security Policy

## Supported Versions

| Version | Status |
|---------|--------|
| 0.3.x | Actively supported |
| 0.2.x | Security fixes only |
| 0.1.x | End of life |

---

## Reporting a Vulnerability

**Do not open a public issue.** Security vulnerabilities must be disclosed privately until a fix is available.

### How to Report

Send a detailed report to the maintainer via GitHub private message or the email listed on the [maintainer's profile](https://github.com/RasyaAndrean).

**Include in your report:**

- Description of the vulnerability
- Steps to reproduce
- Affected version(s)
- Potential impact
- Suggested fix (if any)

### Response Timeline

| Stage | Timeframe |
|-------|-----------|
| Acknowledgment | Within 72 hours |
| Severity assessment | Within 1 week |
| Fix or mitigation | As soon as possible, depending on severity |
| Credit | In changelog and release notes (unless you prefer anonymity) |

---

## Scope

### In Scope

| Area | Examples |
|------|----------|
| **Compiler** | Buffer overflow, crash on crafted input, arbitrary code execution |
| **Runtime** | Memory corruption, bounds check bypass, ref-count manipulation |
| **Generated code** | Codegen producing unsafe C, missing bounds checks |
| **Import system** | Path traversal, unintended file access |
| **HTTP built-ins** | Request injection, unsafe URL handling in `http_get`/`http_post` |
| **Async runtime** | Thread safety issues, race conditions, data races |
| **Package manager** | Dependency confusion, malicious packages, unsafe git clone targets |
| **Closures** | Captured variable lifetime issues, dangling references |
| **Traits / impl** | Incorrect method dispatch, type confusion in trait implementations |

### Out of Scope

- Vulnerabilities in GCC/Clang itself
- Logic bugs in user-written URUS programs
- Denial of service via extremely large input files (known limitation)

---

## Security Design

| Principle | How |
|-----------|-----|
| **Memory safety** | Automatic reference counting with runtime bounds checking |
| **Type safety** | All types verified at compile time, no implicit coercion |
| **No unsafe operations** | No pointer arithmetic or manual memory management in user code (except via `__emit__`) |
| **Immutable by default** | Variables require explicit `mut` for mutation |
| **HTTP access** | `http_get()` / `http_post()` use `curl` — network access is opt-in per call |
| **Raw emit** | `__emit__()` allows inline C code and bypasses all safety checks — use with caution |
| **Thread isolation** | Async functions run on separate threads with isolated stacks; no shared mutable state |
| **Package integrity** | Stdlib modules resolve locally; external dependencies are cloned via git |

For more details, see the [Security Model](./documentation/security/security-model.md).
