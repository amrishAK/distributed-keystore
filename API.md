# Distributed Keystore C API Documentation

## Overview
This document describes the public API for the distributed keystore. The API is designed for thread-safe, high-performance key-value storage and retrieval in C.

---

## Initialization & Cleanup

### int initialise_key_store(unsigned int bucket_size, double pre_memory_allocation_factor, bool is_concurrency_enabled)
Initializes the keystore with the specified number of buckets, memory pool pre-allocation factor, and concurrency mode.
- **bucket_size**: Number of hash buckets (must be a power of two).
- **pre_memory_allocation_factor**: Fraction of memory to pre-allocate for nodes (0 < factor ≤ 1).
- **is_concurrency_enabled**: Enable thread safety (true/false).
- **Returns**: 0 on success, -1 on error.

### int cleanup_key_store(void)
Cleans up all resources used by the keystore.
- **Returns**: 0 on success.


## Key Operations

### int set_key(const char *key, key_store_value *value)
Sets or updates a key-value pair in the keystore.
- **key**: Null-terminated string key.
- **value**: Pointer to a `key_store_value` struct containing data and size. (The caller is responsible for managing the memory of the data pointer)
- **Returns**: 0 on success, -1 on error.


### int get_key(const char *key, key_store_value *value_out)
Retrieves the value for a given key.

- **key**: Null-terminated string key.
- **value_out**: Pointer to a `key_store_value` struct to receive the data. (The caller is responsible for managing the memory of the data pointer)
- **Returns**: 0 on success, -1 if not found or error.

### int delete_key(const char *key)
Deletes a key-value pair from the keystore.
- **key**: Null-terminated string key.
- **Returns**: 0 on success, -1 if not found or error.

---


## Data Structures

### key_store_value
```c
typedef struct {
    unsigned char *data;
    size_t data_size;
} key_store_value;
```
- **data**: Pointer to binary or string data.
- **data_size**: Size of the data in bytes.


## Thread Safety
- All API functions are thread-safe if the keystore is initialized with `is_concurrency_enabled = true`.
- Per-bucket read-write locks ensure high concurrency for set/get/delete operations.


## Error Codes
- **0**: Success
- **-1**: General error (invalid input, allocation failure, not found)

> **Note** Specific error codes will be implemented and documented later
---

## Example Usage
```c
#include "key_store.h"

int main() {
    initialise_key_store(1024, 0.5, true);
    key_store_value value = { (unsigned char*)"hello", 5 };
    set_key("greeting", &value);
    key_store_value out = {0};
    if (get_key("greeting", &out) == 0) {
        // Use out.data and out.data_size
    }
    delete_key("greeting");
    cleanup_key_store();
    return 0;
}
```

## See Also
- `src/core/key_store.h` for full API declarations
- `examples/main.c` for usage examples
