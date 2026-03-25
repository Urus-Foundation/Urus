# URUS Documentation

Welcome to the official documentation for the URUS programming language, version 0.3.x.

URUS is a statically-typed language that transpiles to standard C11. It brings modern language features — pattern matching, Result types, string interpolation, tuples, macros — to a simple, learnable syntax that compiles down to fast native binaries.

## Documentation Map

| Document | What you'll find |
|----------|-----------------|
| [Overview](./overview/overview.md) | What URUS is, who it's for, and how it works at a high level |
| [Installation Guide](./installation/installation-guide.md) | Setting up the toolchain on Windows, Linux, macOS, and Termux |
| [Language Guide](./usage/language-guide.md) | The full language tour — syntax, types, control flow, and examples |
| [Compiler Configuration](./configuration/compiler-configuration.md) | CLI flags, environment variables, and build options |
| [Compiler Architecture](./architecture/compiler-architecture.md) | How the compiler is structured internally |
| [Compiler & Runtime API](./api-reference/compiler-and-runtime-api.md) | C-level API for the compiler and the runtime library |
| [Contributor Guide](./development-guide/contributor-guide.md) | How to build, test, and contribute code |
| [Security Model](./security/security-model.md) | Memory safety, threat model, and known limitations |
| [Project Roadmap](./roadmap/project-roadmap.md) | What's been released and what's coming next |
| [Version History](./changelog/version-history.md) | Per-release changelog summary |
| [Compiler Pipeline Diagram](./diagrams/compiler-pipeline.md) | Visual walkthrough of the compilation stages |
| [Architecture Decisions](./decisions/) | ADRs explaining key design choices |

## Quick Links

- [Language Specification](../SPEC.md) — formal grammar and type rules
- [Contributing](../CONTRIBUTING.md) — how to submit patches and PRs
- [Security Policy](../SECURITY.md) — how to report vulnerabilities
- [License](../LICENSE) — Apache 2.0

## Getting Started in 60 Seconds

```bash
git clone https://github.com/Urus-Foundation/Urus.git
cd Urus/compiler
cmake -S . -B build
cmake --build build
```

Write a program:

```urus
fn main(): void {
    let name = "World";
    print(f"Hello, {name}!");
}
```

Compile and run:

```bash
./build/urusc hello.urus -o hello    # Linux/macOS
./build/Debug/urusc hello.urus -o hello  # Windows
./hello
```

For the full setup walkthrough, head to the [Installation Guide](./installation/installation-guide.md).

## Community

- **Source:** https://github.com/Urus-Foundation/Urus
- **Issues:** https://github.com/Urus-Foundation/Urus/issues
