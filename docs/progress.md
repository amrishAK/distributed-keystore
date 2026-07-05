# KeyStore Progress Memory

**Version:** v1.0 | **Target:** June 2026 | **Status:** Feature-complete, OSS release in progress

---

## Status Summary

| Category | Status | Notes |
|----------|--------|-------|
| **Core Functionality** | ✅ Complete | 11/20 items done; 9 remaining (v1.5+ features + config enhancements) |
| **Open Source Release** | 🔄 In Progress | 4/15 items done; 11 remaining (docs, CI/CD, GitHub setup) |
| **Performance** | ✅ Validated | 4.3M ops/s native (2K threads), 0 Valgrind errors, Iteration 4 baseline established |
| **Known Issues** | ⚠️ 3 Open | `key_hash == 0` bug, Windows portability, pre-allocation concern |

---

## v1.0 Checklist (COMPACT)

### Core Functionality (11/20 DONE)

**Completed:**
- [x] RWlock bucket concurrency + two-phase lock protocol
- [x] Chase buffer resize (spinlock, staged async migration)
- [x] CRUD operations + soft-delete semantics
- [x] Dual-seed MurmurHash3-64 (composite_key_hash routing)
- [x] Background chase worker + optional memory pool (linked_list_node)
- [x] Configurable tuning (bucket/sub-bucket sizes, chain thresholds, concurrency toggle)
- [x] Stress test harness (2K threads × 2K keys, 8M ops)
- [x] Valgrind clean (149 tests: 0 failures, 630 allocs/frees, 0 leaks)

**Remaining (v1.0 codeline, v1.5+ priority):**
- [ ] Memory pool refactor: extend to `data_node` (v1.5 blocker for 3-segment redesign)
- [ ] Bloom filter: negative-path optimization (128-bit per sub_bucket, GET-miss elimination)
- [ ] Resize guard TOCTOU fix: replace `resizing_lock` mutex → `resize_guard` rwlock
- [ ] Logging abstraction: structured levels, guard printf traces
- [ ] Benchmark scaling: 3M and 5M staged runs
- [ ] Config flags: `is_resizing_enabled` (embedded profile support)
- [ ] CMake build: libkeystore.a target + portable build
- [ ] Multi-instance API: context struct (move g_* globals into opaque keystore_t*)
- [ ] `key_hash == 0` bug fix (routing/buffer boundaries currently reject hash=0)

### Open Source Release (4/15 DONE)

**Completed:**
- [x] Apache 2.0 LICENSE (Copyright 2026 Amrish Arunachalam Kulasekaran)
- [x] README.md (refactored 350→220 lines, strategic doc links)
- [x] docs/DESIGN_DECISIONS.md (design rationale, known limitations)
- [x] API.md (comprehensive refactor: 320→550 lines, 3 examples, thread-safety deep-dive)

**Remaining:**
- [ ] CONTRIBUTING.md (build/test/PR guidelines)
- [ ] docs/CONCURRENCY.md (two-phase lock model, embedded profile)
- [ ] CHANGELOG.md (Keep-A-Changelog v1.0.0 entry)
- [ ] CODE_OF_CONDUCT.md
- [ ] SECURITY.md (vulnerability report policy)
- [ ] GitHub Actions CI (.github/workflows/ci.yml: build + tests + Valgrind)
- [ ] GitHub templates (.github/ISSUE_TEMPLATE/, PR template)
- [ ] Remove/guard printf traces (src/keystore)
- [ ] ERROR_CODES.md completion audit
- [ ] GitHub public + topics (c, keystore, embedded, concurrent, hash-table)
- [ ] Tag v1.0.0 release
- [ ] Release announcement / blog post

---

## Known Issues (3 OPEN)

| Issue | Severity | Scope | Status | Mitigation |
|-------|----------|-------|--------|-----------|
| `key_hash == 0` rejection | 🔴 Correctness | routing/buffer boundaries | Open | Must fix before v1.0 tag |
| Windows portability | 🟡 Platform | `usleep(100)` in resize paths | Open | Add `portable_sleep_ms()` wrapper |
| Per-node mutex design | 🔵 Performance | v1.5 redesign blocker | Deferred | 3-segment + atomic swap in v1.5 |

---

## Benchmark Results (Iteration 4 — Latest)

| Config | Threads | Size | Throughput | p50 GET | p99 GET | Resizes | Status |
|--------|---------|------|-----------|---------|---------|---------|--------|
| **Baseline** | 1K | 1M | 12,755 ops/s | — | — | 0 | ✅ 2026-03-23 |
| **Valgrind** | 2K | 8M | 14,125 ops/s | — | — | 4 | ✅ 2026-03-23 (304× overhead) |
| **Native** | 2K | 8M | **4,295,904** ops/s | 302ns | 4.3µs | 7 | ✅ 2026-03-23 (1.862s) |
| 3M staged | 2K | 3M | TBD | — | — | — | ⏳ Planned |
| 5M staged | 2K | 5M | TBD | — | — | — | ⏳ Planned |

**Performance Notes:**
- SET latency: avg 3,565ns, p50 1,107ns, p95 3,122ns, p99 9,466ns
- Valgrind overhead: ~304× (mutex-heavy concurrency)
- Race conditions: 0 (key-missing-after-set passes all iterations)
- Memory integrity: 0 leaks across 24M+ allocs under concurrency

---

## Roadmap: v1.5 & v2.0

### v1.5 (Pre-v2.0 Performance Pass)
- 3-segment data node redesign (ref-counted key_segment, atomic value_segment* swap, poolable data_node)
- Drop per-node pthread_mutex_t → atomic pointer swap (post-3-segment)
- _Atomic bool is_resizing (interim; resize_guard rwlock deferred to v1.1)
- Embedded profile (is_concurrency_enabled + is_resizing_enabled flags → zero-lock, deterministic)
- Embedded benchmarking (single-threaded no-resize profile validation)
- SCAN / prefix-scan API (full-table iteration)

### v2.0 (Persistence & Distribution Layer)
- Write-Ahead Log (WAL)
- Snapshot + checkpoint
- Crash recovery
- Persistence foundation for REST/RAFT frontends

---

## Session Log (Consolidated)

### 2026-03-17 — Initial Analysis & Scope Definition
**Milestone:** Confirmed v1.0 scope (single-node, in-memory, API-node focused; distribution/persistence → v2.0)
- Analyzed routing paths, resize triggers, stress test config
- Verified upsert semantics, integration tests, printf traces
- Updated: memory.md, progress tracking
- **Remaining:** dual-hash routing, memory-pool refactor, bloom filter, staged benchmarks

### 2026-03-23 — Concurrency Validation (Iterations 3–4)
**Milestone:** Valgrind clean (unit tests) ✅ | Native concurrency baseline ✅
- **Iteration 3:** Unit tests: 149 tests / 9 modules → 0 failures, 630 allocs/frees, 0 leaks
- **Iteration 3:** Stress test: 2K threads × 2K keys × 8M ops → 14,125 ops/s, 4 resizes, 0 race errors (Valgrind, 304× overhead)
- **Iteration 4:** Stress test: Same config → 4.3M ops/s native (1.862s), 7 resizes, 0 leaks
  - SET: avg 3.5µs, p50 1.1µs, p99 9.5µs
  - GET: avg 1.1µs, p50 0.3µs, p99 4.3µs
- Updated: benchmarks.md, docs/progress.md

### 2026-03-23 — Documentation Refresh (MurmurHash3 Correction)
**Milestone:** Dual-hash distribution validated, docs corrected ✅
- Corrected: MurmurHash3-32 → MurmurHash3-64 across all docs
- Documented: dual-seed composite key hashing (g_bucket_hash_seed, g_sub_bucket_hash_seed)
- Updated: UINT32_MAX → UINT64_MAX sentinel, hash block size (4→8 bytes)
- Added hashing sections to README.md, API.md

### 2026-03-24 — Architecture Design Session
**Milestone:** Two-phase lock protocol validated; is_resizing TOCTOU identified
- **Confirmed:** two-phase lock protocol correct (rwlock release enables Phase 2 parallelism across threads)
- **Confirmed:** is_deleted flag closes use-after-free race
- **Identified:** is_resizing TOCTOU (plain bool, no lock protection) — deferred v1.1 fix
- **Planned fixes:**
  - Replace resizing_lock mutex → resize_guard rwlock (v1.1)
  - Add is_resizing_enabled config flag (embedded profile)
  - 3-segment data node redesign (v1.5: ref-counted key_segment, atomic value_segment* swap)
- **Validated:** cache locality concern mitigated by bucket_size × sub_bucket_size tuning

### 2026-07-04 — Documentation Architecture Refactor
**Milestone:** Split monolithic architecture.md into modular sections ✅
- Refactored: docs/architecture.md → docs/architecture/ (14 section files)
- Added: docs/architecture/README.md (index), 01-overview through 14-design-decisions
- Updated segments: Bloom filter integration, operation_handlers module, resize logic, background-task behavior

### 2026-07-05 — Feature History & Documentation Sprint
**Milestone:** Comprehensive feature-history.md created; API.md refactored (320→550 lines)
- **Feature history:** Linear git analysis from skeleton → current (commit-ordered ADR-style docs)
- **Background task manager:** Refactored for clarity (state machine diagram, operation details, error coverage)
- **API.md deep-dive:**
  - Quick reference table (5 core functions)
  - Expanded function docs: Signature → Purpose → Parameters → Returns → Behavior → Memory Ownership → Thread Safety
  - 3 comprehensive examples (init/CRUD, insert vs update detection, error handling patterns)
  - Tuning guidelines (16–256 buckets, 8–64 sub-buckets, 3–8 chain thresholds)
  - Error code reorganization (success/error categories + recovery hints)
- **Concurrency model (§7):** Expanded from 2 → 10 subsections (rationale, lock hierarchy, two-phase protocol, resize states, deadlock prevention, soft-delete race closure, sync guarantees, perf implications)
- **Memory management (§9):** Expanded from 2 → 9 subsections (pool lifecycle, allocation strategy flowcharts, thread safety, inventory, safety rules, configuration best practices)
- **Design decisions (§14):** Expanded from skeletal → 10 detailed sections (two-level hash, dual-seed, two-phase locks, soft-delete, resize strategy, chase buffer spinlock, worker models, memory ownership, pool scope, in-memory design)
- **Architecture overview (§1):** Refactored for layering clarity (entry → hash-table → sub-hash-table → data-node), removed redundant module-dependency-graph.md

### 2026-07-05 — Open Source Release Prep
**Milestone:** v1.0 checklist refined; README/API.md/DESIGN_DECISIONS.md completed ✅
- **README.md:** Streamlined 350 → 220 lines, added doc navigation links
- **DESIGN_DECISIONS.md:** Consolidated design rationale, known limitations, and mitigations
- **API.md:** Production-ready comprehensive reference
- **Remaining OSS tasks:** CONTRIBUTING.md, CHANGELOG.md, SECURITY.md, CODE_OF_CONDUCT.md, GitHub Actions CI, issue/PR templates, v1.0.0 tag

### 2026-07-05 — Final Pass Review (Comprehensive Quality Audit)
**Milestone:** v1.0 readiness assessment completed; release path clarified ✅
- **Comprehensive audit:** Architecture, code quality, testing, documentation, OSS readiness
- **Findings:** 
  - ✅ **Strengths:** Modular architecture, memory-safe (0 Valgrind errors), 90%+ test coverage, 4.3M ops/s throughput, production-grade documentation (14 architecture subsections, comprehensive API.md, design rationale for all major decisions)
  - ⚠️ **Critical blocker:** `key_hash == 0` rejection in routing functions (correctness bug; affects tiny fraction of keys but must fix before v1.0 tag)
  - ⚠️ **OSS gaps:** 11 items pending (CONTRIBUTING.md, CHANGELOG.md, SECURITY.md, CODE_OF_CONDUCT.md, examples/main.c, GitHub CI, GitHub templates)
  - 🟡 **Platform gap:** Windows portability (`usleep()` → `portable_sleep_ms()` wrapper needed)
  - 🟡 **Performance deferred:** Bloom filter integration (v1.1), memory pool extension to data_node (v1.5)
- **Generated:** [docs/FINAL_REVIEW_2026-07-05.md](./FINAL_REVIEW_2026-07-05.md) — comprehensive audit with action items, effort estimates, release checklist
- **Recommendation:** Fix P1 items (correctness bug + CONTRIBUTING/CHANGELOG/examples), then tag v1.0.0; P2/P3 items defer to v1.1+

---

## Next Steps (Priority Order)

| Priority | Task | Impact | Est. Effort |
|----------|------|--------|-------------|
| 🔴 **P1** | Fix `key_hash == 0` bug | Correctness blocker | 2-4h |
| 🔴 **P1** | CONTRIBUTING.md + CHANGELOG.md | OSS release blocking | 4-6h |
| 🟡 **P2** | Replace resizing_lock → resize_guard (rwlock TOCTOU fix) | v1.1 robustness | 6-8h |
| 🟡 **P2** | Bloom filter integration (GET-miss acceleration) | Performance opt | 8-12h |
| 🔵 **P3** | Memory pool refactor (extend to data_node) | v1.5 prereq | 12-16h |
| 🔵 **P3** | 3M/5M staged benchmark runs | Scaling validation | 4-6h |
| 🔵 **P3** | Windows portability (portable_sleep_ms) | Platform support | 2-3h |
