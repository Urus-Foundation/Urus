# 04 — Project Layout

A tour of every directory and what lives in it. Read this once and you
will know where to look for anything.

```
Urus/
├── CMakeLists.txt              top-level build entry point
├── LICENSE                     dual Apache-2.0 / MIT
├── README.md                   user-facing landing page
├── CHANGELOG.md                versioned release notes
├── ROADMAP.md                  high-level version targets
├── CONTRIBUTING.md             short-form contributor guide
├── GOVERNANCE.md               project decision-making
├── SECURITY.md                 disclosure policy
│
├── compiler/                   ← THE C COMPILER
│   ├── CMakeLists.txt          build rules for the `urusc` binary
│   ├── include/                public headers (the compiler's "API")
│   │   ├── urus_common.h       shared typedefs, attributes, macros
│   │   ├── arena.h             bump allocator interface
│   │   ├── strbuf.h            growing byte-buffer interface
│   │   ├── diag.h              diagnostic ctx + severity + emit helpers
│   │   ├── lexer.h             token kinds + `Lexer` struct
│   │   ├── parser.h            `Parser` struct + entry point
│   │   ├── ast.h               every AST node enum and payload
│   │   ├── sema.h              semantic analysis entry point
│   │   └── codegen_c.h         codegen entry point
│   └── src/                    implementations (each .c ↔ a .h)
│       ├── arena.c             bump allocator
│       ├── strbuf.c            growing byte buffer
│       ├── diag.c              diagnostic printing
│       ├── lexer.c             tokenizer state machine
│       ├── parser.c            recursive-descent + Pratt parser
│       ├── ast.c               AST node constructors (arena-backed)
│       ├── sema.c              name resolution + checks
│       ├── codegen_c.c         emits C11 source from the AST
│       └── main.c              CLI driver
│
├── stdlib/                     ← LANGUAGE-LEVEL LIBRARIES
│   └── runtime/
│       └── urus_rt.h           header-only C runtime: urus_str,
│                                Result, Option, println, panic, …
│
├── examples/                   ← END-TO-END URUS PROGRAMS
│   ├── hello.urus              println("Hello, Aurochs!")
│   ├── aurochs.urus            struct + impl + match showcase
│   ├── fib.urus                recursion + numeric primitives
│   └── result.urus             Result + `?` + defer demo
│
├── tests/                      ← THE COMPILER'S TEST CORPUS
│   ├── run/                    must compile AND run cleanly
│   │   ├── 01_hello.urus
│   │   ├── 02_arith.urus
│   │   ├── 03_struct.urus
│   │   ├── 04_control_flow.urus
│   │   ├── 05_try_op.urus
│   │   ├── 06_fstring.urus
│   │   └── 07_defer.urus
│   └── fail/                   must produce a specific diagnostic
│       ├── 01_undefined.urus
│       ├── 02_dup_struct.urus
│       └── SEC-01_fstr_injection.urus   ← security regression PoC
│
├── docs/                       ← ALL DOCUMENTATION
│   ├── analysis/
│   │   └── ANALYSIS.md          architecture review + Top 100 roadmap
│   ├── merge/
│   │   └── MERGE-DECISIONS.md   archive → v0.0.1 kept/dropped/deferred
│   ├── review/
│   │   └── REVIEW.md            council-based adoption review
│   ├── security/
│   │   └── SECURITY-AUDIT.md    red-team audit + stop-ship list
│   ├── spec/
│   │   └── SPEC.md              formal language specification
│   └── documentation/           ← YOU ARE HERE
│       ├── 00-INDEX.md           navigation
│       ├── 01-OVERVIEW.md        what URUS is
│       ├── 02-ARCHITECTURE.md    pipeline + data ownership
│       ├── 03-BUILDING.md        build the compiler
│       ├── 04-PROJECT-LAYOUT.md  this file
│       ├── 05-COMPILER-INTERNALS.md   per-module deep dive
│       ├── 06-LANGUAGE-GUIDE.md  tour of URUS the language
│       ├── 07-RUNTIME.md         urus_rt.h internals
│       ├── 08-TESTING.md         test harness
│       ├── 09-CONTRIBUTING-DEEP.md  style + PR flow
│       ├── 10-DESIGN-DECISIONS.md   "why we did it this way"
│       ├── 11-ROADMAP-DETAILED.md   per-version milestones
│       ├── 12-FAQ.md             common questions
│       ├── 13-GLOSSARY.md        terminology
│       └── 14-TROUBLESHOOTING.md  error → cause → fix
│
└── scripts/                    ← BUILD AND TEST HELPERS
    ├── run-tests.ps1            Windows PowerShell test runner
    └── run-tests.sh             POSIX bash test runner
```

## Where things go when you add them

Use this table when you do not know where a new file belongs.

| New thing                                | Goes in                                                |
|------------------------------------------|--------------------------------------------------------|
| A new pass in the compiler                | `compiler/include/<name>.h` + `compiler/src/<name>.c`  |
| A new runtime function                    | `stdlib/runtime/urus_rt.h`                             |
| A new example program                     | `examples/<name>.urus`                                 |
| A new positive test                       | `tests/run/<NN>_<topic>.urus`                          |
| A new negative test                       | `tests/fail/<NN>_<topic>.urus`                         |
| A new short doc                           | `docs/documentation/<NN>-<TOPIC>.md`                   |
| A new specification section               | `docs/spec/SPEC.md` (do not split until v0.1)          |
| A new design discussion                   | `docs/documentation/10-DESIGN-DECISIONS.md`            |
| A security finding                        | `docs/security/SECURITY-AUDIT.md`                      |

## Files you should not edit lightly

- **`stdlib/runtime/urus_rt.h`** — the ABI between the compiler and every
  produced binary. A breaking change here invalidates every prior build.
- **`compiler/include/ast.h`** — the AST shape is depended on by parser,
  sema, and codegen. Adding a payload field is fine; renumbering enums
  is not.
- **`docs/spec/SPEC.md`** — once a behavior is in the spec and shipped,
  changing it is a deprecation cycle, not a patch.

— *Last updated 2026-06-03.*
