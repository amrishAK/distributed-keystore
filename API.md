# Keystore C API Documentation

## Overview
This document describes the public API for the keystore. The API is designed for thread-safe, high-performance key-value storage and retrieval in C.

---

## Initialization & Cleanup


### int initialise_key_store(unsigned int bucket_size, double pre_memory_allocation_factor, bool is_concurrency_enabled)
Initializes the keystore with the specified number of buckets, memory pool pre-allocation factor, and concurrency mode.
- **bucket_size**: Number of hash buckets (must be a power of two).
- **pre_memory_allocation_factor**: Fraction of memory to pre-allocate for nodes (0 < factor ≤ 1).
- **is_concurrency_enabled**: Enable thread safety (true/false).
- **Returns**: 0 on success, or a negative error code on failure (see Error Codes section below).
    - Common errors: -10 (memory allocation), -11 (resource init), -21 (invalid config)


### int cleanup_key_store(void)
Cleans up all resources used by the keystore.
- **Returns**: 0 on success.


## Key Operations


### int set_key(const char *key, key_store_value *value)
Sets or updates a key-value pair in the keystore.
- **key**: Null-terminated string key.
- **value**: Pointer to a `key_store_value` struct containing data and size. (The caller is responsible for managing the memory of the data pointer)
- **Returns**: 0 on success, or a negative error code on failure (see Error Codes section below).
    - Common errors: -20 (invalid argument), -70 (hash error), -71 (bucket index error), -10 (memory allocation), -40 (bucket not found), -41 (data node not found), -46 (edit failure), -42 (duplicate key)



### int get_key(const char *key, key_store_value *value_out)
Retrieves the value for a given key.
- **key**: Null-terminated string key.
- **value_out**: Pointer to a `key_store_value` struct to receive the data. (The caller is responsible for managing the memory of the data pointer)
- **Returns**: 0 on success, or a negative error code on failure (see Error Codes section below).
    - Common errors: -20 (invalid argument), -70 (hash error), -71 (bucket index error), -40 (bucket not found), -41 (data node not found)


### int delete_key(const char *key)
Deletes a key-value pair from the keystore.
- **key**: Null-terminated string key.
- **Returns**: 0 on success, or a negative error code on failure (see Error Codes section below).
    - Common errors: -20 (invalid argument), -70 (hash error), -71 (bucket index error), -40 (bucket not found), -41 (data node not found)

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
- If `is_concurrency_enabled = true` during initialization, all API functions are thread-safe and use per-bucket read-write locks for high concurrency.
- If `is_concurrency_enabled = false`, the keystore runs in single-threaded mode and is **not thread-safe**. Only one thread should access the keystore at a time in this mode.



## Error Codes
All API functions return 0 on success or a negative error code on failure. See [ERROR_CODES.md](./ERROR_CODES.md) for the full list and details.


## Example Usage

The following example demonstrates a typical usage scenario for the distributed keystore API:

1. **Initialization:**
   - `initialise_key_store(1024, 0.5, true);` initializes the keystore with 1024 buckets, pre-allocates 50% of memory for nodes, and enables concurrency (thread safety).
2. **Setting a Key:**
   - `set_key("greeting", &value);` stores the key "greeting" with the value "hello" in the keystore.
3. **Getting a Key:**
   - `get_key("greeting", &out);` retrieves the value for the key "greeting" and stores it in `out`.
4. **Deleting a Key:**
   - `delete_key("greeting");` removes the key-value pair for "greeting" from the keystore.
5. **Cleanup:**
   - `cleanup_key_store();` frees all resources used by the keystore.

Error handling is demonstrated by checking return values and handling errors according to the error codes. This scenario covers the basic operations: initialize, set, get, delete, and cleanup, showing how to use the API in a real application.

```c
#include "key_store.h"

int main() {
    initialise_key_store(1024, 0.5, true);
    key_store_value value = { (unsigned char*)"hello", 5 };
    int set_result = set_key("greeting", &value);
    if (set_result != 0) {
        // Handle error (see ERROR_CODES.md for details)
    }
    key_store_value out = {0};
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
