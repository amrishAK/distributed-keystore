if(NOT BUILD_UNIT_TESTS)
  return()
endif()

message(STATUS "Configuring unit tests")

# ============================================================================
# Unit Test Build Configuration
# ============================================================================

# The test runner entry point includes all test sources directly.
set(TEST_RUNNER "${CMAKE_SOURCE_DIR}/tests/for_c/unit_tests/test_runner.c")

set(TEST_SOURCES
    "${TEST_RUNNER}"
    "${CMAKE_SOURCE_DIR}/tests/for_c/include/unity.c"
)

# Collect test headers
file(GLOB_RECURSE TEST_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/tests/for_c/unit_tests/*.h"
    "${CMAKE_SOURCE_DIR}/tests/for_c/include/*.h"
)

# Create executable for unit tests
add_executable(unit_tests ${TEST_SOURCES} ${TEST_HEADERS})

# Link against the keystore library
target_link_libraries(unit_tests PRIVATE keystore)

# Add test include directories
target_include_directories(unit_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/tests/for_c"
    "${CMAKE_SOURCE_DIR}/tests/for_c/include"
    "${CMAKE_SOURCE_DIR}/tests/for_c/unit_tests"
    "${CMAKE_SOURCE_DIR}/src/keystore"
)

# Set C standard
set_target_properties(unit_tests PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS ON
)

# Compiler flags for tests
target_compile_options(unit_tests PRIVATE -Wall -Wextra -Wpedantic)

# Register test with CTest
add_test(
    NAME unit_tests
    COMMAND $<TARGET_FILE:unit_tests>
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

message(STATUS "Unit tests executable: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/unit_tests")

# ============================================================================
# Custom Targets for Test Execution
# ============================================================================

# Target to run unit tests directly
add_custom_target(run-unit-tests
    COMMAND $<TARGET_FILE:unit_tests>
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS unit_tests
)

# Target for integration test (if enabled)
if(BUILD_INTEGRATION_TESTS)
    add_custom_target(run-concurrency-test
        COMMAND $<TARGET_FILE:concurrency_test>
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS concurrency_test
    )
endif()

# Target for coverage analysis (when BUILD_COVERAGE is ON)
if(BUILD_COVERAGE)
    add_custom_target(coverage-simple
        COMMAND $<TARGET_FILE:unit_tests>
        COMMAND ${CMAKE_COMMAND}
                -DCOVERAGE_BINARY_DIR=${CMAKE_BINARY_DIR}
                -DCOVERAGE_OUTPUT_DIRECTORY=${COVERAGE_OUTPUT_DIRECTORY}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/GenerateCoverage.cmake
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS unit_tests
    )
endif()

message(STATUS "Unit tests configured successfully")
