#!/bin/bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "${script_dir}/common.sh"

project_root_dir="$(project_root)"
build_dir="${project_root_dir}/build/sanitizer"
requested_sanitizers="${SANITIZERS:-}"
if is_wsl && [[ -z "${requested_sanitizers}" ]]; then
  sanitizers="address,undefined"
  log "WSL2 detected; defaulting to AddressSanitizer/UBSan because GCC ThreadSanitizer is unsupported in this runtime"
elif is_wsl && [[ "${requested_sanitizers}" == *thread* ]]; then
  log "ThreadSanitizer is unsupported under WSL2; use SANITIZERS=address,undefined or run TSan on native Linux"
  exit 1
else
  sanitizers="${requested_sanitizers:-thread}"
fi
c_compiler="${CC:-gcc-13}"
cxx_compiler="${CXX:-g++-13}"
clean_build="${CLEAN_BUILD:-1}"

require_command cmake "Install CMake 3.20+."
require_command ctest "CTest ships alongside CMake."

if [[ "${clean_build}" == "1" ]]; then
  rm -rf "${build_dir}"
fi

log "Configuring sanitizer build (sanitizers=${sanitizers}, ${c_compiler} / ${cxx_compiler})"
cmake -S "${project_root_dir}" -B "${build_dir}" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER="${c_compiler}" \
  -DCMAKE_CXX_COMPILER="${cxx_compiler}" \
  -DBUILD_UNIT_TESTS=ON \
  -DBUILD_INTEGRATION_TESTS=ON \
  -DBUILD_BENCHMARKS=OFF \
  -DENABLE_SANITIZERS="${sanitizers}"

log "Building"
cmake --build "${build_dir}" --parallel

if [[ -n "${SANITIZER_RUNTIME_OPTIONS:-}" ]]; then
  export "${SANITIZER_RUNTIME_OPTIONS}"
elif [[ "${sanitizers}" == *thread* ]]; then
  export TSAN_OPTIONS="${TSAN_OPTIONS:-halt_on_error=1}"
elif [[ "${sanitizers}" == *address* || "${sanitizers}" == *undefined* ]]; then
  export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=1:detect_leaks=1}"
fi
log "Running tests"

ctest --test-dir "${build_dir}" --output-on-failure
