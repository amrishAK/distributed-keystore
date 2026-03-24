# KeyStore Progress Memory

## Current Version: v1.0 (Target: June 2026)

### v1.0 Checklist

#### Core Functionality
- [x] Hash table with bucket RW-locks (pthread_rwlock_t)
- [x] Fine-grained spinlock chase buffer resize
- [x] Full CRUD (create, read, update, delete)
- [x] Two-phase lock protocol (rwlock → release → node mutex; enables intra-bucket parallelism)
- [x] Soft-delete + `is_deleted` race guard
- [x] Dual-hash distribution — MurmurHash3-64, dual seeds, composite_key_hash
- [x] Background chase buffer worker
- [x] Optional memory pool for `linked_list_node`
- [x] Configurable: `bucket_size`, `sub_bucket_size`, `max_chain_length`, concurrency on/off
- [x] Concurrency stress test harness present
- [x] Valgrind clean confirmed (149 tests, 0 failures, 630 allocs / 630 frees, 0 errors)
- [ ] Memory pool scope refactor (extend pool to cover `data_node` fixed-size struct allocation — improves pool utilisation; v1.5 migrates value segment to atomic swap)
- [ ] Bloom filter for negative-path lookups (`uint64_t bloom_filter[16]` per sub_hash_bucket) — eliminates list traversal on missing keys, reduces SET and GET-miss latency
- [ ] Replace `resizing_lock` (pthread_mutex_t) with `resize_guard` (pthread_rwlock_t) — closes is_resizing TOCTOU: normal ops take rdlock, resize init takes wrlock
- [ ] Logging abstraction (replace debug printf traces with structured log levels)
- [ ] Benchmark parity with prior 1M global-table baseline
- [ ] Staged benchmark runs at 3M and 5M operations
- [ ] is_resizing_enabled config flag (explicit, replaces implicit max_chain_length=UINT_MAX pattern for embedded deployments)
- [ ] CMake build + libkeystore.a target
- [ ] Multi-instance API: keystore_t* context struct (move g_hash_table_pool, g_bucket_hash_seed, g_sub_bucket_hash_seed into it; opaque typedef in header)
- [ ] Fix `key_hash == 0` rejection at routing/buffer boundaries — correctness bug

#### Open Source Release
- [x] Apache 2.0 LICENSE file added (Copyright 2026 Amrish Arunachalam Kulasekaran)
- [ ] README.md finalized
- [ ] API.md complete
- [ ] ERROR_CODES.md complete
- [ ] CONTRIBUTING.md (build instructions, test instructions, PR guidelines)
- [ ] docs/CONCURRENCY.md (two-phase lock model, resize guard, embedded profile)
- [ ] CHANGELOG.md (Keep-A-Changelog format, v1.0.0 entry)
- [ ] CODE_OF_CONDUCT.md
- [ ] SECURITY.md (vulnerability reporting policy)
- [ ] GitHub Actions CI workflow (.github/workflows/ci.yml — build + unit tests + Valgrind)
- [ ] GitHub issue and PR templates (.github/ISSUE_TEMPLATE/, .github/pull_request_template.md)
- [ ] Remove or guard all debug printf traces under src/keystore
- [ ] GitHub repository public + topics tagged (c, keystore, embedded, concurrent, hash-table)
- [ ] Tagged v1.0.0 on GitHub
- [ ] Initial release announcement / blog post

### Known Issues
- [x] ~~Hash distribution and unique-key set latency are the current primary performance risks.~~ Hash distribution resolved via dual-seed MurmurHash3-64; unique-key set latency remains a concern.
- [ ] `key_hash == 0` is still rejected at multiple routing/buffer boundaries.
- [ ] `usleep(100)` remains in resize-related infrastructure paths, so Windows portability is incomplete.

### Benchmark Results (Latest)
| Workload            | Threads | Buckets      | Ops      | Throughput       | Resizes | Notes                        |
|---------------------|---------|--------------|----------|------------------|---------|------------------------------|
| 50% SET / 50% GET   | 1,000   | 1024 / 1024  | 2M       | 12,755.86 ops/s  | 0       | Iteration 2 baseline         |
| 50% SET / 50% GET   | 2,000   | 1024 / 1024  | 8M       | 14,124.85 ops/s    | 4       | Iteration 3 — Valgrind (304× overhead)     |
| 50% SET / 50% GET   | 2,000   | 1024 / 1024  | 8M       | **4,295,904.57 ops/s** | 7   | Iteration 4 — native (no Valgrind), 1.862s |
| 3M staged run       | TBD     | TBD          | 3M       | TBD              | —       | Planned                      |
| 5M staged run       | TBD     | TBD          | 5M       | TBD              | —       | Planned                      |


### v1.5 Planned (pre-v2.0 performance pass)
- [ ] 3-segment data node: Seg1=immutable ref-counted key_segment (_Atomic uint32_t ref_count), Seg2=value_segment (atomic pointer swap), Seg3=fixed-size data_node (poolable)
- [ ] Drop per-node pthread_mutex_t — replace with atomic pointer swap on value_segment* after 3-segment redesign; value_segment reclamation via ref_count
- [ ] _Atomic bool is_resizing (interim fix until resize_guard rwlock lands)
- [ ] Embedded profile: is_concurrency_enabled=false + is_resizing_enabled=false → zero lock overhead, deterministic memory, no background thread
- [ ] Embedded profile benchmarking + stress tests (validate throughput and deterministic memory under single-threaded no-resize config)
- [ ] SCAN / prefix-scan API (iterate all keys matching a prefix or pattern)


### v2.0 Planned Features
- [ ] Write-Ahead Log (WAL)
- [ ] Snapshot + checkpoint
- [ ] Crash recovery
- [ ] Persistence foundation for later REST/RAFT frontends

### Session Log
#### 2026-03-17
- Analyzed: current repo docs, routing path, resize trigger behavior, stress test config
- Docs updated: memory.md, docs/PROGRESS.md
- Verified: split update-then-add upsert path still exists; integration test frees both returned key and value; no active `printf`/`fprintf` traces under `src/keystore`
- Recorded scope: v1 is single-node, in-memory, API-node focused; persistence and broader distribution remain post-v1
- TODOs added: dual-hash routing, memory-pool refactor, bloom filter, staged 1M/3M/5M benchmark campaign

#### 2026-03-23
- Ran: full unit test suite under Valgrind-3.22.0 (`./bin/key_store_test`)
- Result: 149 tests across 9 modules — 0 failures, 0 ignored
  - memory_manager: 9 | data_node_operation: 22 | linked_list_operation: 16
  - sub_hash_bucket: 20 | sub_hash_table: 15 | hash_bucket: 16
  - hash_table: 19 | key_store: 11 | buffer_operation: 21
- Valgrind: 630 allocs, 630 frees, 290,828 bytes allocated — 0 leaks, 0 errors
- Milestone confirmed: Valgrind clean (unit test scope) — checklist item ticked
- Ran: integration concurrency stress test (Iteration 3) — 2000 threads × 2000 keys, 8M ops
- Result: PASS — 14,124.85 ops/s, 4 resize events, 0 key-missing-after-set, 0 Valgrind errors, 0 leaks
- Valgrind: 24,048,119 allocs, 24,048,119 frees, 1,085,331,053 bytes allocated — 0 leaks, 0 errors
- Chase buffer resize confirmed correct under high concurrency (4 resizes, no data loss)
- Docs updated: docs/benchmarks.md (Iteration 3 added), docs/progress.md (benchmark table updated)
- TODOs remaining: staged 3M/5M runs, dual-hash routing, memory-pool refactor, bloom filter, finer resize locks, background task manager refactor, logging abstraction, docs finalisation

#### 2026-03-23 (docs refresh)
- Full documentation refresh: README.md, API.md, ERROR_CODES.md, docs/architecture.md, docs/memory.md, docs/progress.md
- Corrected hash function references from MurmurHash3-32 to MurmurHash3-64 across all docs
- Documented dual-seed composite key hashing (`g_bucket_hash_seed` + `g_sub_bucket_hash_seed`)
- Updated error sentinel from `UINT32_MAX` to `UINT64_MAX` across all docs
- Updated mix/finalization constants and block size (4→8 bytes) in hash function docs
- Marked "dual-hash distribution" checklist item as **DONE** across all trackers
- Added hashing section to README.md explaining dual-seed design
- Added hashing section to API.md
- Updated all routing descriptions to reference `composite_key_hash.bucket_hash` and `.sub_bucket_hash`
- TODOs remaining: memory-pool refactor, bloom filter, finer resize locks, background task manager refactor, logging, staged benchmarks, docs finalisation, v1.0 tag

#### 2026-03-23 (licensing and test updates)
- Added: Apache 2.0 LICENSE file at repo root (Copyright 2026 Amrish Arunachalam Kulasekaran)
- Updated: unit test suite (test_runner.c) — modules and coverage expanded
- Ran: native concurrency stress test (Iteration 4, no Valgrind) — 2,000 threads × 2,000 keys, 8M ops
  - Total time: 1.862s | Throughput: 4,295,904.57 ops/s
  - SET: avg=3,565ns p50=1,107ns p95=3,122ns p99=9,466ns
  - GET: avg=1,130ns p50=302ns  p95=604ns  p99=4,330ns
  - Resizes: 7 | Race errors: 0 | Result: PASS
- Corrected Valgrind overhead estimate: ~304× (not 25×) under 2,000-thread mutex-heavy load
- Docs updated: README.md (license, author, benchmark stats, example output, v1.0 checklist), docs/progress.md
- TODOs: README.md finalize, API.md, ERROR_CODES.md, drop per-node mutex, bloom filter, 3-segment key_segment design, _Atomic is_resizing, portable_sleep_ms, key_hash==0 fix, multi-instance API, v1.0 tag

#### 2026-03-24 (architecture design session)
- Confirmed: two-phase lock protocol is intentional and correct — rwlock released before node mutex acquired to enable parallel Phase 2 across different nodes in the same sub-bucket
- Confirmed: is_deleted flag correctly closes the use-after-free race in the window between rwlock release and node mutex acquisition
- Confirmed: per-node mutex is not redundant — it is the Phase 2 serialization point after the rwlock scope ends
- Identified: is_resizing TOCTOU — plain bool checked without lock; thread can read false, resize starts, thread writes to mid-migration table outside chase buffer visibility
- Planned fix: replace `resizing_lock` (pthread_mutex_t) with `resize_guard` (pthread_rwlock_t) — normal ops take rdlock (concurrent), resize init takes wrlock (exclusive); makes lock hierarchy uniform across all 3 levels
- Planned: is_resizing_enabled config flag — replaces implicit max_chain_length=UINT_MAX pattern; makes embedded no-resize configuration self-documenting
- Planned: embedded profile = is_concurrency_enabled=false + is_resizing_enabled=false → zero lock overhead, deterministic memory, no background worker thread; sized correctly at init via bucket_size × sub_bucket_size config
- Planned: 3-segment data node (v1.5) — Seg1 immutable ref-counted key_segment, Seg2 value_segment (atomic pointer swap post-3-segment), Seg3 fixed-size poolable data_node; resolves key copy × 3-4 during resize and enables pool coverage for data_node
- Planned: atomic value_segment* swap after 3-segment redesign — drops per-node mutex entirely; old value_segment reclaimed via ref_count; seqlock not needed because composite state lives in one atomically swapped pointer
- Confirmed: linked list cache concern is mitigated by user-controlled bucket_size × sub_bucket_size config — expected chain length ≈ total_keys / (bucket_size × sub_bucket_size); at correct sizing chain depth < 2, cache gap vs open addressing is negligible
- Docs updated: docs/progress.md (checklist, v1.5 planned features, session log), docs/architecture.md (two-phase lock protocol, DELETE flow, GET flow, section 7.2), README.md (concurrency model, soft-delete note)
