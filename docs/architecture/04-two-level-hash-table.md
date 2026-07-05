# Two-Level Hash Table Architecture

The keystore uses a **two-level hashing scheme** to keep top-level routing stable while allowing inner tables to resize independently. This design minimizes lock contention and reduces the overhead of dynamic resizing.

> **Related:** See [05-data-structures.md](05-data-structures.md) for the complete struct hierarchy and how `hash_bucket` and `sub_hash_table_memory_pool` relate to one another in memory.

## Table of Contents
- [1. Architecture Overview](#1-architecture-overview)
- [2. Composite Key Hash](#2-composite-key-hash)
- [3. Routing](#3-routing)
- [4. Collision Resolution](#4-collision-resolution)
- [5. Memory Management](#5-memory-management)
- [6. Design Justification: Why Two Levels?](#6-design-justification-why-two-levels)
- [7. Use Cases & Problem This Design Solves](#7-use-cases--problem-this-design-solves)

## 1. Architecture Overview

```text
      ┌─────────────────────────────────────────┐
      │   HASH TABLE (Level 1)                  │
      │   - Fixed-size (immutable)              │
      │   - Routing via: bucket_hash % size     │
      │   - Each bucket → sub-table             │
      ├────┬────┬────┬────┬────┬────┬────┬──────┤
      │ HB0│ HB1│ HB2│ HB3│ HB4│ HB5│....│ HBn  │
      └─┬──┴────┴────┴─┬──┴────┴─┬──┴────┴────┬─┘
        │              │         │            │      
        │              │         │            │   
        ▼              ▼         ▼            ▼
     ┌───────┐    ┌───────┐   ┌────────┐    ┌────────┐
     │ SUB-  │    │ SUB-  │   │  SUB-  │    │  SUB-  │
     │ TABLE │    │ TABLE │   │  TABLE │ .. │  TABLE │
     │ (L2)  │    │ (L2)  │   │  (L2)  │    │  (L2)  │
     │ Size: │    │ Size: │   │  Size  │    │  Size: │
     │  4-32 │    │  4-32 │   │  4-3   │    │  4-32  │
     ├─┬─┬───┤    ├─┬─┬───┤   ├──┬──┬──┤    ├──┬──┬──┤
     │0│1│2  │... │0│1│2  │...│ 0│ 1│ 2│ .. │ 0│ 1│ 2│
     └─┴─┴───┘    └─┴─┴───┘   └──┴──┴──┘    └──┴──┴──┘
      ║ ║ ║        ║ ║ ║      ║  ║  ║       ║  ║  ║
      ▼ ▼ ▼        ▼ ▼ ▼      ▼  ▼  ▼       ▼  ▼  ▼
      LL LL LL     LL LL LL   LL LL LL      LL LL LL
      (collision   (collision (collision    (collision
       chains)      chains)    chains)        chains)

Legend:
  HB* = Hash Bucket (fixed index, holds pointer to sub-table)
  SUB-TABLE = Sub-Hash-Table (resizable)
  LL = Linked List (for collision handling within a sub-bucket)
```

## 2. Composite Key Hash

Every key is hashed once into a **composite hash** structure containing two independent hash values:

> **Note:** See [Hashing Strategy](06-hashing-strategy.md) for algorithm implementation details (MurmurHash3, seed derivation, and edge cases).

```text
Input Key: "user_12345"
           │
           ├─→ Hash Function
           │
           ▼
       ┌─────────────────────────────────┐
       │  composite_key_hash             │
       ├─────────────────────────────────┤
       │ bucket_hash    : 0x5F3A91C2     │ ← Used for Level 1 routing
       │ sub_bucket_hash: 0x8E4B72F1     │ ← Used for Level 2 routing
       └─────────────────────────────────┘
           │                        │
           │ Level 1              Level 2
           │ (modulo)             (bitwise AND)
           │                        │
           ▼                        ▼
```

## 3. Routing

### Lookup Path Overview

```text
1. Given key, compute composite_key_hash {bucket_hash, sub_bucket_hash}
   
2. Route through Level 1 (fixed)
   
3. Route through Level 2 (resizable)
```

### Level 1: Fixed Hash Table Routing

Determines which sub-table holds the key-value pair.

```text
Routing: bucket_index = bucket_hash % total_buckets
         ↓
         Retrieve: sub_table = hash_table[bucket_index].sub_table_ptr
```

**Characteristics:**
- **Fixed size** across entire lifetime — never resized
- **Power-of-two constraint**: `total_buckets` must be power of 2 for consistency  
- **Modulo operation** (not bitwise AND) ensures stable routing
- **One bucket per sub-table** — no keys reshuffled on Level 1

### Level 2: Resizable Sub-Hash-Table Routing

Determines the bucket within the sub-table where the key-value pair resides.

```text
Routing: sub_bucket_index = sub_bucket_hash & (sub_table_size - 1)
         ↓
         Retrieve linked list at: sub_table[sub_bucket_index]
```

**Characteristics:**
- **Dynamically resizable** — grows when chain length exceeds `max_linked_list_chain_length`
- **Power-of-two requirement**: Mandatory for clean bitwise AND masking
- **Bitwise AND (fast bitmask)** instead of modulo for performance
- **Grows in steps**: `4 → 8 → 16 → 32 → ...`

## 4. Collision Resolution

### Linked List Strategy

When two keys hash to the same bucket at Level 2, they form a linked list (collision chain).

**Insertion:** New key-value pair appended to the bucket's linked list.  
**Lookup:** Traverse linked list sequentially until key matches or end-of-list.  
**Deletion:** Mark node as soft-deleted (cleaned during resize).

### Bloom Filter Optimization (Optional)

**Optional performance optimization** per sub-bucket. When enabled, each sub-hash-bucket maintains a bloom filter to accelerate negative lookups (misses).

#### Bloom Filter Behavior

| Scenario | Filter Result | Action | Outcome |
|----------|---------------|--------|---------|
| Key exists in bucket | "possibly yes" | Traverse linked list | Confirmed found |
| Key does NOT exist | "definitely no" | Skip traversal | **Fast miss** |
| Key does NOT exist | "possibly yes" (false positive) | Traverse linked list | Confirmed not found |

#### When Bloom Filter Is Valuable

- **High collision rates** — Many keys hash to same sub-bucket; most lookups miss
- **Long linked lists** — List traversal is expensive
- **Read-heavy workloads** — Frequent cache misses on negative lookups

#### Trade-off: False Positives vs Memory

Bloom filters guarantee **no false negatives** (never incorrectly say "not found" when key exists) but may have **false positives** (rarely say "found" when key doesn't exist). In the false-positive case, linked list traversal confirms the key is actually absent. Trade-off: Small per-bucket memory overhead vs. faster miss detection.

#### Update Operations

- **Insert:** Key added to bloom filter
- **Update:** Bloom filter unchanged (key already present)
- **Delete:** Key remains in filter (soft deletion; cleaned during resize)

## 5. Memory Management

| Aspect | Details |
|--------|---------|
| **Level 1 allocation** | Single allocation at init; immutable across lifetime |
| **Level 2 allocation** | Per-bucket allocation; resized independently |
| **Resizing mechanism** | Old sub-table → rehash all entries → new sub-table → atomic swap |
| **Soft deletion** | Deleted nodes marked but retained; removed during resize cleanup |

## 6. Design Justification: Why Two Levels?

### Comparison: Single-Level vs Two-Level

| Aspect | Single-Level Table | Two-Level Table |
|--------|-------------------|-----------------|
| **Resize scope** | Rehash all keys globally | Only one sub-table resizes at a time |
| **Lock contention** | Global lock required during resize | Isolated to affected bucket |
| **Resize blocking** | Blocks all operations system-wide | Blocks only that bucket's operations |
| **Memory growth** | Double entire table in one operation | Double one sub-table incrementally |
| **Failure impact** | Failure blocks everything | Failure contained to one bucket |
| **GC pressure** | Large allocations cause pauses | Smaller, frequent allocations |

### Key Design Benefits

1. **Top-level stability** — Level 1 routing never changes; no need to recompute after Level 2 resize
2. **Granular concurrency** — Lock contention isolated to individual sub-tables; other buckets remain accessible
3. **Incremental memory growth** — Allocations spread over time; no sudden large allocations
4. **Background-friendly** — Level 2 resizes can be deferred or run asynchronously without affecting critical paths
5. **Predictable routing** — Hash functions stable throughout lifetime; no rehashing surprises
6. **Tunable balance** — Number of top-level buckets determines concurrency width vs memory overhead

## 7. Use Cases & Problem This Design Solves

### Original Problem

A single-level hash table must resize globally when load factor exceeds threshold. During resize:
- All keys must be rehashed
- Entire table locked
- All operations blocked
- Memory doubled in one operation

This becomes a **bottleneck** for:
- High-concurrency workloads where lock contention compounds under load
- Systems needing predictable latency (resize can cause multi-millisecond stalls)
- Memory-constrained environments (sudden 2× allocation can fail or stall garbage collection)
- Distributed scenarios where one node's resize blocks replication or failover

### This Design's Solution

The two-level scheme distributes the pain:

1. **Resize is now local** — Only one sub-table locks and resizes
2. **Throughput maintained** — Other buckets remain operational during resize
3. **Latency capped** — Blocking is bounded to a single sub-table's size, not the entire keystore
4. **Memory smooth** — Growth spread across multiple smaller allocations
5. **Concurrency scalable** — Effective parallelism increases with number of top-level buckets

### Ideal For

- **High-concurrency, multi-threaded workloads** — Multiple threads can access different buckets simultaneously during resize
- **Low-latency systems** — Resize latency is sub-linear; bounded by sub-table size, not total keystore
- **Distributed storage nodes** — Resize doesn't block replication or peer communication; leadership changes unaffected
- **Read-heavy patterns with Bloom filter** — Negative lookups accelerated; scales well under cache misses
- **Incremental data growth** — No sudden "resize stall" events; memory allocated smoothly over time
