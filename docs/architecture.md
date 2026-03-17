# Distributed KeyStore — Architecture Guide

> **Version:** v1.0 (Chase) | **Tag:** Dynamic resizing with chase buffer &nbsp;|&nbsp; **Language:** C11 &nbsp;|&nbsp; **Last Updated:** 2026-03-17

---

## Table of Contents

1. [High-Level Overview](#1-high-level-overview)
2. [Directory Structure](#2-directory-structure)
3. [Data Flow — How a Request Travels](#3-data-flow--how-a-request-travels)
4. [Two-Level Hash Table Architecture](#4-two-level-hash-table-architecture)
5. [Data Structures In Depth](#5-data-structures-in-depth)
6. [Hashing Strategy](#6-hashing-strategy)
7. [Concurrency Model](#7-concurrency-model)
8. [Dynamic Resizing Architecture](#8-dynamic-resizing-architecture)
9. [Memory Management](#9-memory-management)
10. [Background Task Manager](#10-background-task-manager)
11. [Error Handling Model](#11-error-handling-model)
12. [Module Dependency Graph](#12-module-dependency-graph)
13. [Public API Surface](#13-public-api-surface)
14. [Design Decisions & Trade-offs](#14-design-decisions--trade-offs)

---

## 1. High-Level Overview

The Distributed KeyStore is a **concurrent, in-memory key-value store** written in C11. It is designed for high-throughput CRUD operations under heavy thread contention. The core architecture is a **two-level hash table** with fine-grained locking at every level.

```
┌─────────────────────────────────────────────────────────────────┐
│                        CLIENT THREADS                           │
│                  set_key / get_key / delete_key                 │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                     PUBLIC API (key_store.c)                    │
│         Validates input → MurmurHash3 → Routes to bucket        │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                 HASH TABLE (hash_table_operation.c)             │
│          bucket_index = key_hash % total_buckets                │
│   ┌──────────┬──────────┬──────────┬───────┬──────────┐         │
│   │ Bucket 0 │ Bucket 1 │ Bucket 2 │  ...  │ Bucket N │         │
│   └────┬─────┴────┬─────┴────┬─────┴───────┴────┬─────┘         │
└────────┼──────────┼──────────┼───────────────────┼──────────────┘
         │          │          │                   │
         ▼          ▼          ▼                   ▼
┌─────────────────────────────────────────────────────────────────┐
│         HASH BUCKET (hash_bucket_operation.c)                   │
│   Each bucket owns a sub-hash-table + manages resizing          │
│   Lock: pthread_mutex_t resizing_lock                           │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│           SUB-HASH-TABLE (sub_hash_table_operation.c)           │
│          sub_index = key_hash & (sub_buckets - 1)               │
│   ┌────────────┬────────────┬────────────┬────────────┐         │
│   │ Sub-Bkt 0  │ Sub-Bkt 1  │ Sub-Bkt 2  │ Sub-Bkt M  │         │
│   └─────┬──────┴─────┬──────┴─────┬──────┴─────┬──────┘         │
└─────────┼────────────┼────────────┼────────────┼────────────────┘
          │            │            │            │
          ▼            ▼            ▼            ▼
┌─────────────────────────────────────────────────────────────────┐
│         SUB-HASH-BUCKET (sub_hash_bucket_operation.c)           │
│   Singly-linked list of data nodes (collision chain)            │
│   Lock: pthread_rwlock_t sub_hash_bucket_lock                   │
│                                                                 │
│     ┌────────┐    ┌────────┐    ┌────────┐                      │
│     │ LLNode ├───►│ LLNode ├───►│ LLNode ├───► NULL             │
│     │(hash,  │    │(hash,  │    │(hash,  │                      │
│     │ data*) │    │ data*) │    │ data*) │                      │
│     └───┬────┘    └───┬────┘    └───┬────┘                      │
│         │             │             │                           │
│         ▼             ▼             ▼                           │
│     ┌────────┐    ┌────────┐    ┌────────┐                      │
│     │DataNode│    │DataNode│    │DataNode│                      │
│     │key,val │    │key,val │    │key,val │                      │
│     │ mutex  │    │ mutex  │    │ mutex  │                      │
│     └────────┘    └────────┘    └────────┘                      │
└─────────────────────────────────────────────────────────────────┘
```

### Design Principles

| Principle | Implementation |
|-----------|---------------|
| **Fine-grained locking** | Three lock levels: bucket mutex, sub-bucket rwlock, data-node mutex |
| **Low-overhead resize buffering** | `pthread_spinlock_t` protects the chase buffer during resize |
| **Minimal allocation** | Pre-allocated memory pool for linked list nodes |
| **Soft-delete semantics** | Deletes mark `is_deleted = true`; physical cleanup during resize |
| **Minimal runtime dependencies** | Core runtime uses standard C + pthreads; no third-party runtime library |

---

## 2. Directory Structure

```
src/keystore/
│
├── core/                          ◄── Public API layer
│   ├── key_store.h                    Function declarations
│   └── key_store.c                    init, cleanup, set, get, delete
│
├── hash/                          ◄── Hashing engine
│   ├── hash_functions.h
│   └── hash_functions.c               MurmurHash3 32-bit
│
├── hash_table/                    ◄── Level-1 hash table
│   ├── hash_table_operation.h/c       Table-wide routing
│   ├── hash_bucket_operation.h/c      Per-bucket logic + resize gate
│   └── resizing/                  ◄── Dynamic resize subsystem
│       ├── resize_operation.h/c       Background resize worker
│       ├── hash_bucket_resizing_operation.h/c  Resize coordinator
│       └── buffer_operation.h/c       Chase buffer (spinlock DLL)
│
├── sub_hash_table/                ◄── Level-2 sub-hash-table
│   ├── sub_hash_table_operation.h/c   Sub-table routing
│   └── sub_hash_bucket_operation.h/c  Per-sub-bucket linked list ops
│
├── data_structures/               ◄── Core data structures
│   ├── data_node_operation.h/c        CRUD on key-value leaf nodes
│   ├── linked_list_operation.h/c      Singly-linked collision chains
│   └── double_linked_list_operation.h/c  Doubly-linked (resize buffer)
│
├── type_definitions/              ◄── All struct/enum/constant defs
│   ├── custom_type_definitions.h      data_node, linked_list_node, etc.
│   ├── hash_bucket_type_definition.h  hash_bucket, sub_hash_bucket, etc.
│   ├── config_type_definitions.h      Configuration structs
│   ├── error_code_definitions.h       Negative error codes
│   ├── sucess_code_definitions.h      Positive success codes
│   ├── stats_type_definitions.h       Operation counters
│   └── background_task_manager_type_definitons.h  Thread task types
│
└── utils/                         ◄── Infrastructure utilities
    ├── memory_manager.h/c             Pool allocator + malloc wrappers
    ├── background_task_manager.h/c    Thread lifecycle manager
    └── helper_functions.h/c           is_power_of_two, portable_sleep_ms
```

---

## 3. Data Flow — How a Request Travels

### 3.1 SET Operation (Upsert)

```
  Client calls set_key(&kv_pair)
         │
         ▼
  ┌──────────────────────────────┐
  │ 1. Validate inputs           │  key != NULL, value != NULL
  │ 2. Hash the key              │  MurmurHash3(key, g_hash_seed)
  │ 3. Validate hash             │  hash != UINT32_MAX
  └──────────┬───────────────────┘
             │
             ▼
  ┌──────────────────────────────┐
  │ 4. Route to hash bucket      │  index = hash % total_buckets
  │    hash_table_operation.c    │
  └──────────┬───────────────────┘
             │
             ▼
  ┌──────────────────────────────────────────────────────────┐
  │ 5. Hash bucket decision gate (hash_bucket_operation.c)   │
  │                                                          │
  │    is_resizing == false?                                 │
  │    ├── YES → Direct upsert to sub-hash-table             │
  │    │         Return code == 20 (resize triggered)?       │
  │    │         ├── YES → Acquire resizing_lock             │
  │    │         │         initialize_hash_bucket_resizing() │
  │    │         │         Release resizing_lock             │
  │    │         └── NO  → Done ✓                            │
  │    │                                                     │
  │    └── NO  → Acquire resizing_lock                       │
  │              upsert_during_resizing()                    │
  │              Release resizing_lock                       │
  └──────────┬───────────────────────────────────────────────┘
             │
             ▼
  ┌──────────────────────────────────────────────────────────┐
  │ 6. Sub-hash-table routing (sub_hash_table_operation.c)   │
  │    sub_index = hash & (sub_buckets - 1)   [bitmask]      │
  │                                                          │
  │    Try update_node_in_sub_hash_bucket(sub_bucket)        │
  │    ├── Found  → Lock data_node.mutex → edit value → Done │
  │    └── Not found → add_node_to_sub_hash_bucket()         │
  │         ├── Create data_node (malloc + FAM for key)      │
  │         ├── Create linked_list_node (from memory pool)   │
  │         ├── Acquire sub_bucket WRITE lock                │
  │         ├── Insert at head of linked list                │
  │         ├── Release sub_bucket WRITE lock                │
  │         └── Check resize condition                       │
  │              active_count >= max_chain_length?           │
  │              └── YES → Return code 20                    │
  └──────────────────────────────────────────────────────────┘
```

### 3.2 GET Operation

```
  Client calls get_key("mykey", &output)
         │
         ▼
  ┌──────────────────────────────┐
  │ 1. Validate + Hash           │
  │ 2. Route to hash bucket      │
  └──────────┬───────────────────┘
             │
             ▼
  ┌──────────────────────────────────────────────────┐
  │ 3. Hash bucket: resizing check                   │
  │    is_resizing?                                  │
  │    ├── NO  → Direct read from sub-hash-table     │
  │    └── YES → Acquire resizing_lock               │
  │              Search snapshot sub-hash-table      │
  │              Search resizing buffers (updates,   │
  │              new-ops) for most recent version    │
  │              Release resizing_lock               │
  └──────────┬───────────────────────────────────────┘
             │
             ▼
  ┌──────────────────────────────────────────────────┐
  │ 4. Sub-hash-bucket read path                     │
  │    Acquire sub_bucket READ lock                  │
  │    Walk linked list → match hash + strcmp(key)   │
  │    Acquire data_node MUTEX                       │
  │    Copy value → allocate output buffers          │
  │    Release data_node MUTEX                       │
  │    Release sub_bucket READ lock                  │
  └──────────────────────────────────────────────────┘
```

### 3.3 DELETE Operation

```
  Client calls delete_key("mykey")
         │
         ▼
  ┌──────────────────────────────────────────────────┐
  │ 1. Validate + Hash + Route to bucket             │
  │ 2. is_resizing?                                  │
  │    ├── NO  → Direct soft-delete                  │
  │    │   Acquire sub_bucket READ lock              │
  │    │   Find node → Acquire data_node MUTEX       │
  │    │   Set is_deleted = true                     │
  │    │   Decrement active_node_count               │
  │    │   Release locks                             │
  │    └── YES → Acquire resizing_lock               │
  │              Record in delete_operation_buffer   │
  │              or mark in new_operation_buffer     │
  │              Release resizing_lock               │
  └──────────────────────────────────────────────────┘
```

---

## 4. Two-Level Hash Table Architecture

The keystore uses a **two-level hashing scheme** to achieve both stable top-level routing and dynamically resizable inner tables.

```
                    ┌──────────────────────────────────────┐
                    │        HASH TABLE (Level 1)          │
                    │   Fixed-size bucket array            │
                    │   Index: hash % bucket_count         │
                    │   (modulo — supports any size)       │
                    ├──────┬──────┬──────┬──────┬──────────┤
                    │ HB-0 │ HB-1 │ HB-2 │ HB-3 │   ...    │
                    └──┬───┴──┬───┴──┬───┴──┬───┴──────────┘
                       │      │      │      │
           ┌───────────┘      │      │      └───────────┐
           ▼                  ▼      ▼                  ▼
  ┌─────────────────┐  ┌──────────────────┐   ┌─────────────────┐
  │  SUB-HASH-TABLE │  │  SUB-HASH-TABLE  │   │  SUB-HASH-TABLE │
  │   (Level 2)     │  │   (Level 2)      │   │   (Level 2)     │
  │  Resizable!     │  │  Resizable!      │   │  Resizable!     │
  │  Index: hash &  │  │  Index: hash &   │   │  Index: hash &  │
  │  (size - 1)     │  │  (size - 1)      │   │  (size - 1)     │
  ├────┬────┬────┬──┤  ├────┬────┬────┬───┤   ├────┬────┬────┬──┤
  │SB0 │SB1 │SB2 │..│  │SB0 │SB1 │SB2 │.. │   │SB0 │SB1 │SB2 │..│
  └─┬──┴─┬──┴─┬──┴──┘  └─┬──┴─┬──┴─┬──┴───┘   └─┬──┴─┬──┴─┬──┴──┘
    │    │    │            │    │    │              │    │    │
    ▼    ▼    ▼            ▼    ▼    ▼              ▼    ▼    ▼
  ┌──┐ ┌──┐ ┌──┐        ┌──┐ ┌──┐ ┌──┐          ┌──┐ ┌──┐ ┌──┐
  │LL│ │LL│ │LL│        │LL│ │LL│ │LL│          │LL│ │LL│ │LL│
  └──┘ └──┘ └──┘        └──┘ └──┘ └──┘          └──┘ └──┘ └──┘

  HB = Hash Bucket  |  SB = Sub-Hash-Bucket  |  LL = Linked List
```

### Why Two Levels?

| Concern | Single-Level Table | Two-Level Table (This Design) |
|---------|-------------------|--------------------------------|
| **Resize scope** | Must rehash ALL keys | Only the overloaded bucket's sub-table resizes |
| **Lock contention** | Global lock or stripe-lock on entire table | Lock only the affected sub-hash-bucket |
| **Resize blocking** | All operations blocked | Only operations hitting the resizing bucket are affected |
| **Memory growth** | Doubles entire table at once | Doubles one sub-table at a time (incremental) |

### Index Calculation

```
Level 1 (Hash Table):     bucket_index = key_hash % total_buckets     ← modulo
Level 2 (Sub-Hash-Table): sub_index    = key_hash & (total_sub - 1)   ← bitmask (power-of-2)
```

The Level-1 table uses **modulo** to allow arbitrary bucket counts. The Level-2 sub-tables require **power-of-2** sizes, enabling fast bitmask indexing and clean doubling on resize.

---

## 5. Data Structures In Depth

### 5.1 Struct Hierarchy

```
hash_table_memory_pool
│   ├── hash_buckets_ptr[]                  (array of hash_bucket)
│   ├── total_blocks                        (bucket count)
│   └── sub_hash_table_config               (inherited config)
│
└── hash_bucket [one per top-level slot]
    ├── sub_hash_table_ptr ─────────────┐
    ├── snapshot_sub_hash_table_ptr     │ (non-NULL only during resize)
    ├── resizing_buffer_ptr             │ (non-NULL only during resize)
    ├── resizing_lock                   │ (pthread_mutex_t)
    ├── node_count                      │
    ├── is_resizing                     │
    └── sub_hash_table_config           │
                                        │
    sub_hash_table_memory_pool  ◄───────┘
    ├── sub_hash_buckets_ptr[]          (array of sub_hash_bucket)
    ├── total_blocks                    (sub-bucket count, power-of-2)
    └── max_linked_list_chain_length
    │
    └── sub_hash_bucket [one per sub-level slot]
        ├── linked_list_head ──────────────────┐
        ├── active_node_count                  │
        ├── total_node_count                   │
        ├── sub_hash_bucket_lock (rwlock)      │
        └── max_linked_list_chain_length       │
                                               │
        linked_list_node  ◄────────────────────┘
        ├── key_hash
        ├── data_node_ptr ─────────────────────┐
        └── next_node_ptr → (next LLN or NULL) │
                                               │
        data_node  ◄───────────────────────────┘
        ├── key_hash            (uint32_t, immutable)
        ├── data                (unsigned char*, heap-allocated value)
        ├── data_size           (size_t)
        ├── is_deleted          (bool, soft-delete flag)
        ├── lock                (pthread_mutex_t, per-node)
        └── key[]               (flexible array member, null-terminated)
```

### 5.2 Data Node Layout (In Memory)

```
┌─────────────────────────────────────────────────────────────────┐
│                        data_node (heap)                         │
├──────────────┬─────────────┬───────────┬───────────┬────────────┤
│  key_hash    │  data ──────┼──► [val]  │ data_size │ is_deleted │
│  (4 bytes)   │  (ptr)      │   (heap)  │ (8 bytes) │  (1 byte)  │
├──────────────┴─────────────┴───────────┴───────────┴────────────┤
│  is_concurrency_enabled (1 byte)  │  lock (pthread_mutex_t)     │
├───────────────────────────────────┴─────────────────────────────┤
│  key[] ← flexible array member (null-terminated string)         │
│  "my_key\0"                                                     │
└─────────────────────────────────────────────────────────────────┘
```

The `key` is embedded directly in the `data_node` struct via a **C99 flexible array member**, eliminating a separate heap allocation for the key string. The `data` (value) is a separate heap allocation pointed to by `data_node.data`.

---

## 6. Hashing Strategy

The keystore uses **MurmurHash3 (32-bit)** with a random per-instance seed.

```
┌───────────────────────────────────────────────────────────┐
│                    MurmurHash3 Pipeline                   │
│                                                           │
│  Input: key (string) + seed (uint32_t from time(NULL))    │
│                                                           │
│  ┌─────────┐    ┌──────────────┐    ┌──────────────┐      │ 
│  │ Process  │    │  Process     │    │  Finalize    │     │
│  │ 4-byte   ├───►│  tail bytes  ├───►│  (avalanche) │     │
│  │ blocks   │    │  (0-3 bytes) │    │              │     │
│  └─────────┘    └──────────────┘    └──────┬───────┘      │
│                                            │              │
│  Mix constants:  0xcc9e2d51, 0x1b873593    │              │
│  Avalanche:      0x85ebca6b, 0xc2b2ae35    │              │
│                                            ▼              │
│                                     ┌────────────┐        │
│                                     │  uint32_t  │        │
│                                     │   hash     │        │
│                                     └────────────┘        │
│                                                           │
│  Special cases:                                           │
│    - NULL key     → returns UINT32_MAX (error sentinel)   │
│    - Empty string → valid hash (seed-dependent)           │
└───────────────────────────────────────────────────────────┘
```

### Two-Level Index Derivation

A single hash is computed once and used at both levels:

```
  key = "sensor_42"
  hash = MurmurHash3("sensor_42", seed)  →  e.g., 0xA3F1B2C4

  Level 1:  bucket_idx  = 0xA3F1B2C4  %  16          =  4
  Level 2:  sub_idx     = 0xA3F1B2C4  &  (8 - 1)     =  4   [0b...100]
                                          └── bitmask: 0b111
```

---

## 7. Concurrency Model

### 7.1 Lock Hierarchy

The locking follows a strict **coarse-to-fine** ordering to prevent deadlocks:

```
  LEVEL 0 (Coarsest)                    LEVEL 1                  LEVEL 2 (Finest)
  ┌────────────────────┐    ┌───────────────────────────┐    ┌───────────────────┐
  │ hash_bucket        │    │ sub_hash_bucket           │    │ data_node         │
  │ .resizing_lock     │───►│ .sub_hash_bucket_lock     │───►│ .lock             │
  │ (pthread_mutex_t)  │    │ (pthread_rwlock_t)        │    │ (pthread_mutex_t) │
  └────────────────────┘    └───────────────────────────┘    └───────────────────┘
        │                          │                              │
        │ Guards:                  │ Guards:                      │ Guards:
        │ - resize state           │ - linked list structure      │ - data_node.data
        │ - is_resizing flag       │ - node insertion/deletion    │ - data_node.data_size
        │ - buffer access          │ - active/total node counts   │ - data_node.is_deleted
        │                          │                              │
        │ Acquisition:             │ Modes:                       │ Acquisition:
        │ pthread_mutex_lock()     │ READ  → shared (gets)        │ pthread_mutex_lock()
        │                          │ WRITE → exclusive (inserts)  │
        └──────────────────────────┴──────────────────────────────┘

  SPECIAL: new_operation_buffer.buffer_lock (pthread_spinlock_t)
           → Protects chase buffer head/tail during resize
           → Extremely short critical section (pointer swap only)
```

### 7.2 Read vs Write Lock Usage

```
  GET operation:
    sub_hash_bucket_lock → READ  lock   (multiple readers concurrently)
    data_node.lock       → MUTEX lock   (serialized per-node read)

  SET/UPDATE operation:
    sub_hash_bucket_lock → READ  lock   (for update of existing node)
    data_node.lock       → MUTEX lock   (serialized per-node write)

    sub_hash_bucket_lock → WRITE lock   (for inserting new node — modifies list)

  DELETE operation:
    sub_hash_bucket_lock → READ  lock   (soft-delete only modifies data_node)
    data_node.lock       → MUTEX lock   (set is_deleted = true)
```

> **Key insight:** Even SET (update) uses a READ lock on the sub-bucket because it doesn't modify the linked list structure — it only mutates the data_node's value via the node-level mutex. Only INSERT requires a WRITE lock because it changes the linked list head pointer.

### 7.3 Concurrency Under Resize

```
  ┌───────────────────────────────────────────────────────────────────┐
  │               NORMAL MODE (is_resizing == false)                  │
  │                                                                   │
  │  Thread A (SET existing key) ─► sub_hash_table ─► sub_bucket      │
  │                              [READ lock] ─► data_node [MUTEX]     │
  │  Thread B (SET new key) ─────► sub_hash_table ─► sub_bucket       │
  │                              [WRITE lock]                         │
  │  Thread C (GET) ─────────────► sub_hash_table ─► sub_bucket       │
  │                              [READ lock]                          │
  │  Thread D (GET) ─────────────► sub_hash_table ─► sub_bucket       │
  │                              [READ lock]                          │
  │         (C and D can run concurrently on the same sub-bucket)     │
  └───────────────────────────────────────────────────────────────────┘

  ┌───────────────────────────────────────────────────────────────────┐
  │               RESIZE MODE (is_resizing == true)                   │
  │                                                                   │
  │  Resize Worker ──► snapshot (read-only) ──► new sub-hash-table    │
  │  Chase Worker  ──► new_operation_buffer ──► new sub-hash-table    │
  │                                                                   │
  │  Thread A (SET) ──► resizing_lock ──► new_operation_buffer        │
  │  Thread B (GET) ──► resizing_lock ──► snapshot + buffers          │
  │  Thread C (DEL) ──► resizing_lock ──► delete_operation_buffer     │
  │                                                                   │
  │  All client threads serialize on resizing_lock when bucket is     │
  │  resizing. The resize worker holds no client-facing lock during   │
  │  its main migration phase (only acquires resizing_lock at the     │
  │  end for finalization).                                           │
  └───────────────────────────────────────────────────────────────────┘
```

---

## 8. Dynamic Resizing Architecture

Resizing is triggered by the current `_check_for_resize_condition()` implementation after insertion pressure builds inside a sub-hash-bucket. The code first checks whether `total_node_count` exceeds `max_linked_list_chain_length`, then tries to reclaim soft-deleted nodes, and finally requests resize if pressure still remains. The entire sub-hash-table within that hash bucket is then doubled in size.

Current implementation behavior:

```
  if total_node_count <= max_linked_list_chain_length
    → no resize

  if active_node_count == total_node_count
    → resize immediately

  else
    → cleanup soft-deleted nodes
    → if total_node_count after cleanup >= max_linked_list_chain_length
      → resize
    else
      → no resize
```

### 8.1 Resize Lifecycle

```
  ┌──────────────────────────────────────────────────────────────────┐
  │  PHASE 1: TRIGGER                                                │
  │                                                                  │
  │  add_node_to_sub_hash_bucket()                                   │
  │    → increments active_node_count + total_node_count             │
  │    → _check_for_resize_condition()                               │
  │    → if total_node_count exceeds threshold pressure              │
  │         and cleanup cannot relieve it                            │
  │         returns SUCCESS_ADDED_NEW_NODE_RESIZING_TRIGGERED (20)   │
  │                                                                  │
  │  upsert_node_to_hash_bucket()                                    │
  │    → detects return code 20                                      │
  │    → acquires resizing_lock                                      │
  │    → calls initialize_hash_bucket_resizing()                     │
  └──────────────────────┬───────────────────────────────────────────┘
                         │
                         ▼
  ┌──────────────────────────────────────────────────────────────────┐
  │  PHASE 2: INITIALIZATION                                         │
  │                                                                  │
  │  initialize_hash_bucket_resizing()                               │
  │    1. Set is_resizing = true                                     │
  │    2. snapshot_sub_hash_table_ptr = sub_hash_table_ptr           │
  │       (snapshot is now READ-ONLY)                                │
  │    3. Allocate resizing_buffer:                                  │
  │       ├── new_operation_buffer  (spinlock-protected DLL)         │
  │       ├── delete_operation_buffer (growable array)               │
  │       └── updated_operation_buffer_head (singly linked list)     │
  │    4. Spawn DETACHED resize worker thread                        │
  │    5. Release resizing_lock                                      │
  └──────────────────────┬───────────────────────────────────────────┘
                         │
                         ▼
  ┌──────────────────────────────────────────────────────────────────┐
  │  PHASE 3: MIGRATION   (background threads)                       │
  │                                                                  │
  │  Resize Worker (detached thread):                                │
  │    1. Create new sub-hash-table with 2× bucket_size              │
  │    2. Spawn chase worker (joinable thread)                       │
  │    3. Migrate all nodes from snapshot → new table                │
  │       (iterate every sub-bucket, every linked list node)         │
  │    4. Signal chase worker to stop + pthread_join                 │
  │                                                                  │
  │  Chase Worker (joinable thread, runs concurrently):              │
  │    ┌─────────────────────────────────────────────────────────┐   │
  │    │  Polls new_operation_buffer from TAIL → HEAD            │   │
  │    │  For each buffered node:                                │   │
  │    │    - If delete → skip                                   │   │
  │    │    - If insert/update → upsert into new sub-hash-table  │   │
  │    │  Spin-waits when buffer is empty                        │   │
  │    │  Exits on kill_signal                                   │   │
  │    └─────────────────────────────────────────────────────────┘   │
  └──────────────────────┬───────────────────────────────────────────┘
                         │
                         ▼
  ┌──────────────────────────────────────────────────────────────────┐
  │  PHASE 4: FINALIZATION                                           │
  │                                                                  │
  │  Resize Worker acquires resizing_lock:                           │
  │    1. Commit remaining buffer operations to new table:           │
  │       ├── updated_operation_buffer → upsert to new table         │
  │       ├── delete_operation_buffer  → delete from new table       │
  │       └── new_operation_buffer     → upsert remaining to table   │
  │    2. Swap: sub_hash_table_ptr = new_sub_hash_table              │
  │    3. Free snapshot sub-hash-table                               │
  │    4. Free resizing_buffer                                       │
  │    5. Set is_resizing = false                                    │
  │    6. Release resizing_lock                                      │
  └──────────────────────────────────────────────────────────────────┘
```

### 8.2 Chase Buffer Detail

The chase buffer is the mechanism that allows **writes to continue during resize** without being lost.

```
  Writers (client threads)                   Chase Worker (background thread)
  insert at HEAD ──►                         ◄── reads from TAIL toward HEAD
                                             
  ┌─────────────────────────────────────────────────────────────────┐
  │                    new_operation_buffer                         │
  │                  (doubly-linked list)                           │
  │                                                                 │
  │   HEAD                                                  TAIL    │
  │    │                                                     │      │
  │    ▼                                                     ▼      │
  │  ┌──────┐    ┌──────┐    ┌──────┐    ┌──────┐    ┌──────┐       │
  │  │DLLn-5│◄──►│DLLn-4│◄──►│DLLn-3│◄──►│DLLn-2│◄──►│DLLn-1│       │
  │  │(new) │    │      │    │      │    │      │    │(old) │       │
  │  └──┬───┘    └──┬───┘    └──┬───┘    └──┬───┘    └──┬───┘       │
  │     │           │           │           │           │           │
  │     ▼           ▼           ▼           ▼           ▼           │
  │  data_node   data_node   data_node   data_node   data_node      │
  │                                                                 │
  │  Lock: pthread_spinlock_t buffer_lock                           │
  │  (protects head/tail pointer updates only)                      │
  │                                                                 │
  │  current_consumer_ptr → tracks chase worker's position          │
  └─────────────────────────────────────────────────────────────────┘
```

### 8.3 Buffer Types During Resize

Three separate buffers capture concurrent operations during an active resize:

```
  ┌─────────────────────────────────────────────────────────────┐
  │                    resizing_buffer                          │
  │                                                             │
  │  ┌───────────────────────────────────────────────────┐      │
  │  │  new_operation_buffer (doubly-linked list)         │     │
  │  │  - All NEW writes arriving during resize           │     │
  │  │  - Spinlock-protected (pthread_spinlock_t)         │     │
  │  │  - Chase worker reads from tail → head             │     │
  │  └───────────────────────────────────────────────────┘      │
  │                                                             │
  │  ┌───────────────────────────────────────────────────┐      │
  │  │  updated_operation_buffer (singly-linked list)     │     │
  │  │  - Updates to nodes already IN the snapshot        │     │
  │  │  - No spinlock (protected by resizing_lock)        │     │
  │  │  - Applied during finalization                     │     │
  │  └───────────────────────────────────────────────────┘      │
  │                                                             │
  │  ┌───────────────────────────────────────────────────┐      │
  │  │  delete_operation_buffer (growable array)          │     │
  │  │  - Deletes of nodes in the snapshot                │     │
  │  │  - Stores key + hash pairs                         │     │
  │  │  - Grows by 500 entries via realloc when full      │     │
  │  │  - Applied during finalization                     │     │
  │  └───────────────────────────────────────────────────┘      │
  └─────────────────────────────────────────────────────────────┘
```

---

## 9. Memory Management

### 9.1 Memory Pool Architecture

The keystore uses a **pre-allocated memory pool** for `linked_list_node` allocations to minimize `malloc` overhead in the hot path.

```
  ┌──────────────────────────────────────────────────────────────┐
  │                memory_pool (g_list_pool)                     │
  │                                                              │
  │  pool_start ──────────────────────────────► pool_end         │
  │  │                                              │            │
  │  ▼                                              ▼            │
  │  ┌───────┬───────┬───────┬───────┬───────┬───────┐           │
  │  │ Block │ Block │ Block │ Block │ Block │ Block │           │
  │  │   0   │   1   │   2   │   3   │   4   │   5   │           │
  │  └───────┴───────┴───────┴───────┴───────┴───────┘           │
  │  ▲                         ▲                                 │
  │  │                         │                                 │
  │  │                    next_ptr                               │
  │  │                  (next free block from sequential pool)   │
  │                                                              │
  │  block_size = sizeof(linked_list_node)                       │
  │  total_blocks = ceil(bucket_size * pre_allocation_factor)    │
  │  pool_lock = pthread_mutex_t (when concurrency enabled)      │
  │                                                              │
  │  free_block_list[]  ← returned blocks go here for reuse      │
  │  ┌────┬────┬────┬────┐                                       │
  │  │ptr1│ptr2│NULL│NULL│  ← LIFO reuse stack                   │
  │  └────┴────┴────┴────┘                                       │
  └──────────────────────────────────────────────────────────────┘

  Allocation strategy:
  ┌──────────────┐     ┌───────────────────┐     ┌────────────┐
  │ free_block   │ NO  │ next_ptr <        │ NO  │  malloc()  │
  │ list empty?  ├────►│ pool_end?         ├────►│  fallback  │
  │              │     │                   │     │            │
  │  YES ↓       │     │  YES ↓            │     └────────────┘
  │  Return from │     │  Return next_ptr  │
  │  free_block  │     │  Advance next_ptr │
  │  list (LIFO) │     │                   │
  └──────────────┘     └───────────────────┘
```

### 9.2 What Gets Allocated Where

| Allocation | Source | Freed By |
|------------|--------|----------|
| `linked_list_node` | Memory pool (or `malloc` fallback) | `free_memory(ptr, is_pool=true)` |
| `data_node` | `malloc` (with FAM for key) | `delete_data_node()` |
| `data_node.data` (value) | `malloc` / `realloc` | `delete_data_node` or `edit_data_node_value` |
| `double_linked_list_node` | `malloc` | `delete_double_linked_list()` |
| `sub_hash_bucket[]` | `calloc` (array) | `cleanup_sub_hash_table()` |
| `hash_bucket[]` | `calloc` (array) | `cleanup_hash_table()` |
| `resizing_buffer` | `calloc` | `delete_resizing_buffer()` |
| `delete_operation_buffer` | `malloc` + `realloc` (grows) | `delete_resizing_buffer()` |
| `key_value_pair.key/value` (on read) | `malloc` (by `read_data_node_value`) | **Caller responsibility** |

---

## 10. Background Task Manager

The background task manager handles all thread lifecycle concerns for the resize worker and chase worker threads.

```
  ┌─────────────────────────────────────────────────────────────┐
  │             background_task_manager                         │
  │                                                             │
  │  Registry: background_tasks_registry[100]                   │
  │  Lock:     pthread_mutex_t registry_lock                    │
  │                                                             │
  │  ┌─────────────────────────────────────────────────────┐    │
  │  │  initialize_background_function()                   │    │
  │  │                                                     │    │
  │  │  1. Allocate background_task_args_t                 │    │
  │  │     ├── void* task_input_args                       │    │
  │  │     └── _Atomic bool kill_signal = false            │    │
  │  │                                                     │    │
  │  │  2. Allocate background_task_t                      │    │
  │  │     ├── int (*background_task)(void*)  [fn pointer] │    │
  │  │     ├── background_task_args_t*                     │    │
  │  │     └── bool is_detached                            │    │
  │  │                                                     │    │
  │  │  3. pthread_create() → run task                     │    │
  │  │                                                     │    │
  │  │  4. If detached → pthread_detach()                  │    │
  │  │     If joinable → register in registry with UUID    │    │
  │  └─────────────────────────────────────────────────────┘    │
  │                                                             │
  │  ┌─────────────────────────────────────────────────────┐    │
  │  │  cleanup_background_task(uuid)                      │    │
  │  │                                                     │    │
  │  │  1. Set kill_signal = true (atomic)                 │    │
  │  │  2. pthread_join() (waits for thread to finish)     │    │
  │  │  3. Remove from registry                            │    │
  │  │  4. Free task structs                               │    │
  │  └─────────────────────────────────────────────────────┘    │
  └─────────────────────────────────────────────────────────────┘

  Thread Roles:
  ┌────────────────────┬───────────┬──────────────────────────────┐
  │ Thread             │ Detached? │ Purpose                      │
  ├────────────────────┼───────────┼──────────────────────────────┤
  │ Resize Worker      │ YES       │ Migrates data to new table   │
  │ Chase Worker       │ NO        │ Drains new-ops buffer        │
  └────────────────────┴───────────┴──────────────────────────────┘
```

---

## 11. Error Handling Model

All functions return `int`: **0 for success**, **negative for error**, **positive for special success codes**.

```
  Return Code Spectrum:
  
  ◄─── ERRORS (negative) ───┼─── SUCCESS ───┼─── SPECIAL SUCCESS (positive) ──►
                            │               │
  -102 ─── -1               0              10 ─── 20 ─── 21

  Error Code Categories:
  ┌────────────┬──────────────────────────────────────────────┐
  │  Range     │  Category                                    │
  ├────────────┼──────────────────────────────────────────────┤
  │  -1        │  General failure                             │
  │  -11..-12  │  Argument / validation errors                │
  │  -20..-22  │  Memory / resource management                │
  │  -30..-35  │  Concurrency / locking errors                │
  │  -40..-42  │  Hash computation / indexing                 │
  │  -50..-54  │  Hash table / bucket operations              │
  │  -60..-64  │  Sub-hash-table operations                   │
  │  -70..-72  │  Linked list operations                      │
  │  -80..-83  │  Data node operations                        │
  │  -90..-92  │  Memory pool errors                          │
  │  -100..-102│  Platform / threading errors                 │
  └────────────┴──────────────────────────────────────────────┘

  Special Success Codes:
  ┌────────┬───────────────────────────────────────────────────┐
  │  Code  │  Meaning                                          │
  ├────────┼───────────────────────────────────────────────────┤
  │   0    │  SUCCESS (generic / update completed)             │
  │  10    │  New node inserted successfully                   │
  │  11    │  Added to pending list (resize buffer)            │
  │  20    │  New node inserted + resize should be triggered   │
  │  21    │  Resize still in progress (status check)          │
  └────────┴───────────────────────────────────────────────────┘
```

---

## 12. Module Dependency Graph

```
                        ┌─────────────────────┐
                        │     key_store.c     │  ◄── Public API
                        │   (core layer)      │
                        └─────────┬───────────┘
                                  │
                    ┌─────────────┼──────────────┐
                    │             │              │
                    ▼             ▼              ▼
         ┌──────────────┐ ┌─────────────┐ ┌──────────────┐
         │hash_functions│ │ hash_table  │ │   memory     │
         │   .c/.h      │ │ _operation  │ │  _manager    │
         │ (MurmurHash) │ │   .c/.h     │ │   .c/.h      │
         └──────────────┘ └──────┬──────┘ └──────────────┘
                                 │
                                 ▼
                        ┌────────────────┐
                        │  hash_bucket   │
                        │  _operation    │
                        │    .c/.h       │
                        └───┬────────┬───┘
                            │        │
           ┌────────────────┘        └──────────────┐
           ▼                                        ▼
  ┌──────────────────┐              ┌────────────────────────────┐
  │  sub_hash_table  │              │  hash_bucket_resizing      │
  │  _operation      │              │  _operation.c/.h           │
  │    .c/.h         │              └──────────┬─────────────────┘
  └────────┬─────────┘                         │
           │                        ┌──────────┼──────────┐
           ▼                        ▼          ▼          ▼
  ┌──────────────────┐    ┌──────────────┐ ┌────────┐ ┌──────────────┐
  │  sub_hash_bucket │    │  resize      │ │buffer  │ │  background  │
  │  _operation      │    │  _operation  │ │_oper   │ │  _task_mgr   │
  │    .c/.h         │    │    .c/.h     │ │.c/.h   │ │    .c/.h     │
  └────────┬─────────┘    └──────────────┘ └────────┘ └──────────────┘
           │
           ▼
  ┌──────────────────────────────────────────┐
  │         data_structures/                 │
  │  ┌─────────────┐  ┌──────────────────┐   │
  │  │linked_list  │  │ double_linked    │   │
  │  │_operation   │  │ _list_operation  │   │
  │  └──────┬──────┘  └──────────────────┘   │
  │         │                                │
  │         ▼                                │
  │  ┌─────────────┐                         │
  │  │ data_node   │                         │
  │  │ _operation  │                         │
  │  └─────────────┘                         │
  └──────────────────────────────────────────┘
```

---

## 13. Public API Surface

The public API is intentionally minimal — three CRUD operations plus lifecycle management:

```c
// Initialize the keystore with configuration
int initialise_key_store(hash_table_configuration config, double pre_memory_allocation_factor);

// Clean up all resources
int cleanup_key_store(void);

// Set or update a key-value pair
int set_key(key_value_pair* value);

// Retrieve a value by key (caller must free value->key and value->value)
int get_key(const char* key, key_value_pair* value_out);

// Soft-delete a key
int delete_key(const char* key);
```

### Configuration

```c
hash_table_configuration config = {
    .bucket_size                  = 1024,   // Level-1 buckets (must be power of 2 in current implementation)
    .is_concurrency_enabled       = true,   // Enable all locks
    .sub_hash_table_bucket_size   = 16,     // Level-2 buckets (must be power of 2)
    .max_linked_list_chain_length = 8       // Resize trigger threshold
};

double pre_alloc_factor = 0.5;  // Pre-allocate 50% of estimated LLN pool

int rc = initialise_key_store(config, pre_alloc_factor);
```

### Usage Example

```c
// SET
key_value_pair kv = {
    .key        = "sensor_42",
    .value      = (unsigned char*)"temperature=23.5",
    .value_size = 17
};
int rc = set_key(&kv);

// GET
key_value_pair result = {0};
rc = get_key("sensor_42", &result);
if (rc == 0) {
    printf("Key: %s, Value: %.*s\n", result.key, (int)result.value_size, result.value);
    free(result.key);    // Caller must free
    free(result.value);  // Caller must free
}

// DELETE
rc = delete_key("sensor_42");
```

---

## 14. Design Decisions & Trade-offs

### 14.1 Why Two-Level Hashing?

**Problem:** A single flat hash table either resizes everything at once (blocking all threads) or uses complex lock-free schemes.

**Solution:** The two-level design isolates resize impact. When sub-bucket chains grow too long in Hash Bucket #5, only that bucket's sub-table resizes. All other buckets continue operating normally.

```
  Before Resize:                    After Resize:
  Hash Bucket #5                    Hash Bucket #5
  ┌────────────────┐               ┌─────────────────────────────┐
  │ Sub-Table (4)  │               │ Sub-Table (8)  ← doubled    │
  │ SB0: ■■■■■ (5) │               │ SB0: ■■  SB4: ■■■           │
  │ SB1: ■■■ (3)   │   ──────►     │ SB1: ■   SB5: ■■            │
  │ SB2: ■■ (2)    │               │ SB2: ■   SB6: ■             │
  │ SB3: ■■■■ (4)  │               │ SB3: ■■  SB7: ■■            │
  └────────────────┘               └─────────────────────────────┘
  14 nodes, max chain=5            Same 14 nodes, better distributed
```

### 14.2 Why Soft Deletes?

Physical removal from a linked list under concurrent reads is dangerous (use-after-free). Soft deletes (`is_deleted = true`) allow readers with existing references to safely finish. Physical cleanup happens, while checking for resizing trigger condition (it uses data node cleanup deleted node operation) or during resize when the resize worker has exclusive access to the snapshot.

### 14.3 Why Spinlock for the Chase Buffer?

The chase buffer's spinlock protects an extremely short critical section — just pointer swaps for doubly-linked-list head/tail updates. A mutex would be too expensive here because:
- The lock is acquired and released thousands of times per second during resize
- The critical section is ~5 instructions (no syscalls, no blocking I/O)
- Spinlocks avoid the kernel context-switch overhead of mutexes for ultra-short holds

### 14.4 Why a Detached Resize Worker but Joinable Chase Worker?

```
  Resize Worker (DETACHED):
    - No one waits for it. It self-manages its lifecycle.
    - When done, it finalizes + sets is_resizing = false.
    - Cleanup of hash_bucket busy-waits on is_resizing flag.

  Chase Worker (JOINABLE):
    - The resize worker MUST wait for it to finish (pthread_join)
      before proceeding to finalization.
    - Guarantees all buffered operations are processed.
```

### 14.5 Memory Ownership Model

```
  set_key(&kv):
    - keystore COPIES key and value into internal data_node
    - caller retains ownership of kv.key and kv.value

  get_key(key, &out):
    - keystore ALLOCATES new buffers for out.key and out.value
    - CALLER must free(out.key) and free(out.value)

  delete_key(key):
    - soft-delete only; no memory freed until resize cleanup
```

---

> **Document generated by KeyStore-Copilot** — For implementation details, see [memory.md](memory.md). For API reference, see [API.md](API.md). For error codes, see [ERROR_CODES.md](ERROR_CODES.md).
