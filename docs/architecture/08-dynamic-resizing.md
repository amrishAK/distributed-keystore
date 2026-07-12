# Dynamic Resizing Architecture

Resizing is triggered when insertion pressure builds inside a sub-hash-bucket chain and soft-deleted node cleanup cannot relieve the pressure. The resize mechanism is non-blocking: resizing happens in a background thread while client operations continue, buffering writes to be applied during finalization.

> **Core principle:** Defer rehashing until necessary; clean up soft-deleted nodes first; resize in the background; buffer and replay writes during finalization.
> **Related:** See [05-data-structures.md](05-data-structures.md), §5.4 for the resizing buffer hierarchy and field semantics (`snapshot_sub_hash_table_ptr`, `resizing_buffer_ptr`, chase buffer).

## Table of Contents
- [1. Trigger Logic](#1-trigger-logic)
- [2. Resize Lifecycle](#2-resize-lifecycle)
- [3. Concurrency Model & Thread Coordination](#3-concurrency-model--thread-coordination)
- [4. Write Buffering During Migration](#4-write-buffering-during-migration)
- [5. Client Behavior During Resizing](#5-client-behavior-during-resizing)
- [6. Return Codes](#6-return-codes)
- [7. Memory & Resource Management](#7-memory--resource-management)
- [8. Known Limitations & Future Improvements](#8-known-limitations--future-improvements)

## 1. Trigger Logic

**Step 1: Check pressure threshold**
```text
if total_node_count <= max_linked_list_chain_length
  → No resize needed
  → Exit
```

**Step 2: Evaluate active vs. soft-deleted split**
```text
if active_node_count == total_node_count
  → No soft-deleted nodes to reclaim
  → Trigger resize immediately
  → Return: SUCCESS_ADDED_NEW_NODE_RESIZING_TRIGGERED (20)

else
  → Soft-deleted nodes exist
  → Proceed to cleanup (see §8.1.1)
```

**Step 3: Re-evaluate after cleanup**
```text
cleanup_soft_deleted_nodes(sub_bucket)

if total_node_count after cleanup >= max_linked_list_chain_length
  → Pressure remains
  → Trigger resize
  → Return: SUCCESS_ADDED_NEW_NODE_RESIZING_TRIGGERED (20)

else
  → Pressure relieved by cleanup
  → No resize needed
  → Return: SUCCESS (0)
```

---

## 2. Resize Lifecycle

The resize process unfolds in four sequential phases, with the first and fourth phases guarded by the `resizing_lock` mutex.

### Phase 1: Trigger Detection

**Caller:** `upsert_node_to_sub_hash_table()` (in sub-hash-bucket)  
**Lock held:** None yet  
**Action:**  
- Increment `total_node_count` and `active_node_count`
- Call `_check_for_resize_condition(sub_bucket)`
- If pressure remains → return `SUCCESS_ADDED_NEW_NODE_RESIZING_TRIGGERED` (20)

**Sequence:**
```
Client thread calls upsert_node_to_hash_bucket()
    ↓
[is_resizing == false?]
    ├─ YES → Call upsert_node_to_sub_hash_table()
    │         └─ Detects pressure → returns code 20
    │
    └─ NO → Route to resizing-aware handler
            (will re-check and buffer if needed)

Code 20 detected in upsert_node_to_hash_bucket()
    ↓
Acquire resizing_lock
    ↓
Call initialize_hash_bucket_resizing()
    ↓
Release resizing_lock
    ↓
Return code 20 to caller
```

---

### Phase 2: Initialization

**Caller:** `upsert_node_to_hash_bucket()` (holding `resizing_lock`)  
**Lock held:** `resizing_lock`  
**Action:**  
1. Set `is_resizing = true`
2. Snapshot: `snapshot_sub_hash_table_ptr = sub_hash_table_ptr`
3. Allocate `resizing_buffer_ptr` with three buffers:
   - `new_operation_buffer` (DLL for concurrent writes)
   - `updated_operation_buffer` (pending updates to existing nodes)
   - `delete_operation_buffer` (pending deletes)
4. Spawn **detached** resize worker thread

**Diagram:**
```
┌─────────────────────────────────────────────────────┐
│ Hash Bucket State (Before Phase 2)                  │
├─────────────────────────────────────────────────────┤
│ is_resizing: false                                  │
│ sub_hash_table_ptr: ─────► [Live Table]             │
│ snapshot_sub_hash_table_ptr: NULL                   │
│ resizing_buffer_ptr: NULL                           │
└─────────────────────────────────────────────────────┘
            ↓  (Phase 2 starts)
┌─────────────────────────────────────────────────────┐
│ Hash Bucket State (After Phase 2)                   │
├─────────────────────────────────────────────────────┤
│ is_resizing: **true**                               │
│ sub_hash_table_ptr: ─────► [Live Table]             │
│ snapshot_sub_hash_table_ptr: ─────► [Snapshot]      │
│ resizing_buffer_ptr: ─────► [Resizing Buffers]      │
│                                                     │
│ Resize Worker Thread: SPAWNED (detached)            │
│ ├─ Status: Running in background                    │
│ └─ Not joined yet                                   │
└─────────────────────────────────────────────────────┘
```

---

### Phase 3: Migration (Background Thread)

**Caller:** `hash_bucket_resize_worker()` (background thread)  
**Lock held:** None during migration  
**Action:**  
1. Create new sub-hash-table with 2× `bucket_size`
2. Spawn **joinable** chase worker thread
3. Migrate all nodes from `snapshot_sub_hash_table_ptr` → new table
   - Rehash each node using new bucket count
   - Skip soft-deleted nodes (they are not copied)
4. Stop and **join** chase worker thread
   - Ensures all buffered operations are consumed
5. Perform final consistency checks

**Concurrency during Phase 3:**
```
Background Resize Worker (Phase 3)      Client Threads
─────────────────────────────         ─────────────────

Create new table (2× size)            
                                      Thread A: upsert(key1) → appended to new_operation_buffer
                                      Thread B: get(key2) → reads from snapshot
                                      Thread C: delete(key3) → appended to delete_operation_buffer

Spawn chase worker ───────────┐
                              │
Migrate from snapshot ◄───────┴────── Chase worker consumes & rehashes buffered writes
                                      (moves HEAD→TAIL)

Join chase worker
(waits for consumption)
        ↓
All buffered operations processed
        ↓
Move to Phase 4 (finalization)
```

---

### Phase 4: Finalization

**Caller:** `hash_bucket_resize_worker()` (holding `resizing_lock`)  
**Lock held:** `resizing_lock`  
**Action:**  
1. Commit remaining buffered operations to the new table
2. Swap: `sub_hash_table_ptr = new_sub_hash_table`
3. Free:
   - `snapshot_sub_hash_table_ptr` → old table
   - `resizing_buffer_ptr` → all buffers
4. Set `is_resizing = false`
5. Return to background task manager (thread exits)

**Diagram:**
```
┌─────────────────────────────────────────────────────┐
│ Hash Bucket State (Before Phase 4)                  │
├─────────────────────────────────────────────────────┤
│ is_resizing: true                                   │
│ sub_hash_table_ptr: ─────► [Old Table]              │
│ snapshot_sub_hash_table_ptr: ─────► [Snapshot]      │
│ resizing_buffer_ptr: ─────► [Buffers]               │
│ new_table (local var): ─────► [New Table, 2×size]   │
└─────────────────────────────────────────────────────┘
            ↓  (Phase 4: finalization under lock)
┌─────────────────────────────────────────────────────┐
│ Hash Bucket State (After Phase 4 → Stable)          │
├─────────────────────────────────────────────────────┤
│ is_resizing: **false**                              │
│ sub_hash_table_ptr: ─────► [New Table, 2×size] ✓    │
│ snapshot_sub_hash_table_ptr: NULL (freed)           │
│ resizing_buffer_ptr: NULL (freed)                   │
│                                                     │
│ Result: Capacity doubled, pressure relieved         │
└─────────────────────────────────────────────────────┘
```

---

## 3. Concurrency Model & Thread Coordination

### Thread Roles

| Thread Type | Spawned By | Thread Model | Lifetime | Purpose |
|---|---|---|---|---|
| **Client** | Caller | Synchronous | Caller's duration | Makes keystore requests (upsert/get/delete) |
| **Resize Worker** | `hash_bucket_resize_worker()` | Detached | Phase 2–4 (background) | Orchestrates Phase 3 migration; spawns chase worker |
| **Chase Worker** | Resize worker | Joinable | Phase 3 only | Consumes buffered writes during migration |

### Synchronization Points

```
Phase 1: Trigger detection (no lock)
    │
    ▼
Phase 2: Initialization (resizing_lock held)
    │
    ├─► Resize worker spawned (detached) ────────┐
    │                                            │
    ▼                                            ▼
Client threads routing decisions             Background execution:
├─ is_resizing? YES                          Phase 3: Migration
│  └─ Route through resizing-aware handlers     │
│  └─ Upserts → append to buffers               ├─ Chase worker spawned (joinable)
│  └─ Gets → search snapshot + buffers          │
│  └─ Deletes → append to buffers               ├─ Migrate snapshot → new table
│                                               │
├─ is_resizing? NO                              ├─ Join chase worker (wait completion)
│  └─ Direct operation on new table             │
│                                               ▼
(Clients may see resizing complete             Phase 4: Finalization
 while their request was in-flight)            (resizing_lock held again)
                                                 │
                                                 ├─ Commit remaining buffers
                                                 ├─ Swap pointers
                                                 ├─ Free old table & buffers
                                                 ├─ Set is_resizing = false
                                                 │
                                                 ▼
                                            Worker thread exits
                                            (Clients see stable state)
```

> **Note:** The resize worker is **detached** (Phase 1–2). If the hash bucket is destroyed while resizing, `cleanup_hash_bucket()` enters a polling loop until `is_resizing == false`. This ensures graceful shutdown without resource leaks.

---

## 4. Write Buffering During Migration

During Phase 3, client writes are buffered in a **doubly-linked list** instead of being applied immediately. This is essential because the new table layout (2× buckets) is incompatible with the snapshot's bucket indices. Buffering decouples client threads from the long-running rehash operation.

### Buffer Flow

```
┌─────────────────────────────────────────────────────────────┐
│  new_operation_buffer (Doubly-Linked List)                  │
│  Protected by pthread_spinlock_t buffer_lock                │
│                                                             │
│  Client Insertion Point (HEAD)     Chase Consumer (TAIL)    │
│          │                                      │           │
│          ▼                                      ▼           │
│       ┌──────┐    ┌──────┐    ┌──────┐    ┌──────┐          │
│  HEAD │DLLn-3│◄──►│DLLn-2│◄──►│DLLn-1│◄──►│DLLn-0│ TAIL     │
│       └──────┘    └──────┘    └──────┘    └──────┘          │
│       newest      ...        ...        oldest              │
│       (insert)                           (consume)          │
│                                                             │
│  Direction: Clients write at HEAD (newest)                  │
│             Chase worker reads from TAIL (oldest)           │
│             Two-pointer pattern prevents contention         │
│                                                             │
│  Locking: Quick spinlock hold for link manipulation         │
│           Worker can batch-consume without blocking writes  │
└─────────────────────────────────────────────────────────────┘
```

### Operation Buffering Strategy

**When a client operation arrives during resizing (Phase 3):**

#### Upsert
1. Check if key exists in snapshot → found in snapshot table
2. Append to `updated_operation_buffer` (pending update)
   - To be applied to new table after migration completes
3. Return immediately (non-blocking)

#### Delete
1. Soft-delete the node in the snapshot (mark as deleted)
2. Append key to `delete_operation_buffer` (for final cleanup)
3. Return immediately (non-blocking)

#### Get
1. Search snapshot for key → returns current value if present
2. **Also** search `new_operation_buffer` (in-flight updates)
3. **Also** search `updated_operation_buffer` (pending updates)
4. Return most recent value found (consistent read)

### Three-Buffer Architecture

| Buffer | Role | When Appended | When Consumed |
|---|---|---|---|
| `new_operation_buffer` | Insertions & updates to new keys during resizing | Upsert of new key during Phase 3 | Chase worker during Phase 3; finalization during Phase 4 |
| `updated_operation_buffer` | Updates to keys that existed in snapshot | Upsert of existing key during Phase 3 | Finalization step in Phase 4 (applied after migration) |
| `delete_operation_buffer` | Deletions of snapshot nodes | Delete during Phase 3 | Finalization step in Phase 4 (cleanup after migration) |

---

## 5. Client Behavior During Resizing

### Scenario: Client Thread Arrives During Phase 3

**Timeline:**
```
T0: Resize worker in Phase 3 (migration underway)
    └─ is_resizing = true
    └─ snapshot_sub_hash_table_ptr points to old table
    └─ sub_hash_table_ptr points to new table (being populated)
    └─ Chase worker consuming buffered operations

T1: Client thread calls upsert_node_to_hash_bucket()
    └─ Checks: is_resizing == true
    └─ Routes to resizing-aware code path
    └─ Attempts to acquire resizing_lock (may block briefly)

T2: Client acquires lock (if Phase 4 started, lock is held by worker)
    └─ If Phase 3 still running:
       └─ Immediately release lock
       └─ Call upsert_node_to_hash_bucket_during_resizing()
    
    └─ If Phase 4 already started (resizing complete):
       └─ Resize worker is in finalization
       └─ Return: operation deferred until resizing_lock released

T3: Operation recorded (buffered or applied)
    └─ Returns non-blocking result to caller
```

### Visible Semantics

**Read Consistency:**
- `get()` during resize searches both snapshot and buffers
- Sees buffered updates even though they're not yet in the new table
- **Provides read-your-own-writes semantics**

**Write Ordering:**
- Multiple `upsert()` calls to the same key are **not** serialized during Phase 3
- Last write wins during Phase 4 finalization (when buffers are replayed)
- If atomicity of multiple keys is required, caller must coordinate via higher-level locking

**Delete Visibility:**
- `delete()` soft-deletes in snapshot and buffers the key
- Subsequent `get()` of deleted key returns NOT_FOUND
- Actual memory cleanup deferred to Phase 4

---

## 6. Return Codes

The resize mechanism uses specific return codes to signal state:

| Code | Name | When Returned | Meaning |
|---|---|---|---|
| `0` | `SUCCESS` | Normal operation | Operation completed without resizing needed |
| `10` | `SUCCESS_ADDED_NEW_NODE` | Upsert succeeded | New node inserted; no pressure detected |
| `20` | `SUCCESS_ADDED_NEW_NODE_RESIZING_TRIGGERED` | Upsert succeeded + resize triggered | New node added; resize worker spawned |
| `21` | Resizing in progress | `check_resize_status()` during Phase 3 | Caller should wait or retry |

**Key Pattern:**  
- Return code `20` is the signal that initializes Phase 2
- Caller (`upsert_node_to_hash_bucket`) detects `20`, acquires lock, and spawns resize worker
- After spawning, caller typically propagates `20` to its caller (client library level) or returns `SUCCESS` internally

---

## 7. Memory & Resource Management

### Allocations During Resizing

| Resource | Allocated In | Freed In | Count |
|---|---|---|---|
| Snapshot table | Phase 2 | Phase 4 | 1 (pointer copy of old table's structure) |
| New table | Phase 3 (migrate worker) | Stored in `sub_hash_table_ptr` | 1 (actual new structure) |
| Resizing buffers | Phase 2 | Phase 4 | 3 (new_op, update_op, delete_op) |
| Chase worker context | Phase 3 | Phase 3 (joined) | 1 |
| Nodes in new table | Phase 3 (rehashed) | Phase 4 onward | n (copied from snapshot) |

### Cleanup Guarantee

If hash bucket destruction (`cleanup_hash_bucket()`) is called during resizing:
```c
while (hash_bucket_ptr->is_resizing) 
{
    check_resize_status(hash_bucket_ptr);
    portable_sleep_ms(10);  // Polling loop
}
// Once is_resizing == false, cleanup proceeds safely
cleanup_sub_hash_table(hash_bucket_ptr->sub_hash_table_ptr);
cleanup_sub_hash_table(hash_bucket_ptr->snapshot_sub_hash_table_ptr);
// etc.
```

This ensures **no resource leaks** even if destruction is initiated mid-resize.

---

## 8. Known Limitations & Future Improvements

### Current Limitations

1. **Single resize at a time per bucket**
   - Only one resizing operation can be active per hash bucket
   - If pressure remains after finalization, a new resize must be triggered
   - This is acceptable for most workloads; aggressive growth factors (2×) make cascading resizes rare

2. **Soft-delete is not transparent**
   - `get()` returns soft-deleted nodes to callers (marked with soft-delete flag)
   - Caller must check `is_deleted` field
   - Automatic skipping would simplify API but hurt performance

3. **No partial buffering**
   - All buffered operations are replayed atomically during finalization
   - If finalization fails (OOM), consistency is at risk
   - Mitigated by: pre-allocation of new table during Phase 3

4. **Chase worker blocking on empty buffer**
   - Chase worker may spin briefly if buffer is empty
   - Could be optimized with condition variables (currently uses spinlock only)

### Planned Improvements

- [ ] Adaptive growth factor based on resize frequency
- [ ] Threshold tuning API for different workload profiles
- [ ] Metrics collection: resize latency, buffer sizes, soft-delete counts
- [ ] Optional lazy finalization (defer buffer replay if pressure low)
- [ ] Multi-bucket resizing coordinated at hash-table level
