# Keystore C API Documentation

## Overview

This document describes the public API for the keystore — a thread-safe, high-performance distributed key-value storage system written in C. The API is optimized for concurrent multi-threaded access with minimal contention through fine-grained per-bucket and per-node locking.

All public functions are declared in `src/keystore/core/key_store.h`. For architecture and design details, see [ARCHITECTURE.md](./docs/architecture.md).

## Quick Reference

| Function | Purpose |
|----------|---------|
| `initialise_key_store()` | Initialize keystore with configuration |
| `cleanup_key_store()` | Release all resources |
| `set_key()` | Insert or update a key-value pair |
| `get_key()` | Retrieve value for a key |
| `delete_key()` | Soft-delete a key (mark for deletion) |

---

## Initialization & Cleanup

### `initialise_key_store()`

```c
int initialise_key_store(hash_table_configuration config, double pre_memory_allocation_factor);
```

**Purpose:** Initialize the keystore with hash table configuration and optional memory pre-allocation.

**Parameters:**
| Name | Type | Constraints | Description |
|------|------|-----------|-------------|
| `config` | `hash_table_configuration` | See struct below | Configuration for hash table dimensions and concurrency |
| `pre_memory_allocation_factor` | `double` | 0.0 ≤ factor ≤ 1.0 | Fraction of max memory to pre-allocate for linked-list nodes; reduces allocation latency during insertions |

**Returns:**
- `SUCCESS` (0): Keystore initialized successfully
- `ERR_INVALID_ARGUMENT` (-11): `pre_memory_allocation_factor` out of valid range
- `ERR_INVALID_CONFIG` (-12): `bucket_size` or `sub_hash_table_bucket_size` are zero, not powers of 2, or `max_linked_list_chain_length` is zero
- `ERR_MEMORY_ALLOCATION_FAILED` (-20): Memory allocation failed
- `ERR_RESOURCE_INIT_FAILED` (-21): Thread synchronization resource initialization failed (e.g., mutex/RW-lock creation)

**Preconditions:**
- `config.bucket_size` must be a power of 2 and > 0
- `config.sub_hash_table_bucket_size` must be a power of 2 and > 0
- `config.max_linked_list_chain_length` must be > 0

**Idempotency:** Multiple calls to `initialise_key_store()` without an intervening `cleanup_key_store()` return `SUCCESS` without re-initializing.

**Thread Safety:** Only one thread should call this function; it is not internally synchronized.

---

### `cleanup_key_store()`

```c
int cleanup_key_store(void);
```

**Purpose:** Release all resources allocated by the keystore (hash table, memory pools, locks).

**Returns:**
- `SUCCESS` (0): Always returns success; best-effort cleanup

**Behavior:**
- Frees all hash buckets, sub-hash-tables, and linked-list nodes
- Destroys all mutexes and RW-locks
- Clears internal hash seeds
- After this call, the keystore is in an uninitialized state; `initialise_key_store()` must be called before further operations

**Thread Safety:** Only one thread should call this function; it is not internally synchronized. All other threads must cease keystore operations before calling `cleanup_key_store()`.


## Key Operations

### `set_key()`

```c
int set_key(key_value_pair* kv_pair);
```

**Purpose:** Insert a new key-value pair or update an existing one.

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `kv_pair` | `key_value_pair*` | Pointer to key-value pair struct; keystore makes internal copies of `key` and `value` |

**Returns:**
- `SUCCESS` (0): Key already existed; value updated in-place
- `SUCESS_ADDED_NEW_NODE` (10): New key-value pair inserted
- `SUCESS_ADDED_NEW_NODE_RESZING_TRIGGERED` (20): New key-value pair inserted and sub-hash-table resize triggered
- `ERR_INVALID_ARGUMENT` (-11): `kv_pair` is NULL, `kv_pair->key` is NULL or empty, `kv_pair->value` is NULL, or `value_size` is 0
- `ERR_HASH_COMPUTE_FAILED` (-40): Hash function returned sentinel value (UINT64_MAX)
- `ERR_INVALID_BUCKET_INDEX` (-41): Computed bucket index out of range
- `ERR_MEMORY_ALLOCATION_FAILED` (-20): Failed to allocate memory for data node or resize buffer
- `ERR_HASH_BUCKET_NOT_FOUND` (-51): Hash bucket not found in table
- `ERR_DATA_NODE_UPDATE_FAILED` (-81): Failed to update existing node
- `ERR_DATA_NODE_CREATION_FAILED` (-83): Failed to create new data node

**Behavior:**
- **Update case:** If key exists, the value is updated atomically. Old value is freed and replaced with new value.
- **Insert case:** If key does not exist, a new entry is created in the sub-hash-table's linked list at the collision chain.
- **Resize trigger:** If insertion causes `linked_list_chain_length > max_linked_list_chain_length`, a resize of the sub-hash-table is triggered asynchronously (see docs/architecture.md for details).

**Memory Ownership:** The caller retains ownership of input buffers; the keystore copies all data.

**Thread Safety:** Thread-safe when `is_concurrency_enabled = true`. Uses per-bucket mutex and per-node mutex for fine-grained locking.

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
| `kv_pair_out` | `key_value_pair*` | Output struct to receive the key-value pair on success |

**Returns:**
- `SUCCESS` (0): Key found; `kv_pair_out` populated with heap-allocated key and value
- `ERR_INVALID_ARGUMENT` (-11): `key` is NULL, empty, or `kv_pair_out` is NULL
- `ERR_HASH_COMPUTE_FAILED` (-40): Hash function returned sentinel value
- `ERR_INVALID_BUCKET_INDEX` (-41): Computed bucket index out of range
- `ERR_HASH_BUCKET_NOT_FOUND` (-51): Hash bucket not found in table
- `ERR_DATA_NODE_NOT_FOUND` (-80): Key not found in keystore

**Memory Ownership (on success):**
- Both `kv_pair_out->key` and `kv_pair_out->value` are heap-allocated by the keystore
- **Caller must `free()` both pointers** when no longer needed
- `kv_pair_out->value_size` contains the size of the value in bytes

**Example:**
```c
key_value_pair result = {0};
int status = get_key("greeting", &result);
if (status == SUCCESS) {
    printf("Value size: %zu\n", result.value_size);
    free(result.key);
    free(result.value);
}
```

**Thread Safety:** Thread-safe when `is_concurrency_enabled = true`. Uses per-bucket RW-lock in read mode and per-node mutex for consistent snapshots.

---

### `delete_key()`

```c
int delete_key(const char* key);
```

**Purpose:** Soft-delete a key from the keystore.

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

**Behavior (Soft-Delete):**
- The entry is marked with `is_deleted = true` but **not physically freed immediately**
- Subsequent `get_key()` calls on deleted keys will return `ERR_DATA_NODE_NOT_FOUND`
- Memory is reclaimed during:
  1. Sub-hash-table resize operations (entries are compacted)
  2. Explicit cleanup via `cleanup_key_store()`
- This design minimizes contention during deletion and allows concurrent readers to see a consistent snapshot

**Performance Note:** Soft-deletes are O(1) with minimal locking; hard-delete would require full node deallocation and is deferred to resize epochs.

**Thread Safety:** Thread-safe when `is_concurrency_enabled = true`. Uses per-bucket mutex to mark the node as deleted.

---


## Hashing

The keystore uses **MurmurHash3 (64-bit)** with two independent per-instance hash seeds to achieve both uniform distribution and unique routing across initialization epochs.

**Composite Hash:**
Each key produces a `composite_key_hash` containing two derived values:

| Field | Purpose | Derivation |
|-------|---------|-----------|
| `bucket_hash` | Route to top-level hash bucket | `MurmurHash3(key, seed_1)` % `bucket_size` |
| `sub_bucket_hash` | Route within sub-hash-table | `MurmurHash3(key, seed_2)` % `sub_bucket_size` |

**Seeds:**
- Both seeds are generated during `initialise_key_store()` using a time-based random source
- Seeds are kept distinct via XOR with a golden-ratio constant to ensure different hashes even if both calls resolve to the same nanosecond
- Seeds are stored as global state; the same instance always produces identical hashes for the same key

**Sentinel Value:**
The hash function returns `UINT64_MAX` if the input key is NULL. This is treated as an error condition (`ERR_HASH_COMPUTE_FAILED`).

**Collision Handling:**
Hash collisions in the sub-hash-table are resolved via linked-list chaining. When the collision chain exceeds `max_linked_list_chain_length`, a resize of the sub-hash-table is triggered asynchronously.

---

## Data Structures

### `key_value_pair`

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

### `hash_table_configuration`

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
- **High contention scenario:** Increase `bucket_size` to spread locks across more buckets.
- **High collision rate:** Increase `sub_hash_table_bucket_size` or decrease `max_linked_list_chain_length`.
- **Memory-constrained:** Reduce both sizes; accept higher collision rates.

---

### `composite_key_hash` (Internal)

```c
typedef struct {
    uint64_t bucket_hash;       ///< Hash for routing to top-level bucket
    uint64_t sub_bucket_hash;   ///< Hash for routing within sub-hash-table
} composite_key_hash;
```

**Note:** This structure is internal and used by `set_key()`, `get_key()`, and `delete_key()` internally. User code does not directly interact with it.


## Thread Safety

### Concurrency Model

The keystore supports both **single-threaded** and **multi-threaded** modes, controlled by `is_concurrency_enabled` in the configuration.

**Multi-Threaded Mode (`is_concurrency_enabled = true`):**

All API functions are fully thread-safe with a **hierarchical locking strategy** that minimizes contention:

| Lock Level | Scope | Type | Purpose | Held During |
|-----------|-------|------|---------|------------|
| 1 (Coarse) | Per hash bucket | `pthread_mutex_t` | Serializes resize state transitions | Resize checks only |
| 2 (Medium) | Per sub-hash-table | `pthread_rwlock_t` | Coordinates reads/writes within sub-table | Lookup, insert, delete operations |
| 3 (Fine) | Per data node | `pthread_mutex_t` | Serializes updates to the same key | Value replacement |
| 4 (Ultra-fine) | Resize buffer | `pthread_spinlock_t` | Allows concurrent writes during resize | Data movement to new sub-table |

**Lock Ordering (to prevent deadlock):**
1. Acquire bucket lock
2. Acquire sub-bucket RW-lock
3. Acquire data-node lock (only if modifying value)

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

## Success & Error Codes

All API functions return an `int` status code. By convention:
- **Non-negative values (0+):** Success or informational return codes
- **Negative values (< 0):** Error codes

### Success Codes

| Code | Macro | Meaning | Typical Function |
|------|-------|---------|------------------|
| 0 | `SUCCESS` | Operation succeeded; value updated or retrieved | All functions |
| 10 | `SUCESS_ADDED_NEW_NODE` | New key-value pair inserted | `set_key()` |
| 11 | `SUCCESS_ADDED_TO_PENDING_LIST` | Entry queued for resize (internal) | `set_key()` (during resize) |
| 20 | `SUCESS_ADDED_NEW_NODE_RESZING_TRIGGERED` | New pair inserted; resize triggered | `set_key()` |

### Error Codes

For a complete list of error codes with recovery strategies, see [ERROR_CODES.md](./ERROR_CODES.md).

**Common Errors by Category:**

| Category | Code | Macro | Typical Causes |
|----------|------|-------|----------------|
| **Argument Validation** | -11 | `ERR_INVALID_ARGUMENT` | NULL pointers, empty keys, zero value size |
| | -12 | `ERR_INVALID_CONFIG` | Non-power-of-2 bucket sizes, zero max chain length |
| **Memory** | -20 | `ERR_MEMORY_ALLOCATION_FAILED` | Heap exhausted, cannot allocate node |
| | -21 | `ERR_RESOURCE_INIT_FAILED` | Mutex/lock creation failed (OS limit?) |
| **Hashing** | -40 | `ERR_HASH_COMPUTE_FAILED` | Hash function returned sentinel (UINT64_MAX) |
| | -41 | `ERR_INVALID_BUCKET_INDEX` | Computed index out of range (should not occur) |
| **Lookup** | -80 | `ERR_DATA_NODE_NOT_FOUND` | Key does not exist or is deleted |
| **Update** | -81 | `ERR_DATA_NODE_UPDATE_FAILED` | Failed to update existing node value |
| | -83 | `ERR_DATA_NODE_CREATION_FAILED` | Failed to create new node (memory issue?) |

**Debugging Tip:** Check return codes immediately after each API call. Negative values indicate failure; log the specific error code to diagnose the root cause.


---

## Usage Examples

### Example 1: Basic Insert, Retrieve, and Delete

```c
#include "key_store.h"
#include "type_definitions/error_code_definitions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    // Initialize with default configuration
    hash_table_configuration config = {
        .bucket_size = 16,                      // 16 top-level buckets
        .is_concurrency_enabled = true,         // Thread-safe mode
        .sub_hash_table_bucket_size = 8,        // 8 sub-buckets per bucket
        .max_linked_list_chain_length = 4       // Resize at 4+ collisions
    };

    int status = initialise_key_store(config, 0.5);  // Pre-allocate 50% of memory
    if (status != SUCCESS) {
        fprintf(stderr, "Initialization failed: %d\n", status);
        return 1;
    }

    // Set a key-value pair
    key_value_pair kv = {
        .key = "user_id_1001",
        .value = (unsigned char*)"Alice",
        .value_size = 5
    };
    status = set_key(&kv);
    if (status < 0) {
        fprintf(stderr, "Set failed: %d\n", status);
        cleanup_key_store();
        return 1;
    }
    printf("Inserted: %s -> %s\n", kv.key, (char*)kv.value);

    // Retrieve the value
    key_value_pair result = {0};
    status = get_key("user_id_1001", &result);
    if (status == SUCCESS) {
        printf("Retrieved: %s -> %.*s\n", result.key, (int)result.value_size, (char*)result.value);
        free(result.key);
        free(result.value);
    } else if (status == ERR_DATA_NODE_NOT_FOUND) {
        printf("Key not found\n");
    }

    // Update the value
    key_value_pair updated = {
        .key = "user_id_1001",
        .value = (unsigned char*)"Alice Smith",
        .value_size = 11
    };
    status = set_key(&updated);
    printf("Update result: %d\n", status);  // 0 = update, 10 = insert

    // Soft-delete the key
    status = delete_key("user_id_1001");
    if (status == SUCCESS) {
        printf("Deleted: user_id_1001\n");
    }

    // Verify it's gone
    result = (key_value_pair){0};
    status = get_key("user_id_1001", &result);
    if (status == ERR_DATA_NODE_NOT_FOUND) {
        printf("Key confirmed deleted\n");
    }

    cleanup_key_store();
    return 0;
}
```

### Example 2: Detecting Insert vs. Update

```c
#include "key_store.h"
#include "type_definitions/sucess_code_definitions.h"

void demo_insert_vs_update(void) {
    hash_table_configuration config = {.bucket_size = 8, .is_concurrency_enabled = false, 
                                        .sub_hash_table_bucket_size = 4, .max_linked_list_chain_length = 3};
    initialise_key_store(config, 0.3);

    // First insert
    key_value_pair kv = {.key = "config_setting", .value = (unsigned char*)"value1", .value_size = 6};
    int result1 = set_key(&kv);
    if (result1 == SUCESS_ADDED_NEW_NODE) {
        printf("New key inserted\n");
    }

    // Update same key
    kv.value = (unsigned char*)"value2";
    kv.value_size = 6;
    int result2 = set_key(&kv);
    if (result2 == SUCCESS) {
        printf("Existing key updated\n");
    }

    // Insert with resize trigger
    kv.key = "another_key";
    int result3 = set_key(&kv);
    if (result3 == SUCESS_ADDED_NEW_NODE_RESZING_TRIGGERED) {
        printf("Resize triggered after insert\n");
    }

    cleanup_key_store();
}
```

### Example 3: Error Handling

```c
#include "key_store.h"
#include "type_definitions/error_code_definitions.h"
#include <stdio.h>

int safe_set_key(const char* key, unsigned char* value, size_t value_size) {
    // Validate inputs
    if (key == NULL || key[0] == '\0') {
        fprintf(stderr, "Error: key is NULL or empty\n");
        return ERR_INVALID_ARGUMENT;
    }
    if (value == NULL || value_size == 0) {
        fprintf(stderr, "Error: value is NULL or size is 0\n");
        return ERR_INVALID_ARGUMENT;
    }

    key_value_pair kv = {.key = (char*)key, .value = value, .value_size = value_size};
    int status = set_key(&kv);

    if (status < 0) {
        switch (status) {
            case ERR_MEMORY_ALLOCATION_FAILED:
                fprintf(stderr, "Out of memory\n");
                break;
            case ERR_HASH_COMPUTE_FAILED:
                fprintf(stderr, "Hash computation error\n");
                break;
            case ERR_DATA_NODE_CREATION_FAILED:
                fprintf(stderr, "Failed to create node\n");
                break;
            default:
                fprintf(stderr, "Unknown error: %d\n", status);
        }
        return status;
    }

    return SUCCESS;
}
```

---

## See Also

**Header & Implementation:**
- [key_store.h](src/keystore/core/key_store.h) — Main API declarations with Doxygen comments
- [key_store.c](src/keystore/core/key_store.c) — Implementation details and private functions

**Related Documentation:**
- [README.md](./README.md) — Project overview, building, and quick start
- [ARCHITECTURE.md](./docs/architecture.md) — System design, subsystems, and data flow
- [ERROR_CODES.md](./ERROR_CODES.md) — Complete error code reference and recovery strategies
- [docs/DESIGN_DECISIONS.md](./docs/DESIGN_DECISIONS.md) — Key design trade-offs and rationale

**Examples:**
- [examples/main.c](examples/main.c) — Full working example with multiple operations
- [tests/for_c/unit_tests/](tests/for_c/unit_tests/) — Unit tests demonstrating API usage patterns

**Internal Modules:**
- [hash_table/](src/keystore/hash_table/) — Two-level hash table implementation
- [data_structures/](src/keystore/data_structures/) — Linked lists, bloom filters, and nodes
- [utils/memory_manager.h](src/keystore/utils/memory_manager.h) — Memory pool management
