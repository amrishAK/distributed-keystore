# ============================================================================
# Unified Output Directory Configuration
# ============================================================================
# This module centralizes all output directory configuration to keep the
# workspace clean and organized. All build artifacts go to CMAKE_BINARY_DIR.
#
# Usage: include(${CMAKE_CURRENT_SOURCE_DIR}/build-system/cmake/Directories.cmake)
#        Place this FIRST in the root CMakeLists.txt before other configs.

include_guard(GLOBAL)

# ============================================================================
# Runtime & Library Output Directories
# ============================================================================
# All executables and libraries are placed in predictable locations
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    CACHE PATH "Runtime output directory")

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    CACHE PATH "Library output directory")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    CACHE PATH "Archive output directory")

# For multi-config generators (Visual Studio, Xcode)
foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG} CONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${CMAKE_BINARY_DIR}/bin" CACHE PATH "")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${CMAKE_BINARY_DIR}/lib" CACHE PATH "")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${CMAKE_BINARY_DIR}/lib" CACHE PATH "")
endforeach()

# ============================================================================
# Test & Coverage Output Directories
# ============================================================================
# Keep test and coverage artifacts organized and separate from general binaries
set(TEST_RESULTS_DIRECTORY "${CMAKE_BINARY_DIR}/test-results"
    CACHE PATH "Test results output directory")

set(COVERAGE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/coverage"
    CACHE PATH "Coverage output directory")

# ============================================================================
# Benchmark Output Directories
# ============================================================================
# Keep benchmark results organized by type
set(BENCHMARK_RESULTS_DIRECTORY "${CMAKE_BINARY_DIR}/benchmark-results"
    CACHE PATH "Benchmark results output directory")

set(BENCHMARK_SINGLE_THREAD_RESULTS "${BENCHMARK_RESULTS_DIRECTORY}/single-thread"
    CACHE PATH "Single-thread benchmark results")

set(BENCHMARK_MULTI_THREAD_RESULTS "${BENCHMARK_RESULTS_DIRECTORY}/multi-thread"
    CACHE PATH "Multi-thread benchmark results")

# ============================================================================
# Create All Output Directories
# ============================================================================
file(MAKE_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
file(MAKE_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
file(MAKE_DIRECTORY "${TEST_RESULTS_DIRECTORY}")
file(MAKE_DIRECTORY "${COVERAGE_OUTPUT_DIRECTORY}")
file(MAKE_DIRECTORY "${BENCHMARK_RESULTS_DIRECTORY}")
file(MAKE_DIRECTORY "${BENCHMARK_SINGLE_THREAD_RESULTS}")
file(MAKE_DIRECTORY "${BENCHMARK_MULTI_THREAD_RESULTS}")

message(STATUS "Build artifacts organized in: ${CMAKE_BINARY_DIR}")
message(STATUS "  - Binaries:  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "  - Libraries: ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
message(STATUS "  - Tests:     ${TEST_RESULTS_DIRECTORY}")
message(STATUS "  - Coverage:  ${COVERAGE_OUTPUT_DIRECTORY}")
message(STATUS "  - Benchmarks: ${BENCHMARK_RESULTS_DIRECTORY}")
