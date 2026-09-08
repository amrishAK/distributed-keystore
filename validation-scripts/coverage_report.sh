#!/bin/bash
# Local reproduction of the "coverage" job in
# .github/workflows/build-validation.yml (Linux/WSL only; requires gcovr).
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "${script_dir}/common.sh"

project_root_dir="$(project_root)"
build_dir="${project_root_dir}/build/coverage-gcc"
c_compiler="${CC:-gcc-13}"
cxx_compiler="${CXX:-g++-13}"
gcov_executable="${GCOV:-gcov-13}"
fail_under_line="${FAIL_UNDER_LINE:-85}"
clean_build="${CLEAN_BUILD:-1}"

require_command cmake "Install CMake 3.20+."
require_command gcovr "Install with: python3 -m pip install gcovr==8.3"

if [[ "${clean_build}" == "1" ]]; then
  rm -rf "${build_dir}"
fi

log "Configuring coverage build (${c_compiler} / ${cxx_compiler})"
cmake -S "${project_root_dir}" -B "${build_dir}" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER="${c_compiler}" \
  -DCMAKE_CXX_COMPILER="${cxx_compiler}" \
  -DBUILD_UNIT_TESTS=ON \
  -DBUILD_INTEGRATION_TESTS=ON \
  -DBUILD_BENCHMARKS=OFF \
  -DBUILD_COVERAGE=ON

log "Building"
cmake --build "${build_dir}" --parallel

log "Running tests"
ctest --test-dir "${build_dir}" --output-on-failure

log "Generating coverage report (fail-under-line=${fail_under_line})"
gcovr \
  --root "${project_root_dir}" \
  --object-directory "${build_dir}" \
  --filter src/keystore/ \
  --gcov-executable "${gcov_executable}" \
  --gcov-ignore-parse-errors negative_hits.warn_once_per_file \
  --html-details "${build_dir}/coverage/index.html" \
  --xml "${build_dir}/coverage/coverage.xml" \
  --print-summary \
  --fail-under-line "${fail_under_line}" \
  "${build_dir}"
