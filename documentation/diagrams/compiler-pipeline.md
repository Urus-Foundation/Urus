# Compiler Pipeline Diagrams

Visual walkthroughs of how the URUS compiler transforms source code into a native binary.

## End-to-end compilation

This diagram shows the complete journey of a simple "Hello, World!" program through every compiler stage:

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
 │   │     PREPROCESSOR         │                              │
 │   │  • Resolve imports       │                              │
 │   │  • Expand rune macros    │                              │
 │   └────────────┬─────────────┘                              │
 │                │ merged source                              │
 │                ▼                                            │
 │   ┌──────────────────────────┐                              │
 │   │        LEXER             │                              │
 │   │  "fn" → TOK_FN           │                              │
 │   │  "main" → TOK_IDENT      │                              │
 │   │  "(" → TOK_LPAREN        │                              │
 │   │  ")" → TOK_RPAREN        │                              │
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
 │   │  Pass 1: Register decls  │                              │
 │   │  Pass 2: Type-check      │                              │
 │   │  ✓ "print" exists        │                              │
 │   │  ✓ arg type: str ✓       │                              │
 │   └────────────┬─────────────┘                              │
 │                │ AstNode* (typed)                           │
 │                ▼                                            │
 │   ┌──────────────────────────┐                              │
 │   │       CODEGEN            │                              │
 │   │  // embedded runtime     │                              │
 │   │  void urus_main(void) {  │                              │
 │   │    urus_print_str(       │                              │
 │   │      urus_str_from(      │                              │
 │   │        "Hello!"));       │                              │
 │   │  }                       │                              │
 │   └────────────┬─────────────┘                              │
 │                │ C source code                              │
 │                ▼                                            │
 │   ┌──────────────────────────┐                              │
 │   │       GCC / Clang        │                              │
 │   │  gcc -std=c11 -O2 -lm   │                              │
 │   │      _urus_tmp.c         │                              │
 │   │      -o hello            │                              │
 │   └────────────┬─────────────┘                              │
 │                │                                            │
 │                ▼                                            │
 │            hello(.exe)                                      │
 │                                                             │
 └─────────────────────────────────────────────────────────────┘
```

## Import resolution

When a file contains `import` statements, the preprocessor pulls in the imported declarations before lexing begins:

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
     ├── fn helper() from utils.urus
     ├── fn format() from utils.urus
     └── fn main()   from main.urus
```

If the file isn't found relative to the importing file, the compiler checks `URUSCPATH`. Circular imports are detected and rejected.

## Rune expansion

Runes are expanded during preprocessing by textual substitution:

```
 Definition:
   rune square(x) { x * x }      → stored in rune table

 Usage:
   let n = square!(5);            → expanded to: let n = 5 * 5;

 The expansion happens before parsing, so the parser sees:
   let n = 5 * 5;
```

## Defer code insertion

The code generator inserts defer blocks before every return path and at the end of the function:

```
 Source:                              Generated C (simplified):

 fn process(): void {                void urus_process(void) {
     defer { print("done"); }           urus_print_str(...);  // "start"
     print("start");                    urus_print_str(...);  // "working"
     print("working");                  urus_print_str(...);  // "done" ← defer
 }                                  }

 fn early_return(): void {           void urus_early_return(void) {
     defer { print("cleanup"); }        if (cond) {
     if cond {                              urus_print_str(...);  // "cleanup" ← defer
         return;                            return;
     }                                  }
     print("work");                     urus_print_str(...);  // "work"
 }                                      urus_print_str(...);  // "cleanup" ← defer
                                    }
```

Multiple defers execute in LIFO order (last declared runs first).

## Reference counting

The codegen inserts retain/release calls to manage heap memory automatically:

```
 let a: str = "hello";            a.rc = 1
 let b: str = a;                  a.rc = 2  (retain)
 {
     let c: str = a;              a.rc = 3  (retain)
 }                                a.rc = 2  (release c — scope exit)
 b = "world";                     a.rc = 1  (release old b value)
                                  "world".rc = 1
 // end of scope
 // release a → rc = 0 → free
 // release b → "world" rc = 0 → free
```

## Enum representation in C

Enums with data variants compile to a tagged union:

```
 URUS:                               Generated C:

 enum Shape {                        typedef enum {
     Circle(r: float);                   SHAPE_CIRCLE,
     Rect(w: float, h: float);          SHAPE_RECT,
     Point;                              SHAPE_POINT,
 }                                   } Shape_Tag;

                                     typedef struct {
                                         Shape_Tag tag;
                                         union {
                                             struct { double f0; } Circle;
                                             struct { double f0;
                                                      double f1; } Rect;
                                         };
                                     } Shape;
```

## Tuple representation in C

Each unique tuple signature gets its own typedef and, when it contains heap-allocated fields, a drop function:

```
 URUS:                               Generated C:

 let t: (int, str) = (42, "hi");    typedef struct {
 print(t.0);                            int64_t f0;
 print(t.1);                            urus_str *f1;
                                     } urus_tuple_int_str;

                                     // Auto-generated cleanup:
                                     static void urus_tuple_int_str_drop(
                                         urus_tuple_int_str *t) {
                                         urus_str_release(t->f1);
                                     }
```
