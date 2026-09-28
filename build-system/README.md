# Build System

This directory contains the CMake implementation used by the project root `CMakeLists.txt`.

## Layout

- `cmake/` - Shared CMake modules for output directories, compiler options, coverage, and packaging.
- `tests/unit/` - CMake configuration for the Unity-based unit test executable.
- `tests/integration/` - CMake configuration for the concurrency integration test.
- `benchmark/` - CMake configuration for the single-thread and multi-thread benchmark executables.

The project source code, test sources, and benchmark sources remain in their respective top-level directories. This directory contains their build configuration only.

## Configuration Flow

The root `CMakeLists.txt` loads the shared modules first, then adds the build configuration in dependency order:

1. `cmake/Directories.cmake` configures output locations under the selected build directory.
2. `cmake/CompilerOptions.cmake` applies compiler warnings, coverage flags, and optional sanitizers.
3. `../src/keystore/` defines the core `keystore` library.
4. The test and benchmark subdirectories define executables that link against `keystore`.
5. `cmake/Packaging.cmake` defines installation and CMake package-export rules.

## Build Options

The root project exposes these options:

- `BUILD_UNIT_TESTS` - Build and register the Unity unit tests. Enabled by default.
- `BUILD_INTEGRATION_TESTS` - Build and register the concurrency integration test.
- `BUILD_BENCHMARKS` - Build the single-thread and multi-thread benchmark executables.
- `BUILD_COVERAGE` - Enable GCC coverage instrumentation.
- `ENABLE_SANITIZERS` - Enable a comma-separated GCC or Clang sanitizer list, such as `address,undefined`.

The exact commands and default configurations are documented in the [complete build guide](../docs/BUILD.md).

## Generated Targets

Depending on the enabled options, this directory contributes the following targets:

- `unit_tests`, `run-unit-tests`, and the CTest test `unit_tests`.
- `concurrency_test`, `integration_tests`, and the CTest test `concurrency_test`.
- `single_thread_benchmark`, `multi_thread_benchmark`, `benchmarks`, and the `benchmark-*` run targets.
- `coverage-simple` when unit tests and GCC coverage are enabled.

The core library and installation targets are defined outside this directory and are only coordinated here through the root build configuration.

## Build Documentation

Use the canonical project documentation for commands and configuration details:

- [Complete build guide](../docs/BUILD.md)
- [Build quick reference](../QUICK_REFERENCE.md)

To change the build, update the relevant CMake file here and keep the user-facing documentation in `docs/` or the repository root.
