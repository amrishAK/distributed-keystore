# Concurrency Model

The keystore uses **conditional locking** controlled by the `is_concurrency_enabled` configuration flag. When disabled, all lock acquisition is bypassed and operations proceed directly on the data structures (single-threaded safety). When enabled, a **hierarchical four-level lock regime** provides fine-grained concurrency with minimal contention.

> **Related:** See [05-data-structures.md](05-data-structures.md) for lock field placement within `data_node`, `sub_hash_bucket`, and `hash_bucket` structures.

## Table of Contents
- [1. Design Rationale](#1-design-rationale)
- [2. Lock Hierarchy](#2-lock-hierarchy)
- [3. Two-Phase Lock Protocol](#3-two-phase-lock-protocol)
- [4. Concurrency During Resize](#4-concurrency-during-resize)
- [5. Deadlock Prevention](#5-deadlock-prevention)
- [6. Soft-Delete and Race Condition Prevention](#6-soft-delete-and-race-condition-prevention)
- [7. Single-Threaded Mode](#7-single-threaded-mode)
- [8. Synchronization Guarantees](#8-synchronization-guarantees)
- [9. Error Handling Under Concurrency](#9-error-handling-under-concurrency)
- [10. Performance Implications](#10-performance-implications)

## 1. Design Rationale

The concurrency model achieves two goals:

1. **Low Contention:** Locks are scoped to the smallest required critical section. The two-phase lock protocol (§3) separates list traversal from node modification, allowing different threads to safely operate on different nodes within the same sub-bucket.
2. **Deadlock Avoidance:** A strict, enforced lock ordering (coarse → fine) eliminates circular wait conditions. Locks are always acquired in the same order and released in reverse.

## 2. Lock Hierarchy

The system uses four synchronization primitives, organized from **coarsest scope to finest scope**:

| Level | Scope | Primitive | Guards | Wait Type |
|-------|-------|-----------|--------|-----------|
| 0 | `hash_bucket` | `pthread_mutex_t resizing_lock` | Resize state flag, snapshot/buffer transitions | Blocking Mutex |
| 1 | `sub_hash_bucket` | `pthread_rwlock_t sub_hash_bucket_lock` | Linked-list structure, insert/remove/cleanup, counters | Blocking RW-Lock |
| 2 | `data_node` | `pthread_mutex_t lock` | Node value, `is_deleted` state | Blocking Mutex |
| 3 | Chase Buffer | `pthread_spinlock_t buffer_lock` | Buffer head/tail pointers during resize | Spinning Lock |

**Mandatory Ordering (must acquire in this sequence):**
```
resizing_lock (0)
    ↓
sub_hash_bucket_lock (1)
    ↓
data_node.lock (2)
    ↓
buffer_lock (3, special case — see §4)
```

**Visual Hierarchy:**

```
ACQUISITION ORDER (Never Violate)
═════════════════════════════════════════════════════════════════════════

  Normal Operations                      During Resize
  ──────────────────                     ──────────────

        NO LOCK (single-threaded)             resizing_lock ◄─ Phase 0
              │                                     │
        [is_concurrency_enabled?]           [exclusive, serializes clients]
             /    \                                 │
           YES    NO                                │
            │      │                                ├─► snapshot data (read-only)
            ▼      │                                │
    sub_hash_bucket_lock                   ├─► new_operation_buffer
         [RWlock]  │                            ├─► buffer_lock (spinlock, fast)
            │      │                            │
            ├─► Phase 1: Traverse              └─► resume clients → Phase 0
            │                                     lock released
            ├─► Capture node pointer
            │
            ├─► Release sub_hash_bucket_lock
            │
            ▼
      data_node.lock
       [Mutex] ◄─ Phase 2: Modify
```


## 3. Two-Phase Lock Protocol

The **core insight** is that list traversal and node modification are **independent critical sections**. This separation enables high concurrency within a single sub-bucket: one thread may traverse the list while another modifies a different node in the same list.

### Phase 1: List Traversal (RW-Lock Scope)

Acquire the sub-bucket's read or write lock, traverse the linked list, capture the target node pointer, then release the lock immediately. The lock is held for minimal duration—only the list walk, not the subsequent node modification.

### Phase 2: Node Modification (Node Mutex Scope)

Acquire the target node's mutex, verify `is_deleted` state (skip if deleted), read/modify/delete the node value, then release the mutex. This is fully independent of the list structure.

```
┌─────────────────────────────────────────────────────────────────────┐
│  THREAD A (SET existing key)          THREAD B (SET new key)         │
├─────────────────────────────────────────────────────────────────────┤
│                                                                       │
│  Time T1:                           Time T1:                         │
│  ┌─────────────────────────────┐    ┌─────────────────────────────┐  │
│  │ acquire_read_lock(sub_lock) │    │ acquire_write_lock(sub_lock)│  │
│  │ [Phase 1: Traversal]        │    │ [Phase 1: Traversal]        │  │
│  │ traverse list...            │    │ traverse list...            │  │
│  │ find node_A                 │    │ key not found               │  │
│  │ capture: node_ptr = node_A  │    │ allocate new_node_B         │  │
│  └─────────────────────────────┘    │ insert_at_head(new_node_B)  │  │
│                                     └─────────────────────────────┘  │
│                                                                       │
│  Time T2:                           Time T2:                         │
│  ┌─────────────────────────────┐                                     │
│  │ release_read_lock(sub_lock) │    ┌─────────────────────────────┐  │
│  │ [Phase 1 complete]          │    │ release_write_lock(sub_lock)│  │
│  └─────────────────────────────┘    │ [Phase 1 complete]          │  │
│                                     └─────────────────────────────┘  │
│                                                                       │
│  Time T3:                           Time T3:                         │
│  ┌─────────────────────────────┐    ┌─────────────────────────────┐  │
│  │ acquire_mutex(node_A->lock) │    │ acquire_mutex(node_B->lock) │  │
│  │ [Phase 2: Modification]     │    │ [Phase 2: Modification]     │  │
│  │ check is_deleted            │    │ check is_deleted            │  │
│  │ update node_A->value        │    │ write node_B->value         │  │
│  │ release_mutex(node_A->lock) │    │ release_mutex(node_B->lock) │  │
│  └─────────────────────────────┘    └─────────────────────────────┘  │
│                                                                       │
│  ✓ PARALLEL EXECUTION (different nodes, different mutexes)           │
│                                                                       │
└─────────────────────────────────────────────────────────────────────┘
```

**Key Advantages:**

| Property | Benefit |
|----------|---------|
| **Phase 1 is short** | List lock held only during traversal (~100 CPU cycles), not during node modification |
| **Phase 2 is independent** | Different node mutexes allow parallel modification of different nodes in the same list |
| **No nested locks** | Phase 1 lock is always released before Phase 2 lock acquired → no deadlock |
| **RW-Lock read advantage** | Multiple threads can traverse the same sub-bucket simultaneously (acquiring read locks), then each modifies its own node |

### Example: Why This Matters

**Worst case (coarse-grain lock holding entire node->value through traversal):**
- 1,000 threads competing on single sub-bucket lock
- Throughput = ~1 lock acquisition per critical section (≈100–1000 ns serialization per-thread)

**With two-phase protocol:**
- 1,000 threads: each acquires sub-lock for ~100 CPU cycles (~30 ns), then releases
- If they target different nodes → all acquire their node mutexes in parallel
- Throughput = near-linear scaling (minimal contention)


## 4. Concurrency During Resize

When a sub-hash-table becomes too crowded (measured by `max_chain_length`), a background resize worker migrates data to a larger table. During this time, client operations must remain safe. The system achieves this through **serialization under resizing_lock** and a **chase buffer** for in-flight operations.

### States and Transitions

```
STATE 0: NORMAL (is_resizing = false)
═════════════════════════════════════════════════════════════════

  ┌─────────────────────────────────────────────────┐
  │  live_sub_hash_table (current table)            │
  │  ├─ sub_bucket[0]   ──► linked_list_node -> DN │
  │  ├─ sub_bucket[1]   ──► linked_list_node -> DN │
  │  └─ sub_bucket[...] ──► linked_list_node -> DN │
  └─────────────────────────────────────────────────┘

  • Thread A (SET) ─► acquire sub_lock (read/write) ─► traverse ─► release
                  ─► acquire node->lock ─► modify ─► release
  • Thread B (GET) ─► acquire sub_lock (read) ─► traverse ─► release
                  ─► acquire node->lock ─► read ─► release
  • Thread C (DEL) ─► acquire sub_lock (write) ─► traverse ─► release
                  ─► acquire node->lock ─► set is_deleted ─► release

  ✓ All threads bypass resizing_lock, operate directly on live_table


STATE 1: RESIZE_INIT (is_resizing = true, SNAPSHOT_STABLE phase)
════════════════════════════════════════════════════════════════════

  Resize worker acquires resizing_lock (exclusive) to initialize:

  ┌─────────────────────────────┐         ┌──────────────────────┐
  │ live_sub_hash_table         │         │ new_sub_hash_table   │
  │ (read-only snapshot)        │         │ (destination, empty) │
  │ ├─ sub_bucket[0]            │         │ ├─ sub_bucket[0]     │
  │ └─ sub_bucket[...]          │         │ └─ sub_bucket[...]   │
  └─────────────────────────────┘         └──────────────────────┘
           │                                         ▲
           │                                         │
           └──────────────► [resize_worker migrates data]
                           [copies from snapshot → new_table]

  • new_operation_buffer  (FIFO queue, holds pending SET/DEL)
  • delete_operation_buffer (FIFO queue, holds pending DEL)

  Client threads attempting SET/GET/DEL:
  ├─► ACQUIRE resizing_lock [BLOCKING on resize_worker's exclusive lock]
  ├─► See is_resizing=true
  ├─► Route to new_operation_buffer or delete_operation_buffer (for DEL)
  ├─► RELEASE resizing_lock
  └─► [buffers drained by chase_worker or finalization]


STATE 2: RESIZE_DRAIN (is_resizing = true, DRAIN_BUFFERS phase)
═══════════════════════════════════════════════════════════════════

  Resize worker releases resizing_lock, chase_worker drains buffers:

  ┌────────────────────────────────────────────────────────────┐
  │ Concurrency Detail:                                        │
  │                                                            │
  │  Resize worker:                                           │
  │  ├─ NOT holding resizing_lock                             │
  │  ├─ copying snapshot → new_table (no locks held)          │
  │  └─ [periodic flush to new_table when buffer full]        │
  │                                                            │
  │  Chase worker:                                            │
  │  ├─ drains new_operation_buffer ──► new_table             │
  │  │  (acquires buffer_lock only for head/tail pointers)    │
  │  ├─ applies DEL operations ──► delete_operation_buffer    │
  │  └─ [spinlock is brief — just pointer updates]            │
  │                                                            │
  │  Client threads:                                          │
  │  ├─ ACQUIRE resizing_lock [brief contention]              │
  │  ├─ Route SET/GET to buffers + snapshot                   │
  │  ├─ RELEASE resizing_lock [fast path]                     │
  │  └─ [buffers are eventually applied by chase_worker]      │
  │                                                            │
  └────────────────────────────────────────────────────────────┘


STATE 3: RESIZE_COMPLETE (is_resizing = false, switch table)
═════════════════════════════════════════════════════════════════

  Resize worker acquires resizing_lock (exclusive) to finalize:
  ├─ drain remaining buffers
  ├─ swap: live_sub_hash_table = new_sub_hash_table
  ├─ free old table memory
  ├─ reset is_resizing = false
  └─ RELEASE resizing_lock

  Client threads now route back to STATE 0 (NORMAL).
```

### Lock Contention Analysis

```
Operation         | Normal Mode              | Resize Mode
──────────────────┼──────────────────────────┼──────────────────────
SET (key exists)  | sub_lock (RW) → node_lock | resizing_lock → buffer
                  | [2 phases, fast]         | [single acquire, fast]
──────────────────┼──────────────────────────┼──────────────────────
SET (new key)     | sub_lock (W) → node_lock | resizing_lock → buffer
                  | [2 phases]               | [single acquire]
──────────────────┼──────────────────────────┼──────────────────────
GET               | sub_lock (R) → node_lock | resizing_lock → snap+buf
                  | [2 phases]               | [single acquire]
──────────────────┼──────────────────────────┼──────────────────────
DEL               | sub_lock (W) → node_lock | resizing_lock → del_buf
                  | [2 phases, soft-delete]  | [single acquire]
──────────────────┼──────────────────────────┼──────────────────────

Resize Worker:
- Acquires resizing_lock twice (init + finalize)
- Does NOT hold resizing_lock during bulk copy (DRAIN phase)
- ✓ Clients continue to run in DRAIN phase (buffers absorb them)
```

### Chase Buffer: The Fast Path During Resize

When `is_resizing = true`, client operations are enqueued into a **chase buffer** (in-flight operations queue) instead of directly modifying the live table. The chase worker consumes this buffer and applies operations to the new table with minimal lock contention.

```
CLIENT THREAD (during resize)              CHASE WORKER
═════════════════════════════════════════════════════════════

Step 1: Acquire resizing_lock
  [brief mutex wait — fast path]

Step 2: Check is_resizing
  ✓ is_resizing == true
  → route to new_operation_buffer

Step 3: Enqueue operation
  acquire buffer_lock (spinlock)
  append to buffer tail
  release buffer_lock
  ✓ [spinlock: ~10s of ns]

Step 4: Release resizing_lock
  [fast — uncontended]

Step 5: Return to client
  ✓ total time: ~1 microsecond
     (depends on mutex contention)

                                           Step 6: Drain buffer
                                           acquire buffer_lock
                                           read head → tail
                                           release buffer_lock

                                           Step 7: Apply ops
                                           for each op in buffer:
                                             apply_to_new_table()
                                             [no locks held]

                                           Step 8: Check snapshot
                                           for overwritten keys:
                                             remove from snapshot
```

## 5. Deadlock Prevention

The strict lock ordering enforced in §2 **guarantees no deadlock**. Here's why:

### The Ordering Rule (Always Enforce)

```
resizing_lock (0) ─► sub_hash_bucket_lock (1) ─► data_node.lock (2)
                          │
                          ├─► buffer_lock (3) [only during resize drain]
                          └─► [never loop back to (0), (1), (2)]
```

- **Never acquire in reverse order** (e.g., node_lock before sub_bucket_lock).
- **Never skip levels** (e.g., node_lock directly without sub_bucket_lock).
- **No circular dependencies:** Lock(A) never waits on Lock(B) if Lock(B) can wait on Lock(A).

### Verification by Code Path

| Code Path | Lock Sequence | Cycle? |
|-----------|-------|--------|
| Normal SET/GET/DEL | resizing_lock → sub_lock → node_lock | ✓ No |
| Resize init/finalize | resizing_lock (exclusive) only | ✓ No |
| Resize drain (chase worker) | buffer_lock (spinlock only) | ✓ No |
| Snapshot migration | NO LOCKS | ✓ No |

> **Proof:** If there exists a wait-for cycle, some thread T1 must wait on lock L1, and T1 holds lock L0 such that L0 < L1 in the ordering. But the ordering rule forbids acquiring L1 after L0, so the cycle is impossible. ∎

## 6. Soft-Delete and Race Condition Prevention

### The `is_deleted` Flag

Data nodes are never immediately deallocated. Instead, they are **soft-deleted**: the `is_deleted` flag is set to true, marking the node as logically removed. This decouples deletion from deallocation and avoids use-after-free races.

```
Timeline: Thread A (DELETE) vs Thread B (GET same key)
═════════════════════════════════════════════════════════════════

Thread A                                   Thread B
────────────────────────────────────────────────────────────────
T1: acquire sub_lock (WRITE)
    traverse list
    find node_X
    release sub_lock

T2: acquire node_X->lock
    set node_X->is_deleted = true       T2: acquire sub_lock (READ)
    release node_X->lock                    traverse list
                                          find node_X
                                          release sub_lock

T3:                                     T3: acquire node_X->lock
                                           check is_deleted
                                           ✓ is_deleted == true
                                           → return KEY_NOT_FOUND
                                           release node_X->lock

✓ Outcome: GET returns KEY_NOT_FOUND, no crash, no use-after-free
✓ node_X remains allocated until next GC/cleanup pass
```

### RW-Lock Protection

The sub-bucket's read/write lock ensures that concurrent readers and writers see a consistent view of the list structure:

- **Write locks** (DELETE, INSERT) are exclusive: only one writer at a time.
- **Read locks** (GET, traverse for SET) allow multiple readers in parallel.
- **Lock ordering:** Writers must not be starved; the RW-lock typically favors writers on new acquisitions.

## 7. Single-Threaded Mode

When `is_concurrency_enabled = false`, the configuration disables all lock acquisition:

```c
// Pseudo-code
if (is_concurrency_enabled) {
    pthread_mutex_lock(&resizing_lock);
}
// ... operation ...
if (is_concurrency_enabled) {
    pthread_mutex_unlock(&resizing_lock);
}
```

**Advantages:**
- Zero lock overhead for embedded systems or single-process deployments.
- Identical code path → no separate ST/MT logic branches.
- Debugging simplified: no race conditions, no deadlock concerns.

**When to Use:**
- Embedded systems with limited threads.
- Initialization/setup phases where concurrency is not needed.
- Benchmarking to measure lock overhead.

## 8. Synchronization Guarantees

| Guarantee | Provided? | Notes |
|-----------|-----------|-------|
| **Atomicity** | Per-node | SET/GET/DEL on a single key is atomic (§3, Phase 2). Multi-key ops are not atomic. |
| **Visibility** | YES | `pthread_mutex_t` and `pthread_rwlock_t` provide acquire-release semantics. All threads see consistent state after lock release. |
| **Ordering** | Partial | Operations on the **same key** preserve order. Operations on **different keys** may reorder (no global ordering). |
| **No Lost Updates** | YES | Write locks + `is_deleted` guard prevent lost updates on concurrent DELETE+SET. |
| **No Use-After-Free** | YES | `is_deleted` prevents read of freed node; node freed only after no refs exist. |
| **Resize Safety** | YES | `resizing_lock` + buffers + snapshot ensure ops see consistent state during resize. |

## 9. Error Handling Under Concurrency

### Out-of-Memory (OOM)

If `malloc()` fails during SET:
- The node allocation fails → error code returned to client.
- No partial state; state unchanged.
- Other threads unaffected; locks released cleanly.

### Lock Acquisition Failures

If `pthread_mutex_lock()` or `pthread_rwlock_rdlock()` fails (rare):
- Operation returns error code.
- No state change; locks not held.
- System remains consistent.

### Abort During Resize

If resize is explicitly cancelled or times out:
- `resizing_lock` released.
- `is_resizing` set to false.
- Live table unchanged; resize buffers discarded.
- Clients resume normal operations.

## 10. Performance Implications

```
Workload                   | Lock Contention | Throughput Impact
───────────────────────────┼─────────────────┼──────────────────
1 thread, 1M ops          | None            | Baseline (ST mode)
10 threads, same bucket   | Moderate        | 8–9× speedup (sub_lock contention)
100 threads, 100 buckets  | Low             | ~95× speedup (minimal contention)
Resize in progress        | High            | 50–80% throughput (buffers absorb)
Post-resize               | Low             | Full throughput restored
```

**Tuning Knobs:**
- Increase `bucket_size` (Level 1) to reduce per-bucket contention.
- Increase `sub_bucket_size` (Level 2) to reduce per-node contention (shorter linked lists).
- Disable resizing (`is_resizing_enabled = false`) for embedded deployments.
- Use single-threaded mode (`is_concurrency_enabled = false`) if threads are not available.

---

## Summary

The two-level lock hierarchy with two-phase protocol delivers **fine-grained parallelism** while maintaining **deadlock-free**, **race-free** operation. The design sacrifices global ordering for local atomicity and scales well under contention. Soft-delete semantics and resize-time buffering allow seamless table growth without blocking clients.
