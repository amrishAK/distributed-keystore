# CI/CD Pipelines

Reference guide for continuous integration, validation, benchmarking, and release automation.

## Table of Contents

- [Overview](#overview)
- [Pipeline Architecture](#pipeline-architecture)
- [build-and-validate](#build-and-validate-main-validation-gate)
- [benchmark](#benchmark-performance-validation)
- [build-and-package](#build-and-package-release-workflow)
- [Artifacts and Outputs](#artifacts-and-outputs)
- [Troubleshooting](#troubleshooting)

---

## Overview

The keystore CI/CD system consists of four workflows:

| Workflow | Trigger | Purpose | Audience |
|----------|---------|---------|----------|
| **build-and-validate** | Push to main/master, PR | Validate builds and tests on all platforms | Contributors, maintainers |
| **benchmark** | Manual dispatch, called by build-and-package | Measure performance and validate targets | Maintainers, release engineers |
| **build-and-package** | Manual dispatch | Create and publish release artifacts | Release engineers |

**Key invariant:** All workflows validate that the code compiles cleanly, passes tests, meets coverage thresholds, and runs correctly with sanitizers enabled. Release builds additionally validate performance baselines.

---

## Pipeline Architecture

### build-and-validate Flow

Triggered on every push/PR to main. Acts as the automated quality gate.

```
     ┌──────────────────────────────┐
     │   Event: push to main/       │
     │   master or PR to main       │
     └────────────┬─────────────────┘
                  │
                  ▼
     ┌──────────────────────────────────┐
     │ Call reusable workflow:          │
     │ build-validation.yml             │
     └────────────┬────────────────────┘
                  │
         ┌────────┼───────┬─────────┐
         │        │       │         │
         ▼        ▼       ▼         ▼
    ┌─────────┐ ┌─────┐ ┌──────┐ ┌────────┐
    │ Linux   │ │Linux│ │Windows│ │Coverage│
    │ GCC     │ │Clang│ │ MinGW │ │ Report │
    │ Tests   │ │Tests│ │ Tests │ │ (GCC)  │
    └────┬────┘ └──┬──┘ └───┬───┘ └───┬────┘
         │         │        │        │
         └─────────┼────────┼────────┘
                   │        │
                   ▼        ▼
          ┌──────────────────────────┐
          │  All tests passing?      │
          │  Coverage ≥ 85%?         │
          │  Sanitizers clean?       │
          └────────────┬─────────────┘
                       │
         ┌─────────────┴─────────────┐
         │                           │
      ✅ PASS               ❌ FAIL
         │                           │
         ▼                           ▼
    PR status           PR blocked, logs
    succeeds            available for download
```

---

### build-validation Flow (Reusable)

Called by build-and-validate and build-and-package. Runs comprehensive tests across all platforms.

```
     ┌───────────────────────────────────┐
     │ Workflow input: None              │
     │ Called from: parent workflow      │
     └────────────┬──────────────────────┘
                  │
                  ▼
         ┌────────────────────────┐
         │ Matrix: 3 platforms    │
         │ • Linux GCC 13         │
         │ • Linux Clang 18       │
         │ • Windows MinGW        │
         └────────────┬───────────┘
                      │
          ┌───────────┴───────────┐
          │                       │
          ▼                       ▼
     Platform 1             Platform N
     ┌──────────────┐       ┌──────────────┐
     │ 1. Checkout  │       │ 1. Checkout  │
     │ 2. Toolchain │       │ 2. Toolchain │
     │ 3. CMake     │       │ 3. CMake     │
     │ 4. Build     │       │ 4. Build     │
     │ 5. Unit test │       │ 5. Unit test │
     │ 6. Concur    │       │ 6. Concur    │
     │ 7. Integr    │       │ 7. Integr    │
     │ 8. Upload    │       │ 8. Upload    │
     └────────┬─────┘       └────────┬─────┘
              │                     │
              └──────────┬──────────┘
                         │
                    ┌────▼──────┐
                    │ All tests  │
                    │ pass?      │
                    └────┬───────┘
                         │
         ┌───────────────┴───────────────┐
         │                               │
         ▼                               ▼
   ┌──────────────────┐          (Tests failed)
   │ Coverage job     │      Upload test logs
   │ (GCC only)       │      (GCC only)
   │ • Config debug   │
   │ • Run full suite │
   │ • gcovr report   │
   │ • Check ≥ 85%    │
   │ • Upload report  │
   └────────┬─────────┘
            │
   ┌────────▼─────────────┐
   │ Sanitizer jobs       │
   │ (GCC only, async)    │
   │                      │
   │ ASan + UBSan         │
   │ TSan                 │
   │ (parallel)           │
   └────────┬─────────────┘
            │
   ┌────────┴─────────┐
   │                  │
   ▼                  ▼
✅ ALL PASS      ❌ ANY FAIL
```

---

### benchmark Flow

Measures performance and validates against baselines. Can be run standalone or called by build-and-package.

```
     ┌──────────────────────────┐
     │ Trigger: Manual dispatch │
     │ or called by release     │
     │ workflow                 │
     └────────────┬─────────────┘
                  │
                  ▼
         ┌─────────────────────┐
         │ Linux GCC (Release) │
         │ • Canonical baseline│
         └────────┬────────────┘
                  │
                  ▼
     ┌────────────────────────────┐
     │ 1. Checkout repo           │
     │ 2. Setup toolchain         │
     │ 3. CMake -DBUILD_BENCHM… │
     │ 4. Build benchmarks        │
     │ 5. Run single-threaded     │
     │ 6. Run multi-threaded      │
     │    (2K threads × 2K keys)  │
     │ 7. Output: .jsonl files    │
     └────────┬───────────────────┘
              │
              ▼
     ┌─────────────────────────┐
     │ validate.py             │
     │ Check: ops/sec, latency │
     │ vs baseline targets     │
     └────────┬────────────────┘
              │
     ┌────────┴────────┐
     │                 │
     ▼                 ▼
  ✅ Pass          ❌ Fail
  Upload          (Perf regression)
  results         Upload logs
```

---

### build-and-package Flow (Release Workflow)

Complete release process: validate → test → benchmark → package → publish. Runs sequentially, each stage gates the next.

```
     ┌────────────────────────────────┐
     │ Trigger: Manual dispatch with  │
     │ version input (e.g., 1.0.0)    │
     └────────────┬───────────────────┘
                  │
                  ▼
     ┌─────────────────────────────────┐
     │ Job 1: release-metadata         │
     │                                 │
     │ Check:                          │
     │ • From main branch?             │
     │ • Valid semver format?          │
     │ • Version matches CMakeLists    │
     │ • Tag doesn't exist?            │
     │ Output: validated version       │
     └────────────┬────────────────────┘
                  │
        ┌─────────▼─────────┐
        │ GATE 1: Metadata  │
        │ valid?            │
        └─────────┬─────────┘
                  │
          ❌      │      ✅ (version output)
                  │
                  ▼
     ┌─────────────────────────────────┐
     │ Job 2: build-and-test           │
     │ (calls build-validation.yml)    │
     │                                 │
     │ • Linux GCC tests               │
     │ • Linux Clang tests             │
     │ • Windows MinGW tests           │
     │ • Coverage report (GCC)         │
     │ • Sanitizer runs (GCC)          │
     └────────────┬────────────────────┘
                  │
        ┌─────────▼─────────────┐
        │ GATE 2: All tests     │
        │ pass + ≥85% coverage? │
        └─────────┬─────────────┘
                  │
          ❌      │      ✅
                  │
                  ▼
     ┌─────────────────────────────────┐
     │ Job 3: benchmark                │
     │ (calls benchmark.yml)           │
     │                                 │
     │ • Single-thread perf            │
     │ • Multi-thread perf             │
     │ • Validate vs baselines         │
     │ • Results: .jsonl files         │
     └────────────┬────────────────────┘
                  │
        ┌─────────▼──────────┐
        │ GATE 3: Perf       │
        │ targets met?       │
        └─────────┬──────────┘
                  │
          ❌      │      ✅
                  │
                  ▼
     ┌─────────────────────────────────┐
     │ Job 4: package                  │
     │                                 │
     │ a) Build GCC release:           │
     │    • Config, build, test        │
     │    • cmake install              │
     │    • package-consumer test      │
     │    • Create tarball (.gz)       │
     │                                 │
     │ b) Build Clang release:         │
     │    • Same as GCC                │
     │                                 │
     │ c) Create source package:       │
     │    • git archive to .tar.gz     │
     │                                 │
     │ d) Generate SHA256SUMS          │
     │                                 │
     │ Output:                         │
     │ • distributed-keystore-...-    │
     │   linux-gcc-x64.tar.gz          │
     │ • distributed-keystore-...-    │
     │   linux-clang-x64.tar.gz        │
     │ • distributed-keystore-...-    │
     │   source.tar.gz                 │
     │ • SHA256SUMS                    │
     └────────────┬────────────────────┘
                  │
        ┌─────────▼──────────────┐
        │ GATE 4: Artifacts      │
        │ created?               │
        └─────────┬──────────────┘
                  │
          ❌      │      ✅
                  │
                  ▼
     ┌──────────────────────────────────┐
     │ Job 5: release                   │
     │ (Publish to GitHub)              │
     │                                  │
     │ • Download assets                │
     │ • Create GitHub release          │
     │ • Tag: v{VERSION}                │
     │ • Attach tarballs, checksums     │
     │ • Mark prerelease if v*-*        │
     │ • Generate release notes         │
     └────────────┬─────────────────────┘
                  │
        ┌─────────▼────────────────┐
        │ GATE 5: Token has        │
        │ contents:write?          │
        └─────────┬────────────────┘
                  │
          ❌      │      ✅
                  │
                  ▼
     ┌──────────────────────────────┐
     │ Release published on GitHub   │
     └──────────────────────────────┘
```

---

## build-and-validate: Main Validation Gate

**Trigger:** Push to `main` or `master`, pull request to `main` or `master`, manual dispatch  
**Duration:** ~10–15 minutes  
**Audience:** Contributors (automatic on PR), maintainers

### Responsibilities

Calls `build-validation.yml` (reusable) to run on all target platforms. Acts as the main quality gate for every code change.

### Output

- Pass/fail status on the pull request
- Test logs and coverage artifacts
- Sanitizer reports (ASan, UBSan, TSan)

---

## build-validation: Reusable Test & Coverage Workflow

**Called by:** build-and-validate.yml, build-and-package.yml  
**Duration:** ~20 minutes per platform  
**Platform matrix:** Linux GCC, Linux Clang, Windows MinGW

### Jobs

#### 1. Multi-Platform Compilation & Testing

Runs in parallel across three configurations:

| Platform | Compiler | Preset | C/C++ | Notes |
|----------|----------|--------|-------|-------|
| Linux | GCC 13 | linux-gcc | gcc-13 / g++-13 | Primary target |
| Linux | Clang 18 | linux-clang | clang-18 / clang++-18 | Alternative target |
| Windows | MinGW | windows-mingw | gcc (MinGW) | Cross-platform validation |

**Steps per platform:**

1. Checkout repository
2. Set up toolchain (custom action: `.github/actions/setup-toolchain`)
3. Configure with CMake preset and integration tests enabled
4. Build with parallel jobs
5. Run unit tests (`ctest -R '^unit_tests$'`)
6. Run concurrency stress test (`ctest -R '^concurrency_test$'`)
7. Run integration tests (all others)
8. Upload test logs if any failures occur

**Test suite:**
- Unit tests: Core functionality and data structure correctness
- Concurrency tests: Multi-threaded stress tests
- Integration tests: End-to-end workflows

**Failure behavior:** `fail-fast: false` — all platforms complete even if one fails.

#### 2. Coverage Report (Linux GCC only)

Runs after tests complete successfully.

**Steps:**

1. Checkout repository
2. Configure with Debug build + coverage flags (`-DBUILD_COVERAGE=ON`)
3. Run full test suite
4. Generate coverage with `gcovr`:
   - Filters to `src/keystore/` only
   - HTML and XML reports
   - **Minimum threshold: 85% line coverage** (fails if below)
5. Upload coverage artifacts

**Output:**
- `coverage/index.html` — detailed interactive coverage report
- `coverage/coverage.xml` — machine-readable summary

**Why 85%?** Balances practical test coverage with maintainability. See [docs/DESIGN_DECISIONS.md](./DESIGN_DECISIONS.md) for rationale.

#### 3. Sanitizer Suite (Linux GCC only)

Runs in parallel for two configurations:

| Sanitizer | Flags | Detects |
|-----------|-------|---------|
| **ASan + UBSan** | `address,undefined` | Memory leaks, use-after-free, undefined behavior |
| **TSan** | `thread` | Data races and concurrency bugs |

**Steps per sanitizer:**

1. Checkout repository
2. Configure with Debug build + sanitizer flags (`-DENABLE_SANITIZERS=<type>`)
3. Build
4. Run tests with sanitizer runtime enabled (`ASAN_OPTIONS=halt_on_error=1:detect_leaks=1`, etc.)
5. Upload logs if failures occur

**Failure behavior:** Any sanitizer error halts the test (return code 1). Logs are uploaded for analysis.

**Why sanitizers?**
- Catch memory safety issues before production
- Detect concurrency bugs under load
- Complement static analysis and manual review

### Artifacts Produced

- `test-logs-{preset}` — CTest logs and failure details (all platforms)
- `coverage-linux-gcc` — HTML and XML coverage reports (GCC only)
- `sanitizer-test-logs-{sanitizers}` — Sanitizer output (GCC only)

---

## benchmark: Performance Validation

**Trigger:** Manual dispatch, called by build-and-package.yml  
**Duration:** ~20–45 minutes (single-thread + multi-thread suites)  
**Platform:** Linux GCC (canonical performance baseline)

### Responsibilities

Measures throughput and latency across single-threaded and multi-threaded scenarios. Validates that performance meets or exceeds target baselines.

### Steps

1. Checkout repository
2. Set up toolchain
3. Configure with Release build + benchmarks enabled (`-DBUILD_BENCHMARKS=ON`)
4. Build benchmarks
5. Run benchmarks (output: `build/linux-gcc/benchmark-results/`)
   - Single-threaded suite
   - Multi-threaded suite (2,000 threads × 2,000 keys)
6. Validate against performance targets: `python3 benchmark/scripts/validate.py`
7. Upload results

### Performance Targets

Minimum acceptable values defined in `benchmark/scripts/validate.py`:

- **Single-thread throughput:** Target baseline (varies by operation)
- **Multi-thread throughput:** Target baseline (2,000 threads × 2,000 keys × 4M ops)
- **Latency (p50, p99):** Target baseline

If actual results fall below targets, the workflow fails. This prevents performance regressions.

### Artifacts Produced

- `benchmark-results-linux-gcc` — Results in `.jsonl` format, structured logs

---

## build-and-package: Release Workflow

**Trigger:** Manual dispatch with version input  
**Duration:** ~45–60 minutes (sequential: metadata → build-validation → benchmark → package → release)  
**Platforms:** Linux (GCC, Clang), source archive

### Responsibilities

Validates, builds, benchmarks, packages, and publishes a release to GitHub Releases.

### Job Sequence

#### 1. release-metadata: Validate Release Metadata

**Preconditions checked:**

- Dispatch is from `main` branch only (no side branches)
- Version format is valid semver: `X.Y.Z` or `X.Y.Z-PRERELEASE` (e.g., `1.0.0-rc.1`)
- Declared version in CMakeLists.txt matches input version
- Git tag does not already exist for this version

**Output:** Validated version string passed to downstream jobs.

**Failure paths:**
- Not on main → rejected
- Invalid semver → rejected
- Version mismatch → rejected
- Tag exists → rejected

#### 2. build-and-test: Full Validation

Calls `build-validation.yml` to run the complete test matrix (Linux GCC, Linux Clang, Windows MinGW).

**Gate:** All tests must pass to proceed to packaging.

#### 3. benchmark: Performance Validation

Calls `benchmark.yml` standalone.

**Gate:** Performance targets must be met to proceed to packaging.

#### 4. package: Create Release Assets

Runs after both build-and-test and benchmark pass. Creates distributable packages for GCC and Clang, plus source archive.

**Steps:**

1. Build GCC Release package:
   - Configure with Release flags
   - Build all targets
   - Run tests
   - Install to staging directory
   - Build and run package-consumer test (verifies CMake integration)
   - Create tarball: `distributed-keystore-{VERSION}-linux-gcc-x64.tar.gz`

2. Build Clang Release package:
   - Same steps as GCC for clang-18

3. Create source package:
   - `git archive` HEAD to tarball: `distributed-keystore-{VERSION}-source.tar.gz`

4. Generate checksums:
   - `sha256sum` for all `.tar.gz` files → `SHA256SUMS`

**Output:** Three tarballs per release:
- `distributed-keystore-{VERSION}-linux-gcc-x64.tar.gz` (~ 5–10 MB)
- `distributed-keystore-{VERSION}-linux-clang-x64.tar.gz` (~ 5–10 MB)
- `distributed-keystore-{VERSION}-source.tar.gz` (~ 1–2 MB)
- `SHA256SUMS`

#### 5. release: Publish to GitHub

Runs after packaging completes.

**Steps:**

1. Download release assets
2. Create GitHub release:
   - Tag: `v{VERSION}`
   - Assets: all tarballs, checksums
   - Pre-release flag: set if version contains `-` (e.g., `-rc.1`)
   - Release notes: auto-generated from commits since last tag

**Gate:** Requires `contents: write` permission (workflow token).

---

## Artifacts and Outputs

### Persistent Artifacts (30-day retention)

All workflows upload results to GitHub Actions artifacts:

| Artifact | Produced by | Contains | Retention |
|----------|-------------|----------|-----------|
| `test-logs-{preset}` | build-validation | CTest output, failure details | 30 days |
| `coverage-linux-gcc` | build-validation (coverage job) | HTML/XML coverage reports | 30 days |
| `sanitizer-test-logs-{sanitizers}` | build-validation (sanitizer job) | Sanitizer output and reports | 30 days |
| `benchmark-results-linux-gcc` | benchmark | Perf metrics (.jsonl), logs | 30 days |
| `release-assets-{VERSION}` | build-and-package | Tarballs, SHA256SUMS | 30 days |

### Release Artifacts (Published to GitHub Releases)

Created by `build-and-package` and published permanently:

- `distributed-keystore-{VERSION}-linux-gcc-x64.tar.gz`
- `distributed-keystore-{VERSION}-linux-clang-x64.tar.gz`
- `distributed-keystore-{VERSION}-source.tar.gz`
- `SHA256SUMS`

---

## Troubleshooting

### PR Checks Failing

**Check:** Does the PR have a failing status badge for `build-and-validate`?

1. Click the badge or "Details" on the PR check
2. Download `test-logs-*` artifacts for your platform
3. Check for:
   - Compilation errors (scroll to build output)
   - Test failures (search for `FAIL`)
   - Sanitizer errors (search for `ERROR` or `SUMMARY`)

**Common issues:**

| Error | Cause | Fix |
|-------|-------|-----|
| `CMake configuration failed` | Toolchain not found or incompatible | Verify CMakePresets.json and compiler version |
| `Test failed: SIGSEGV` | Crash in test | Check test logs, run locally with gdb |
| `Sanitizer: heap-use-after-free` | Memory safety bug | Add to .gitignore, suppress, or fix |
| `Coverage below 85%` | New code not tested | Add unit tests to `tests/for_c/unit_tests/` |

### Benchmark Validation Failing

**Check:** Does `benchmark.yml` fail at the "Validate performance targets" step?

1. Download `benchmark-results-linux-gcc` artifact
2. Review `benchmark.log` and `.jsonl` result files
3. Compare against baseline in `benchmark/scripts/validate.py`

**Common causes:**

- System was under load (GitHub Actions can be variable)
- Code change introduced performance regression
- Baseline targets need adjustment (rare)

**Action:** Re-run the benchmark workflow. If it consistently fails, investigate code changes for unnecessary allocations, locks, or algorithmic regressions.

### Release Workflow Failing

**At metadata validation:**

- **"Must be dispatched from main"** → Ensure dispatch is from main branch, not a feature branch
- **"Invalid version"** → Use semver: `1.0.0`, `1.0.0-rc.1`, `1.0.0-beta.2`, etc.
- **"Does not match declared version"** → Update `DISTRIBUTED_KEYSTORE_VERSION` in CMakeLists.txt before dispatch
- **"Tag already exists"** → Version already released; increment and try again

**At build-and-test or benchmark:**

- Check outputs of those workflows (same as above)

**At packaging:**

- Download `release-test-logs-*` artifacts
- Check CMake install and package-consumer test for errors
- Verify tarball was created: check `release-assets-*` artifact

**At release publication:**

- Usually a permissions issue; verify workflow has `contents: write` permission
- Check GitHub token is valid (should be auto-provided)

---

## Platform and Compiler Support

### Tested Configurations

| OS | Compiler | Preset | Status | Notes |
|----|----------|--------|--------|-------|
| Linux | GCC 13 | linux-gcc | ✅ Primary | Canonical performance baseline |
| Linux | Clang 18 | linux-clang | ✅ Supported | Alternative for developers |
| Windows | MinGW (GCC) | windows-mingw | ✅ Supported | Cross-platform validation |

### Coverage Requirements

- **Line coverage:** ≥ 85% (enforced)
- **Branch coverage:** Monitored but not enforced
- **Excludes:** External headers, test utilities

### Concurrency Validation

- **ASan + UBSan:** Detects memory and undefined-behavior bugs
- **TSan:** Detects data races under 2,000-thread load
- **Integration test:** Concurrent ops at scale (see `tests/for_c/integration_test/`)

---

## Related Documentation

- [BUILD.md](./BUILD.md) — Local build and test procedures
- [API.md](../API.md) — Public API and usage examples
- [docs/benchmarks.md](./benchmarks.md) — Benchmark results and methodology
- [DESIGN_DECISIONS.md](./DESIGN_DECISIONS.md) — Rationale for pipeline thresholds and choices
