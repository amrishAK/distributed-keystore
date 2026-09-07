# Design Decisions & Known Caveats

This document details key design choices, rationale, and known limitations in KeyStore v1.0.

## Design Decisions

### Soft-Delete Semantics

**Decision:** Delete operations mark `data_node.is_deleted = true` under the node mutex. Physical removal happens only during `cleanup_deleted_linked_list_nodes` (triggered at resize or chain-length threshold, under a WRITE rwlock).

**Rationale:**
- Avoids expensive in-place list unlink operations under heavy concurrent load.
- `active_node_count` tracks live nodes; `total_node_count` includes soft-deleted.
- The `is_deleted` flag also closes the use-after-free race in the window between rwlock release and node mutex acquisition: a concurrent DELETE sets `is_deleted = true` under the node mutex before unlinking the node, so any reader hitting the same node in that window returns `ERR_DATA_NODE_NOT_FOUND` rather than accessing freed memory.

**Trade-off:** Some memory is held by soft-deleted nodes until the next resize. See [docs/architecture/07-concurrency-model.md](./architecture/07-concurrency-model.md) for details on the two-phase lock protocol.

---

### Resize Strategy: Always Double

**Decision:** When a sub-hash-bucket's chain exceeds `max_linked_list_chain_length`, the sub-table size is doubled (multiplied by 2). No shrinking is implemented.

**Rationale:**
- Doubling is a well-proven strategy that guarantees amortized O(1) insertion.
- Shrinking adds complexity and is rarely needed in append-heavy or steady-state workloads.

**Trade-off:** Memory utilization after bulk deletions may be suboptimal. Shrinking is reserved for v2.0 if workload patterns demand it.

See [docs/architecture/04-two-level-hash-table.md](./architecture/04-two-level-hash-table.md) for collision resolution and threshold details.

---

### Memory Pool Scope: Linked List Nodes Only

**Decision:** Only `linked_list_node` allocations use the pre-allocated memory pool. `data_node`, `double_linked_list_node`, and management structs use standard `malloc`/`calloc`.

**Rationale:**
- Linked list nodes are the highest-frequency allocation in the hot path (collision chains, chase buffer).
- Pooling them provides measurable latency reduction for common operations.
- Other structs are allocated less frequently and have diverse sizes, making pooling less beneficial.

**Trade-off:** Fragmentation for `data_node` and other structs is not eliminated. A future v1.5 may extend pooling to fixed-size `data_node` allocations.

**Note:** The `free_memory` pool flag is nearly always passed as `false` in caller code; only the memory manager itself uses `is_pool = true` internally via `_free_memory_to_pool`.

---

### Dual-Seed MurmurHash3

**Decision:** Each key is hashed twice using MurmurHash3 (64-bit) with two independent per-instance seeds. The two seeds are generated via `clock_gettime(CLOCK_MONOTONIC)` and guaranteed distinct by XOR-ing the second with the golden-ratio constant `0x9e3779b97f4a7c15`.

**Rationale:**
- Eliminates correlation between level-1 (bucket hash) and level-2 (sub-bucket hash) routing.
- A single hash split across both levels suffered from biased key distribution under load (e.g., keys with low entropy in low bits ended up in the same sub-buckets across all buckets).
- Dual-seed approach improves uniformity and reduces cache contention.

**Trade-off:** Slightly higher per-key CPU cost at insert/lookup (two hash computations vs. one split hash), but overall throughput improves due to better load balancing.

---

### Two-Phase Lock Protocol

**Decision:** The concurrency model uses a two-phase lock protocol:

- **Phase 1 (rwlock):** acquire → traverse list → capture node pointer → **release**
- **Phase 2 (node mutex):** acquire → check `is_deleted` → read/write value or soft-delete → **release**

Locks are never held simultaneously.

**Rationale:**
- Releasing the rwlock before acquiring the node mutex allows threads targeting *different nodes within the same sub-bucket* to run Phase 2 in parallel, beyond what a single rwlock would permit.
- The `is_deleted` flag (checked under the node mutex) closes the use-after-free race in the window between rwlock release and node mutex acquisition.

**Trade-off:** Requires careful reasoning about invariants and potential TOCTOU races. See [docs/architecture/07-concurrency-model.md](./architecture/07-concurrency-model.md) for full details.

---

## Known Limitations

### `key_hash == 0` Treated as Invalid

**Issue:** `_get_hash_table_bucket` and `_get_sub_hash_table_bucket` reject `key_hash == 0`. Keys that naturally hash to 0 would fail. `UINT64_MAX` is the murmur error sentinel.

**Status:** Open — correctness bug, affects a tiny fraction of keys but should be fixed.

**Suggested Fix:** Use a separate flag or sentinel value (e.g., `HASH_INVALID` bit pattern) instead of rejecting 0.

---

### No WAL / Persistence

**Decision:** v1.0 is fully in-memory. No write-ahead log (WAL) or persistence layer.

**Rationale:** In-memory design simplifies concurrency and maximizes throughput. Persistence can be added at the application layer via snapshots or external replication.

**Reserved for v2.0:** WAL stubs are marked throughout with `/* WAL: v2.0 */`.

---

### Background Task Registry Capacity

**Limitation:** The background task registry has a hard-coded capacity of 100 tasks.

**Impact:** In extreme concurrency scenarios (> 100 concurrent resize operations), overflow may cause silent task failures.

**Workaround:** Increase `BACKGROUND_TASK_MANAGER_CAPACITY` in [src/keystore/type_definitions/background_task_manager_type_definitons.h](../src/keystore/type_definitions/background_task_manager_type_definitons.h).

**Future:** v2.0 may implement dynamic registry growth.

---

### Debug Traces in Production Code

**Status:** ✅ Resolved — Production code is clean.

The core keystore library (`src/keystore/`) contains no debug `printf` or `fprintf` traces. Test and benchmark code use output appropriately. A logging abstraction header `utils/logging.h` is provided for future use if needed.

---

### Platform-Specific Code: POSIX Sleep

**Status:** ✅ Resolved — Cross-platform abstraction implemented.

The codebase uses `portable_sleep_ms()` and `portable_sleep_us()` from `utils/helper_functions.h`, which provides Windows and POSIX implementations. All resize paths and chase buffer worker use these functions for portability.

---

### Memory Ownership of `read_data_node_value`

**Issue:** `read_data_node_value` allocates both `key` and `value` buffers. The caller must `free()` both. The integration test correctly frees `value` but skips `free(key)` (potential minor leak in test harness, not in the library).

**Status:** Open — integration test cleanup pending.

**Recommended API:** Future versions may provide a single `read_data_node_value_free()` function for convenience.

---

## See Also

- [Architecture Overview](./architecture/01-overview.md) — complete subsystem descriptions
- [Concurrency Model](./architecture/07-concurrency-model.md) — two-phase lock protocol, invariants
- [Memory Management](./architecture/09-memory-management.md) — memory pool, ownership rules
- [Error Codes](../ERROR_CODES.md) — error propagation strategy
- [Progress](./progress.md) — v1.0 checklist and open issues
