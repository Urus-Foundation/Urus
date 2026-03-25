# Compiler and Runtime API

This document describes the internal C APIs exposed by the URUS compiler and the runtime library. These are relevant if you're embedding the compiler in another tool, extending the runtime, or working on the compiler itself.

The struct layouts and function signatures shown here reflect the public API. Internal implementation details may vary — refer to the source code for exact definitions.

## Compiler C API

### Lexer

```c
// lexer.h
typedef struct {
    const char *source;
    size_t length;
    size_t pos;
    int line;
} Lexer;

void   lexer_init(Lexer *l, const char *source, size_t length);
Token *lexer_tokenize(Lexer *l, int *out_count);
```

| Function | Description |
|----------|-------------|
| `lexer_init` | Initialize lexer from a source string and its length |
| `lexer_tokenize` | Tokenize the entire source, returns an array of Token |

### Parser

```c
// parser.h
typedef struct {
    Token *tokens;
    int count;
    int pos;
} Parser;

void     parser_init(Parser *p, Token *tokens, int count);
AstNode *parse_program(Parser *p);
```

| Function | Description |
|----------|-------------|
| `parser_init` | Initialize parser from a token array |
| `parse_program` | Parse the entire program, returns AST root (NODE_PROGRAM) |

### Semantic Analysis

```c
// sema.h (implementation in Sema/sema.c, Sema/builtins.c, Sema/scope.c)
int sema_analyze(AstNode *program);
```

| Function | Description |
|----------|-------------|
| `sema_analyze` | Type-check the AST. Returns 0 on success, prints error to stderr on failure. |

### Code Generation

```c
// codegen.h
typedef struct {
    char *data;
    size_t len, cap;
    int indent;
    int tmp_counter;
} CodeBuf;

void codegen_init(CodeBuf *buf);
void codegen_free(CodeBuf *buf);
void codegen_generate(CodeBuf *buf, AstNode *program);
```

| Function | Description |
|----------|-------------|
| `codegen_init` | Allocate output buffer |
| `codegen_generate` | Generate C code from AST into buffer |
| `codegen_free` | Free buffer |

### AST

```c
// ast.h
typedef enum {
    TYPE_INT, TYPE_FLOAT, TYPE_BOOL, TYPE_STR, TYPE_VOID,
    TYPE_ARRAY, TYPE_NAMED, TYPE_RESULT, TYPE_FN, TYPE_TUPLE,
} TypeKind;

typedef struct AstType {
    TypeKind kind;
    char *name;              // for TYPE_NAMED
    struct AstType *element; // for TYPE_ARRAY
    struct AstType *ok_type; // for TYPE_RESULT
    struct AstType *err_type;// for TYPE_RESULT
} AstType;
```

#### Type Constructors

| Function | Return | Description |
|----------|--------|-------------|
| `ast_type_simple(TypeKind)` | `AstType*` | Create a primitive type (INT, FLOAT, BOOL, STR, VOID) |
| `ast_type_array(AstType*)` | `AstType*` | Create an array type `[T]` |
| `ast_type_named(char*)` | `AstType*` | Create a named type (struct/enum) |
| `ast_type_result(AstType*, AstType*)` | `AstType*` | Create a `Result<T, E>` |
| `ast_type_tuple(AstType**, int)` | `AstType*` | Create a tuple type `(T1, T2, ...)` |

#### Utility Functions

| Function | Description |
|----------|-------------|
| `ast_type_str(AstType*)` | String representation of a type (for error messages) |
| `ast_type_clone(AstType*)` | Deep clone a type |
| `ast_type_equal(AstType*, AstType*)` | Check equality of two types |
| `ast_new(NodeKind, int line)` | Create a new AST node |
| `ast_print(AstNode*, int indent)` | Debug print an AST tree |

---

## Runtime API (urus_runtime.h)

The URUS runtime library is **header-only** — all functions are defined directly in `runtime/urus_runtime.h` (embedded into the compiler binary at build time).

### String Type

```c
typedef struct {
    int rc;        // reference count
    size_t len;    // string length
    char data[];   // flexible array member (null-terminated)
} urus_str;
```

### String Operations

| Function | Signature | Description |
|----------|-----------|-------------|
| `urus_str_new` | `(const char *s, size_t len) → urus_str*` | Create string from buffer + length |
| `urus_str_from` | `(const char *s) → urus_str*` | Create string from C string |
| `urus_str_concat` | `(urus_str*, urus_str*) → urus_str*` | Concatenate two strings |
| `urus_str_len` | `(urus_str*) → int64_t` | String length |
| `urus_str_slice` | `(urus_str*, int64_t start, int64_t end) → urus_str*` | Substring |
| `urus_str_find` | `(urus_str*, urus_str*) → int64_t` | Find position (-1 if not found) |
| `urus_str_contains` | `(urus_str*, urus_str*) → bool` | Check if contains substring |
| `urus_str_upper` | `(urus_str*) → urus_str*` | Uppercase |
| `urus_str_lower` | `(urus_str*) → urus_str*` | Lowercase |
| `urus_str_trim` | `(urus_str*) → urus_str*` | Trim whitespace |
| `urus_str_replace` | `(urus_str*, urus_str* old, urus_str* new) → urus_str*` | Replace all occurrences |
| `urus_str_starts_with` | `(urus_str*, urus_str*) → bool` | Check prefix |
| `urus_str_ends_with` | `(urus_str*, urus_str*) → bool` | Check suffix |
| `urus_str_split` | `(urus_str*, urus_str* delim) → urus_array*` | Split into string array |
| `urus_char_at` | `(urus_str*, int64_t) → urus_str*` | Character at index |

### String Memory Management

| Function | Description |
|----------|-------------|
| `urus_str_retain(urus_str*)` | Increment refcount |
| `urus_str_release(urus_str*)` | Decrement refcount, free if 0 |

### Array Type

```c
typedef struct {
    int rc;           // reference count
    size_t len;       // number of elements
    size_t cap;       // capacity
    size_t elem_size; // size per element
    void *data;       // data buffer
} urus_array;
```

### Array Operations

| Function | Signature | Description |
|----------|-----------|-------------|
| `urus_array_new` | `(size_t elem_size, size_t initial_cap) → urus_array*` | Create new array |
| `urus_array_push` | `(urus_array*, const void *elem) → void` | Add element |
| `urus_array_get_int` | `(urus_array*, size_t index) → int64_t` | Get int element |
| `urus_array_get_float` | `(urus_array*, size_t index) → double` | Get float element |
| `urus_array_get_bool` | `(urus_array*, size_t index) → bool` | Get bool element |
| `urus_array_get_str` | `(urus_array*, size_t index) → urus_str*` | Get string element |
| `urus_array_get_ptr` | `(urus_array*, size_t index) → void*` | Get pointer element |
| `urus_array_set` | `(urus_array*, size_t index, const void *elem) → void` | Set element at index |
| `urus_len` | `(urus_array*) → int64_t` | Array length |
| `urus_pop` | `(urus_array*) → void` | Remove last element |

### Array Memory Management

| Function | Description |
|----------|-------------|
| `urus_array_retain(urus_array*)` | Increment refcount |
| `urus_array_release(urus_array*)` | Decrement refcount, free if 0 |

### Result Type

```c
typedef union {
    int64_t as_int;
    double as_float;
    bool as_bool;
    void *as_ptr;
} urus_box;

typedef struct {
    int rc;
    int tag;  // 0 = Ok, 1 = Err
    union {
        urus_box ok;
        urus_str *err;
    } data;
} urus_result;
```

### Result Operations

| Function | Signature | Description |
|----------|-----------|-------------|
| `urus_result_ok` | `(urus_box*) → urus_result*` | Create Ok result |
| `urus_result_err` | `(urus_str*) → urus_result*` | Create Err result |
| `urus_result_is_ok` | `(urus_result*) → bool` | Check if Ok |
| `urus_result_is_err` | `(urus_result*) → bool` | Check if Err |
| `urus_result_unwrap` | `(urus_result*) → int64_t` | Get Ok int (aborts on Err) |
| `urus_result_unwrap_float` | `(urus_result*) → double` | Get Ok float |
| `urus_result_unwrap_bool` | `(urus_result*) → bool` | Get Ok bool |
| `urus_result_unwrap_str` | `(urus_result*) → urus_str*` | Get Ok string |
| `urus_result_unwrap_ptr` | `(urus_result*) → void*` | Get Ok pointer |
| `urus_result_unwrap_err` | `(urus_result*) → urus_str*` | Get Err message (aborts on Ok) |

### Conversion Macros

```c
// C11 _Generic macros — dispatch based on argument type
#define to_str(x)   _Generic((x), int64_t: urus_int_to_str, double: urus_float_to_str, bool: urus_bool_to_str, urus_str*: urus_str_to_str)(x)
#define to_int(x)   _Generic((x), urus_str*: urus_str_to_int, double: urus_float_to_int, int64_t: urus_int_to_int)(x)
#define to_float(x) _Generic((x), urus_str*: urus_str_to_float, int64_t: urus_int_to_float, double: urus_float_to_float)(x)
```

### Print

```c
// _Generic macro — dispatch based on argument type
#define urus_print(x) _Generic((x), urus_str*: urus_print_str, int64_t: urus_print_int, double: urus_print_float, bool: urus_print_bool)(x)
```

| Function | Description |
|----------|-------------|
| `urus_print_str(urus_str*)` | Print string + newline |
| `urus_print_int(int64_t)` | Print integer + newline |
| `urus_print_float(double)` | Print float + newline (format `%g`) |
| `urus_print_bool(bool)` | Print "true"/"false" + newline |

### Math

| Function | Signature | Description |
|----------|-----------|-------------|
| `urus_abs` | `(int64_t) → int64_t` | Absolute value integer |
| `urus_fabs` | `(double) → double` | Absolute value float |
| `urus_sqrt` | `(double) → double` | Square root |
| `urus_pow` | `(double, double) → double` | Power (x^y) |
| `urus_min` | `(int64_t, int64_t) → int64_t` | Minimum integer |
| `urus_max` | `(int64_t, int64_t) → int64_t` | Maximum integer |
| `urus_fmin` | `(double, double) → double` | Minimum float |
| `urus_fmax` | `(double, double) → double` | Maximum float |

### I/O

| Function | Signature | Description |
|----------|-----------|-------------|
| `urus_input` | `() → urus_str*` | Read one line from stdin |
| `urus_read_file` | `(urus_str* path) → urus_str*` | Read file as string |
| `urus_write_file` | `(urus_str* path, urus_str* content) → void` | Write string to file |
| `urus_append_file` | `(urus_str* path, urus_str* content) → void` | Append string to file |

### HTTP

| Function | Signature | Description |
|----------|-----------|-------------|
| `urus_http_get` | `(urus_str* url) → urus_str*` | HTTP GET via curl, returns response body |
| `urus_http_post` | `(urus_str* url, urus_str* body) → urus_str*` | HTTP POST via curl, returns response body |

> Requires `curl` to be available on the system.

### Misc

| Function | Signature | Description |
|----------|-----------|-------------|
| `urus_exit` | `(int64_t code) → void` | Exit program with exit code |
| `urus_assert` | `(bool cond, urus_str* msg) → void` | Abort with message if false |

---

## Error Handling

Compiler errors are printed to stderr with the format:
```
<filename>:<line>: error: <message>
```

Runtime errors (bounds check, unwrap failure) use `fprintf(stderr, ...)` and `exit(1)`.
