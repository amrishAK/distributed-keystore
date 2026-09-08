if(NOT BUILD_BENCHMARKS)
  return()
endif()

message(STATUS "Configuring benchmarks")

# ============================================================================
# Single-Thread Benchmark
# ============================================================================

# Collect single-thread benchmark sources
file(GLOB_RECURSE SINGLE_THREAD_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/benchmark/src/common/benchmark_common.c"
    "${CMAKE_SOURCE_DIR}/benchmark/src/single_thread/*.c"
)

# Create single-thread benchmark executable
add_executable(single_thread_benchmark ${SINGLE_THREAD_SOURCES})

# Link against the keystore library
target_link_libraries(single_thread_benchmark PRIVATE keystore)

# Add benchmark include directories
target_include_directories(single_thread_benchmark PRIVATE
    "${CMAKE_SOURCE_DIR}/benchmark/src"
    "${CMAKE_SOURCE_DIR}/benchmark/src/common"
    "${CMAKE_SOURCE_DIR}/benchmark/src/single_thread"
    "${CMAKE_SOURCE_DIR}/tests/for_c/include"
    "${CMAKE_SOURCE_DIR}/src/keystore"
)

# Set C standard
set_target_properties(single_thread_benchmark PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS ON
)

message(STATUS "Single-thread benchmark: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/single_thread_benchmark")

# ============================================================================
# Multi-Thread Benchmark
# ============================================================================

# Collect multi-thread benchmark sources
file(GLOB_RECURSE MULTI_THREAD_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/benchmark/src/common/benchmark_common.c"
    "${CMAKE_SOURCE_DIR}/benchmark/src/multi_thread/*.c"
)

# Create multi-thread benchmark executable
add_executable(multi_thread_benchmark ${MULTI_THREAD_SOURCES})

# Link against the keystore library
target_link_libraries(multi_thread_benchmark PRIVATE keystore)

# Add benchmark include directories
target_include_directories(multi_thread_benchmark PRIVATE
    "${CMAKE_SOURCE_DIR}/benchmark/src"
    "${CMAKE_SOURCE_DIR}/benchmark/src/common"
    "${CMAKE_SOURCE_DIR}/benchmark/src/multi_thread"
    "${CMAKE_SOURCE_DIR}/tests/for_c/include"
    "${CMAKE_SOURCE_DIR}/src/keystore"
)

# Set C standard
set_target_properties(multi_thread_benchmark PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS ON
)

message(STATUS "Multi-thread benchmark: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/multi_thread_benchmark")

# ============================================================================
# Custom Targets for Benchmark Execution
# ============================================================================

# Aggregate benchmark target (builds both)
add_custom_target(benchmarks
    DEPENDS single_thread_benchmark multi_thread_benchmark
)

# Run single-thread benchmark
add_custom_target(benchmark-single
    COMMAND $<TARGET_FILE:single_thread_benchmark>
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS single_thread_benchmark
)

# Run multi-thread benchmark
add_custom_target(benchmark-multi
    COMMAND $<TARGET_FILE:multi_thread_benchmark>
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS multi_thread_benchmark
)

# Run all benchmarks
add_custom_target(benchmark
    COMMAND $<TARGET_FILE:single_thread_benchmark>
            --results-file "${BENCHMARK_SINGLE_THREAD_RESULTS}/results.jsonl"
    COMMAND $<TARGET_FILE:multi_thread_benchmark>
            --results-file "${BENCHMARK_MULTI_THREAD_RESULTS}/results.jsonl"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS single_thread_benchmark multi_thread_benchmark
)

# Build-only target (no execution)
add_custom_target(build-benchmarks
    DEPENDS benchmarks
)

message(STATUS "Benchmarks configured successfully")
message(STATUS "  Available targets:")
message(STATUS "    cmake --build <build-dir> --target benchmark          (build and run all)")
message(STATUS "    cmake --build <build-dir> --target benchmark-single   (run single-thread)")
message(STATUS "    cmake --build <build-dir> --target benchmark-multi    (run multi-thread)")
message(STATUS "    cmake --build <build-dir> --target benchmarks         (build only)")
