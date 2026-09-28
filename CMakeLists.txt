cmake_minimum_required(VERSION 3.20)

set(DISTRIBUTED_KEYSTORE_VERSION "1.0.0" CACHE STRING
  "distributed_keystore semantic version")
set(ENABLE_SANITIZERS "" CACHE STRING
  "Comma-separated sanitizers supported by the compiler")

if(NOT DISTRIBUTED_KEYSTORE_VERSION MATCHES
   "^[0-9]+\\.[0-9]+\\.[0-9]+(-[0-9A-Za-z]+([.-][0-9A-Za-z]+)*)?$")
  message(FATAL_ERROR
    "DISTRIBUTED_KEYSTORE_VERSION must be a semantic version: ${DISTRIBUTED_KEYSTORE_VERSION}")
endif()

string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+"
  DISTRIBUTED_KEYSTORE_CMAKE_VERSION "${DISTRIBUTED_KEYSTORE_VERSION}")
project(distributed_keystore VERSION ${DISTRIBUTED_KEYSTORE_CMAKE_VERSION} LANGUAGES C CXX)

if(MSVC)
  message(FATAL_ERROR
    "MSVC is not supported because distributed_keystore requires POSIX pthread APIs. "
    "Use MinGW or WSL2 on Windows.")
endif()

# ============================================================================
# STEP 0: Configure Output Directories (Must be first!)
# ============================================================================
# Include unified directory configuration BEFORE anything else
include(${CMAKE_CURRENT_SOURCE_DIR}/build-system/cmake/Directories.cmake)

# ============================================================================
# STEP 0.5: Compiler Options
# ============================================================================
include(${CMAKE_CURRENT_SOURCE_DIR}/build-system/cmake/CompilerOptions.cmake)

# ============================================================================
# Build Options
# ============================================================================
option(BUILD_UNIT_TESTS "Build unit tests" ON)
option(BUILD_INTEGRATION_TESTS "Build integration tests" OFF)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)
option(BUILD_COVERAGE "Build with code coverage instrumentation" OFF)

if(BUILD_UNIT_TESTS OR BUILD_INTEGRATION_TESTS)
  enable_testing()
endif()

message(STATUS "Configuring distributed keystore build")
message(STATUS "  Unit tests:        ${BUILD_UNIT_TESTS}")
message(STATUS "  Integration tests: ${BUILD_INTEGRATION_TESTS}")
message(STATUS "  Benchmarks:        ${BUILD_BENCHMARKS}")
message(STATUS "  Coverage:          ${BUILD_COVERAGE}")

# ============================================================================
# STEP 1: Build the core keystore library
# ============================================================================
add_subdirectory(src/keystore)

# ============================================================================
# STEP 2: Build tests against the keystore library
# ============================================================================
if(BUILD_UNIT_TESTS)
  message(STATUS "Adding unit tests (depends on keystore library)")
  add_subdirectory(build-system/tests/unit)
endif()

if(BUILD_INTEGRATION_TESTS)
  message(STATUS "Adding integration tests (depends on keystore library)")
  add_subdirectory(build-system/tests/integration)
endif()

# ============================================================================
# STEP 3: Build benchmarks against the keystore library
# ============================================================================
if(BUILD_BENCHMARKS)
  message(STATUS "Adding benchmarks (depends on keystore library)")
  add_subdirectory(build-system/benchmark)
endif()

# ============================================================================
# STEP 4: Packaging & Installation
# ============================================================================
include(${CMAKE_CURRENT_SOURCE_DIR}/build-system/cmake/Packaging.cmake)

message(STATUS "Build system configured successfully")
message(STATUS "Build directory: ${CMAKE_BINARY_DIR}")
message(STATUS "To build: cmake --build <build-dir>")
message(STATUS "To test: ctest --test-dir <build-dir> --output-on-failure")
