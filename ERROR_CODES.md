

# Error Codes Reference

This document lists all custom error and return codes used in the distributed-keystore project. Use these codes for consistent error handling, debugging, and documentation. Always return `0` for success, and use the most specific negative code for errors.

---

## Error Code Table


### General Success/Failure
| Code  | Name         | Meaning/Description                  |
|-------|--------------|--------------------------------------|
| 0     | SUCCESS      | Operation completed successfully     |
| -1    | ERR_FAILURE  | General/unspecified failure          |

### Extended Success Codes
| Code  | Name                                     | Meaning/Description                                      |
|-------|------------------------------------------|----------------------------------------------------------|
| 10    | SUCESS_ADDED_NEW_NODE                    | New key-value node inserted successfully                 |
| 11    | SUCCESS_ADDED_TO_PENDING_LIST            | Entry added to resize pending buffer (internal)          |
| 20    | SUCESS_ADDED_NEW_NODE_RESZING_TRIGGERED  | New node inserted and sub-hash-table resize triggered    |

### Argument/Validation Errors
| Code  | Name                   | Meaning/Description                       |
|-------|------------------------|-------------------------------------------|
| -11   | ERR_INVALID_ARGUMENT   | Invalid argument (e.g., NULL pointer)     |
| -12   | ERR_INVALID_CONFIG     | Invalid configuration or parameter        |

### Memory/Resource Management
| Code  | Name                        | Meaning/Description                                 |
|-------|-----------------------------|-----------------------------------------------------|
| -20   | ERR_MEMORY_ALLOCATION_FAILED| Memory allocation failed (malloc/calloc returned 0) |
| -21   | ERR_RESOURCE_INIT_FAILED    | Resource initialization failed (e.g., mutex init)   |
| -22   | ERR_RESOURCE_CLEANUP_FAILED | Resource cleanup failed (e.g., mutex destroy)       |

### Concurrency/Locking
| Code  | Name                         | Meaning/Description                |
|-------|------------------------------|------------------------------------|
| -30   | ERR_RW_LOCK_ACQUIRE_FAILED   | RW lock acquire failed             |
| -31   | ERR_RW_LOCK_RELEASE_FAILED   | RW lock release failed             |
| -32   | ERR_MUTEX_LOCK_ACQUIRE_FAILED| Mutex lock acquire failed          |
| -33   | ERR_MUTEX_LOCK_RELEASE_FAILED| Mutex lock release failed          |
| -34   | ERR_GENERIC_LOCK_ACQUIRE_FAILED | Custom lock acquire failed        |
| -35   | ERR_GENERIC_LOCK_RELEASE_FAILED | Custom lock release failed        |

### Hash/Indexing Errors
| Code  | Name                    | Meaning/Description         |
|-------|-------------------------|-----------------------------|
| -40   | ERR_HASH_COMPUTE_FAILED | Hash computation failure    |
| -41   | ERR_INVALID_BUCKET_INDEX| Invalid bucket index        |
| -42   | ERR_INVALID_SUB_BUCKET_INDEX | Invalid sub-bucket index  |

### Hash Table/Bucket Operation Errors
| Code  | Name                          | Meaning/Description           |
|-------|-------------------------------|-------------------------------|
| -50   | ERR_HASH_TABLE_NOT_INITIALIZED| Hash table not initialized    |
| -51   | ERR_HASH_BUCKET_NOT_FOUND     | Hash bucket not found         |
| -52   | ERR_UNSUPPORTED_HASH_BUCKET_OP| Unsupported hash bucket op    |
| -53   | ERR_HASH_BUCKET_FULL          | Hash bucket full              |
| -54   | ERR_HASH_BUCKET_NOT_INITIALIZED| Hash bucket not initialized  |

### Sub Hash Table Operation Errors
| Code  | Name                               | Meaning/Description           |
|-------|------------------------------------|-------------------------------|
| -60   | ERR_SUB_HASH_TABLE_NOT_INITIALIZED | Sub-hash table not initialized|
| -61   | ERR_SUB_HASH_BUCKET_NOT_FOUND      | Sub-hash bucket not found     |
| -62   | ERR_UNSUPPORTED_SUB_HASH_TABLE_OP  | Unsupported sub-hash table op |
| -63   | ERR_SUB_HASH_TABLE_RESIZE_FAILED   | Sub-hash table resize failed  |
| -64   | ERR_SUB_HASH_BUCKET_NOT_INITIALIZED| Sub-hash bucket not initialized|

### Linked List Operation Errors
| Code  | Name                             | Meaning/Description           |
|-------|----------------------------------|-------------------------------|
| -70   | ERR_LINKED_LIST_NODE_NOT_FOUND   | Linked list node not found    |
| -71   | ERR_LINKED_LIST_NODE_CREATION_FAILED | Linked list node creation failed |
| -72   | ERR_UNSUPPORTED_LINKED_LIST_OP   | Unsupported linked list op    |

### Data Node Operation Errors
| Code  | Name                        | Meaning/Description           |
|-------|-----------------------------|-------------------------------|
| -80   | ERR_DATA_NODE_NOT_FOUND     | Data node not found           |
| -81   | ERR_DATA_NODE_UPDATE_FAILED | Data node update failed       |
| -82   | ERR_UNSUPPORTED_DATA_NODE_OP| Unsupported data node op      |
| -83   | ERR_DATA_NODE_CREATION_FAILED | Data node creation failed    |

### Pool/Manager Specific
| Code  | Name                      | Meaning/Description           |
|-------|---------------------------|-------------------------------|
| -90   | ERR_POOL_NOT_INITIALIZED  | Memory pool not initialized   |
| -91   | ERR_POOL_EXHAUSTED        | Memory pool exhausted         |
| -92   | ERR_POOL_CORRUPTION_DETECTED | Memory pool corruption detected |

### Platform/Threading
| Code   | Name                        | Meaning/Description         |
|--------|-----------------------------|-----------------------------|
| -100   | ERR_UNSUPPORTED_PLATFORM    | Platform not supported      |
| -101   | ERR_THREAD_CREATION_FAILED  | Thread creation failed      |
| -102   | ERR_MAX_RETRY_EXCEEDED      | Max retry attempts exceeded |

---

## Usage Examples

```c
#include "error_code_definitions.h"
#include "sucess_code_definitions.h"

// Extended Success Codes
// result == SUCESS_ADDED_NEW_NODE (10)          — new key-value pair inserted
// result == SUCCESS_ADDED_TO_PENDING_LIST (11)  — added to resize pending buffer
// result == SUCESS_ADDED_NEW_NODE_RESZING_TRIGGERED (20) — inserted + resize triggered

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
uint32_t key_hash = hash_function_murmur_32(key, seed);
if (key_hash == UINT32_MAX) return ERR_HASH_COMPUTE_FAILED; // -40
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
