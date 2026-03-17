# KeyStore Progress Memory

## Current Version: v1.0 (Target: June 2026)

### v1.0 Checklist
- [x] Hash table with bucket RW-locks (pthread_rwlock_t)
- [x] Fine-grained spinlock chase buffer resize
- [x] Full CRUD (create, read, update, delete)
- [x] Concurrency stress test harness present
- [ ] Benchmark parity with prior 1M global-table baseline
- [ ] Staged benchmark runs at 3M and 5M operations
- [ ] Valgrind clean confirmed
- [ ] Dual-hash distribution for routing/sub-routing
- [ ] Memory pool sizing and scope refactor
- [ ] Bloom filter for negative-path lookups
- [ ] Finer resize locks
- [ ] Background task manager refactor
- [ ] Logging abstraction
- [ ] README.md finalized
- [ ] API.md complete
- [ ] ERROR_CODES.md complete
- [ ] Tagged v1.0.0 on GitHub

### Benchmark Results (Latest)
| Workload | Threads | Buckets | Throughput | Notes |
|----------|---------|---------|------------|-------|
| Set + get, unique keys | 120 | 1024 / 1024 | TBD ops/sec | Current checked-in integration test |
| 1M baseline parity | TBD | TBD | TBD | Target after routing and set-path fixes |
| 3M staged run | TBD | TBD | TBD | Planned |
| 5M staged run | TBD | TBD | TBD | Planned |

### Known Issues
- [ ] Hash distribution and unique-key set latency are the current primary performance risks.
- [ ] `key_hash == 0` is still rejected at multiple routing/buffer boundaries.
- [ ] `usleep(100)` remains in resize-related infrastructure paths, so Windows portability is incomplete.

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
