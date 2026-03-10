# Catatan Rasya #2 — V0.2/1

**Tanggal:** 2 Maret 2026
**Oleh:** Rasya Andrean

---

## Lompatan Besar

V0.2/1 itu upgrade gede dari V0.1. Banyak fitur fundamental yang ditambahin dan bikin URUS mulai keliatan kayak bahasa yang beneran bisa dipakai, bukan cuma eksperimen.

## Fitur Baru

### Enums dan Tagged Unions
Sekarang bisa bikin enum:
```urus
enum Shape {
    Circle(r: float);
    Rect(w: float, h: float);
    Point;
}
```

### Pattern Matching
Bisa match enum variants:
```urus
match shape {
    Circle(r) => { print(f"radius: {r}"); }
    Rect(w, h) => { print(f"{w}x{h}"); }
    Point => { print("titik"); }
}
```

### String Interpolation
Nggak perlu concat manual lagi:
```urus
let name: str = "Urus";
print(f"Hello {name}!");
```

### Modules dan Imports
Bisa pisah kode ke file berbeda:
```urus
import "utils.urus";
```

### Error Handling
Ada `Result<T, E>` dengan `Ok`, `Err`, `unwrap`, dll. Proper error handling tanpa exception.

### Array Support Lengkap
Sekarang support `[float]`, `[bool]`, `[str]` — bukan cuma `[int]`. Plus array index assignment (`nums[i] = value`).

### Reference Counting
Memory management pakai `retain`/`release` buat string dan array. Nggak perlu manual free tapi juga nggak pake GC yang bikin pause.

## Infrastruktur

- Test suite lengkap: valid tests, invalid tests, run tests
- Dokumentasi: README, SPEC, CHANGELOG, dan folder documentation/
- 9 contoh program
- Lisensi Apache 2.0
- File restructuring ke `compiler/src/` dan `compiler/include/`

## Bug Fixes dari V0.1

- `ast_type_str` buffer clobber — fix pakai round-robin 4 buffer
- `urus_str_replace` underflow — pakai `ptrdiff_t`
- GCC statement expressions dihapus — pakai temp variable pattern (standard C11)
- Makefile yang missing `sema.c codegen.c` dan `-lm`
- Array codegen yang cuma support `int`
- Array index assignment yang generate invalid C lvalue

---

*V0.2/1 — dari eksperimen jadi bahasa beneran.*
