# Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| 0.3.x   | Yes (current) |
| 0.2.x   | Security fixes only |
| 0.1     | No |

## Reporting a Vulnerability

If you discover a security vulnerability in the URUS compiler or runtime, please report it responsibly.

### How to Report

1. **Do not open a public issue.** Security vulnerabilities should not be disclosed publicly until a fix is available.
2. **Email:** Send a detailed report to the maintainer via GitHub private message or the email listed on the [maintainer's profile](https://github.com/RasyaAndrean).
3. **Include:**
   - Description of the vulnerability
   - Steps to reproduce
   - Affected version(s)
   - Potential impact
   - Suggested fix (if any)

### What to Expect

- **Acknowledgment** within 72 hours of your report
- **Assessment** of severity and impact within 1 week
- **Fix or mitigation** as soon as possible, depending on severity
- **Credit** in the changelog and release notes (unless you prefer anonymity)

## Scope

The following are in scope for security reports:

| Area | Examples |
|------|----------|
| **Compiler** | Buffer overflow, crash on crafted input, arbitrary code execution |
| **Runtime** | Memory corruption, bounds check bypass, ref-count manipulation |
| **Generated code** | Codegen producing unsafe C, missing bounds checks |
| **Import system** | Path traversal, unintended file access |
| **HTTP built-ins** | Request injection, unsafe URL handling in `http_get`/`http_post` |

The following are **out of scope**:

- Vulnerabilities in GCC/Clang itself
- Issues in user-written URUS programs (e.g., logic bugs)
- Denial of service via extremely large input files (known limitation)

## Security Design

### Key Points

- **Memory safety:** Automatic reference counting with runtime bounds checking
- **Type safety:** All types verified at compile time, no implicit coercion
- **No unsafe operations:** No pointer arithmetic, no manual memory management in user code (except via `__emit__`)
- **Immutable by default:** Variables require explicit `mut` for mutation
- **HTTP access:** `http_get()` and `http_post()` built-ins use `curl` — network access is opt-in per function call
- **Raw emit:** `__emit__()` allows inline C code and bypasses all safety checks — use with caution

For more details, see the [Security Model](./documentation/security/security-model.md).
