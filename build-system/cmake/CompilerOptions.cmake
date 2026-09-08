include_guard(GLOBAL)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
  add_compile_definitions(_POSIX_C_SOURCE=200809L)
endif()

if(BUILD_COVERAGE)
  if(NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "BUILD_COVERAGE requires GCC and gcov")
  endif()

  add_compile_options(--coverage -O0 -g)
  add_link_options(--coverage)
endif()

if(ENABLE_SANITIZERS)
  if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    message(FATAL_ERROR "ENABLE_SANITIZERS requires GCC or Clang")
  endif()

  add_compile_options(-fsanitize=${ENABLE_SANITIZERS} -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=${ENABLE_SANITIZERS})
endif()
