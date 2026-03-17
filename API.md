# Keystore C API Documentation

## Overview
This document describes the public API for the keystore. The API is designed for thread-safe, high-performance key-value storage and retrieval in C.

All public functions are declared in `src/keystore/core/key_store.h`.

---

## Initialization & Cleanup

### int initialise_key_store(hash_table_configuration config, double pre_memory_allocation_factor)
Initializes the keystore with the specified hash table configuration and memory pool pre-allocation factor.
- **config**: Hash table configuration struct (see [hash_table_configuration](#hash_table_configuration) below).
  - `bucket_size` and `sub_hash_table_bucket_size` must be **powers of 2** and non-zero.
  - `max_linked_list_chain_length` must be non-zero.
- **pre_memory_allocation_factor**: Fraction of memory to pre-allocate for linked-list nodes (0.0 ≤ factor ≤ 1.0).
- **Returns**: `SUCCESS` (0) on success, or a negative error code on failure.
    - Common errors: `ERR_INVALID_ARGUMENT` (-11), `ERR_INVALID_CONFIG` (-12), `ERR_MEMORY_ALLOCATION_FAILED` (-20), `ERR_RESOURCE_INIT_FAILED` (-21)

### int cleanup_key_store(void)
Cleans up and releases all resources used by the keystore (hash table, memory pool, hash seed).
- **Returns**: `SUCCESS` (0) on success.


## Key Operations

### int set_key(key_value_pair* value)
Sets or updates a key-value pair in the keystore. If the key already exists, its value is updated in-place. If the key does not exist, a new entry is created.
- **value**: Pointer to a `key_value_pair` struct containing the key, value, and value size. The keystore copies the data internally — the caller retains ownership of the input buffers.
- **Validation**: Returns `ERR_INVALID_ARGUMENT` (-11) if `value` is NULL, `value->key` is NULL or empty, `value->value` is NULL, or `value->value_size` is 0.
- **Returns**: `SUCCESS` (0) on update, `SUCESS_ADDED_NEW_NODE` (10) on insert, or a negative error code on failure.
    - Common errors: `ERR_INVALID_ARGUMENT` (-11), `ERR_HASH_COMPUTE_FAILED` (-40), `ERR_INVALID_BUCKET_INDEX` (-41), `ERR_MEMORY_ALLOCATION_FAILED` (-20), `ERR_HASH_BUCKET_NOT_FOUND` (-51), `ERR_DATA_NODE_UPDATE_FAILED` (-81), `ERR_DATA_NODE_CREATION_FAILED` (-83)

### int get_key(const char *key, key_value_pair* value_out)
Retrieves the value for a given key.
- **key**: Null-terminated string key (must not be NULL or empty).
- **value_out**: Pointer to a `key_value_pair` struct to receive the result.
- **Returns**: `SUCCESS` (0) on success, or a negative error code on failure.
    - Common errors: `ERR_INVALID_ARGUMENT` (-11), `ERR_HASH_COMPUTE_FAILED` (-40), `ERR_INVALID_BUCKET_INDEX` (-41), `ERR_HASH_BUCKET_NOT_FOUND` (-51), `ERR_DATA_NODE_NOT_FOUND` (-80)
- **Memory ownership**: On success, both `value_out->key` and `value_out->value` are heap-allocated by the keystore. **The caller must `free()` both** when done.

### int delete_key(const char *key)
Soft-deletes a key-value pair from the keystore. The entry is marked as deleted (`is_deleted = true`) but not physically freed until a resize or explicit cleanup occurs.
- **key**: Null-terminated string key (must not be NULL or empty).
- **Returns**: `SUCCESS` (0) on success, or a negative error code on failure.
    - Common errors: `ERR_INVALID_ARGUMENT` (-11), `ERR_HASH_COMPUTE_FAILED` (-40), `ERR_INVALID_BUCKET_INDEX` (-41), `ERR_HASH_BUCKET_NOT_FOUND` (-51), `ERR_DATA_NODE_NOT_FOUND` (-80)

---


## Data Structures

### key_value_pair
```c
typedef struct {
    char* key;
    unsigned char* value;
    size_t value_size;
} key_value_pair;
```
- **key**: Pointer to a null-terminated string representing the key.
- **value**: Pointer to the byte array representing the value.
- **value_size**: Size of the value in bytes.

### hash_table_configuration
```c
typedef struct {
    unsigned int bucket_size;
    bool is_concurrency_enabled;
    unsigned int sub_hash_table_bucket_size;
    unsigned int max_linked_list_chain_length;
} hash_table_configuration;
```
- **bucket_size**: Number of top-level hash buckets. Must be a power of 2.
- **is_concurrency_enabled**: Set to `true` for thread-safe operation (enables per-bucket RW-locks and per-node mutexes).
- **sub_hash_table_bucket_size**: Number of buckets in each sub-hash-table. Must be a power of 2.
- **max_linked_list_chain_length**: Maximum collision chain length before a sub-hash-table resize is triggered.


## Thread Safety
- If `is_concurrency_enabled = true` during initialization, all API functions are thread-safe. The locking hierarchy is:
  - `pthread_mutex_t` per hash bucket (resize state transitions)
  - `pthread_rwlock_t` per sub-hash bucket (read-shared / write-exclusive)
  - `pthread_mutex_t` per data node (serialises value updates to the same key)
  - `pthread_spinlock_t` on the chase buffer (concurrent resize writes)
- If `is_concurrency_enabled = false`, the keystore runs in single-threaded mode and is **not thread-safe**. Only one thread should access the keystore at a time in this mode.


## Error Codes
All API functions return 0 on success or a negative error code. See [ERROR_CODES.md](./ERROR_CODES.md) for the full list. Error codes are defined as named macros in `error_code_definitions.h`.

### Success Codes
| Code | Macro | Meaning |
|------|-------|---------|
| 0 | `SUCCESS` | Operation completed / value updated |
| 10 | `SUCESS_ADDED_NEW_NODE` | New key-value pair inserted |
| 11 | `SUCCESS_ADDED_TO_PENDING_LIST` | Added to resize pending buffer (internal) |
| 20 | `SUCESS_ADDED_NEW_NODE_RESZING_TRIGGERED` | New node inserted, sub-hash-table resize triggered |


## Example Usage

```c
#include "key_store.h"
#include "type_definitions/error_code_definitions.h"

int main() {
    hash_table_configuration config = {
        .bucket_size = 16,
        .is_concurrency_enabled = true,
        .sub_hash_table_bucket_size = 8,
        .max_linked_list_chain_length = 4
    };

    int init = initialise_key_store(config, 0.5);
    if (init != SUCCESS) {
        return init;
    }

    // Set a key
    key_value_pair kv = { .key = "greeting", .value = (unsigned char*)"hello", .value_size = 5 };
    int set_result = set_key(&kv);
    if (set_result < 0) {
        cleanup_key_store();
        return set_result;
    }

    // Get a key
    key_value_pair out = {0};
    int get_result = get_key("greeting", &out);
    if (get_result == SUCCESS) {
        // Use out.value (out.value_size bytes) and out.key
        free(out.value);  // caller must free
        free(out.key);    // caller must free
    }

    // Delete a key (soft-delete)
    int del_result = delete_key("greeting");
    if (del_result < 0) {
        // Handle error
    }

    cleanup_key_store();
    return 0;
}
```

## See Also
- `src/keystore/core/key_store.h` for full API declarations
- `examples/main.c` for usage examples
- [ERROR_CODES.md](./ERROR_CODES.md) for all error codes
- [README.md](./README.md) for architecture and concurrency model details
