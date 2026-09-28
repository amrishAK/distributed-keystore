

# Error Codes Reference

This document lists all custom error and return codes used in the KeyStore project. Use these codes for consistent error handling, debugging, and documentation. Always return `0` for success, and use the most specific negative code for errors.

---

## Error Code Table


### General Success/Failure
| Code  | Name         | Meaning/Description                  |
|-------|--------------|--------------------------------------|
| 0     | SUCCESS      | Operation completed successfully     |
| -1    | ERR_FAILURE  | General/unspecified failure          |

### Extended Success Codes — Insertion
| Code  | Name                                     | Meaning/Description                                      | When Used |
|-------|------------------------------------------|----------------------------------------------------------|-----------|
| 10    | SUCESS_ADDED_NEW_NODE                    | New key-value node inserted successfully                 | Direct insert into bucket |
| 11    | SUCCESS_ADDED_TO_PENDING_LIST            | Entry added to resize pending buffer (internal)          | Pending resize queue |
| 20    | SUCESS_ADDED_NEW_NODE_RESZING_TRIGGERED  | New node inserted and sub-hash-table resize triggered    | Insert triggers resize |

### Extended Success Codes — Bloom Filter Checks
| Code  | Name                                     | Meaning/Description                                      | When Used |
|-------|------------------------------------------|----------------------------------------------------------|-----------|
| 30    | BLOOM_FILTER_DISABLED                    | Bloom filter check skipped (feature disabled)            | Bloom filter off |
| 31    | BLOOM_FILTER_CHECK_KEY_MAY_EXIST         | Key may exist in data structure (bloom filter positive)  | Proceed with lookup |
| 32    | BLOOM_FILTER_CHECK_KEY_NOT_EXIST         | Key definitely does not exist (bloom filter negative)    | Short-circuit lookup |

### Argument/Validation Errors
| Code  | Name                   | Meaning/Description                       | Recovery |
|-------|------------------------|-------------------------------------------|----------|
| -11   | ERR_INVALID_ARGUMENT   | Invalid argument (e.g., NULL pointer)     | Validate all inputs before calling; check for NULL pointers |
| -12   | ERR_INVALID_CONFIG     | Invalid configuration or parameter        | Verify config values (bucket size > 0, valid sizes) |


### Memory/Resource Management
| Code  | Name                        | Meaning/Description                                 | Recovery |
|-------|-----------------------------|-----------------------------------------------------|----------|
| -20   | ERR_MEMORY_ALLOCATION_FAILED| Memory allocation failed (malloc/calloc returned 0) | Check available memory; reduce allocation size; retry with backoff |
| -21   | ERR_RESOURCE_INIT_FAILED    | Resource initialization failed (e.g., mutex init)   | Verify system resources available; check platform support |
| -22   | ERR_RESOURCE_CLEANUP_FAILED | Resource cleanup failed (e.g., mutex destroy)       | Log error; continue cleanup; investigate resource state |


### Concurrency/Locking
| Code  | Name                         | Meaning/Description                | Recovery |
|-------|------------------------------|------------------------------------|----------|
| -30   | ERR_RW_LOCK_ACQUIRE_FAILED   | RW lock acquire failed             | Check lock state; verify no deadlock; retry with timeout |
| -31   | ERR_RW_LOCK_RELEASE_FAILED   | RW lock release failed             | Log error; investigate lock ownership; enable debug tracing |
| -32   | ERR_MUTEX_LOCK_ACQUIRE_FAILED| Mutex lock acquire failed          | Check lock availability; verify lock initialization |
| -33   | ERR_MUTEX_LOCK_RELEASE_FAILED| Mutex lock release failed          | Log error; verify caller owns lock; check for corruption |
| -34   | ERR_GENERIC_LOCK_ACQUIRE_FAILED | Custom lock acquire failed        | Review lock implementation; check for lock inversions |
| -35   | ERR_GENERIC_LOCK_RELEASE_FAILED | Custom lock release failed        | Verify lock ownership; check lock state consistency |


### Hash/Indexing Errors
| Code  | Name                    | Meaning/Description         | Recovery |
|-------|------------------------|---------------------------|-----------|
| -40   | ERR_HASH_COMPUTE_FAILED | Hash computation failure    | Verify hash function; check for integer overflow; enable assertions |
| -41   | ERR_INVALID_BUCKET_INDEX| Invalid bucket index        | Verify table size and computed index; check for boundary errors |
| -42   | ERR_INVALID_SUB_BUCKET_INDEX | Invalid sub-bucket index | Verify sub-table size; check index calculation logic |


### Hash Table/Bucket Operation Errors
| Code  | Name                          | Meaning/Description           | Recovery |
|-------|-------------------------------|-------------------------------|----------|
| -50   | ERR_HASH_TABLE_NOT_INITIALIZED| Hash table not initialized    | Call initialization function before operations |
| -51   | ERR_HASH_BUCKET_NOT_FOUND     | Hash bucket not found         | Verify hash computation; check table size |
| -52   | ERR_UNSUPPORTED_HASH_BUCKET_OP| Unsupported hash bucket op    | Use correct operation for bucket type |
| -53   | ERR_HASH_BUCKET_FULL          | Hash bucket full              | Trigger resize; increase bucket capacity |
| -54   | ERR_HASH_BUCKET_NOT_INITIALIZED| Hash bucket not initialized  | Initialize bucket before use |


### Sub Hash Table Operation Errors
| Code  | Name                               | Meaning/Description           | Recovery |
|-------|------------------------------------|---------------------------------|----------|
| -60   | ERR_SUB_HASH_TABLE_NOT_INITIALIZED | Sub-hash table not initialized | Initialize sub-table; verify parent table setup |
| -61   | ERR_SUB_HASH_BUCKET_NOT_FOUND      | Sub-hash bucket not found     | Verify secondary hash computation; check sub-table state |
| -62   | ERR_UNSUPPORTED_SUB_HASH_TABLE_OP  | Unsupported sub-hash table op | Review operation type; use correct API |
| -63   | ERR_SUB_HASH_TABLE_RESIZE_FAILED   | Sub-hash table resize failed  | Check memory availability; verify resize logic |
| -64   | ERR_SUB_HASH_BUCKET_NOT_INITIALIZED| Sub-hash bucket not initialized | Initialize bucket; verify parent table state |

### Linked List Operation Errors
| Code  | Name                             | Meaning/Description           | Recovery |
|-------|----------------------------------|-------------------------------|----------|
| -70   | ERR_LINKED_LIST_NODE_NOT_FOUND   | Linked list node not found    | Verify key exists; check list traversal logic |
| -71   | ERR_LINKED_LIST_NODE_CREATION_FAILED | Linked list node creation failed | Check memory availability; verify node factory |
| -72   | ERR_UNSUPPORTED_LINKED_LIST_OP   | Unsupported linked list op    | Use correct list operation; check API |

### Data Node Operation Errors
| Code  | Name                        | Meaning/Description           | Recovery |
|-------|-----------------------------|-------------------------------|----------|
| -80   | ERR_DATA_NODE_NOT_FOUND     | Data node not found           | Verify key exists in table; check traversal |
| -81   | ERR_DATA_NODE_UPDATE_FAILED | Data node update failed       | Check lock state; verify value size; retry |
| -82   | ERR_UNSUPPORTED_DATA_NODE_OP| Unsupported data node op      | Use correct node operation; verify API |
| -83   | ERR_DATA_NODE_CREATION_FAILED | Data node creation failed    | Check memory; verify value size valid |

### Pool/Manager Specific Errors
| Code  | Name                      | Meaning/Description           | Recovery |
|-------|---------------------------|-------------------------------|----------|
| -90   | ERR_POOL_NOT_INITIALIZED  | Memory pool not initialized   | Initialize memory pool before allocation |
| -91   | ERR_POOL_EXHAUSTED        | Memory pool exhausted         | Increase pool size; free unused blocks; retry |
| -92   | ERR_POOL_CORRUPTION_DETECTED | Memory pool corruption detected | Verify pool integrity; enable memory sanitizer; inspect allocations |

### Platform/Threading Errors
| Code   | Name                        | Meaning/Description         | Recovery |
|--------|-----------------------------|-----------------------------|----------|
| -100   | ERR_UNSUPPORTED_PLATFORM    | Platform not supported      | Check platform compatibility; enable platform-specific code |
| -101   | ERR_THREAD_CREATION_FAILED  | Thread creation failed      | Verify thread limit; check system resources; retry |
| -102   | ERR_MAX_RETRY_EXCEEDED      | Max retry attempts exceeded | Increase retry limit or backoff threshold; investigate root cause |

---

## Error Handling by Operation

### SET Operations
Possible codes: `SUCCESS` (0), `SUCESS_ADDED_NEW_NODE` (10), `SUCESS_ADDED_NEW_NODE_RESZING_TRIGGERED` (20), errors: -11, -12, -20, -21, -30, -40, -41, -50, -51, -83

**Recovery Strategy:** Retry memory errors with backoff; check initialization if -50/-51 returned.

### GET Operations
Possible codes: `SUCCESS` (0), errors: -11, -40, -41, -50, -51, -80

**Recovery Strategy:** If -80 returned, key does not exist or is deleted; check if key should have been added first.

### DELETE Operations
Possible codes: `SUCCESS` (0), errors: -11, -40, -41, -50, -51, -80

**Recovery Strategy:** If -80 returned, key does not exist; verify correct key name.

### Initialization/Cleanup
Possible codes: `SUCCESS` (0), errors: -11, -12, -20, -21, -22

**Recovery Strategy:** For -21/-22, check system resources (file descriptors, mutex limits); for -20, free memory and retry.

---

## Best Practices

1. **Always check return codes** — do not assume success
2. **Log specific error codes** — helps debugging and monitoring
3. **Use error patterns for recovery:**
   - Memory errors (-20, -21, -22): Retry with exponential backoff
   - Lock errors (-30 to -35): Check for deadlock; enable debug tracing
   - Initialization errors (-50, -60, -90): Verify init was called before operations
   - "Not found" errors (-80): Check if key should exist
4. **Memory management:** Always `free()` allocated pointers on SUCCESS from get_key()
5. **Thread safety:** All error codes are thread-safe; no implicit side effects

---

## Known Typos in Source Code

> **Note:** Source files contain typos that will be addressed in a future cleanup:
> - `SUCESS` should be `SUCCESS` (missing 'C')
> - `RESZING` should be `RESIZING` (missing 'I')
>
> For now, use the constants as defined in the header files and reference this document for canonical descriptions.
| -62   | ERR_UNSUPPORTED_SUB_HASH_TABLE_OP  | Unsupported sub-hash table op | Review operation type; use correct API |
| -63   | ERR_SUB_HASH_TABLE_RESIZE_FAILED   | Sub-hash table resize failed  | Check memory availability; verify resize logic |
| -64   | ERR_SUB_HASH_BUCKET_NOT_INITIALIZED| Sub-hash bucket not initialized | Initialize bucket; verify parent table state |


### Linked List Operation Errors
| Code  | Name                             | Meaning/Description           | Recovery |
|-------|----------------------------------|-------------------------------|----------|
| -70   | ERR_LINKED_LIST_NODE_NOT_FOUND   | Linked list node not found    | Verify key exists; check list traversal logic |
| -71   | ERR_LINKED_LIST_NODE_CREATION_FAILED | Linked list node creation failed | Check memory availability; verify node factory |
| -72   | ERR_UNSUPPORTED_LINKED_LIST_OP   | Unsupported linked list op    | Use correct list operation; check API |


### Data Node Operation Errors
| Code  | Name                        | Meaning/Description           | Recovery |
|-------|-----------------------------|-------------------------------|----------|
| -80   | ERR_DATA_NODE_NOT_FOUND     | Data node not found           | Verify key exists in table; check traversal |
| -81   | ERR_DATA_NODE_UPDATE_FAILED | Data node update failed       | Check lock state; verify value size; retry |
| -82   | ERR_UNSUPPORTED_DATA_NODE_OP| Unsupported data node op      | Use correct node operation; verify API |
| -83   | ERR_DATA_NODE_CREATION_FAILED | Data node creation failed    | Check memory; verify value size valid |


### Pool/Manager Specific
| Code  | Name                      | Meaning/Description           | Recovery |
|-------|---------------------------|-------------------------------|----------|
| -90   | ERR_POOL_NOT_INITIALIZED  | Memory pool not initialized   | Initialize memory pool before allocation |
| -91   | ERR_POOL_EXHAUSTED        | Memory pool exhausted         | Increase pool size; free unused blocks; retry |
| -92   | ERR_POOL_CORRUPTION_DETECTED | Memory pool corruption detected | Verify pool integrity; enable memory sanitizer; inspect allocations |


### Platform/Threading
| Code   | Name                        | Meaning/Description         | Recovery |
|--------|-----------------------------|-----------------------------|----------|
| -100   | ERR_UNSUPPORTED_PLATFORM    | Platform not supported      | Check platform compatibility; enable platform-specific code |
| -101   | ERR_THREAD_CREATION_FAILED  | Thread creation failed      | Verify thread limit; check system resources; retry |
| -102   | ERR_MAX_RETRY_EXCEEDED      | Max retry attempts exceeded | Increase retry limit or backoff threshold; investigate root cause |


---

## Developer Guide

### Success Code Patterns

**Insertion Codes (10, 11, 20):**
Inform the caller about insertion outcome:
- `10` — Standard new node insertion (most common)
- `11` — Entry queued for pending resize operation (internal use)
- `20` — Insertion triggered a sub-table resize (useful for metrics/logging)

**Bloom Filter Codes (30, 31, 32):**
Guide lookup optimization:
- `30` — Bloom filter disabled; proceed with full lookup
- `31` — Bloom filter says *may exist*; perform full lookup
- `32` — Bloom filter says *definitely not exist*; skip lookup and return KEY_NOT_FOUND

### Error Handling Best Practices

1. **Always check return codes:**
   ```c
   int result = keystore_insert(keystore, key, value);
   if (result < 0) {
       // Error occurred; handle based on specific code
   } else if (result > 0) {
       // Success with additional context (e.g., resize triggered)
   }
   ```

2. **Use specific codes for logging:**
   - Log the exact error code, not generic "operation failed"
   - Pair with file:line context for debugging

3. **Recovery strategies:**
   - Memory errors (`-20`, `-21`, `-22`): Retry with backoff
   - Lock errors (`-30` to `-35`): Check for deadlock; enable tracing
   - Initialization errors (`-50`, `-60`, `-90`): Call init before operations
   - Exhaustion errors (`-91`): Expand capacity or evict stale data

4. **Thread-safe error handling:**
   - Each error code is independent of caller context
   - No implicit side effects from error returns
   - Use error codes to coordinate retry logic across threads

### Known Issues in Source Headers

> **Note:** Source files contain typos that will be addressed in a future cleanup:
> - `SUCESS` should be `SUCCESS` (missing 'C')
> - `RESZING` should be `RESIZING` (missing 'I')
>
> For now, use the constants as defined in the header files and reference this document for canonical descriptions.

---

## Usage Examples

```c
#include "error_code_definitions.h"
#include "sucess_code_definitions.h"

// Extended Success Codes — Insertion
// result == SUCESS_ADDED_NEW_NODE (10)          — new key-value pair inserted
// result == SUCCESS_ADDED_TO_PENDING_LIST (11)  — added to resize pending buffer
// result == SUCESS_ADDED_NEW_NODE_RESZING_TRIGGERED (20) — inserted + resize triggered

// Extended Success Codes — Bloom Filter
int bloom_result = bloom_filter_check(filter, key);
if (bloom_result == BLOOM_FILTER_DISABLED) {
    // Proceed with full lookup
} else if (bloom_result == BLOOM_FILTER_CHECK_KEY_MAY_EXIST) {
    // Key may exist; perform full lookup
} else if (bloom_result == BLOOM_FILTER_CHECK_KEY_NOT_EXIST) {
    // Key definitely not present; short-circuit
    return KEY_NOT_FOUND;
}

// Argument/Validation
if (key == NULL) return ERR_INVALID_ARGUMENT; // -11
if (config.bucket_size == 0) return ERR_INVALID_CONFIG; // -12

// Memory/Resource
void* buffer = malloc(size);
if (!buffer) return ERR_MEMORY_ALLOCATION_FAILED; // -20
if (pthread_mutex_init(&lock, NULL) != 0) return ERR_RESOURCE_INIT_FAILED; // -21
if (pthread_mutex_destroy(&lock) != 0) return ERR_RESOURCE_CLEANUP_FAILED; // -22

// Concurrency/Locking
if (pthread_rwlock_rdlock(&bucket->lock) != 0) return ERR_RW_LOCK_ACQUIRE_FAILED; // -30
if (pthread_rwlock_unlock(&bucket->lock) != 0) return ERR_RW_LOCK_RELEASE_FAILED; // -31
if (pthread_mutex_lock(&bucket->mutex) != 0) return ERR_MUTEX_LOCK_ACQUIRE_FAILED; // -32
if (pthread_mutex_unlock(&bucket->mutex) != 0) return ERR_MUTEX_LOCK_RELEASE_FAILED; // -33

// Hash/Indexing
uint64_t key_hash = hash_function_murmur_64(key, seed);
if (key_hash == UINT64_MAX) return ERR_HASH_COMPUTE_FAILED; // -40
if (index < 0) return ERR_INVALID_BUCKET_INDEX; // -41

// Hash Table/Bucket
if (!table) return ERR_HASH_TABLE_NOT_INITIALIZED; // -50
if (!bucket) return ERR_HASH_BUCKET_NOT_FOUND; // -51
if (bucket->count >= MAX_BUCKET_SIZE) return ERR_HASH_BUCKET_FULL; // -53

// Sub Hash Table
if (!sub_table) return ERR_SUB_HASH_TABLE_NOT_INITIALIZED; // -60
if (!sub_bucket) return ERR_SUB_HASH_BUCKET_NOT_FOUND; // -61

// Linked List
if (!node) return ERR_LINKED_LIST_NODE_NOT_FOUND; // -70
if (create_failed) return ERR_LINKED_LIST_NODE_CREATION_FAILED; // -71

// Data Node
if (!node) return ERR_DATA_NODE_NOT_FOUND; // -80
if (update_failed) return ERR_DATA_NODE_UPDATE_FAILED; // -81

// Pool/Manager
if (!pool->is_initialized) return ERR_POOL_NOT_INITIALIZED; // -90
if (pool->free_blocks == 0) return ERR_POOL_EXHAUSTED; // -91

// Platform/Threading
if (platform_not_supported) return ERR_UNSUPPORTED_PLATFORM; // -100
if (pthread_create(...) != 0) return ERR_THREAD_CREATION_FAILED; // -101
```

---

**Usage Guidelines:**

- Always return `0` for success.
- Use the most specific negative code for errors.
- Document any new codes in this file for consistency.
