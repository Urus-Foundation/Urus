# Rasya's Notes #2 — V0.2/1

**Date:** March 2, 2026
**By:** Rasya Andrean

---

## A Big Leap

V0.2/1 was a major upgrade from V0.1. Many fundamental features were added that made URUS start to look like a real language you could actually use, not just an experiment.

## New Features

### Enums and Tagged Unions
```urus
enum Shape {
    Circle(r: float);
    Rect(w: float, h: float);
    Point;
}
```

### Pattern Matching
```urus
match shape {
    Circle(r) => { print(f"radius: {r}"); }
    Rect(w, h) => { print(f"{w}x{h}"); }
    Point => { print("point"); }
}
```

### String Interpolation
No more manual concatenation:
```urus
let name: str = "Urus";
print(f"Hello {name}!");
```

### Modules and Imports
Split code across files:
```urus
import "utils.urus";
```

### Error Handling
`Result<T, E>` with `Ok`, `Err`, `unwrap`, etc. Proper error handling without exceptions.

### Full Array Support
Now supports `[float]`, `[bool]`, `[str]` — not just `[int]`. Plus array index assignment (`nums[i] = value`).

### Reference Counting
Memory management using `retain`/`release` for strings and arrays. No manual free, no GC pauses.

## Infrastructure

- Full test suite: valid tests, invalid tests, run tests
- Documentation: README, SPEC, CHANGELOG, and documentation/ folder
- 9 example programs
- Apache 2.0 license
- File restructuring to `compiler/src/` and `compiler/include/`

## Bug Fixes from V0.1

- `ast_type_str` buffer clobber — fixed with round-robin 4 buffers
- `urus_str_replace` underflow — use `ptrdiff_t`
- GCC statement expressions removed — use temp variable pattern (standard C11)
- Makefile missing `sema.c codegen.c` and `-lm`
- Array codegen only supporting `int`
- Array index assignment generating invalid C lvalue

---

*V0.2/1 — from experiment to real language.*
