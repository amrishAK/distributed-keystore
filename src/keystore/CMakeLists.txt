cmake_minimum_required(VERSION 3.16)

include(GNUInstallDirs)

# ============================================================================
# Keystore Core Library
# ============================================================================
# This CMakeLists.txt defines the keystore library target that is used by
# tests, benchmarks, and applications.

# Find all source files in the keystore subdirectories
file(GLOB_RECURSE KEYSTORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/**/*.c"
)

file(GLOB_RECURSE KEYSTORE_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/**/*.h"
)

message(STATUS "Keystore library sources found: ${KEYSTORE_SOURCES}")

# Create the library target
add_library(keystore STATIC ${KEYSTORE_SOURCES} ${KEYSTORE_HEADERS})

# Set include paths (so tests and other targets can include the headers)
target_include_directories(keystore 
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/keystore>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

if(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(keystore PUBLIC Threads::Threads m)
endif()

# Set C standard
set_target_properties(keystore PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS ON
)

message(STATUS "Keystore library target created: keystore")
