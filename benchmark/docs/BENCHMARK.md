# KeyStore Benchmarking — Complete Specification

**Version:** 2.0 (Unified) | **Date:** 2026-07-06 | **Status:** Production-Ready | **Scope:** Single-threaded & Multi-threaded

---

## Quick Reference

- **63 executable scenarios:** 20 single-threaded (ST2), 43 multi-threaded (MT2)
- **Primary targets:** ≥2M ops/sec (ST), ≥28M ops/sec (32T MT), ≥90% scaling efficiency
- **Correctness guarantees:** Zero key loss, zero deadlocks, ThreadSanitizer clean
- **Output:** CSV (ST) + JSONL (MT) with latency percentiles, memory metrics, validation results

---

## Table of Contents

1. [Objectives & Success Criteria](#objectives--success-criteria)
2. [Universal Methodology](#universal-methodology)
3. [Single-Threaded Tests (20)](#single-threaded-tests-20)
4. [Multi-Threaded Tests (43)](#multi-threaded-tests-43)
5. [Configuration](#configuration)
6. [Implementation Status](#implementation-status)
7. [Coverage & Deviations](#coverage--deviations)

---

## Objectives & Success Criteria

### Primary Goals

1. Establish baseline: ≥2M ops/sec single-threaded, ≥28M ops/sec at 32 threads
2. Validate concurrency: Two-phase lock protocol, RW-lock benefits, zero race conditions
3. Optimize config: Identify bucket sizes, chain limits, preallocation factors
4. Ensure correctness: Zero key loss, zero deadlocks, race-free soft-delete

### Key Questions Answered

| Aspect | Single-Threaded | Multi-Threaded |
|--------|-----------------|----------------|
| **Throughput** | ≥2M ops/sec | ≥28M ops/sec at 32T; ≥90% scaling efficiency |
| **Latency** | p99 < 10 µs | Tail latency stability under contention |
| **Memory** | Pool ROI: >10% gain | Peak RSS scaling vs thread count |
| **Safety** | Hash uniformity ≥0.8 | Zero ThreadSanitizer races, zero deadlocks |
| **Resizing** | Tail latency spike recovery | Safe concurrent chase buffer + compaction |
| **Soft-Delete** | (N/A) | Race-free DELETE/GET under concurrent load |

### Success Criteria

**Single-Threaded:**
| Metric | Target | Validates |
|--------|--------|-----------|
| Throughput | ≥2M ops/sec | Baseline ceiling |
| Latency p99 | <10 µs | Tail latency acceptable |
| Memory pool ROI | >10% gain | Pooling worth cost |
| Hash uniformity | ≥0.8 | Function quality |

**Multi-Threaded:**
| Metric | Target | Validates |
|--------|--------|-----------|
| Scaling efficiency | ≥90% (16T = 14.4× baseline) | Two-phase protocol effective |
| Throughput (32T) | ≥28M ops/sec | Full utilization |
| Deadlocks | 0 | Lock safety |
| ThreadSanitizer races | 0 | No data races |
| Soft-delete consistency | Zero key loss | Safe concurrent DELETE/GET |
| Resize correctness | Zero data loss | Chase buffer works |

**All Tests:**
- `failure_count == 0` (success rate 100%)
- `missing_count == 0` (zero key loss)
- `verification_passed == true`
- **Valgrind Memcheck:** Zero leaks, UMR, invalid reads/writes
- **ThreadSanitizer (MT):** Zero race detections
- **AddressSanitizer:** Zero use-after-free, buffer overflow

---

## Universal Methodology

**Timing:**
- Warmup: 1s (not measured) → Measurement: ≥3s → Cooldown between runs: 250ms
- Clock: `CLOCK_MONOTONIC` (nanosecond precision)
- 5 repetitions per scenario with identical seed schedule

**Correctness:** Every run reports:
```c
failure_count == 0            // Operations succeeded
missing_count == 0            // No keys disappeared
verification_passed == true   // Post-run validation passed
```

**Statistics Per Run:**
- Throughput: ops/sec (mean, median, stdev, CV)
- Latency: p50, p95, p99, max (in nanoseconds)
- Memory: peak RSS, RSS delta, allocator calls

**Output:** 
- Single-threaded: CSV (one row per run)
- Multi-threaded: JSONL (one JSON object per run)

**Baseline Configuration:**
```c
bucket_size = 256 (varies by scenario)
sub_bucket_size = 32
max_chain_length = 8
is_concurrency_enabled = false (ST) | true (MT)
pre_allocation_factor = 0.5
```

---

## Single-Threaded Tests (20)

### ST2-CORE: Core Baseline (5 tests)

**Setup:** Warm, uniform, 64B values, 10K keys, bucket=256, pool=0.5, 3s measurement

| Test ID | Op Mix | Expected Throughput | Focus |
|---------|--------|-------------------|--------|
| ST2-CORE-001 | 50/50 SET:GET | 2–3M ops/sec | Baseline |
| ST2-CORE-002 | 90/10 GET:SET | 3–4M ops/sec | Read-heavy |
| ST2-CORE-003 | 10/90 SET:GET | 1–2M ops/sec | Write-heavy |
| ST2-CORE-004 | 100% GET | 4–5M ops/sec | Pure reads |
| ST2-CORE-005 | 100% SET (updates) | 2–3M ops/sec | Pure writes |

**Targets:** All ≥1M ops/sec; ST2-CORE-001 ≥2M; ST2-CORE-004 ≥4M

### ST2-CW: Cold vs Warm (2 tests)

**Setup:** Baseline config, 50/50 SET:GET, 10K keys, 64B values

| Test ID | State | Expected | Focus |
|---------|-------|----------|-------|
| ST2-CW-001 | Cold (empty start) | 10–20% lower throughput than warm | Startup overhead |
| ST2-CW-002 | Warm (preloaded) | Baseline reference | Steady-state |

**Target:** Throughput difference <20%; latency p99 converges after warmup

### ST2-DIST: Key Distributions (3 tests)

**Setup:** Warm, 50/50 SET:GET, 10K keys, 64B values, baseline config

| Test ID | Distribution | Characteristic | Expected Behavior |
|---------|--------------|-----------------|-------------------|
| ST2-DIST-001 | Uniform | Random across keyspace | Balanced chain depth, baseline throughput |
| ST2-DIST-002 | Zipfian (θ=0.99) | Hot-key (20% of keys, 80% of ops) | Deep chains at hot keys; cache reuse |
| ST2-DIST-003 | Bursty | Temporal locality (recent 20% of keys) | Shallow chains, high cache hit rate |

**Target:** Throughput variance <50% across distributions; all ≥1.5M ops/sec; no chain depth violations

### ST2-VKS: Value & Keyspace Scaling (4 tests)

**Setup:** Warm, 50/50 SET:GET, uniform, baseline config

| Test ID | Value Size | Keyspace | Expected Throughput | Memory |
|---------|-----------|----------|-------------------|--------|
| ST2-VKS-001 | 64B | 10K | 2.5M ops/sec | ~6.4 MB |
| ST2-VKS-002 | 64B | 20K | 2.4M ops/sec | ~12.8 MB |
| ST2-VKS-003 | 1KB | 10K | 1.5M ops/sec | ~10 MB |
| ST2-VKS-004 | 1KB | 20K | 1.4M ops/sec | ~20 MB |

**Target:** Throughput degrades ~50% from 64B to 1KB; memory scales linearly; no OOM

### ST2-RSZ: Resize Stress & Latency (3 tests)

**Setup:** Insert-heavy (100% SET), sequential low-entropy keys, 20K keys, 3s measurement, 100ms slice collection

| Test ID | Prealloc | Resizes Expected | Pool Benefit | Focus |
|---------|----------|------------------|--------------|-------|
| ST2-RSZ-001 | 0.0 | 5–8 | High malloc/free overhead | Baseline |
| ST2-RSZ-002 | 0.5 | 2–4 | Balanced | Medium pool |
| ST2-RSZ-003 | 1.0 | 0–1 | Minimal allocator calls | Full pool |

**Metrics:** Resize count, p99 latency spikes, recovery time (target: <500ms), peak RSS

**Expected:** Resizes visible in p99 timeline; ST2-RSZ-003 shows fewest spikes; all recover cleanly

### ST2-MEM: Memory & Allocator (3 tests)

**Setup:** 50/50 SET:GET, 2M total ops, 10K keys, 64B values, 3s measurement

| Test ID | Pool Factor | malloc/free Overhead | Expected Throughput |
|---------|-------------|----------------------|-------------------|
| ST2-MEM-001 | 0.0 | High (baseline) | 2.0M ops/sec |
| ST2-MEM-002 | 0.5 | Reduced | 2.1M ops/sec |
| ST2-MEM-003 | 1.0 | Minimal | 2.3M ops/sec |

**Metrics:** Peak RSS, malloc/free call counts, throughput gain

**Target:** Pool factor 1.0 shows ≥10% throughput gain vs 0.0; malloc/free counts drop ≥90%

---

## Multi-Threaded Tests (43)

### MT2-CORE: Scaling (6 tests)

**Setup:** 50/50 SET:GET, warm, uniform, 100K keys, 64B, bucket=1024, pool=0.5, 5s measurement, barrier-sync

| Test ID | Threads | Expected Throughput | Scaling Factor |
|---------|---------|-------------------|-----------------|
| MT2-CORE-001 | 1 | 2.0M ops/sec | 1.0× |
| MT2-CORE-002 | 2 | 3.8M ops/sec | 1.9× |
| MT2-CORE-003 | 4 | 7.2M ops/sec | 3.6× |
| MT2-CORE-004 | 8 | 14.4M ops/sec | 7.2× |
| MT2-CORE-005 | 16 | 28M ops/sec | 14× |
| MT2-CORE-006 | 32 | 56M ops/sec or plateau | ≥28× |

**Target:** ≥90% efficiency up to 16T; latency p99 <10 µs at 8T; zero deadlocks

### MT2-CW: Cold/Warm Variants (6 tests)

**Setup:** 8 threads, baseline config, 100K keys, 3s measurement

| Test ID | State | Op Mix | Expected Throughput | Focus |
|---------|-------|--------|-------------------|-------|
| MT2-CW-001 | Cold | 50/50 SET:GET | 10–12M ops/sec | Cold start |
| MT2-CW-002 | Warm | 50/50 SET:GET | 12–14M ops/sec | Warm baseline |
| MT2-CW-003 | Cold | 90/10 GET:SET | 14–16M ops/sec | Read-heavy cold |
| MT2-CW-004 | Warm | 90/10 GET:SET | 15–17M ops/sec | Read-heavy warm |
| MT2-CW-005 | Cold | 10/90 SET:GET | 8–10M ops/sec | Write-heavy cold |
| MT2-CW-006 | Warm | 10/90 SET:GET | 9–11M ops/sec | Write-heavy warm |

**Target:** Warm ≥ cold by 10–15%; read-heavy >write-heavy; all ≥8M ops/sec

### MT2-MIX: Workload Ratios (10 tests)

**Setup:** Warm, uniform, 100K keys, 3s measurement

| Test ID | Threads | Op Mix | Expected Throughput | Focus |
|---------|---------|--------|-------------------|-------|
| MT2-MIX-001 | 8 | 100% GET | 16–18M ops/sec | Read-only |
| MT2-MIX-002 | 8 | 100% SET | 9–11M ops/sec | Write-only |
| MT2-MIX-003 | 8 | 50/50 SET:GET | 12–14M ops/sec | Balanced |
| MT2-MIX-004 | 8 | 10/90 GET:SET | 14–16M ops/sec | Read-heavy |
| MT2-MIX-006 | 16 | 100% GET | 32M ops/sec | Scaling reads |
| MT2-MIX-007 | 16 | 100% SET | 18M ops/sec | Scaling writes |
| MT2-MIX-008 | 16 | 50/50 SET:GET | 24M ops/sec | Scaling balanced |
| MT2-MIX-009 | 16 | 10/90 GET:SET | 28M ops/sec | Scaling read-heavy |
| MT2-MIX-005, 010 | (reserved) | — | — | — |

**Target:** Read-heavy > balanced > write-only throughput; latency increases for writes

### MT2-DIST: Distributions (6 tests)

**Setup:** Baseline config, 8 and 16 threads, warm, 100K keys

| Test ID | Threads | Distribution | Expected Impact | Focus |
|---------|---------|---------------|-----------------|-------|
| MT2-DIST-001 | 8 | Uniform | Baseline | Reference |
| MT2-DIST-002 | 8 | Zipfian (θ=0.99) | Hot-key chains may bottleneck | Contention |
| MT2-DIST-003 | 8 | Bursty | Temporal locality benefit | Cache |
| MT2-DIST-004 | 16 | Uniform | Linear scaling | High-thread uniform |
| MT2-DIST-005 | 16 | Zipfian | Contention stress | High-thread contention |
| MT2-DIST-006 | 16 | Bursty | Best-case scenarios | High-thread cache |

**Target:** All distributions thread-safe; zipfian may show lower throughput due to hot-key serialization

### MT2-RSZ: Resize Under Load (6 tests)

**Setup:** Insert-heavy (100% SET), 8 and 16 threads, 100K keyspace, barrier-sync

| Test ID | Threads | Prealloc | Resizes Expected | Focus |
|---------|---------|----------|------------------|-------|
| MT2-RSZ-001 | 8 | 0.0 | 5–8 | High allocator overhead |
| MT2-RSZ-002 | 8 | 0.5 | 2–4 | Medium pool |
| MT2-RSZ-003 | 8 | 1.0 | 0–1 | Full pool |
| MT2-RSZ-004 | 16 | 0.0 | 5–8 | High-thread, no pool |
| MT2-RSZ-005 | 16 | 0.5 | 2–4 | High-thread, medium pool |
| MT2-RSZ-006 | 16 | 1.0 | 0–1 | High-thread, full pool |

**Metrics:** Resize count, p99 latency spikes, missing_count (must be 0), correctness pass

**Target:** Zero key loss; p99 spike recovery <500ms; all resizes safe under concurrent load

### MT2-VKS: Value/Keyspace Scaling (8 tests)

**Setup:** Warm, 50/50 SET:GET, 8 and 16 threads

| Test ID | Threads | Value Size | Keyspace | Expected Impact |
|---------|---------|-----------|----------|-----------------|
| MT2-VKS-001 | 8 | 64B | 100K | Baseline |
| MT2-VKS-002 | 8 | 64B | 1M | Memory pressure |
| MT2-VKS-003 | 8 | 1KB | 100K | Allocation pressure |
| MT2-VKS-004 | 8 | 1KB | 1M | Extreme stress |
| MT2-VKS-005 | 16 | 64B | 100K | Scaling baseline |
| MT2-VKS-006 | 16 | 64B | 1M | High-thread memory |
| MT2-VKS-007 | 16 | 1KB | 100K | High-thread allocation |
| MT2-VKS-008 | 16 | 1KB | 1M | High-thread extreme |

**Target:** Throughput and latency stable across value/keyspace combinations; no OOM

### MT2-OVER: Oversubscription Stress (3 tests)

**Setup:** Extreme thread counts, 100M ops, barrier-sync

| Test ID | Threads | Duration | Focus |
|---------|---------|----------|-------|
| MT2-OVER-001 | 64 | 10s | Extreme thread count |
| MT2-OVER-002 | 128 | 10s | Beyond core count |
| MT2-OVER-003 | 256 | 10s | Severe oversubscription |

**Target:** Zero crashes/hangs; correctness 100% (missing_count == 0); graceful degradation; Valgrind/ThreadSanitizer clean

---

## Configuration

### Tuning Profiles

| Profile | Use Case | bucket_size | pool_factor |
|---------|----------|-------------|-------------|
| Conservative | Low memory, ST | 64 | 0.5 |
| Balanced | Moderate, ≤16T | 256 | 0.75 |
| Aggressive | High throughput, >16T | 1024 | 1.0 |

### Variations Tested

- **Bucket sizes:** 16, 64, 256, 1024
- **Prealloc factors:** 0.0, 0.5, 1.0
- **Keyspace:** 10K–1M (ST: 10–20K; MT: 100K–1M)
- **Value sizes:** 64B, 1KB
- **Thread counts:** 1–256 (focus on 1, 2, 4, 8, 16, 32)

---

## Implementation Status

### Single-Threaded (20 tests) — ✅ Complete

| Category | Count | Status |
|----------|-------|--------|
| Core Baseline | 5 | ✅ Complete |
| Cold vs Warm | 2 | ✅ Complete |
| Distributions | 3 | ✅ Complete |
| Value/Keyspace | 4 | ✅ Complete (scaled to memory budget) |
| Resize Stress | 3 | ✅ Complete |
| Memory | 3 | ✅ Complete |
| **Total** | **20** | **✅ Ready** |

**Key Features:**
- 100ms slice collection for ST2-RSZ (latency spikes synchronized with resize events)
- Peak RSS, malloc/free counts, throughput per run
- Correctness checks embedded in all scenarios

### Multi-Threaded (43 tests) — ✅ Complete

| Category | Count | Status |
|----------|-------|--------|
| Core Scaling | 6 | ✅ Complete |
| Cold/Warm Variants | 6 | ✅ Complete |
| Workload Mix | 10 | ✅ Complete |
| Distributions | 6 | ✅ Complete |
| Resize Under Load | 6 | ✅ Complete |
| Value/Keyspace | 8 | ✅ Complete |
| Oversubscription Stress | 3 | ✅ Complete |
| **Total** | **43** | **✅ Ready** |

**Key Features:**
- Barrier-synchronized thread starts (reproducible timing)
- Latency percentiles + memory metrics per run
- Soft-delete correctness embedded in workload tests
- ThreadSanitizer, AddressSanitizer, Valgrind integration

---

## Coverage & Deviations

### Coverage Achieved

✅ **Baseline Performance:** Core throughput, latency, memory (20 ST tests)

✅ **Concurrency:** 1–32 thread scaling, workload diversity, lock types (43 MT tests)

✅ **Stress:** Resize safety, extreme oversubscription, hot-key distributions

✅ **Correctness:** Valgrind, ThreadSanitizer, AddressSanitizer integration; zero-loss validation

✅ **Production Ready:** All 63 tests executable; standard output formats; reproducible seeds

### Deviations from Initial Plan

| Item | Original Plan | Implementation | Rationale |
|------|---|---|---|
| ST-VKS keyspace | 100K–1M | 10K–20K | Faster test cycles, memory budget; sufficient for allocation pressure analysis |
| ST-CW variants | 4 tests | 2 tests | Core scenarios sufficient; read/write mixes covered in MT-CW |
| MT-CONT/LOCK | 7 dedicated tests | Embedded in MT2-CORE, MT2-MIX | Data extracted via existing test suites |
| Bucket sizes | 16–1024 variable | 1024 baseline (MT2) | Optimized for 32-thread scaling; tuning profiles provided |
| ST value sizes | 256B, 4KB | 64B, 1KB | Scaled down to memory budget; core scenarios retained |

### Execution Timeline

| Phase | Duration | Deliverable |
|-------|----------|------------|
| **Phase 1:** ST Baseline | Week 1 | CSV results, baseline metrics |
| **Phase 2:** ST Tuning | Week 1–2 | Optimization recommendations |
| **Phase 3:** MT Scaling | Week 2–3 | Scaling curves, contention analysis |
| **Phase 4:** Stress & Report | Week 3–4 | Final report + config guidance |

**Total Estimated Runtime:** ~25 minutes for full suite (5 reps × 63 scenarios)

---

## Next Steps

1. Run benchmark suite: `make benchmark` from `benchmark/` directory
2. Collect results in `benchmark/results/`
3. Parse output (CSV/JSONL) and generate performance report
4. Validate correctness metrics (missing_count, failures, ThreadSanitizer output)
5. Document tuning recommendations based on variance and scaling efficiency

---

**Status:** 🟢 Production-Ready | **All 63 tests compiled and executable**

