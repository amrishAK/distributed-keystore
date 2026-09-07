if(NOT BUILD_INTEGRATION_TESTS)
  return()
endif()

message(STATUS "Configuring integration tests")

# ============================================================================
# Integration Test Build Configuration
# ============================================================================

# Concurrency test source
set(CONCURRENCY_TEST_SOURCE 
    "${CMAKE_SOURCE_DIR}/tests/for_c/integration_test/concurrency_test.c")

# Create executable for concurrency test
add_executable(concurrency_test ${CONCURRENCY_TEST_SOURCE})

# Link against the keystore library
target_link_libraries(concurrency_test PRIVATE keystore)

# Add test include directories
target_include_directories(concurrency_test PRIVATE
    "${CMAKE_SOURCE_DIR}/tests/for_c"
    "${CMAKE_SOURCE_DIR}/tests/for_c/include"
    "${CMAKE_SOURCE_DIR}/src/keystore"
)

# Set C standard
set_target_properties(concurrency_test PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS ON
)

# Compiler flags for tests
target_compile_options(concurrency_test PRIVATE -Wall -Wextra -Wpedantic)

# Register test with CTest
add_test(
    NAME concurrency_test
    COMMAND $<TARGET_FILE:concurrency_test>
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

message(STATUS "Integration tests executable: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/concurrency_test")

# ============================================================================
# Custom Targets for Test Execution
# ============================================================================

# Target to run integration tests
add_custom_target(integration_tests
    COMMAND $<TARGET_FILE:concurrency_test>
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS concurrency_test
)

# Target for running concurrency test under Valgrind (if available)
if(UNIX)
    add_custom_target(run-ct-valgrind
        COMMAND valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all
                $<TARGET_FILE:concurrency_test>
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS concurrency_test
    )
endif()

message(STATUS "Integration tests configured successfully")
