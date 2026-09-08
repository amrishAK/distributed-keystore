# Validation Scripts

Local wrappers for checks that mirror the repository's GitHub Actions
workflows (`.github/workflows/build-validation.yml`). Run these before
committing/pushing to catch failures early instead of waiting on CI.

| Script | Platform | Mirrors |
|---|---|---|
| `validate_all.sh [fast\|full]` | Linux / WSL | Orchestrates the scripts below |
| `validate_windows.ps1 [-Clean]` | Windows (native, MinGW) | "Windows MinGW" matrix job |
| `build_and_test.sh [preset]` | Linux / WSL | "test" matrix job (build + ctest) |
| `coverage_report.sh` | Linux / WSL | "coverage" job (gcovr, fail-under-line 85) |
| `sanitizer_check.sh` | Linux / WSL | "sanitizer" job (ASan/UBSan, TSan) |
| `common.sh` | n/a | Shared helpers sourced by the `*.sh` scripts above |

## Quick Start

**Windows (native MinGW, no WSL required):**

```powershell
validation-scripts\validate_windows.ps1
```

Runs the `windows-mingw` preset build (with `BUILD_INTEGRATION_TESTS=ON`,
same as CI) plus `unit_tests` and `concurrency_test`. Add `-Clean` to force a
fresh configure.

**Linux / WSL:**

```bash
bash validation-scripts/validate_all.sh          # fast: linux-gcc build + unit + concurrency tests
bash validation-scripts/validate_all.sh full     # + linux-clang, ASan/UBSan, TSan, coverage (>=85% lines)
```

Use `fast` as your everyday pre-commit check and `full` before opening a pull
request, since sanitizer and coverage runs take several minutes. Note that
`concurrency_test` itself is a stress test and can take a while to finish on
any platform — this is expected, not a hang.

## Individual Checks

Build and test a single preset (`linux-gcc`, `linux-clang`, or
`windows-mingw` under WSL/MSYS2):

```bash
bash validation-scripts/build_and_test.sh linux-gcc
bash validation-scripts/build_and_test.sh linux-clang
```

Run the sanitizer build with a chosen sanitizer set:

```bash
SANITIZERS=address,undefined bash validation-scripts/sanitizer_check.sh
SANITIZERS=thread bash validation-scripts/sanitizer_check.sh
```

On WSL2, running the script without `SANITIZERS` defaults to ASan/UBSan because
GCC ThreadSanitizer cannot reserve its shadow memory in the WSL2 runtime. An
explicit `SANITIZERS=thread` request is rejected; run TSan on native Linux or
in CI instead. `validate_all.sh full` automatically skips TSan on WSL2.

Toolchain and runtime options can be overridden when necessary:

```bash
CC=gcc-13 CXX=g++-13 \
SANITIZER_RUNTIME_OPTIONS='TSAN_OPTIONS=halt_on_error=1' \
bash validation-scripts/sanitizer_check.sh
```

Generate a coverage report (requires `gcovr`: `python3 -m pip install gcovr==8.3`):

```bash
bash validation-scripts/coverage_report.sh
```

## Common Overrides

All scripts respect these environment variables where applicable:

- `CC` / `CXX` — override the compiler (defaults match CI: `gcc-13`/`g++-13`
  for GCC, `clang-18`/`clang++-18` for Clang).
- `CLEAN_BUILD=0` — reuse the existing build directory instead of removing it
  first (default `1`, matching the clean CI runner).
- `SANITIZER_RUNTIME_OPTIONS` — override sanitizer runtime options passed to
  `sanitizer_check.sh`, e.g. `SANITIZER_RUNTIME_OPTIONS='TSAN_OPTIONS=halt_on_error=1'`.
- `FAIL_UNDER_LINE` — override the coverage line threshold in
  `coverage_report.sh` (default `85`, matching CI).

The GitHub Actions workflows remain the canonical CI checks; these scripts are
local reproductions of the same jobs.
