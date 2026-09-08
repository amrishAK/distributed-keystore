# Data Structures In Depth

## Table of Contents
- [1. Overview](#1-overview)
- [2. Struct Hierarchy](#2-struct-hierarchy)
- [3. Data Node Memory Layout](#3-data-node-memory-layout)
- [4. Bloom Filter Structure (Optional)](#4-bloom-filter-structure-optional)
- [5. Resize-Support Structures](#5-resize-support-structures)
- [6. Configuration Structures (Immutable Config State)](#6-configuration-structures-immutable-config-state)
- [7. Memory Pool Structure](#7-memory-pool-structure)
- [8. Internal Operation Counters](#8-internal-operation-counters)
- [9. Navigation](#9-navigation)

## 1. Overview

This section documents the complete memory layout of the keystore's runtime state: how the two-level hash table, buckets, sub-buckets, and data nodes relate to one another, their field semantics, and the internal structures that support resizing and concurrency.

**Prerequisites:**
- [4. Two-Level Hash Table Architecture](04-two-level-hash-table.md) — routing and bucket organization
- [6. Hashing Strategy](06-hashing-strategy.md) — composite key hash generation
- [7. Concurrency Model](07-concurrency-model.md) — lock semantics and ordering

**Read next after this section:**
- [8. Dynamic Resizing Architecture](08-dynamic-resizing.md) — how `snapshot_sub_hash_table_ptr` and `resizing_buffer_ptr` are used
- [9. Memory Management](09-memory-management.md) — ownership, allocation patterns, and cleanup

---

## 2. Struct Hierarchy

This diagram shows the complete runtime ownership chain: each parent structure contains pointers to its children. Indentation represents *depth* in the tree; vertical pipes indicate *ownership relationships*.

**Key concept:** The hierarchy is stable for normal operations. During resize, temporary structures (`snapshot_sub_hash_table_ptr`, `resizing_buffer_ptr`) are attached to hold migration state.

```text
hash_table_memory_pool
│   ├── hash_buckets_ptr[]                  (array of hash_bucket)
│   ├── total_blocks                        (bucket count, immutable)
│   └── sub_hash_table_config               (inherited config)
│
└── hash_bucket [one per top-level slot; index = bucket_hash % total_blocks]
    ├── sub_hash_table_ptr ─────────────┐
    ├── snapshot_sub_hash_table_ptr     │ (non-NULL only during resize)
    ├── resizing_buffer_ptr             │ (non-NULL only during resize)
    ├── resizing_lock (mutex)           │ (guards resize state transitions)
    ├── node_count                      │
    ├── is_resizing (bool)              │
    └── sub_hash_table_config           │
                                        │
    sub_hash_table_memory_pool  ◄───────┘ (resizable, power-of-two growth)
    ├── sub_hash_buckets_ptr[]          (array of sub_hash_bucket)
    ├── total_blocks                    (sub-bucket count, always power of 2)
    ├── max_linked_list_chain_length    (resize trigger threshold)
    │
    └── sub_hash_bucket [one per sub-level slot; index = sub_bucket_hash & (total_blocks - 1)]
        ├── linked_list_head ──────────────────┐
        ├── active_node_count                  │ (atomic; soft-deleted nodes not counted)
        ├── total_node_count                   │ (atomic; active + soft-deleted)
        ├── max_linked_list_chain_length       │
        ├── is_bloom_filter_enabled (bool)     │
        ├── bloom_filter_ptr                   │ (non-NULL if enabled)
        ├── sub_hash_bucket_lock (rwlock)      │ (protects list structure)
                                               │
        linked_list_node  ◄────────────────────┘ (collision chain for this sub-bucket)
        ├── key_hash                            (cached composite_key_hash)
        ├── data_node_ptr ─────────────────────┐
        └── next_node_ptr → (next LLN or NULL) │
                                               │
        data_node  ◄───────────────────────────┘ (heap-allocated; immutable after creation)
        ├── key_hash            (composite_key_hash; redundant but quick access)
        ├── data                (unsigned char*, heap-allocated value blob)
        ├── data_size           (size_t, value byte count)
        ├── is_deleted          (bool, soft-delete flag; cleared by background cleanup)
        ├── is_concurrency_enabled (bool, inherited from config)
        ├── lock                (pthread_mutex_t, per-node; used if concurrency enabled)
        └── key[]               (flexible array member; null-terminated C string)
```

## 3. Data Node Memory Layout

Each `data_node` is a single heap allocation that contains the key as a flexible array member (FAM) at the end. The value data is stored in a separate allocation pointed to by `data`.

### Layout and Ownership

```text
┌───────────────────────────────────────────────────────────────────────────┐
│                           data_node (heap allocation)                     │
├──────────────┬──────────────┬───────────┬───────────┬──────────────┬──────┤
│  key_hash    │  data ──────→│ [value]   │ data_size │ is_deleted   │ lock │
│  (16 bytes)  │  (ptr)       │  (heap)   │ (size_t)  │  (bool)      │      │
├──────────────┴──────────────┴───────────┴───────────┴──────────────┴──────┤
│  is_concurrency_enabled (bool)  │  Key-frame for concurrency decisions    │
├───────────────────────────────────────────────────────────────────────────┤
│  key[] ← flexible array member (null-terminated C string)                 │
│  "user_12345\0"                                  ← embedded in same alloc │
└───────────────────────────────────────────────────────────────────────────┘
```

### Field Semantics

| Field | Size | Purpose | Mutability | Thread-Safe |
|-------|------|---------|-----------|-------------|
| `key_hash` | 16 bytes | Cached composite hash; avoids re-hashing on access | Immutable after creation | Yes (immutable) |
| `data` | sizeof(void*) | Points to separate heap allocation for value bytes | Mutable (under lock) | If `lock` held |
| `data_size` | size_t | Byte count of `data` allocation | Immutable after creation | Yes (immutable) |
| `is_deleted` | bool | Soft-delete flag; set to true on delete, physically freed later by background worker | Mutable (under lock) | If `lock` held |
| `is_concurrency_enabled` | bool | Copy of global config; gates lock acquisition in operations | Immutable after creation | Yes (immutable) |
| `lock` | pthread_mutex_t | Per-node mutex; serializes concurrent access to `data` and `is_deleted` | N/A (primitive) | If acquired |
| `key[]` | variable | Null-terminated key string; embedded using flexible array member (FAM) trick | Immutable after creation | Yes (immutable) |

### Memory Layout Rationale

**Why separate `data` allocation?**
- Allows arbitrary value sizes without resizing the `data_node` structure itself
- Keys (typically short strings) stay inline for cache locality; large values live separately
- See [Memory Management](09-memory-management.md) for allocation strategy and ownership

**Why flexible array member for key?**
- Avoids double pointer dereference during key lookup and comparison
- `data_node` and its key are a single contiguous allocation, reducing fragmentation
- Fast string comparison with no additional pointer chase

**Why cache `key_hash`?**
- Quick equality checks during collision chain traversal (compare hash first before string compare)
- Avoids re-hashing the key on every operation

---

## 4. Bloom Filter Structure (Optional)

When `is_bloom_filter_enabled` is true, each `sub_hash_bucket` may have a `bloom_filter_ptr` pointing to an in-place Bloom filter structure. This is used for **negative-path optimization**: quickly determine if a key is *definitely not* in the sub-bucket before traversing the collision chain.

```text
sub_hash_bucket.bloom_filter_ptr
│
└── bloom_filter
    ├── bit_array_ptr (unsigned char*)
    ├── bit_array_size (size_t, bytes)
    ├── hash_function_count (unsigned int, number of independent hash functions)
    ├── filter_lock (pthread_rwlock_t)
    └── false_positive_rate (double, for diagnostics)
```

**When is it used?**
- **On GET:** Before traversing the collision chain, check the Bloom filter. If `bloom_filter_lookup(key) == NOT_PRESENT`, return KEY_NOT_FOUND immediately without walking the list.
- **On INSERT/DELETE:** After modifying the collision chain, update the filter with the new key (INSERT) or mark appropriately (DELETE, depending on cleanup policy).

**Why optional?**
- Adds memory overhead (~5–10% per sub-bucket) for marginal gain on cache-miss-heavy workloads
- Disabled by default in configuration; enable for read-heavy workloads with many negative lookups

See [docs/architecture/06-hashing-strategy.md](06-hashing-strategy.md) for Bloom filter hash function details.

---

## 5. Resize-Support Structures

During a bucket resize (triggered by [8. Dynamic Resizing Architecture](08-dynamic-resizing.md)), temporary structures are allocated and attached to the `hash_bucket`. These exist *only while a resize is in progress* and are cleaned up upon finalization.

### Resizing Buffer Hierarchy

```text
hash_bucket.resizing_buffer_ptr
│
└── resizing_buffer (allocated at start of Phase 2)
    │
    ├── new_sub_hash_table_ptr ────────┐
    │                                  ├─→ Destination table built in Phase 2
    ├── delete_operation_buffer_ptr    │
    │   └── dynamic array of           │   Deferred deletes from snapshot
    │       delete_op_t                │   phase, replayed after migration
    │                                  │
    ├── updated_operation_buffer_head  ┤   (Linked-list head)
    │   └── linked list of             │   Updates racing against snapshot
    │       updated_node_buffer_t      │   nodes during Phase 2–3
    │                                  │
    └── new_operation_buffer_ptr ──────┤
        │                              │   Chase buffer: real-time
        ├── head_ptr                   │   capture of new writes
        ├── tail_ptr                   │   (enqueue/dequeue pointers)
        ├── current_consumer_ptr ──────┘
        │
        ├── buffer_lock (pthread_spinlock_t) ─ Protects head/tail/consumer
        ├── pause_chasing (bool)             ─ Pause signal for Phase 3
        └── is_running (bool)                ─ Consumer thread state
```

### What Each Field Does

| Field | Purpose | Lifetime |
|-------|---------|----------|
| `new_sub_hash_table_ptr` | Destination sub-table being built incrementally during Phase 2. Becomes `sub_hash_table_ptr` after finalization. | Phase 2–Phase 3 finalization |
| `delete_operation_buffer_ptr` | Dynamic array recording all deletions that occur against snapshot-backed nodes. Replayed against final table after migration. | Phase 2–Phase 3 |
| `updated_operation_buffer_head` | Linked list of `updated_node_buffer_t` records. Captures updates (writes) that race with migration against snapshot nodes. Replayed into new table. | Phase 2–Phase 3 |
| `new_operation_buffer_ptr` | Circular buffer of new write operations (INSERT, UPDATE, DELETE) arriving *after* snapshot was taken. Consumer thread drains this queue in real-time to keep pace with ongoing clients. | Phase 2–Phase 4 |
| `buffer_lock` | Spinlock protecting `head_ptr`, `tail_ptr`, `current_consumer_ptr` in `new_operation_buffer_ptr`. Used for lock-free enqueue/dequeue. | Phase 2–Phase 4 |
| `pause_chasing` | Boolean flag; when set, signals the background consumer thread to pause draining `new_operation_buffer_ptr`. Used to coordinate Phase 3 finalization. | Phase 3 |
| `is_running` | Background consumer thread state; set to false when thread should exit. | Phase 2–Phase 4 |

**Why these structures?**
- Snapshot preserves the table state as it was at resize start
- Delete and update buffers capture operations that race with migration
- Chase buffer decouples producers (client threads) from the consumer (background worker), preventing clients from blocking on resize
- See [Dynamic Resizing Architecture](08-dynamic-resizing.md) Phase 2–4 for choreography details

---

## 6. Configuration Structures (Immutable Config State)

Configuration flows down through the struct hierarchy at creation time and is immutable thereafter. This ensures all subsystems operate with a consistent view of tuning parameters.

```text
sub_hash_table_config (held in both hash_bucket and sub_hash_table_memory_pool)
├── max_linked_list_chain_length (unsigned int)
│   └── Threshold for resize trigger in any sub-bucket
│
├── initial_sub_hash_table_size (unsigned int)
│   └── Starting size for sub-tables (power of 2, typically 4–32)
│
├── memory_pool_size (size_t)
│   └── Pre-allocated arena size for linked_list_node allocations
│
├── bloom_filter_enabled (bool)
│   └── Enable Bloom filters in all sub-buckets for negative-lookup optimization
│
└── is_concurrency_enabled (bool)
    └── Gate all lock acquisitions; if false, operate lock-free (single-threaded mode)
```

**Why immutable after creation?**
- No need to synchronize config reads with config writes
- Each thread can safely cache config values locally
- Resizing decisions are deterministic across the hierarchy

See [src/keystore/type_definitions/config_type_definitions.h](../../src/keystore/type_definitions/config_type_definitions.h) for the struct definition.

---

## 7. Memory Pool Structure

The keystore maintains a fast allocation arena for `linked_list_node` structures. This reduces malloc overhead for frequently allocated short-lived collision chain nodes.

```text
memory_pool (per sub-hash-table)
├── pool_ptr (void*, pre-allocated arena)
├── pool_size (size_t, total bytes allocated)
├── block_size (size_t, sizeof(linked_list_node))
├── available_blocks (atomic counter)
├── total_blocks (size_t)
├── next_free_block_ptr (void*, stack of free blocks)
└── pool_lock (pthread_spinlock_t, protects next_free_block_ptr)
```

**Allocation strategy:**
1. On `linked_list_node` allocation request: try pool first
2. If pool exhausted: fall back to `malloc()` (with fallback counter increment)
3. On deallocation: return to pool if it came from the pool; free otherwise

**Why a pool?**
- `linked_list_node` is small (few cache lines) and frequently allocated/freed during collisions
- Reduces malloc fragmentation and allocation latency
- Lock contention is minimal (spinlock, short hold time)

See [Memory Management](09-memory-management.md) for detailed ownership semantics and cleanup policy.

The pool mutex is initialized only for concurrent configurations. Single-threaded
configurations use the same pool metadata without lock overhead.

---

The `active_node_count` and `total_node_count` fields remain live C11 atomic
metadata because insert, delete, and resize paths use them for concurrent
bookkeeping.

---

## 9. Navigation

**Backward dependencies** (read before this section):
- [1. High-Level Overview](01-overview.md) — system organization and responsibilities
- [4. Two-Level Hash Table Architecture](04-two-level-hash-table.md) — routing mechanics and bucket assignment
- [6. Hashing Strategy](06-hashing-strategy.md) — composite key hash generation and Bloom filter algorithms

**Forward dependencies** (read this before those sections):
- [8. Dynamic Resizing Architecture](08-dynamic-resizing.md) — uses `snapshot_sub_hash_table_ptr`, `resizing_buffer_ptr`, chase buffer semantics
- [7. Concurrency Model](07-concurrency-model.md) — relies on lock field placement and per-node semantics
- [9. Memory Management](09-memory-management.md) — allocation and deallocation policies; relationship between `data`, `key[]`, and pool structures
- [10. Background Task Manager](10-background-task-manager.md) — uses `is_deleted` flag and cleanup policy

**Source code references:**
- [src/keystore/type_definitions/](../../src/keystore/type_definitions/) — struct definitions
- [src/keystore/data_structures/](../../src/keystore/data_structures/) — linked list and data node operations
- [src/keystore/sub_hash_table/](../../src/keystore/sub_hash_table/) — sub-bucket and sub-table operations
- [src/keystore/hash_table/](../../src/keystore/hash_table/) — top-level hash table operations
