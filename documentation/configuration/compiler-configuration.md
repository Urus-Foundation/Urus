# Compiler Configuration

The URUS compiler is configured entirely through command-line flags and environment variables. There are no configuration files.

## Command-line flags

| Flag | Description |
|------|-------------|
| `--help`, `-h` | Print a usage summary and exit |
| `--version`, `-v` | Print the compiler version and exit |
| `--emit-c` | Generate C code and print it to stdout without invoking GCC |
| `--tokens` | Print the token stream produced by the lexer |
| `--ast` | Print the parsed Abstract Syntax Tree |
| `-o <name>` | Set the output binary name. Defaults to `a.exe` on Windows, `a.out` elsewhere |

### Typical workflows

Compile a program to a named binary:

```bash
urusc program.urus -o myapp
```

Inspect the generated C code (useful for debugging codegen issues):

```bash
urusc --emit-c program.urus
```

Pipe the generated C to a file for manual inspection or external compilation:

```bash
urusc --emit-c program.urus > output.c
gcc -std=c11 -O2 output.c -lm -o myapp
```

Debug the lexer or parser when working on the compiler itself:

```bash
urusc --tokens program.urus
urusc --ast program.urus
```

## Environment variables

### PATH

The compiler needs `gcc` (or another C11 compiler) in your PATH to compile the generated code. On Windows, the compiler auto-detects GCC in several common locations:

1. `C:/msys64/mingw64/bin/gcc.exe` (MSYS2)
2. `C:/mingw64/bin/gcc.exe` (standalone MinGW)
3. `C:/Program Files/mingw-w64/*/mingw64/bin/gcc.exe`

When found, the compiler injects the GCC directory into PATH automatically so that GCC's internal tools (`cc1`, `collect2`, etc.) are also reachable.

### URUSCPATH

The `URUSCPATH` variable specifies where the compiler should look for standard library modules. When an `import` statement references a file that doesn't exist relative to the importing source, the compiler falls back to searching in `URUSCPATH`.

```bash
# Linux / macOS
export URUSCPATH="/usr/local/lib/urus/stdlib"

# Windows (PowerShell)
$env:URUSCPATH = "C:\Urus\compiler\stdlib"
```

If `URUSCPATH` is not set, `import` only resolves files relative to the importing file's directory.

## Build configuration (CMake)

The compiler itself is built with CMake. The standard build commands:

```bash
cd compiler
cmake -S . -B build
cmake --build build
```

### CMake variables

| Variable | Default | Description |
|----------|---------|-------------|
| `CMAKE_BUILD_TYPE` | (generator default) | `Debug` includes debug symbols; `Release` enables `-O2` optimization |
| `CMAKE_INSTALL_PREFIX` | system default | Where `cmake --install` puts the binary. Use `$PREFIX` on Termux |

### Platform notes

**Windows with MSVC.** CMake automatically adds `_CRT_SECURE_NO_WARNINGS` and `_CRT_NONSTDC_NO_DEPRECATE` to suppress MSVC deprecation warnings for standard C functions like `strdup`.

**Termux.** Use the `-DCMAKE_INSTALL_PREFIX=$PREFIX` flag so the installed binary lands in Termux's local prefix:

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=$PREFIX
cmake --build build
cmake --install build
```

## Generated code details

When `urusc` compiles a program, it:

1. Generates a C file containing the embedded runtime and the translated user code.
2. Writes it to `_urus_tmp.c` in the current working directory.
3. Invokes GCC with: `gcc -std=c11 -O2 _urus_tmp.c -lm -o <output>`
4. Deletes `_urus_tmp.c` after GCC finishes successfully.

The generated C is always standard C11 — no compiler extensions are used. The `-lm` flag links the math library for functions like `sqrt`, `pow`, and `fabs`.

If GCC fails, the temp file is left in place so you can inspect it. You can also use `--emit-c` to see the generated code without creating the temp file at all.

## Import resolution order

When the compiler encounters `import "some_module.urus"`:

1. Look for the file relative to the importing file's directory.
2. If not found, look in the `URUSCPATH` directory.
3. If still not found, report an error.

The compiler tracks which files have already been imported and rejects circular dependencies. The maximum number of imports per program is 64.
