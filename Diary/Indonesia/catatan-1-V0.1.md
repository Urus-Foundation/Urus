# Catatan Rasya #1 — V0.1

**Tanggal:** 2025
**Oleh:** Rasya Andrean

---

## Awal Mula

Ini adalah versi pertama dari URUS. Idenya simpel — bikin bahasa pemrograman yang gampang dipelajari tapi tetap bisa compile ke native binary lewat C. Bahasa yang nggak terlalu low-level kayak C, tapi juga nggak lambat kayak Python.

## Apa yang Dibuat

Compiler dasar udah jalan. Ada lexer buat tokenisasi, parser buat bikin AST, semantic analyzer buat cek tipe, dan codegen buat generate kode C. Pipeline-nya: `.urus` → lexer → parser → sema → codegen → `.c` → GCC → binary.

Fitur yang masuk di versi ini:
- Tipe primitif: `int`, `float`, `bool`, `str`, `void`
- Variabel immutable by default, pakai `mut` kalau mau mutable
- Fungsi dengan parameter dan return type
- Struct sederhana
- Array (cuma `[int]` dulu)
- Control flow: `if/else`, `while`, `for..in` range
- Operator standar: aritmatika, perbandingan, logika
- Built-in functions: `print`, `len`, `push`, `to_str`
- String operations: `str_len`, `str_upper`, `str_lower`, `str_trim`, `str_contains`, `str_slice`, `str_replace`
- File I/O: `read_file`, `write_file`, `append_file`
- Komentar: `//` dan `/* */`
- Runtime header-only (`urus_runtime.h`)

## Kondisi

Masih sangat awal. Compiler jalan tapi belum stabil. Belum ada enum, belum ada pattern matching, belum ada string interpolation. Array cuma support `int`. Dokumentasi minim. Tapi fondasi udah ada — dan itu yang penting.

---

*V0.1 — langkah pertama.*
