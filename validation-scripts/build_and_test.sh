#!/bin/bash
# Local reproduction of the "test" job in .github/workflows/build-validation.yml
# (linux-gcc / linux-clang presets; run on Linux or WSL).
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "${script_dir}/common.sh"

project_root_dir="$(project_root)"
preset="${1:-${PRESET:-linux-gcc}}"
build_dir="${project_root_dir}/build/${preset}"
clean_build="${CLEAN_BUILD:-1}"
build_integration_tests="${BUILD_INTEGRATION_TESTS:-ON}"

case "${preset}" in
  linux-gcc)
    c_compiler="${CC:-gcc-13}"
    cxx_compiler="${CXX:-g++-13}"
    ;;
  linux-clang)
    c_compiler="${CC:-clang-18}"
    cxx_compiler="${CXX:-clang++-18}"
    ;;
  windows-mingw)
    c_compiler="${CC:-gcc}"
    cxx_compiler="${CXX:-g++}"
    ;;
  *)
    log "Unknown preset '${preset}'. Expected linux-gcc, linux-clang, or windows-mingw."
    exit 1
    ;;
esac

require_command cmake "Install CMake 3.20+."
require_command ctest "CTest ships alongside CMake."

if [[ "${clean_build}" == "1" ]]; then
  rm -rf "${build_dir}"
fi

log "Configuring preset '${preset}' (${c_compiler} / ${cxx_compiler})"
cmake --preset "${preset}" \
  -DCMAKE_C_COMPILER="${c_compiler}" \
  -DCMAKE_CXX_COMPILER="${cxx_compiler}" \
  -DBUILD_INTEGRATION_TESTS="${build_integration_tests}"

log "Building"
cmake --build --preset "${preset}" --parallel

log "Running unit tests"
ctest --test-dir "${build_dir}" --output-on-failure -R '^unit_tests$' --no-tests=error

log "Running concurrency test"
ctest --test-dir "${build_dir}" --output-on-failure -R '^concurrency_test$' --no-tests=error

log "Running remaining integration tests (if any)"
ctest --test-dir "${build_dir}" --output-on-failure \
  -E '^(unit_tests|concurrency_test)$' --no-tests=ignore
