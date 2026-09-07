# KeyStore

A high-performance, concurrent, in-memory key-value store written in C (C11). Features a two-level hash table with fine-grained locking, lock-free resize signalling, and a background chase buffer worker for safe concurrent resizing. Designed for throughput, correctness, and scalability.

**Key Characteristics:**
- **Two-level hash table** → independent hashing at each level for uniform key distribution
- **Fine-grained locking** → per-bucket RW-locks + per-node mutexes enable intra-bucket parallelism
- **Safe concurrent resizing** → snapshot + chase buffer + background worker = zero data loss
- **Soft-delete semantics** → lazy physical cleanup, reduced latency under high concurrency
- **Custom memory pool** → fast allocation for collision chains
- **Thoroughly tested** → 90%+ coverage, stress-tested at 2,000 threads × 2,000 keys (8M ops, 4.3M ops/sec)


## Key Features

- **Thread-Safe and Concurrent**
    - Per-bucket read-write locks (`pthread_rwlock_t`) and per-data-node mutexes (`pthread_mutex_t`)
    - Lock-free chase buffer for concurrent resize
    - Concurrency can be enabled/disabled via configuration
- **Automatic, Safe Resizing**
    - Hash tables and sub-buckets double in size automatically as needed
    - Background resize worker and chase buffer ensure no data loss or race conditions
- **Flexible Value Support**
    - Store any C data type: int, float, double, string, bytes, structs, enums, etc.
- **Custom Memory Pool**
    - Fast, efficient memory pool for all `linked_list_node` allocations
    - Falls back to standard malloc if pool is exhausted
- **Comprehensive Testing**
    - 90%+ code coverage with Unity-based unit tests and integration stress tests
    - Stress-tested for thread safety and performance (120 threads × 150 keys, with p50/p99 latency tracking)
- **Detailed Error Handling**
    - All functions return clear error codes (see [ERROR_CODES.md](./ERROR_CODES.md))
- **Built-in Statistics**
    - Runtime stats: key counts, memory use, operation counters, per-error-code counters
- **Modular and Maintainable**
    - Clean separation of core logic, data structures, memory management, and tests
    - Easy to extend for new features or data types
- **Scalable and Proven**
    - Handles thousands of threads and millions of operations with no data loss


## Architecture Overview

KeyStore is built from modular layers: an entry API → top-level hash table → sub-tables → collision chains. All operations use a two-phase lock protocol (Phase 1: traverse under RW-lock, Phase 2: modify under node mutex) enabling parallel updates on different nodes within the same bucket.

**Key Design Principles:**
- **Two-level hashing** with independent seeds improves key distribution
- **Two-phase locking** enables intra-bucket parallelism: Phase 2 (node update) can run in parallel across different nodes
- **Soft-delete semantics** with lazy physical cleanup reduce latency under high concurrency
- **Chase buffer** with background worker ensures safe concurrent resizing with zero data loss

**For Detailed Information:**
- [docs/architecture/01-overview.md](./docs/architecture/01-overview.md) — System shape and component responsibilities
- [docs/architecture/03-data-flow.md](./docs/architecture/03-data-flow.md) — How requests flow through the system
- [docs/architecture/07-concurrency-model.md](./docs/architecture/07-concurrency-model.md) — Lock hierarchy and two-phase protocol
- [docs/DESIGN_DECISIONS.md](./docs/DESIGN_DECISIONS.md) — Design rationale and known limitations

## Building and Testing

This project uses **CMake 3.20+** as the primary build system. For full build instructions, see [BUILD.md](./docs/BUILD.md) and [QUICK_REFERENCE.md](./QUICK_REFERENCE.md).

### Quick Start

```bash
# 1. Create build directory
mkdir build && cd build

# 2. Configure (all features enabled)
cmake -DBUILD_UNIT_TESTS=ON -DBUILD_INTEGRATION_TESTS=ON -DBUILD_BENCHMARKS=ON ..

# 3. Build everything
cmake --build .

# 4. Run tests
cmake --build . --target test
```

### Key Build Targets

| Command | Purpose |
|---------|---------|
| `cmake --build . --target test` | Build and run unit tests |
| `cmake --build . --target run-concurrency-test` | Run integration/stress test (2K threads × 2K keys) |
| `cmake --build . --target benchmark` | Build and run all benchmarks |
| `cmake --build . --target coverage-simple` | Generate code coverage reports |

### Platform Notes

- **Linux/macOS:** Fully supported with GCC/Clang
- **Windows WSL2:** Recommended (native Linux environment)
- **Windows MinGW:** Supported with pthread library
- **Windows MSVC:** Not supported (requires POSIX pthread APIs)

For detailed build configuration, compiler options, and troubleshooting, see **[BUILD.md](./docs/BUILD.md)**.

### Clean

Delete the selected CMake build directory and configure it again when a clean build is needed. For example:

```powershell
Remove-Item -Recurse -Force build\windows-mingw
```

See [QUICK_REFERENCE.md](./QUICK_REFERENCE.md) for preset-specific cleanup commands.

## Benchmarking

KeyStore includes a production benchmark suite (V2) with **63 executable scenarios**:

- **20 single-threaded scenarios (ST2)** for baseline throughput, latency, distribution behavior, resize spikes, and memory-pool ROI.
- **43 multi-threaded scenarios (MT2)** for scaling, contention, workload mix, resize correctness, and oversubscription stress.

### Benchmark At a Glance

- **Primary targets:** >=2M ops/sec (single-thread), >=28M ops/sec at 32 threads (multi-thread).
- **Correctness checks:** `failure_count == 0`, `missing_count == 0`, and post-run verification.
- **Outputs:** single-thread and multi-thread JSONL/HTML reports under `benchmark/.result/`.
- **Run history:** curated run summaries in `benchmark/runs/`.

### Build and Run

```bash
cd benchmark

# Single-thread benchmark + HTML report
make benchmark

# Multi-thread benchmark + HTML report
make benchmark-multi

# Run both suites
make benchmark-all
```

### Useful Benchmark Targets

```bash
# Build binaries only
make bin
make multi-build

# Run core scaling scenarios only (MT2-CORE)
make multi-run-core

# Run distribution stress scenarios only (MT2-DIST)
make multi-run-dist

# Run resize-heavy scenarios only (MT2-RSZ)
make multi-run-resize

# Run oversubscription scenario (up to very high thread counts)
make multi-run-high
```

### Result Artifacts

- **Raw results:** `benchmark/.result/*.jsonl`
- **Generated HTML reports:** `benchmark/.result/*.html`
- **Indexed run snapshots:** `benchmark/runs/INDEX.md`
- **Latest run snapshot example:** `benchmark/runs/2026-07-06-run-2/METRICS_SNAPSHOT.md`

### Benchmark Documentation

- [benchmark/docs/BENCHMARK.md](./benchmark/docs/BENCHMARK.md) — Complete benchmark spec, success criteria, scenario matrix (ST2 + MT2)
- [benchmark/runs/INDEX.md](./benchmark/runs/INDEX.md) — Historical run index and status tracking
- [docs/progress.md](./docs/progress.md) — Project-level benchmark progress and release tracking

### Performance Baselines (v1.0)

| Scenario | Throughput | Notes |
|----------|-----------|-------|
| **Single-threaded** | 2–2.5M ops/sec | 50/50 SET:GET, baseline config |
| **Multi-threaded (32 threads)** | ≥28M ops/sec | Efficient scaling target |
| **Native stress test** | **4.3M ops/sec** | 2K threads × 2K keys (8M ops, 7 resizes, 0 data loss) |
| **Valgrind clean** | 14K ops/sec | 304× overhead; 0 leaks, 0 races detected |

See [docs/progress.md](./docs/progress.md) for detailed benchmark results and [benchmark/docs/BENCHMARK.md](./benchmark/docs/BENCHMARK.md) for the complete benchmark specification.

## Example Output

```
Starting concurrency stress test...
Test scenario: Bucket-level concurrency with 2000 threads each setting/getting 2000 unique keys.
==== Concurrency Test Report ====
Initialization: bucket_size=1024, sub_bucket_size=1024, max_chain_length=15, concurrency_enabled=true
Total threads: 2000
Number of keys per thread: 2000
Set operations: 4000000, Get operations: 4000000
Total ops: 8000000
Total time: 1.862s
Throughput: 4295904.57 ops/sec
SET latency (ns): avg=3565, p50=1107, p95=3122, p99=9466
GET latency (ns): avg=1130, p50=302, p95=604, p99=4330
Key missing after set (bucket-level concurrency): 0
Number of resizings triggered: 7
Result: PASS
```

## Error & Success Codes

All functions return explicit error or success codes. Common codes:

| Code  | Name         | Description                  |
|-------|--------------|------------------------------|
| 0     | SUCCESS      | Operation completed successfully |
| -1    | ERR_FAILURE  | General/unspecified failure  |
| -20   | ERR_MEMORY_ALLOCATION_FAILED | Memory allocation failed |
| -30   | ERR_RW_LOCK_ACQUIRE_FAILED | RW lock acquire failed |
| -40   | ERR_HASH_COMPUTE_FAILED | Hash computation failure |
| -50   | ERR_HASH_TABLE_NOT_INITIALIZED | Hash table not initialized |
| -80   | ERR_DATA_NODE_NOT_FOUND | Data node not found |
| 10    | SUCESS_ADDED_NEW_NODE | New node inserted |
| 20    | SUCESS_ADDED_NEW_NODE_RESZING_TRIGGERED | New node inserted, resize triggered |

See [ERROR_CODES.md](./ERROR_CODES.md) for the complete list.

## Hashing Strategy

KeyStore uses **MurmurHash3 (64-bit)** with two independent per-instance seeds (generated via `clock_gettime(CLOCK_MONOTONIC)` and guaranteed distinct via XOR with the golden-ratio constant).

- **Bucket hash** — routes to top-level bucket (`bucket_hash % total_buckets`)
- **Sub-bucket hash** — routes within sub-table (`sub_bucket_hash & (sub_buckets - 1)`)

This dual-seed design eliminates correlation between level-1 and level-2 routing, improving key distribution under load. See [docs/architecture/06-hashing-strategy.md](./docs/architecture/06-hashing-strategy.md) for details.

## Design Decisions & Known Limitations

See [docs/DESIGN_DECISIONS.md](./docs/DESIGN_DECISIONS.md) for a detailed discussion of:

- **Soft-delete semantics** — lazy cleanup, use-after-free protection
- **Resize strategy** — always doubles, no shrinking
- **Memory pool scope** — linked list nodes only, with fallback to malloc
- **Dual-seed MurmurHash3** — independent routing, improved distribution
- **Two-phase lock protocol** — intra-bucket parallelism via phase release
- **Known limitations** — `key_hash == 0` rejection, no persistence, platform specifics, etc.

## Documentation & Resources

**Core Documentation:**
- [API.md](./API.md) — Public API reference with usage examples and error codes
- [ERROR_CODES.md](./ERROR_CODES.md) — Complete error and success code reference
- [QUICK_REFERENCE.md](./QUICK_REFERENCE.md) — Build and test quick-start
- [docs/BUILD.md](./docs/BUILD.md) — Detailed build configuration and troubleshooting
- [docs/DESIGN_DECISIONS.md](./docs/DESIGN_DECISIONS.md) — Design rationale, tradeoffs, known limitations

**Architecture & Deep Dives:**
- [docs/architecture/](./docs/architecture/) — Modular architecture documentation:
  - [01-overview.md](./docs/architecture/01-overview.md) — System overview
  - [03-data-flow.md](./docs/architecture/03-data-flow.md) — Request flow through the system
  - [07-concurrency-model.md](./docs/architecture/07-concurrency-model.md) — Lock hierarchy and two-phase protocol
  - [09-memory-management.md](./docs/architecture/09-memory-management.md) — Memory pools and allocation

**Project Tracking:**
- [docs/progress.md](./docs/progress.md) — v1.0 checklist, roadmap, known issues
- [benchmark/docs/BENCHMARK.md](./benchmark/docs/BENCHMARK.md) — Complete benchmark specification

**Examples:**
- [examples/main.c](./examples/main.c) — Simple usage example

## License

Apache 2.0 — see [LICENSE](./LICENSE) for full terms.

Copyright 2026 Amrish Arunachalam Kulasekaran

## Author & Maintainer

**Amrish Arunachalam Kulasekaran** — [@amrishAK](https://github.com/amrishAK)

This is a solo-maintained open source project. Bug reports and feedback are welcome via GitHub Issues. Pull requests may be accepted at maintainer discretion — please open an issue to discuss significant changes before submitting a PR.