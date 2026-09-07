# Memory Management

## Table of Contents
- [1. Overview](#1-overview)
- [2. Memory Pool Lifecycle](#2-memory-pool-lifecycle)
- [3. Allocation Strategy](#3-allocation-strategy)
- [4. Deallocation & Reuse](#4-deallocation--reuse)
- [5. Thread Safety](#5-thread-safety)
- [6. Allocation Inventory](#6-allocation-inventory)
- [7. Memory Safety Guarantees](#7-memory-safety-guarantees)
- [8. Configuration Best Practices](#8-configuration-best-practices)
- [9. Known Limitations & Future Improvements](#9-known-limitations--future-improvements)

## 1. Overview

The KeyStore uses a **hybrid allocation strategy**: a pre-allocated memory pool for collision-chain nodes (`linked_list_node`), plus standard heap allocation for all other structures. The pool reduces allocator overhead on the hot path, while fallback to `malloc` guarantees correctness when the pool is exhausted.

> **Note:** Only `linked_list_node` allocations benefit from pooling. All other allocations use direct heap calls (`malloc`, `calloc`, `realloc`).
> **Related:** See [Data Structures In Depth](05-data-structures.md), §2 for memory pool structure and layout. See §3 for `data_node` allocation strategy (key FAM vs. separate value allocation).

---

## 2. Memory Pool Lifecycle

### Initialization

The pool is created during `initialize_memory_manager()` based on configuration:

- **Pool size:** `ceil(bucket_size * pre_allocation_factor)` nodes
- **Block size:** `sizeof(linked_list_node)`
- **Thread-safety:** Optional `pthread_mutex_t` if concurrency is enabled

Example:
```c
memory_manager_config cfg = {
    .bucket_size = 256,
    .pre_allocation_factor = 0.75,  // Pre-allocate 192 nodes
    .allocate_list_pool = true,
    .is_concurrency_enabled = true
};
initialize_memory_manager(cfg);  // Creates pool with 192 nodes
```

### Pool Structure

```text
Pool State Machine:
┌─────────────────────────────────────────────────────────────┐
│ Memory Pool (g_list_pool)                                   │
│                                                             │
│  PRE-ALLOCATED REGION          REUSABLE BLOCK STACK         │
│  ┌──────────────────────┐       ┌──────────────────────┐    │
│  │ Block 0 │ Block 1 │..│       │ free_block_list[]    │    │
│  │ UNUSED  │ UNUSED  │  │       │ ┌─────┬─────┬─────┐  │    │
│  └──────────────────────┘       │ │ptr1 │ptr2 │ NULL│  │    │
│  ▲                              │ └─────┴─────┴─────┘  │    │
│  └─next_block_ptr               │ reusable_blocks: 2   │    │
│  (Sequential allocation)        └──────────────────────┘    │
│                                  (LIFO stack for reuse)     │
│  available_blocks: 190                                      │
│  total_blocks: 192                                          │
│  pool_lock: pthread_mutex_t (optional)                      │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. Allocation Strategy

### Step 1: Check Reusable Blocks

When a request arrives, the allocator first checks the **free_block_list** (LIFO stack) for previously returned blocks:

```text
    allocate_memory_from_pool()
           │
           ▼
    reusable_blocks > 0?
      / \
    YES NO
    │   │
    │   └─► Step 2 (below)
    │
    └─► Return free_block_list[--reusable_blocks]
        (Most recently freed block)
```

**Benefit:** Reused blocks remain hot in CPU cache.

### Step 2: Allocate from Pre-allocated Region

If no reused blocks are available, allocate sequentially from the pre-allocated pool:

```text
    available_blocks > 0?
      / \
    YES NO
    │   │
    │   └─► Step 3 (below)
    │
    └─► mem = next_block_ptr
        next_block_ptr += block_size
        available_blocks--
        Return mem
```

**Benefit:** No allocator involved; O(1) pointer arithmetic.

### Step 3: Fallback to Heap

If the pool is exhausted, fall back to standard `malloc`:

```text
    Fallback (Pool Exhausted)
           │
           ▼
    malloc(block_size)
           │
           ├─► On success: return ptr
           │
           └─► On failure: return NULL
                (Caller handles error)
```

**When this happens:** Pre-allocation factor was too small, or workload exceeded estimate. The KeyStore remains functional but uses heap allocation.

### Complete Allocation Flowchart

```text
allocate_memory_from_pool()
│
├─ Lock pool_lock (if concurrent)
│
├─ Is reusable_blocks > 0?
│  ├─ YES ─► Return free_block_list[--reusable_blocks]
│  │
│  └─ NO ──► Is available_blocks > 0?
│           ├─ YES ─► Return *next_block_ptr; advance pointer; available_blocks--
│           │
│           └─ NO ──► Return malloc(block_size)  [exhausted → heap]
│
└─ Unlock pool_lock (if concurrent)
```

---

## 4. Deallocation & Reuse

### The Free Process

When `free_memory(ptr, is_pool=true)` is called:

```text
free_memory(ptr, is_pool=true)
│
├─ Lock pool_lock (if concurrent)
│
├─ Is this pointer from the pool?
│  ├─ YES ──► Is reusable_blocks < total_blocks?
│  │          ├─ YES ─► free_block_list[reusable_blocks++] = ptr
│  │          │         Return (block added to LIFO stack)
│  │          │
│  │          └─ NO ──► free(ptr)  [stack full → fallback]
│  │
│  └─ NO ───► free(ptr)  [external block → heap]
│
└─ Unlock pool_lock (if concurrent)
```

**Pool ownership check:** The allocator verifies:
- `pool_start_ptr <= ptr < pool_end_ptr` (pointer in pool range)
- `(ptr - pool_start_ptr) % block_size == 0` (aligned to block boundary)

---

## 5. Thread Safety

When `is_concurrency_enabled = true`:

| Operation | Protection |
|-----------|-----------|
| All pool operations (allocate, free) | `pool_lock` mutex |
| Configuration read | Lock-free (immutable after init) |
| Pool metadata updates | Protected by `pool_lock` |
| Allocator metrics updates | C11 atomic counters |

**Lock scope:** Minimal — only protects pool state, not node content.

Allocator metric updates use relaxed C11 atomic operations and do not acquire
`pool_lock`. `get_memory_allocator_metrics()` returns a race-free snapshot of
the individual counters; counters may reflect slightly different instants when
allocations continue concurrently. Reset has the same per-counter semantics.

---

## 6. Allocation Inventory

### Hot Path (Pooled)

| Object | Source | Freed By | Count |
|--------|--------|----------|-------|
| `linked_list_node` | Pool or `malloc` | `free_memory(ptr, is_pool=true)` | ~`bucket_size * pre_allocation_factor` |

### Cold Path (Heap)

| Object | Source | Freed By | Lifetime |
|--------|--------|----------|----------|
| `data_node` | `malloc` (with FAM for key) | `delete_data_node()` | Per key-value pair |
| `data_node.data` | `malloc` / `realloc` | Node cleanup or replacement | Per value update |
| `double_linked_list_node` | `malloc` | DLL cleanup routine | Per stats collection |
| `bloom_filter_t` | `calloc` | `cleanup_bloom_filter()` | One per sub-hash-table |
| Bloom bit array | Heap (part of filter) | Filter cleanup | One per sub-hash-table |
| `sub_hash_bucket[]` | `calloc` | `cleanup_sub_hash_table()` | One per sub-hash-table |
| `hash_bucket[]` | `calloc` | `cleanup_hash_table()` | One per hash table |
| `resizing_buffer` | `calloc` | `delete_resizing_buffer()` | Temporary (resizing) |
| `delete_operation_buffer` | `malloc` + `realloc` | `delete_resizing_buffer()` | Temporary (batch deletes) |
| **Read path** (key/value) | Heap | **Caller** | Query response |

> **Ownership:** Caller owns returned key/value pointers from `get_key()`. KeyStore does not track or free them.

---

## 7. Memory Safety Guarantees

### Checked at Runtime

| Check | When | Action on Fail |
|-------|------|---|
| Pool pointer alignment | Free time | Fall back to heap `free()` |
| Pool pointer in range | Free time | Fall back to heap `free()` |
| Pre-allocation factor valid | Init time | `ERR_INVALID_CONFIG` |
| Configuration idempotency | Init time | Silently re-entry OK |

### Design Constraints

- **No manual `free()` on pooled nodes.** Always use `free_memory(ptr, true)`.
- **No pointer reuse after free.** Pooled block may be reallocated immediately.
- **Thread-unsafe after cleanup.** All pool operations invalid after `cleanup_memory_manager()`.
- **Single pool instance.** Global `g_list_pool` — no multiple pools per process.

---

## 8. Configuration Best Practices

### Sizing the Pre-allocation Factor

```c
// Conservative (safe, smaller pool):
pre_allocation_factor = 0.5;  // 50% of bucket_size

// Balanced (typical):
pre_allocation_factor = 0.75;  // 75% of bucket_size

// Aggressive (large workloads, more collisions):
pre_allocation_factor = 1.0;  // 100% of bucket_size
```

| Factor | Max Collisions Supported | Excess Uses Heap | Use Case |
|--------|-------------------------|-----------------|----------|
| 0.5 | ~50% of buckets | Yes (acceptable) | Light, predictable loads |
| 0.75 | ~75% of buckets | Rare | Typical production |
| 1.0 | ~100% of buckets | Very rare | High-collision workloads |

> **Note:** Pre-allocation happens once at startup. Underestimation is not an error—it just triggers heap allocation fallback on excess collisions.

### Concurrency Considerations

**Enable concurrency if:**
- Multiple threads call `set_key()`, `get_key()`, or `delete_key()` concurrently
- Pool exhaustion is possible (threads competing for reusable blocks)

**Disable if:**
- Single-threaded workload
- Micro-optimization needed (small performance gain from no mutex)

---

## 9. Known Limitations & Future Improvements

| Issue | Impact | Mitigation |
|-------|--------|-----------|
| Single pool instance (global) | Cannot partition pools per NUMA node | N/A (architectural) |
| LIFO reuse only | No preference for hot blocks | Acceptable for small pools |
| No reallocation strategy | Exhausted pool → permanent heap | Pre-size conservatively |
| Mutex contention on high concurrency | Possible lock serialization | Future: lock-free stack |
