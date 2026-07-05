# Design Decisions & Trade-offs

This chapter explains the key architectural decisions that shape KeyStore's concurrency model, memory management, and resize strategy. For a complete picture including known limitations, see [DESIGN_DECISIONS.md](../../DESIGN_DECISIONS.md).

## Table of Contents
- [1. Two-Level Hash Table](#1-two-level-hash-table)
- [2. Dual-Seed MurmurHash3 for Independent Routing](#2-dual-seed-murmurhash3-for-independent-routing)
- [3. Two-Phase Lock Protocol](#3-two-phase-lock-protocol)
- [4. Soft-Delete Semantics](#4-soft-delete-semantics)
- [5. Resize Strategy: Always Double, Never Shrink](#5-resize-strategy-always-double-never-shrink)
- [6. Spinlock for Chase Buffer Head/Tail](#6-spinlock-for-chase-buffer-headtail)
- [7. Detached Resize Worker, Joinable Chase Worker](#7-detached-resize-worker-joinable-chase-worker)
- [8. Memory Ownership Model](#8-memory-ownership-model)
- [9. Memory Pool for Linked-List Nodes Only](#9-memory-pool-for-linked-list-nodes-only)
- [10. In-Memory, No Persistence](#10-in-memory-no-persistence)

---

## 1. Two-Level Hash Table

**Decision:** Organize hash lookups across two levels — level-1 bucket index and level-2 sub-bucket index — rather than a monolithic hash table.

**Rationale:**
- **Localized resize cost:** When a single sub-bucket's collision chain exceeds `max_linked_list_chain_length`, only that sub-bucket resizes by doubling. The level-1 bucket remains unchanged, and other buckets are unaffected. This avoids the O(n) cost of migrating the entire table.
- **Reduced contention:** Threads targeting different buckets do not compete on the same lock. Threads targeting different sub-buckets *within the same bucket* still acquire the same RWlock but can parallelize Phase 2 (node mutex) operations.
- **Predictable hot-path latency:** Most operations stay within a single sub-bucket, making cache behavior and lock contention more predictable.

**Trade-off:** Adds one extra hash computation and routing decision. However, throughput gains from reduced contention more than compensate for this overhead.

**See also:** [04-two-level-hash-table.md](04-two-level-hash-table.md), [06-hashing-strategy.md](06-hashing-strategy.md)

---

## 2. Dual-Seed MurmurHash3 for Independent Routing

**Decision:** Hash each key twice using MurmurHash3-64 with two independent per-instance seeds. Level-1 bucket selection uses seed-1; level-2 sub-bucket selection uses seed-2.

**Rationale:**
- **Decorrelation:** A single hash split across two levels creates correlation: keys with biased bit patterns (e.g., sequential IDs) end up in the same sub-buckets across *all* buckets, causing uneven load distribution and cache contention.
- **Dual-seed design:** Two independent hashes with guaranteed distinct seeds (generated via `clock_gettime()` and XOR-ed with the golden ratio `0x9e3779b97f4a7c15`) eliminate this correlation. Keys are spread uniformly across both dimensions.
- **Measured improvement:** Reduces collision-chain variance and improves cache locality.

**Trade-off:** Slightly higher per-operation CPU cost (two MurmurHash3 calls vs. one split hash), but overall throughput improves due to better load balancing and lower cache misses.

**See also:** [06-hashing-strategy.md](06-hashing-strategy.md)

---

## 3. Two-Phase Lock Protocol

**Decision:** Separate lock acquisition into two phases:
- **Phase 1:** Acquire bucket RWlock → traverse collision chain → capture node pointer → **release RWlock**.
- **Phase 2:** Acquire node mutex → check `is_deleted` flag → read/write/soft-delete → **release node mutex**.

Locks are never held simultaneously.

**Rationale:**
- **Intra-bucket parallelism:** Releasing the RWlock before acquiring the node mutex allows threads targeting *different nodes within the same sub-bucket* to execute Phase 2 in parallel. A single RWlock held for the entire operation would serialize all threads on the same bucket, limiting throughput.
- **TOCTOU safety via `is_deleted`:** Between RWlock release and node mutex acquisition, a concurrent DELETE may soft-delete the node. The `is_deleted` check under the node mutex prevents reading stale or freed data. See [07-concurrency-model.md](07-concurrency-model.md) for full invariant proofs.

**Trade-off:** Requires careful reasoning about race conditions and invariants. Incorrect implementation can lead to use-after-free or stale reads. The trade-off is worthwhile because it significantly improves throughput under concurrent load.

**See also:** [07-concurrency-model.md](07-concurrency-model.md)

---

## 4. Soft-Delete Semantics

**Decision:** DELETE operations mark `data_node.is_deleted = true` under the node mutex (a "soft delete"). Physical removal from the collision chain happens only during batch cleanup operations (`cleanup_deleted_linked_list_nodes`), which run under a WRITE rwlock during resize or when chain length exceeds a threshold.

**Rationale:**
- **Avoid expensive in-place unlink:** Removing a node from a singly-linked collision chain requires traversing from the bucket head. Under heavy concurrent load, performing this traversal and unlink under a write lock for every delete would serialize all deletions on the same bucket, killing throughput.
- **Defer work:** Soft-delete decouples logical deletion (fast, just set a flag) from physical removal (batched, amortized). This allows the common case (DELETE on a live node) to complete in the critical path without triggering expensive cleanup.
- **`is_deleted` closes the use-after-free window:** With the two-phase protocol, a node may be read by another thread between when the RWlock is released and the node mutex is acquired. If the node is physically removed during that window, the reader would access freed memory. Marking `is_deleted = true` before any unlink prevents this: any thread seeing `is_deleted = true` returns an error rather than accessing the freed node.

**Trade-off:** Soft-deleted nodes occupy memory until the next resize. In workloads with frequent bulk deletions (e.g., expiring cache entries), memory may be underutilized. Shrinking and reclamation strategies are reserved for v2.0.

**Metrics:**
- `active_node_count` — live nodes (not soft-deleted).
- `total_node_count` — includes soft-deleted nodes; difference indicates cleanup backlog.

**See also:** [07-concurrency-model.md](07-concurrency-model.md), [09-memory-management.md](09-memory-management.md)

---

## 5. Resize Strategy: Always Double, Never Shrink

**Decision:** When a sub-bucket's collision chain exceeds `max_linked_list_chain_length`, double the sub-bucket size. No shrinking is implemented.

**Rationale:**
- **Amortized O(1) insertion:** Doubling is a well-proven strategy. Each element migrates a constant number of times across the lifetime of the table, guaranteeing amortized O(1) insertion cost.
- **Simplicity:** Shrinking logic adds complexity: when to trigger shrink? How much to shrink? How to coordinate shrink with ongoing reads/writes? These questions are deferred.
- **Steady-state workloads:** Most production workloads are append-heavy or reach a stable size. Shrinking is rarely needed.

**Trade-off:** Memory utilization after bulk deletions is suboptimal. Reserved for v2.0 if workload telemetry demands it.

**See also:** [08-dynamic-resizing.md](08-dynamic-resizing.md)

---

## 6. Spinlock for Chase Buffer Head/Tail

**Decision:** The chase buffer (write-flush queue for async resize) uses a spinlock, not a mutex, to protect head/tail pointer updates.

**Rationale:**
- **Minimal critical section:** The spinlock only protects pointer arithmetic and a compare-and-swap. No blocking work (no allocations, no I/O) occurs under the spinlock.
- **Low contention:** Multiple threads may try to enqueue writes, but actual hold time is nanoseconds. Context switching overhead would outweigh the brief busy-wait.
- **Appropriate primitive:** Spinlocks are ideal for very short critical sections on multi-core systems where contention is rare.

**Trade-off:** On single-core systems or under extreme contention, spinlocks can cause busy-waiting. For single-threaded deployments, the spinlock can be disabled via `CONCURRENCY_ENABLED` config.

**See also:** [10-background-task-manager.md](10-background-task-manager.md)

---

## 7. Detached Resize Worker, Joinable Chase Worker

**Decision:** The resize worker thread (migrating buckets during resize) is spawned detached. The chase worker thread (flushing async writes) is spawned joinable and explicitly awaited during resize finalization.

**Rationale:**
- **Resize detached:** The resize operation is long-running and decoupled from the caller's critical path. Detaching avoids the caller paying for thread join overhead. The resize task is tracked internally via the background task manager.
- **Chase joinable:** The chase worker buffers writes issued by the resize worker. Before declaring resize complete and switching to the new table, we must ensure all buffered writes are drained. Joining the chase worker guarantees this: we know all buffered data has been flushed before proceeding.

**Trade-off:** Requires careful lifecycle management. The detached resize worker must be tracked via the background task manager to avoid spawning unbounded threads. Forgetting to join the chase worker can leave the table in an inconsistent state.

**See also:** [08-dynamic-resizing.md](08-dynamic-resizing.md), [10-background-task-manager.md](10-background-task-manager.md)

---

## 8. Memory Ownership Model

**Decision:**

| API | Responsibility |
|-----|-----------------|
| `set_key(key, value)` | Copies key and value into internal storage. Caller retains ownership of input buffers; can free them immediately after return. |
| `get_key(key, ...)` | Allocates and returns output buffers for key and value. Caller must `free()` both buffers. |
| `delete_key(key)` | Performs logical delete (soft-delete); physical memory reclamation is deferred. |

**Rationale:**
- **API symmetry:** SET copies to storage (caller provides), GET allocates output (caller must clean up). This is a common and intuitive pattern.
- **Deferred reclamation:** Aligns with soft-delete semantics. Freeing memory happens during cleanup passes or resizes, not on every delete call.

**Trade-off:** Caller must remember to free buffers returned by `get_key()`. Future versions may provide a convenience function `get_key_free()` to simplify cleanup.

**See also:** [13-public-api-surface.md](13-public-api-surface.md), [DESIGN_DECISIONS.md](../../DESIGN_DECISIONS.md)

---

## 9. Memory Pool for Linked-List Nodes Only

**Decision:** Only `linked_list_node` allocations use the pre-allocated memory pool. `data_node`, `double_linked_list_node`, and management structs use standard `malloc`/`calloc`.

**Rationale:**
- **Allocation hotspot:** Linked-list nodes are allocated on every collision-chain insertion and heavily exercised in the hot path. Pooling provides measurable latency reduction (microseconds per operation).
- **Diverse sizes:** `data_node` and other structs vary in size depending on key/value lengths. Pooling diverse sizes leads to fragmentation and wasted slots. Not economical.
- **Measured trade-off:** Benchmark data shows pool hit rate on linked-list nodes is > 90% during steady state, while extending pooling to other structs yields diminishing returns.

**Trade-off:** Fragmentation for `data_node` is not eliminated. A future v1.5 may extend pooling to fixed-size `data_node` allocations if profiling shows benefit.

**Future:** The `free_memory` pool flag is rarely used by callers; only the memory manager itself sets `is_pool = true` internally. See [09-memory-management.md](09-memory-management.md) for details.

**See also:** [09-memory-management.md](09-memory-management.md)

---

## 10. In-Memory, No Persistence

**Decision:** v1.0 is fully in-memory. No write-ahead log (WAL), snapshots, or crash-recovery mechanism.

**Rationale:**
- **Simplicity:** Persistence adds significant complexity to the concurrency model (coordinating disk writes with in-memory updates, recovery state machines, etc.). An in-memory design maximizes throughput and minimizes latency.
- **Application-layer flexibility:** Persistence and replication can be implemented at the application layer via periodic snapshots, external replication, or event streams. This allows KeyStore to remain a high-performance core and lets users choose their own durability/consistency model.
- **Aligned with use case:** KeyStore v1.0 targets in-process caching and ephemeral key-value storage, where durability is not a primary requirement.

**Trade-off:** Data is lost on process crash. For applications requiring durability, use external replication or add a snapshot layer. WAL support is reserved for v2.0.

**See also:** [DESIGN_DECISIONS.md](../../DESIGN_DECISIONS.md) — "No WAL / Persistence" section

---

## Related Docs

- [01-overview.md](01-overview.md) — High-level architecture summary
- [07-concurrency-model.md](07-concurrency-model.md) — Two-phase lock protocol invariants and proofs
- [08-dynamic-resizing.md](08-dynamic-resizing.md) — Resize lifecycle and optimization
- [09-memory-management.md](09-memory-management.md) — Memory pool and allocation policies
- [DESIGN_DECISIONS.md](../../DESIGN_DECISIONS.md) — Known limitations and future plans
