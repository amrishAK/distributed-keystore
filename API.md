# Keystore C API Documentation

## Overview
This document describes the public API for the keystore. The API is designed for thread-safe, high-performance key-value storage and retrieval in C.

---

## Initialization & Cleanup

### int initialise_key_store(hash_table_configuration config, double pre_memory_allocation_factor)
Initializes the keystore with the specified hash table configuration and memory pool pre-allocation factor.
- **config**: Hash table configuration struct (see `hash_bucket_type_definition.h`).
- **pre_memory_allocation_factor**: Fraction of memory to pre-allocate for nodes (0 < factor ≤ 1).
- **Returns**: 0 on success, or a negative error code on failure (see Error Codes section below).
    - Common errors: ERR_MEMORY_ALLOCATION_FAILED, ERR_RESOURCE_INIT_FAILED, ERR_INVALID_CONFIG

### int cleanup_key_store(void)
Cleans up all resources used by the keystore.
- **Returns**: 0 on success.


## Key Operations

### int set_key(key_value_pair* value)
Sets or updates a key-value pair in the keystore.
- **value**: Pointer to a `key_value_pair` struct containing the key, data, and size. (The caller is responsible for managing the memory of the data pointer)
- **Returns**: 0 on success, or a negative error code on failure (see Error Codes section below).
    - Common errors: ERR_INVALID_ARGUMENT, ERR_HASH_COMPUTE_FAILED, ERR_INVALID_BUCKET_INDEX, ERR_MEMORY_ALLOCATION_FAILED, ERR_HASH_BUCKET_NOT_FOUND, ERR_DATA_NODE_NOT_FOUND, ERR_DATA_NODE_UPDATE_FAILED, ERR_DATA_NODE_CREATION_FAILED

### int get_key(const char *key, key_value_pair* value_out)
Retrieves the value for a given key.
- **key**: Null-terminated string key.
- **value_out**: Pointer to a `key_value_pair` struct to receive the data. (The caller is responsible for managing the memory of the data pointer)
- **Returns**: 0 on success, or a negative error code on failure (see Error Codes section below).
    - Common errors: ERR_INVALID_ARGUMENT, ERR_HASH_COMPUTE_FAILED, ERR_INVALID_BUCKET_INDEX, ERR_HASH_BUCKET_NOT_FOUND, ERR_DATA_NODE_NOT_FOUND

### int delete_key(const char *key)
Deletes a key-value pair from the keystore.
- **key**: Null-terminated string key.
- **Returns**: 0 on success, or a negative error code on failure (see Error Codes section below).
    - Common errors: ERR_INVALID_ARGUMENT, ERR_HASH_COMPUTE_FAILED, ERR_INVALID_BUCKET_INDEX, ERR_HASH_BUCKET_NOT_FOUND, ERR_DATA_NODE_NOT_FOUND

---


## Data Structures

### key_value_pair
```c
typedef struct {
    const char *key;
    unsigned char *data;
    size_t data_size;
} key_value_pair;
```
- **key**: Null-terminated string key.
- **data**: Pointer to binary or string data.
- **data_size**: Size of the data in bytes.


## Thread Safety
- If `is_concurrency_enabled = true` during initialization, all API functions are thread-safe and use per-bucket read-write locks for high concurrency.
- If `is_concurrency_enabled = false`, the keystore runs in single-threaded mode and is **not thread-safe**. Only one thread should access the keystore at a time in this mode.


## Error Codes
All API functions return 0 on success or a negative error code (see [ERROR_CODES.md](./ERROR_CODES.md) for the full list and details). Error codes are defined as named macros (e.g., ERR_INVALID_ARGUMENT) for clarity and maintainability.


## Example Usage

The following example demonstrates a typical usage scenario for the distributed keystore API:

1. **Initialization:**
   - `initialise_key_store(config, 0.5);` initializes the keystore with the given configuration and pre-allocates 50% of memory for nodes.
2. **Setting a Key:**
   - `set_key(&value);` stores the key-value pair in the keystore.
3. **Getting a Key:**
   - `get_key("greeting", &out);` retrieves the value for the key "greeting" and stores it in `out`.
4. **Deleting a Key:**
   - `delete_key("greeting");` removes the key-value pair for "greeting" from the keystore.
5. **Cleanup:**
   - `cleanup_key_store();` frees all resources used by the keystore.

Error handling is demonstrated by checking return values and handling errors according to the error codes. This scenario covers the basic operations: initialize, set, get, delete, and cleanup, showing how to use the API in a real application.

```c
#include "key_store.h"
#include "error_code_definitions.h"

int main() {
    hash_table_configuration config = {/* ... fill config ... */};
    if (initialise_key_store(config, 0.5) != 0) {
        // Handle initialization error
        return ERR_FAILURE;
    }
    key_value_pair value = { "greeting", (unsigned char*)"hello", 5 };
    int set_result = set_key(&value);
    if (set_result != 0) {
        // Handle error (see ERROR_CODES.md for details)
        return set_result;
    }
    key_value_pair out = {0};
    int get_result = get_key("greeting", &out);
    if (get_result == 0) {
        // Use out.data and out.data_size
    } else {
        // Handle error (see ERROR_CODES.md for details)
    }
    int del_result = delete_key("greeting");
    if (del_result != 0) {
        // Handle error (see ERROR_CODES.md for details)
    }
    cleanup_key_store();
    return 0;
}
```

## See Also
- `src/core/key_store.h` for full API declarations
- `examples/main.c` for usage examples
