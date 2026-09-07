# Keystore C API Documentation

## Overview

This document describes the public API for the keystore — a thread-safe, high-performance distributed key-value storage system written in C. The API is optimized for concurrent multi-threaded access with minimal contention through fine-grained per-bucket and per-node locking.

All public functions are declared in `src/keystore/core/key_store.h`. For architecture and design details, see [Architecture Overview](./docs/architecture/01-overview.md) or the [Design Decisions Guide](./docs/DESIGN_DECISIONS.md).

## Quick Reference

| Function | Purpose |
|----------|---------|
| `initialise_key_store()` | Initialize keystore with configuration |
| `cleanup_key_store()` | Release all resources |
| `set_key()` | Insert or update a key-value pair |
| `get_key()` | Retrieve value for a key |
| `delete_key()` | Mark a key for deletion |
| `get_key_store_max_chain_depth()` | Get maximum observed collision chain depth |

---

## Initialization & Cleanup

### `get_key_store_max_chain_depth()`

```c
unsigned int get_key_store_max_chain_depth(void);
```

**Purpose:** Retrieve the maximum observed collision chain depth across all sub-hash-tables.

**Returns:**
- Unsigned integer: Maximum chain length observed in any sub-table
- Returns 0 if keystore is empty or uninitialized

**Use Case:** Monitor chain lengths during benchmarking or tuning. If chains are consistently long, increase `sub_hash_table_bucket_size` or decrease `max_linked_list_chain_length`.

**Thread Safety:** Thread-safe; uses atomic read or locks as needed.

---

### `initialise_key_store()`

```c
int initialise_key_store(hash_table_configuration config, double pre_memory_allocation_factor);
```

**Purpose:** Initialize keystore with configuration and optional memory pre-allocation.

**Parameters:**
| Name | Type | Constraints | Description |
|------|------|-----------|-------------|
| `config` | `hash_table_configuration` | Power-of-2 sizes, positive chain length | Hash table shape and concurrency mode |
| `pre_memory_allocation_factor` | `double` | 0.0 ≤ factor ≤ 1.0 | Fraction of max memory to pre-allocate for collision-chain nodes (reduces insertion latency) |

**Returns:**
- `SUCCESS` (0): Keystore initialized
- `ERR_INVALID_ARGUMENT` (-11): Pre-allocation factor out of range [0, 1]
- `ERR_INVALID_CONFIG` (-12): `bucket_size` or `sub_hash_table_bucket_size` not powers of 2, or `max_linked_list_chain_length` ≤ 0
- `ERR_MEMORY_ALLOCATION_FAILED` (-20): Heap allocation failed
- `ERR_RESOURCE_INIT_FAILED` (-21): Mutex/lock creation failed (OS resource limit?)

**Preconditions:**
- `config.bucket_size`: Power of 2, > 0. Typical: 16–256 depending on contention.
- `config.sub_hash_table_bucket_size`: Power of 2, > 0. Typical: 8–64.
- `config.max_linked_list_chain_length`: > 0. Typical: 3–8. Higher = fewer resizes but longer chains.

**Idempotency:** Multiple calls to `initialise_key_store()` without an intervening `cleanup_key_store()` return `SUCCESS` without re-initializing (no-op).

**Thread Safety:** Only one thread should call this function; it is not internally synchronized.

---

### `cleanup_key_store()`

```c
int cleanup_key_store(void);
```

**Purpose:** Release all resources (hash tables, locks, memory pools).

**Returns:**
- `SUCCESS` (0): Always returns success (best-effort cleanup)

**Behavior:**
- Frees all buckets, sub-tables, nodes, and buffers
- Destroys all mutexes and RW-locks
- Clears hash seeds
- Keystore enters uninitialized state; must call `initialise_key_store()` before reuse

**Thread Safety:** Single-threaded only. All other threads must stop using the keystore before calling this function. Not internally synchronized.


## Key Operations (Lifecycle)

**Typical workflow:**
1. **Initialize** → `initialise_key_store()` — Set up hash table and locks
2. **Operate** → `set_key()`, `get_key()`, `delete_key()` — Use the keystore (thread-safe if concurrency enabled)
3. **Cleanup** → `cleanup_key_store()` — Release all resources before exit

### `set_key()`

```c
int set_key(key_value_pair* kv_pair);
```

**Purpose:** Insert a new key-value pair or update an existing key's value.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `kv_pair` | `key_value_pair*` | Pointer to key-value pair; keystore copies key and value |

**Returns:**
- `SUCCESS` (0): Key existed; value updated
- `SUCESS_ADDED_NEW_NODE` (10): New key inserted
- `SUCESS_ADDED_NEW_NODE_RESZING_TRIGGERED` (20): New key inserted; sub-table resize triggered
- `ERR_INVALID_ARGUMENT` (-11): NULL pointer, NULL/empty key, NULL value, or value_size = 0
- `ERR_MEMORY_ALLOCATION_FAILED` (-20): Cannot allocate memory for node
- `ERR_HASH_COMPUTE_FAILED` (-40): Hash returned sentinel
- `ERR_DATA_NODE_CREATION_FAILED` (-83): Node creation failed

**Behavior:**
- **Update:** If key exists, atomically replace value (old value freed)
- **Insert:** If key does not exist, create entry in sub-hash-table's collision chain
- **Delete then Insert:** If you `delete_key()` then `set_key()` with the same key, a new node is created; the old deleted entry remains (compacted later during resize)
- **Resize Trigger:** If chain length exceeds `max_linked_list_chain_length`, resize is triggered asynchronously (background worker)

**Memory:** Caller retains input buffer ownership; keystore copies all data.

**Thread Safety:** Thread-safe when `is_concurrency_enabled = true`. Per-bucket and per-node locking.

---

### `get_key()`

```c
int get_key(const char* key, key_value_pair* kv_pair_out);
```

**Purpose:** Retrieve the value for a given key.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `key` | `const char*` | Null-terminated string key; must not be NULL or empty |
| `kv_pair_out` | `key_value_pair*` | Output struct; keystore allocates and populates on success |

**Returns:**
- `SUCCESS` (0): Key found; `kv_pair_out` populated
- `ERR_INVALID_ARGUMENT` (-11): `key` is NULL, empty, or `kv_pair_out` is NULL
- `ERR_HASH_COMPUTE_FAILED` (-40): Hash function returned sentinel value
- `ERR_INVALID_BUCKET_INDEX` (-41): Computed bucket index out of range
- `ERR_HASH_BUCKET_NOT_FOUND` (-51): Hash bucket not found in table
- `ERR_DATA_NODE_NOT_FOUND` (-80): Key not found or is deleted

⚠️ **CRITICAL MEMORY OWNERSHIP:**
- **On SUCCESS:** Keystore heap-allocates BOTH `kv_pair_out->key` AND `kv_pair_out->value`
- **You must free both pointers after use**, or you will leak memory:
  ```c
  key_value_pair result = {0};
  if (get_key("greeting", &result) == SUCCESS) {
      printf("Value: %.*s (size: %zu)\n", (int)result.value_size, (char*)result.value, result.value_size);
      free(result.key);      // REQUIRED
      free(result.value);    // REQUIRED
  }
  ```
- **On error:** No allocation; pointers should not be dereferenced
- **Caller responsibility:** Do NOT assume keystore cleans up; YOU own the memory after SUCCESS

**Thread Safety:** Thread-safe when `is_concurrency_enabled = true`. Uses per-bucket RW-lock in read mode.

---

### `delete_key()`

```c
int delete_key(const char* key);
```

**Purpose:** Mark a key as deleted. Does not immediately free memory; deletion is lazy and completed during resize operations.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `key` | `const char*` | Null-terminated string key; must not be NULL or empty |

**Returns:**
- `SUCCESS` (0): Key marked as deleted
- `ERR_INVALID_ARGUMENT` (-11): `key` is NULL or empty
- `ERR_HASH_COMPUTE_FAILED` (-40): Hash function returned sentinel value
- `ERR_INVALID_BUCKET_INDEX` (-41): Computed bucket index out of range
- `ERR_HASH_BUCKET_NOT_FOUND` (-51): Hash bucket not found in table
- `ERR_DATA_NODE_NOT_FOUND` (-80): Key not found in keystore

**Behavior:**
- The entry is marked as deleted but **not physically freed**; this is a lazy deletion strategy
- Subsequent `get_key()` calls on deleted keys return `ERR_DATA_NODE_NOT_FOUND`
- Memory is reclaimed during sub-hash-table resize operations (entries are compacted)
- If `set_key()` is called with a deleted key's name, a **new entry is created** (old deleted entry is left as-is)
- Physical cleanup via `cleanup_key_store()` releases all deleted entries
- **Rationale:** Lazy deletion reduces lock contention and latency under high concurrency; resize epochs batch cleanup work

**Performance:** O(1) with minimal locking.

**Thread Safety:** Thread-safe when `is_concurrency_enabled = true`. Uses per-bucket mutex to mark the node as deleted.

---


## Hashing Strategy

The keystore uses **MurmurHash3 (64-bit)** with a **two-level hashing strategy** to distribute keys uniformly and independently route within each level.

**Two-Level Distribution:**
1. **Top-level hash:** Routes key to a bucket in the primary hash table
2. **Secondary hash:** Routes key within the sub-hash-table of that bucket

This separation achieves:
- **Reduced contention:** Each bucket is independently locked; separate hashes reduce intra-bucket collisions
- **Better resize parallelism:** Resizing one bucket's sub-table doesn't affect others
- **Uniform spread:** Dual-seeding ensures keys don't cluster even across multiple initialization epochs

**Hash Derivation:**
Each key produces two hash values:

| Value | Purpose | Derivation |
|-------|---------|-----------|
| `bucket_hash` | Route to top-level bucket | `MurmurHash3(key, seed_1)` % `bucket_size` |
| `sub_bucket_hash` | Route within sub-hash-table | `MurmurHash3(key, seed_2)` & (`sub_bucket_size` - 1) |

**Seed Generation:**
- Both seeds generated during `initialise_key_store()` using time-based randomness
- Guaranteed distinct: XOR with golden-ratio constant prevents seed collision even if generated in rapid succession
- **Immutable per instance:** Same key always hashes identically during the keystore's lifetime

**Collision Handling:**
Hash collisions in sub-tables are resolved via **linked-list chaining**. When chain length exceeds `max_linked_list_chain_length`, a resize of that sub-table is triggered asynchronously (background worker).

**Error Handling:**
Hash function returns `UINT64_MAX` (sentinel) for NULL keys; this is caught and returns `ERR_HASH_COMPUTE_FAILED`.

---

## Data Structures

### Public API Structures

#### `key_value_pair`

```c
typedef struct {
    char* key;                      ///< Null-terminated string key (64+ chars typical)
    unsigned char* value;           ///< Binary value; can contain any byte sequence
    size_t value_size;              ///< Size of value in bytes; must be > 0 on insert
} key_value_pair;
```

**Fields:**
| Field | Type | Usage Notes |
|-------|------|------------|
| `key` | `char*` | **Input:** User-allocated; keystore copies. **Output:** Heap-allocated by keystore; caller must `free()`. |
| `value` | `unsigned char*` | **Input:** User-allocated; keystore copies. **Output:** Heap-allocated by keystore; caller must `free()`. |
| `value_size` | `size_t` | Size in bytes. Must be > 0 when calling `set_key()`. On `get_key()`, populated with retrieved value size. |

**Usage Context:**
- **For `set_key()`:** Populate all three fields with data to insert. Keystore makes internal copies.
- **For `get_key()`:** Pass empty struct (initialized to zeros). On success, keystore allocates and populates all fields.

---

#### `hash_table_configuration`

```c
typedef struct {
    unsigned int bucket_size;                   ///< Top-level buckets (must be power of 2)
    bool is_concurrency_enabled;                ///< Enable thread-safe locking
    unsigned int sub_hash_table_bucket_size;    ///< Sub-table buckets per bucket (power of 2)
    unsigned int max_linked_list_chain_length;  ///< Resize threshold
} hash_table_configuration;
```

**Fields:**
| Field | Type | Constraints | Purpose |
|-------|------|-----------|---------|
| `bucket_size` | `unsigned int` | Power of 2; > 0 | Number of top-level hash buckets. Larger values reduce lock contention. Typical: 16–256. |
| `is_concurrency_enabled` | `bool` | N/A | If `true`, all API calls are thread-safe with fine-grained locking. If `false`, only single-threaded access allowed. |
| `sub_hash_table_bucket_size` | `unsigned int` | Power of 2; > 0 | Number of buckets in each sub-hash-table. Larger values reduce collision chains. Typical: 8–64. |
| `max_linked_list_chain_length` | `unsigned int` | > 0 | Maximum collisions before sub-hash-table resize triggers. Typical: 3–8. Lower values = more frequent resizes but shorter chains. |

**Tuning Guidelines:**

| Scenario | Adjustment | Rationale |
|----------|-----------|----------|
| High thread contention | Increase `bucket_size` (e.g., 64→256) | More locks = less blocking |
| High collision rate or long chains | Increase `sub_hash_table_bucket_size` or decrease `max_linked_list_chain_length` | Distributes keys better within buckets |
| Memory-constrained | Reduce `bucket_size` and `sub_hash_table_bucket_size` | Trade throughput for memory |
| Predictable, small dataset | `bucket_size=8`, `sub_hash_table_bucket_size=4` | Minimal overhead |
| High-performance production | `bucket_size=64–256`, `sub_hash_table_bucket_size=16–32` | Balance concurrency and distribution |

**Pre-allocation Factor Tuning:**
- **0.0:** No pre-allocation; memory allocated on-demand (higher latency on first insertions)
- **0.3–0.5:** Recommended for balanced workloads; reduces allocation spikes
- **0.7–1.0:** For latency-sensitive workloads; higher memory footprint

---

### Internal Structures

**Note:** The structures below are internal and not directly used by application code. They are documented for reference only.

#### `composite_key_hash` (Internal)

Used internally by `set_key()`, `get_key()`, and `delete_key()` to hold both routing hashes for a single key. Not exposed in public API.


## Thread Safety & Concurrency

### Overview

The keystore is designed for safe concurrent access with **fine-grained, hierarchical locking** that minimizes contention. Concurrency is controlled via `is_concurrency_enabled` in the configuration; disable it for single-threaded workloads to avoid lock overhead.

### Concurrency Model

**Multi-Threaded Mode (`is_concurrency_enabled = true`):**

All API functions are fully thread-safe with a **hierarchical locking strategy** that minimizes contention:

| Lock Level | Scope | Type | Purpose | Held During |
|-----------|-------|------|---------|------------|
| 1 (Coarse) | Per hash bucket | `pthread_mutex_t` | Serializes resize state transitions | Resize checks only |
| 2 (Medium) | Per sub-hash-table | `pthread_rwlock_t` | Coordinates reads/writes within sub-table | Lookup, insert, delete operations |
| 3 (Fine) | Per data node | `pthread_mutex_t` | Serializes updates to the same key | Value replacement |
| 4 (Ultra-fine) | Resize buffer | `pthread_spinlock_t` | Allows concurrent writes during resize | Data movement to new sub-table |

**Strict Lock Ordering (prevents deadlock):**
1. Bucket mutex (coarse-grained state)
2. Sub-bucket RW-lock (medium-grained access)
3. Data-node mutex (fine-grained value update)

Threads always acquire locks in this order and never hold locks across function boundaries.

**Performance Characteristics:**
- **Reads:** Use RW-lock in read mode; multiple threads can read concurrently.
- **Writes:** Use RW-lock in write mode; only one writer per sub-hash-table, but different buckets proceed in parallel.
- **Same-key updates:** Serialized via per-node mutex; minimizes conflicts for most workloads.

**Single-Threaded Mode (`is_concurrency_enabled = false`):**

All locks are disabled. Only one thread may access the keystore at any time. Provides minimal overhead for single-threaded workloads.

### Thread Safety Guarantees

- **Atomicity:** Individual operations (`set_key()`, `get_key()`, `delete_key()`) are atomic. Reads see a consistent snapshot of the value at operation start.
- **Isolation:** Concurrent updates to different keys never block each other (fine-grained locking).
- **Memory ordering:** All locks use proper synchronization; no undefined behavior due to data races (when concurrency is enabled).
- **Deadlock freedom:** Lock ordering is strict; no circular dependencies.


---

## Return Codes

All API functions return an `int` status code:
- **Non-negative (0+):** Success or informational codes
- **Negative (< 0):** Error codes

**For a comprehensive list of all return codes and recovery strategies, see [ERROR_CODES.md](./ERROR_CODES.md).** That document is the canonical reference and is kept in sync with the implementation.

**Quick Reference — Common Returns:**

| Code | Macro | Meaning |
|------|-------|----------|
| 0 | `SUCCESS` | Operation succeeded |
| 10 | `SUCESS_ADDED_NEW_NODE` | New key inserted |
| 20 | `SUCESS_ADDED_NEW_NODE_RESZING_TRIGGERED` | New key inserted; resize triggered |
| -11 | `ERR_INVALID_ARGUMENT` | NULL/empty input |
| -12 | `ERR_INVALID_CONFIG` | Bad configuration |
| -20 | `ERR_MEMORY_ALLOCATION_FAILED` | Heap allocation failed |
| -80 | `ERR_DATA_NODE_NOT_FOUND` | Key does not exist |

**Best Practice:** Always check return codes immediately after each API call. Negative codes indicate failures that require recovery logic.


---

## Usage Examples

### Example 1: Quick Start

```c
#include "key_store.h"
#include "type_definitions/error_code_definitions.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    // Initialize keystore
    hash_table_configuration config = {
        .bucket_size = 16,
        .is_concurrency_enabled = true,
        .sub_hash_table_bucket_size = 8,
        .max_linked_list_chain_length = 4
    };

    if (initialise_key_store(config, 0.5) != SUCCESS) {
        fprintf(stderr, "Init failed\n");
        return 1;
    }

    // Insert
    key_value_pair kv = {
        .key = "user_id_1001",
        .value = (unsigned char*)"Alice",
        .value_size = 5
    };
    int status = set_key(&kv);
    printf("Insert: %d (10=new, 0=update)\n", status);

    // Retrieve
    key_value_pair result = {0};
    if (get_key("user_id_1001", &result) == SUCCESS) {
        printf("Value: %.*s\n", (int)result.value_size, (char*)result.value);
        free(result.key);
        free(result.value);
    }

    // Delete
    delete_key("user_id_1001");
    printf("Deleted\n");

    cleanup_key_store();
    return 0;
}
```

### Example 2: Error Handling Pattern

```c
#include "key_store.h"
#include "type_definitions/error_code_definitions.h"
#include <stdio.h>
#include <stdlib.h>

int robust_set_key(const char* key, unsigned char* value, size_t value_size) {
    // Validate inputs first
    if (!key || key[0] == '\0') {
        fprintf(stderr, "Error: key is NULL or empty\n");
        return ERR_INVALID_ARGUMENT;
    }
    if (!value || value_size == 0) {
        fprintf(stderr, "Error: value is NULL or size is 0\n");
        return ERR_INVALID_ARGUMENT;
    }

    key_value_pair kv = {.key = (char*)key, .value = value, .value_size = value_size};
    int status = set_key(&kv);

    // Handle errors
    if (status < 0) {
        switch (status) {
            case ERR_MEMORY_ALLOCATION_FAILED:
                fprintf(stderr, "Out of memory; cannot insert key\n");
                break;
            case ERR_HASH_COMPUTE_FAILED:
                fprintf(stderr, "Hash function failure\n");
                break;
            case ERR_DATA_NODE_CREATION_FAILED:
                fprintf(stderr, "Failed to allocate node\n");
                break;
            default:
                fprintf(stderr, "Insert failed: %d\n", status);
        }
    } else if (status == SUCESS_ADDED_NEW_NODE) {
        printf("New key inserted\n");
    } else if (status == SUCCESS) {
        printf("Key updated\n");
    }

    return status;
}
```

---

## Additional References

**Core Documentation:**
- [README.md](./README.md) — Project overview, building, and quick start
- [ERROR_CODES.md](./ERROR_CODES.md) — Canonical error code reference with recovery strategies
- [docs/DESIGN_DECISIONS.md](./docs/DESIGN_DECISIONS.md) — Trade-offs, architectural rationale, and alternatives

**Architecture & Design:**
- [docs/architecture/01-overview.md](docs/architecture/01-overview.md) — System design and component overview
- [docs/architecture/07-concurrency-model.md](docs/architecture/07-concurrency-model.md) — Locking strategy and synchronization details
- [docs/architecture/06-hashing-strategy.md](docs/architecture/06-hashing-strategy.md) — Hash function design and distribution

**Code & Examples:**
- [src/keystore/core/key_store.h](src/keystore/core/key_store.h) — API declarations with Doxygen comments
- [src/keystore/core/key_store.c](src/keystore/core/key_store.c) — Implementation
- [examples/main.c](examples/main.c) — Complete working example
- [tests/for_c/unit_tests/](tests/for_c/unit_tests/) — Unit tests demonstrating patterns

**Internal Modules (for maintainers):**
- [src/keystore/hash_table/](src/keystore/hash_table/) — Two-level hash table
- [src/keystore/data_structures/](src/keystore/data_structures/) — Lists, filters, nodes
- [src/keystore/utils/memory_manager.h](src/keystore/utils/memory_manager.h) — Memory pool
