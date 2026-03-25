# Installation Guide

This guide covers how to set up the URUS compiler on your system. The process has two parts: installing the prerequisite tools, and building the compiler from source.

## Prerequisites

You need two things:

1. **A C compiler** to build the URUS compiler itself (GCC, Clang, or MSVC).
2. **CMake 3.10+** as the build system.

You'll also need GCC or Clang available at runtime, because `urusc` invokes it to compile the generated C code into a binary. MSVC can build the compiler, but the generated code needs GCC or Clang.

## Building from source

Clone the repository and build with CMake:

```bash
git clone https://github.com/Urus-Foundation/Urus.git
cd Urus/compiler
cmake -S . -B build
cmake --build build
```

The compiled binary ends up at:
- **Linux / macOS:** `compiler/build/urusc`
- **Windows:** `compiler/build/Debug/urusc.exe`

To verify the build worked:

```bash
./build/urusc --version            # Linux/macOS
./build/Debug/urusc.exe --version  # Windows
```

## Platform-specific setup

### Windows

There are several ways to get GCC on Windows. Pick whichever suits your setup.

**MSYS2 (recommended).** Download from https://www.msys2.org/, open the MSYS2 terminal, and run:

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake
```

Then add `C:\msys64\mingw64\bin` to your system PATH.

**WinLibs.** Download a standalone MinGW build from https://winlibs.com/. Extract the archive and add the `bin/` folder to your PATH.

**Chocolatey.** If you use Chocolatey:

```powershell
choco install mingw cmake
```

**Visual Studio.** Install Visual Studio with the "Desktop development with C++" workload. CMake will use MSVC to build the compiler. You still need MinGW or another GCC installation for compiling the generated C code — MSVC alone isn't enough for that step.

### Linux

Install GCC and CMake through your package manager.

Ubuntu / Debian:
```bash
sudo apt update && sudo apt install gcc cmake git
```

Fedora / RHEL:
```bash
sudo dnf install gcc cmake git
```

Arch Linux:
```bash
sudo pacman -S gcc cmake git
```

### macOS

The Xcode Command Line Tools include a GCC-compatible Clang:

```bash
xcode-select --install
```

Or install GCC explicitly via Homebrew:

```bash
brew install gcc cmake
```

### Termux (Android)

URUS works on Termux. Install the toolchain and build with a custom install prefix:

```bash
pkg install gcc cmake git
cd Urus/compiler
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=$PREFIX
cmake --build build
```

## Installing system-wide

After building, you can install `urusc` so it's available from any directory:

```bash
# Linux / macOS / Termux
sudo cmake --install build

# Windows (run as Administrator)
cmake --install build
```

Or add the build directory to your PATH manually:

```bash
# Linux / macOS
export PATH="$PATH:$(pwd)/build"

# Windows (PowerShell)
$env:PATH += ";$(Get-Location)\build\Debug"
```

## Setting up URUSCPATH

The `URUSCPATH` environment variable tells the compiler where to find standard library modules. If you plan to use `import` with library modules, set it to the stdlib directory:

```bash
# Linux / macOS
export URUSCPATH="/path/to/Urus/compiler/stdlib"

# Windows (PowerShell)
$env:URUSCPATH = "C:\path\to\Urus\compiler\stdlib"
```

Without this, `import` only resolves files relative to the current source file.

## Verifying the installation

Create a test program and compile it:

```bash
echo 'fn main(): void { print("Hello, URUS!"); }' > test.urus
urusc test.urus -o test
./test
```

You should see:
```
Hello, URUS!
```

If `urusc` can't find GCC, you'll get an error at the compilation step. Make sure `gcc` is in your PATH.

## Docker

A Dockerfile is included in the repository root:

```bash
docker build -t urus .
docker run --rm urus urusc --version
```

This gives you a self-contained environment with GCC and the compiler pre-built.

## Troubleshooting

**`gcc: command not found`** — GCC isn't installed or isn't in your PATH. Follow the platform-specific instructions above to install it.

**`cmake: command not found`** — Install CMake 3.10 or later.

**`fatal error: cannot execute 'cc1'`** — GCC's internal tools aren't in PATH. Make sure the GCC `bin/` directory is in your PATH, not just the `gcc` binary itself. On Windows, the compiler tries to auto-detect this, but it may fail with non-standard installations.

**`LINK : fatal error LNK...`** — This happens when MSVC tries to link the generated C code. Use MinGW/GCC for that step instead.

**Compilation succeeds but the program crashes** — Try `urusc --emit-c program.urus` to inspect the generated C. If the C looks wrong, file a bug report.

If none of this helps, search the [issue tracker](https://github.com/Urus-Foundation/Urus/issues) or open a new issue with your OS, compiler version, and the exact error message.
