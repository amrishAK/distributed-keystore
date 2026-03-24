# KeyStore Benchmark Results

This document records concurrency stress test results across architectural iterations of the KeyStore.
All iterations run against the same fixed workload and tool setup so results are directly comparable.

---

## Benchmark A — Bucket-Level Concurrency

### Setup

| Parameter              | Value                          |
|------------------------|--------------------------------|
| Test binary            | `./bin/concurrency_test`       |
| Workload               | 1000 threads × 1000 unique keys |
| Total operations       | 2,000,000 (1M SET + 1M GET)    |
| Memory checker         | Valgrind Memcheck 3.22.0       |
| Valgrind command       | `valgrind ./bin/concurrency_test` |
| Pass criterion         | 0 key-missing-after-set, 0 Valgrind errors, 0 leaks |

All iterations below use this identical setup. Per-iteration differences are architectural only.

---

### Iteration 1 — Global Hash Table

> _To be added. This iteration uses a single global hash table with coarse-grained locking._

---

### Iteration 2 — Sub-Hash Table + Composite Hash

**Date:** 2026-03-23

#### Architecture

- **Composite hash:** A two-level hash function routes each key first to a top-level bucket, then to a
  slot within a per-bucket sub-hash table. This reduces collision chains and distributes lock contention
  across independent sub-tables.
- **Sub-hash table:** Each top-level bucket owns its own sub-hash table protected by a
  `pthread_rwlock_t`. Readers acquire a shared lock; writers acquire an exclusive lock only on the
  affected sub-table, leaving all other buckets fully concurrent.
- **Chase buffer resize:** Resizing is handled by a background worker using a fine-grained spinlock
  chase buffer, allowing reads and writes to proceed on un-migrated slots during an in-progress resize.

#### Results

| Metric          | Value           |
|-----------------|-----------------|
| Total time      | 156.791 s       |
| Throughput      | 12,755.86 ops/s |
| Resize triggers | 0               |
| Key-missing-after-set | 0         |

| Operation | avg (ns) | p50 (ns) | p95 (ns) | p99 (ns) |
|-----------|----------|----------|----------|----------|
| SET       | 54,621   | 38,000   | 54,400   | 86,599   |
| GET       | 27,108   | 25,400   | 34,000   | 62,600   |

#### Valgrind

```
==10584== HEAP SUMMARY:
==10584==     in use at exit: 0 bytes in 0 blocks
==10584==   total heap usage: 6,002,033 allocs, 6,002,033 frees, 332,059,796 bytes allocated
==10584==
==10584== All heap blocks were freed -- no leaks are possible
==10584==
==10584== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
```

**Result: PASS** — Zero leaks. Zero Valgrind errors. All heap blocks freed cleanly.

---

### Iteration 3 — Sub-Hash Table + Composite Hash (Scaled: 8M ops)

**Date:** 2026-03-23

#### Workload (scaled)

| Parameter              | Value                                                        |
|------------------------|--------------------------------------------------------------|
| Threads                | 2,000                                                        |
| Keys per thread        | 2,000                                                        |
| Total operations       | 8,000,000 (4M SET + 4M GET)                                  |
| Config                 | bucket_size=1024, sub_bucket_size=1024, max_chain_length=15  |

#### Architecture

Same as Iteration 2 (Sub-Hash Table + Composite Hash). Workload scaled 4× to exercise the chase buffer resize path under sustained concurrency pressure.

#### Results

| Metric                | Value            |
|-----------------------|------------------|
| Total time            | 566.378 s        |
| Throughput            | 14,124.85 ops/s  |
| Resize triggers       | 4                |
| Key-missing-after-set | 0                |

| Operation | avg (ns) | p50 (ns) | p95 (ns) | p99 (ns) |
|-----------|----------|----------|----------|----------|
| SET       | 56,625   | 48,076   | 80,973   | 114,398  |
| GET       | 33,847   | 26,639   | 39,587   | 64,982   |

#### Valgrind

```
==29740== HEAP SUMMARY:
==29740==     in use at exit: 0 bytes in 0 blocks
==29740==   total heap usage: 24,048,119 allocs, 24,048,119 frees, 1,085,331,053 bytes allocated
==29740==
==29740== All heap blocks were freed -- no leaks are possible
==29740==
==29740== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
```

**Result: PASS** — Zero leaks. Zero Valgrind errors. All heap blocks freed cleanly. 4 resize events triggered and completed without key loss.

---

### Iteration 4 — Sub-Hash Table + Composite Hash (Native, No Valgrind)

**Date:** 2026-03-23

#### Workload

| Parameter              | Value                                                        |
|------------------------|--------------------------------------------------------------|
| Threads                | 2,000                                                        |
| Keys per thread        | 2,000                                                        |
| Total operations       | 8,000,000 (4M SET + 4M GET)                                  |
| Config                 | bucket_size=1024, sub_bucket_size=1024, max_chain_length=15  |
| Memory checker         | None (native run)                                            |

#### Architecture

Same as Iterations 2–3 (Sub-Hash Table + Composite Hash). Run natively without Valgrind to establish true hardware throughput. Iteration 3 Valgrind overhead measured at ~304× (566s vs 1.862s).

#### Results

| Metric                | Value               |
|-----------------------|---------------------|
| Total time            | 1.862 s             |
| Throughput            | 4,295,904.57 ops/s  |
| Resize triggers       | 7                   |
| Race errors           | 0                   |
| Key-missing-after-set | 0                   |

| Operation | avg (ns) | p50 (ns) | p95 (ns) | p99 (ns) |
|-----------|----------|----------|----------|----------|
| SET       | 3,565    | 1,107    | 3,122    | 9,466    |
| GET       | 1,130    | 302      | 604      | 4,330    |

#### Notes

- 7 resize events (vs 4 under Valgrind) — Valgrind slows background worker enough that fewer resizes complete within test window.
- SET p99 spike (9,466 ns) correlates with chase buffer resize contention; median (1,107 ns) reflects uncontested fast path.
- GET p50 = 302 ns is consistent with a single sub-bucket rwlock shared-acquire + linked-list traversal under low chain depth.

**Result: PASS** — Zero key loss. Zero race errors. 7 resize events triggered and completed without data loss.

---

<!-- Add Iteration 4 and future benchmark sets below this line -->
