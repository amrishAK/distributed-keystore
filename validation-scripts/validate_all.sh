#!/bin/bash
# Orchestrates the scripts in this directory to validate a change before
# committing/pushing. Mirrors the jobs in
# .github/workflows/build-validation.yml. Run on Linux or WSL.
#
# Usage:
#   validation-scripts/validate_all.sh          # fast: linux-gcc build + unit + concurrency
#   validation-scripts/validate_all.sh full     # fast checks + clang build + sanitizers + coverage
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
mode="${1:-fast}"

case "${mode}" in
  fast)
    "${script_dir}/build_and_test.sh" linux-gcc
    ;;
  full)
    "${script_dir}/build_and_test.sh" linux-gcc
    "${script_dir}/build_and_test.sh" linux-clang
    SANITIZERS=address,undefined "${script_dir}/sanitizer_check.sh"
    if is_wsl; then
      log "Skipping ThreadSanitizer under WSL2; run the TSan job on native Linux"
    else
      SANITIZERS=thread "${script_dir}/sanitizer_check.sh"
    fi
    "${script_dir}/coverage_report.sh"
    ;;
  *)
    echo "Usage: $0 [fast|full]" >&2
    exit 1
    ;;
esac

echo "[validate] Validation (${mode}) passed." >&2
