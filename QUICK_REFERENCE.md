# CMake Build Quick Reference

The project uses CMake presets. Each preset creates an isolated build directory under `build/`.

## Requirements

- CMake 3.20 or later
- Windows: MinGW-w64 with its `bin` directory on `PATH`
- Linux: GCC or Clang and Make

MSVC is not supported because the project uses POSIX pthread APIs.

## Build And Test

### Windows (MinGW)

Run from the repository root in PowerShell:

```powershell
cmake --preset windows-mingw
cmake --build --preset windows-mingw --parallel
ctest --test-dir build/windows-mingw --output-on-failure
```

### Linux (GCC)

```bash
cmake --preset linux-gcc
cmake --build --preset linux-gcc --parallel
ctest --test-dir build/linux-gcc --output-on-failure
```

### Linux (Clang)

```bash
cmake --preset linux-clang
cmake --build --preset linux-clang --parallel
ctest --test-dir build/linux-clang --output-on-failure
```

## Build Directories

| Preset | Build directory |
|---|---|
| `windows-mingw` | `build/windows-mingw` |
| `linux-gcc` | `build/linux-gcc` |
| `linux-clang` | `build/linux-clang` |

Each build directory contains the generated build files and project artifacts, including `bin/`, `lib/`, coverage output when enabled, test results, and benchmark results.

## Common Commands

Replace `<preset>` with `windows-mingw`, `linux-gcc`, or `linux-clang`.

```bash
# Build and run unit tests
cmake --build --preset <preset> --target test

# Run all registered tests
ctest --test-dir build/<preset> --output-on-failure

# Run integration tests (when configured)
cmake --build --preset <preset> --target run-concurrency-test

# Build and run benchmarks (when configured)
cmake --build --preset <preset> --target benchmark

# Run one benchmark suite
cmake --build --preset <preset> --target benchmark-single
cmake --build --preset <preset> --target benchmark-multi
```

The default presets enable unit tests and disable integration tests, benchmarks, and coverage. To enable different options, configure a separate build directory with `cmake -S . -B <build-dir>` and the required `-D` options. See [docs/BUILD.md](docs/BUILD.md) for examples.

## Cleanup

Delete an individual preset build directory to reconfigure it from scratch:

```powershell
Remove-Item -Recurse -Force build/windows-mingw
```

```bash
rm -rf build/linux-gcc
```

## More Detail

See [docs/BUILD.md](docs/BUILD.md) for build options, coverage, benchmark targets, and installation instructions.
