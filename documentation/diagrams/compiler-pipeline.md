# Compiler Pipeline Diagram

## High-Level Flow

```
 ┌─────────────────────────────────────────────────────────────┐
 │                                                             │
 │   hello.urus                                                │
 │   ┌──────────────────────────┐                              │
 │   │ fn main(): void {        │                              │
 │   │     print("Hello!");     │                              │
 │   │ }                        │                              │
 │   └────────────┬─────────────┘                              │
 │                │                                            │
 │                ▼                                            │
 │   ┌──────────────────────────┐                              │
 │   │        LEXER             │                              │
 │   │  "fn" → TOK_FN           │                              │
 │   │  "main" → TOK_IDENT      │                              │
 │   │  "(" → TOK_LPAREN        │                              │
 │   │  ...                     │                              │
 │   └────────────┬─────────────┘                              │
 │                │ Token[]                                    │
 │                ▼                                            │
 │   ┌──────────────────────────┐                              │
 │   │        PARSER            │                              │
 │   │  NODE_PROGRAM            │                              │
 │   │   └─ NODE_FN_DECL "main" │                              │
 │   │       └─ NODE_BLOCK      │                              │
 │   │           └─ NODE_CALL   │                              │
 │   │               "print"    │                              │
 │   └────────────┬─────────────┘                              │
 │                │ AstNode*                                   │
 │                ▼                                            │
 │   ┌──────────────────────────┐                              │
 │   │        SEMA              │                              │
 │   │  Pass 1: Register fns    │                              │
 │   │  Pass 2: Type-check      │                              │
 │   │  ✓ "print" exists        │                              │
 │   │  ✓ arg type: str ✓       │                              │
 │   └────────────┬─────────────┘                              │
 │                │ AstNode* (typed)                           │
 │                ▼                                            │
 │   ┌──────────────────────────┐                              │
 │   │       CODEGEN            │                              │
 │   │  #include "urus_runtime.h"                              │
 │   │  void urus_main(void) {  │                              │
 │   │    urus_print_str(       │                              │
 │   │      urus_str_new(       │                              │
 │   │        "Hello!"));       │                              │
 │   │  }                       │                              │
 │   └────────────┬─────────────┘                              │
 │                │ C source                                   │
 │                ▼                                            │
 │   ┌──────────────────────────┐                              │
 │   │         GCC              │                              │
 │   │  gcc -std=c11 -o hello   │                              │
 │   │      _urus_tmp.c -lm     │                              │
 │   └────────────┬─────────────┘                              │
 │                │                                            │
 │                ▼                                            │
 │            hello.exe                                        │
 │                                                             │
 └─────────────────────────────────────────────────────────────┘
```

## Import Resolution Flow

```
 main.urus ──import "utils.urus"──▶ utils.urus
     │                                  │
     │                             lex + parse
     │                                  │
     │         ◀── merge declarations ──┘
     │
     ▼
Combined AST
     │
     ├── fn from utils.urus
     ├── fn from utils.urus
     └── fn main() from main.urus
```

## Memory Management (Ref-counting)

```
let a: str = "hello";                         a.refcount = 1
let b: str = a;                               a.refcount = 2  (retain)
{
    let c: str = a;                           a.refcount = 3  (retain)
}                                             a.refcount = 2  (release c)
b = "world";                                  a.refcount = 1  (release old b)
                                              "world".refcount = 1
// end of scope
// release a → refcount = 0 → FREE
// release b → "world" refcount = 0 → FREE
```

## Enum / Tagged Union in C

```
URUS:                                          Generated C:

enum Shape {                                   typedef enum {
    Circle(r: float);                              SHAPE_CIRCLE,
    Rect(w: float, h: float);                     SHAPE_RECT,
    Point;                                         SHAPE_POINT,
}                                              } Shape_Tag;

                                               typedef struct {
                                                   Shape_Tag tag;
                                                   union {
                                                       struct { double f0; } Circle;
                                                       struct { double f0; double f1; } Rect;
                                                   };
                                               } Shape;
```
