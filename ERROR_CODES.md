
# Error Codes Reference

This document lists the custom error and return codes used throughout the distributed-keystore codebase. Use these codes for consistent error handling and debugging.

## General Success/Failure
| Code | Meaning                  | Example/Description                      |
|------|--------------------------|------------------------------------------|
| 0    | Success                  | Operation completed successfully         |
| -1   | General failure          | Unspecified error, catch-all             |

## Argument/Validation Errors
| Code | Meaning                  | Example/Description                      |
|------|--------------------------|------------------------------------------|
| -20  | Invalid argument/null ptr | Function received NULL pointer           |
| -21  | Invalid configuration    | Bucket size not power of two             |

## Memory/Resource Management
| Code | Meaning                        | Example/Description                      |
|------|---------------------------------|------------------------------------------|
| -10  | Memory allocation failure       | malloc/calloc returned NULL              |
| -11  | Resource initialization failure | pthread_mutex_init failed                |
| -12  | Resource cleanup failure        | pthread_mutex_destroy failed             |

## Concurrency/Locking
| Code | Meaning                  | Example/Description                      |
|------|--------------------------|------------------------------------------|
| -30  | Lock acquisition failure | pthread_mutex_lock failed                |
| -31  | Lock release failure     | pthread_mutex_unlock failed              |

## Hash/Indexing Errors
| Code | Meaning                  | Example/Description                      |
|------|--------------------------|------------------------------------------|
| -70  | Hash computation failure | hash_function_murmur_32 returned UINT32_MAX |
| -71  | Invalid bucket index     | Computed index is out of bounds          |

## Hash Bucket Operation Errors
| Code | Meaning                                   | Example/Description                      |
|------|-------------------------------------------|------------------------------------------|
| -40  | Hash bucket not found or not initialized   | get_hash_bucket returned NULL            |
| -43  | Unsupported hash bucket operation         | Unknown bucket type                      |
| -44  | Hash bucket full                          | No space for new node in bucket          |
| -45  | Hash bucket edit/update failure           | Failed to update bucket metadata         |

## Data Node Operation Errors
| Code | Meaning                                   | Example/Description                      |
|------|-------------------------------------------|------------------------------------------|
| -41  | Data node not found                       | Node with key/hash not found in list or tree    |
| -42  | Duplicate key                             | Attempt to insert duplicate key          |
| -46  | Data node edit/update failure             | Failed to update node value              |
| -47  | Unsupported data node operation           | Unknown node type                        |

## Pool/Manager Specific
| Code | Meaning                  | Example/Description                      |
|------|--------------------------|------------------------------------------|
| -50  | Pool not initialized     | Memory pool pointer is NULL              |
| -51  | Pool exhausted           | No free blocks in pool                   |
| -52  | Pool corruption detected | Pool metadata invalid                    |

## File/IO (if applicable)
| Code | Meaning                  | Example/Description                      |
|------|--------------------------|------------------------------------------|
| -60  | File not found           | fopen returned NULL                      |
| -61  | File read/write error    | fread/fwrite returned error              |

---


## Usage Examples


pthread_mutex_t lock;

### General Success/Failure
```c
int status = do_operation();
if (status == 0) {
	printf("Operation succeeded!\n");
} else if (status == -1) {
	fprintf(stderr, "General failure occurred\n");
}
```

### Argument/Validation Errors
```c
int set_value(const char* key, int value) {
	if (key == NULL) return -20; // Invalid argument/null pointer
	if (strlen(key) == 0) return -21; // Invalid configuration (empty key)
	// ...
	return 0;
}
```

### Memory/Resource Management
```c
void* buffer = malloc(size);
if (!buffer) {
	return -10; // Memory allocation failure
}

fclose(fp);
if (pthread_mutex_init(&lock, NULL) != 0) {
	return -11; // Resource initialization failure
}
// ...
if (pthread_mutex_destroy(&lock) != 0) {
	return -12; // Resource cleanup failure
}
```

### Concurrency/Locking
```c
if (pthread_rwlock_rdlock(&bucket->lock) != 0) {
	return -30; // Lock acquisition failure
}
// ... critical section ...
if (pthread_rwlock_unlock(&bucket->lock) != 0) {
	return -31; // Lock release failure
}
```

### Hash/Indexing Errors
```c
uint32_t key_hash = hash_function_murmur_32(key, seed);
if (key_hash == UINT32_MAX) return -70; // Hash computation failure

int index = _get_bucket_index(key_hash);
if (index < 0) return -71; // Invalid bucket index
```

### Hash Bucket Operation Errors
```c
hash_bucket* bucket = get_hash_bucket(index);
if (!bucket) return -40; // Hash bucket not found or not initialized
if (bucket->count >= MAX_BUCKET_SIZE) return -44; // Hash bucket full
if (bucket->type != BUCKET_LIST) return -43; // Unsupported hash bucket operation
if (update_bucket_metadata(bucket) != 0) return -45; // Hash bucket edit/update failure
```

### Data Node Operation Errors
```c
data_node* node = find_data_node(bucket, key, key_hash);
if (!node) return -41; // Data node not found
if (key_exists_in_bucket(bucket, key)) return -42; // Duplicate key
if (update_data_node(node, value) != 0) return -46; // Data node edit/update failure
if (node->type != SUPPORTED_TYPE) return -47; // Unsupported data node operation
```

### Pool/Manager Specific
```c
if (!pool->is_initialized) return -50; // Pool not initialized
if (pool->free_blocks == 0) return -51; // Pool exhausted
if (!validate_pool_metadata(pool)) return -52; // Pool corruption detected
```

### File/IO
```c
FILE* fp = fopen(path, "rb");
if (!fp) return -60; // File not found
if (fread(buffer, 1, size, fp) != size) return -61; // File read/write error
fclose(fp);
```


---

**Usage:**
- Always return `0` for success.
- Use the most specific negative code for errors.
- Document any new codes in this file for consistency.
