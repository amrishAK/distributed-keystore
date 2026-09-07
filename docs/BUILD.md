# CMake Build System - Complete Guide

This document describes the unified CMake build system for the distributed keystore project.

## Overview

The build system is fully integrated using CMake 3.20+ and supports:
- ✅ Core keystore library compilation
- ✅ Unit tests (Unity framework)
- ✅ Integration tests (concurrency stress test)
- ✅ Benchmarks (single-thread and multi-thread)
- ✅ Code coverage analysis
- ✅ Cross-platform builds (Linux, macOS, Windows)

## Directory Structure

All build artifacts are organized in a single `build/` directory:

```
build/                          # CMake build directory
├── bin/                        # All executables
│   ├── unit_tests
│   ├── concurrency_test
│   ├── single_thread_benchmark
│   └── multi_thread_benchmark
├── lib/                        # All libraries
│   └── libkeystore.a (or .lib on Windows)
├── coverage/                   # Coverage files (.gcov, .gcda, .gcno)
├── test-results/               # CTest results
└── benchmark-results/          # Benchmark output
    ├── single-thread/
    └── multi-thread/
```

## Quick Start

### 1. Configure the Build

On Windows, use the MinGW preset because MSVC is not supported:

```powershell
cmake --preset windows-mingw
cmake --build --preset windows-mingw
ctest --test-dir build/windows-mingw --output-on-failure
```

On Linux and macOS, use one of the GCC or Clang presets, or follow the generic
commands below.

```bash
# Create build directory
mkdir -p build
cd build

# Configure with default options (unit tests only)
cmake ..

# Configure with all targets enabled
cmake -DBUILD_UNIT_TESTS=ON -DBUILD_INTEGRATION_TESTS=ON -DBUILD_BENCHMARKS=ON ..

# Configure with coverage instrumentation
cmake -DBUILD_UNIT_TESTS=ON -DBUILD_COVERAGE=ON ..
```

### 2. Build Everything

```bash
# Build all targets (depends on configuration)
cmake --build .

# Build only specific target
cmake --build . --target unit_tests
cmake --build . --target concurrency_test
cmake --build . --target single_thread_benchmark
```

### 3. Run Tests

```bash
# Run unit tests
cmake --build . --target test

# Run integration tests (if enabled)
cmake --build . --target run-concurrency-test

# Run all tests
ctest
```

### 4. Run Benchmarks

```bash
# Run all benchmarks
cmake --build . --target benchmark

# Run only single-thread
cmake --build . --target benchmark-single

# Run only multi-thread
cmake --build . --target benchmark-multi
```

### 5. Generate Coverage Reports

```bash
# Configure with coverage
cmake -DBUILD_COVERAGE=ON ..

# Build and run tests
cmake --build . --target test

# Generate coverage reports
cmake --build . --target coverage-simple
```

Coverage files are placed in `build/coverage/`.

## Build Options

Configure these with `-D<OPTION>=ON|OFF` when running cmake:

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_UNIT_TESTS` | ON | Build Unity-based unit tests |
| `BUILD_INTEGRATION_TESTS` | OFF | Build concurrency integration test |
| `BUILD_BENCHMARKS` | OFF | Build benchmark executables |
| `BUILD_COVERAGE` | OFF | Add code coverage instrumentation |

### Example Configurations

**Minimal build (library only):**
```bash
cmake -DBUILD_UNIT_TESTS=OFF -DBUILD_INTEGRATION_TESTS=OFF -DBUILD_BENCHMARKS=OFF ..
```

**Install for a CMake consumer:**
```bash
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release -DBUILD_UNIT_TESTS=OFF
cmake --build build/release --parallel
cmake --install build/release --prefix /path/to/prefix
```

Consumers can discover the installed target with:
```cmake
find_package(distributed_keystore CONFIG REQUIRED)
target_link_libraries(my_application PRIVATE distributed_keystore::keystore)
```

Set `CMAKE_PREFIX_PATH` to the installation prefix when it is outside CMake's standard search locations.

**Full CI build (all tests, no benchmarks):**
```bash
cmake -DBUILD_UNIT_TESTS=ON -DBUILD_INTEGRATION_TESTS=ON -DBUILD_BENCHMARKS=OFF ..
```

**Full build with coverage:**
```bash
cmake -DBUILD_UNIT_TESTS=ON -DBUILD_INTEGRATION_TESTS=ON -DBUILD_BENCHMARKS=ON -DBUILD_COVERAGE=ON ..
```

## Detailed Target Reference

### Core Targets

| Target | Type | Command |
|--------|------|---------|
| `keystore` | Library | (auto-built) |
| `unit_tests` | Executable | `cmake --build . --target unit_tests` |
| `concurrency_test` | Executable | `cmake --build . --target concurrency_test` |
| `single_thread_benchmark` | Executable | `cmake --build . --target single_thread_benchmark` |
| `multi_thread_benchmark` | Executable | `cmake --build . --target multi_thread_benchmark` |

### Test Targets

| Target | Purpose |
|--------|---------|
| `test` | Build and run unit tests |
| `run-concurrency-test` | Run integration tests (requires BUILD_INTEGRATION_TESTS=ON) |
| `run-ct-valgrind` | Run concurrency test under Valgrind (Linux/macOS only) |
| `coverage-simple` | Generate coverage reports (requires BUILD_COVERAGE=ON) |

### Benchmark Targets

| Target | Purpose |
|--------|---------|
| `benchmarks` | Build all benchmark executables |
| `benchmark` | Build and run all benchmarks |
| `benchmark-single` | Build and run single-thread benchmark |
| `benchmark-multi` | Build and run multi-thread benchmark |
| `build-benchmarks` | Build benchmarks without running them |

### CTest Integration

```bash
# Run all registered tests
ctest

# Run with verbose output
ctest --verbose

# Run specific test
ctest -R unit_tests -VV

# Run tests in parallel
ctest -j4
```

## Compiler & Platform Support

### Linux (GCC/Clang)

```bash
# Standard build
cmake -DCMAKE_C_COMPILER=gcc ..
cmake --build .

# With coverage
cmake -DCMAKE_C_COMPILER=gcc -DBUILD_COVERAGE=ON ..
cmake --build . --target test
cmake --build . --target coverage-simple
```

The built-in `coverage-simple` target requires GCC and `gcov`. Clang builds are supported for compilation and testing, but use the CI `gcovr` workflow or another external coverage tool for coverage reports.

### macOS (Clang)

```bash
cmake -DCMAKE_C_COMPILER=clang ..
cmake --build .
```

### Windows (MinGW)

Native Windows builds require MinGW-w64 because the project uses POSIX pthread APIs.
Ensure the MinGW `bin` directory is on `PATH`, then run from PowerShell:

```powershell
cmake --preset windows-mingw
cmake --build --preset windows-mingw --parallel
ctest --test-dir build/windows-mingw --output-on-failure
```

MSVC is intentionally rejected during configuration because it does not provide the required pthread API.

### WSL2 / Linux (GCC)

Install the required tools in WSL, then run the same CMake workflow:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake
cmake --preset linux-gcc
cmake --build --preset linux-gcc --parallel
ctest --test-dir build/linux-gcc --output-on-failure
```

For Clang, replace `linux-gcc` with `linux-clang` after installing `clang`.

## Output Locations

After building, find artifacts in `build/`:

```
build/bin/unit_tests                     # Unit test executable
build/bin/concurrency_test               # Integration test executable
build/bin/single_thread_benchmark        # Single-thread benchmark
build/bin/multi_thread_benchmark         # Multi-thread benchmark
build/lib/libkeystore.a                  # Static library (or keystore.lib on Windows)
build/coverage/                          # Coverage reports (if BUILD_COVERAGE=ON)
build/test-results/                      # CTest results
build/benchmark-results/                 # Benchmark outputs
```

## Troubleshooting

### "CMake Error: The source directory does not appear to contain CMakeLists.txt"

Ensure you're running cmake from the build directory or with correct paths:
```bash
mkdir -p build && cd build
cmake ..
```

### Tests fail to find keystore library

Verify that `keystore` library was built successfully:
```bash
cmake --build . --target keystore
cmake --build .
```

### Coverage target not available

Coverage requires GCC and gcov:
```bash
# Verify gcov is installed
which gcov    # Linux/macOS
gcov --version
```

### Benchmarks not building

Enable them explicitly:
```bash
cmake -DBUILD_BENCHMARKS=ON ..
cmake --build .
```

### Build artifacts cluttering working directory

All artifacts are now placed in `build/` directory. Clean build with:
```bash
rm -rf build/
mkdir build && cd build
cmake ..
cmake --build .
```

## File Organization Changes

The refactored build system fixes these issues:

✅ **Before**: `.gcov` files scattered in `tests/for_c/`  
✅ **After**: All coverage files in `build/coverage/`

✅ **Before**: Multiple build directories (`build/`, `build-all/`, `build-cmake/`, etc.)  
✅ **After**: Single unified `build/` directory

✅ **Before**: Test and benchmark artifacts mixed with source  
✅ **After**: All outputs organized in `build/bin/`, `build/lib/`, etc.

## Next Steps

The CMake build system is now fully integrated. You can:

1. **Replace any Makefile-based workflows** with equivalent CMake commands
2. **Set up CI/CD pipelines** using the cmake commands documented above
3. **Customize build options** per environment (dev, CI, release)
4. **Add more targets** by extending the `build-system/tests/` and `build-system/benchmark/` CMakeLists.txt files

## CMake Build System Architecture

The hierarchy is:

```
CMakeLists.txt (root)
├── build-system/cmake/Directories.cmake        (output dir config)
├── build-system/cmake/CompilerOptions.cmake    (compiler flags)
├── build-system/cmake/Packaging.cmake          (install rules)
├── src/keystore/CMakeLists.txt                 (core library)
├── build-system/tests/unit/CMakeLists.txt      (unit tests)
├── build-system/tests/integration/CMakeLists.txt (integration tests)
└── build-system/benchmark/CMakeLists.txt       (benchmarks)
```

Each subdirectory CMakeLists.txt can be independently configured and can add custom targets.

## References

- [CMake Documentation](https://cmake.org/cmake/help/latest/)
- [CTest Documentation](https://cmake.org/cmake/help/latest/manual/ctest.1.html)
- [Unity Test Framework](http://www.throwtheswitch.org/unity)
