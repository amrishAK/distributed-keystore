# Distributed KeyStore — Codebase Memory Reference
> Updated: 2026-03-23 | Version: v1.0 (pre-release)

---

## 1. Project Overview

A concurrent, in-memory key-value store written in C (C11). The architecture is a **two-level hash table** (hash table → hash bucket → sub-hash-table → sub-hash-bucket → linked list → data node). The design prioritises:
- Fine-grained locking (per-bucket `pthread_rwlock_t`, per-data-node `pthread_mutex_t`)
- Lock-free resize signalling via `pthread_spinlock_t`
- Dual-seed MurmurHash3 (64-bit) for independent bucket and sub-bucket routing
- Optional memory pool for `linked_list_node` pre-allocation
- A background "chase buffer" worker for concurrent resize data migration

Current delivery scope agreed in this session:
- **v1.0** targets a production-quality **single-node, in-memory keystore** intended to sit behind a single cloud/API node.
- **v1.0 focus** is performance and operational hardening, not persistence or distributed consensus.
- **v2.0** is reserved for persistence primitives: WAL, snapshot/checkpoint, and crash recovery.
- **Later phases** may add REST and/or RAFT layers and broader distribution support after the single-node path is stable.

---

## 2. Directory Layout

```
src/keystore/
  core/               — Public API (key_store.h / key_store.c)
  hash/               — MurmurHash3 implementation
  hash_table/         — Hash table + hash bucket operations
    resizing/         — Resize orchestration, chase buffer worker
  sub_hash_table/     — Sub-hash-table + sub-hash-bucket operations
  data_structures/    — data_node, linked_list_node, double_linked_list_node
  type_definitions/   — All structs, enums, error/success codes
  utils/              — memory_manager, background_task_manager, helper_functions

tests/for_c/
  unit_tests/         — Unity-based unit tests (one file per module)
  integration_test/   — Concurrency stress test (120 threads × 150 keys in current checked-in config)
  include/            — Unity test framework source
  Makefile            — Targets: test, valgrind-test, coverage, run-concurrency-test, run-ct-valgrind
```

---

## 3. Type Definitions

### 3.1 `custom_type_definitions.h`

| Type | Description |
|------|-------------|
| `data_node` | Leaf storage: `key_hash (composite_key_hash)`, `data (uchar*)`, `data_size`, `is_concurrency_enabled`, `pthread_mutex_t lock`, `is_deleted`, `key[]` (FAM) |
| `linked_list_node` | Singly-linked collision chain: `key_hash`, `data_node* data_node_ptr`, `linked_list_node* next_node_ptr` |
| `double_linked_list_node` | Doubly-linked: `key_hash`, `data_node*`, `prev_node_ptr`, `next_node_ptr` — used exclusively in the resize `new_operation_buffer` |
| `key_value_pair` | Transfer struct: `char* key`, `unsigned char* value`, `size_t value_size` |
| `memory_pool` | Pre-allocated block pool: `block_size`, `pool_start/end/next_ptr`, `total/available/reusable_blocks`, `void** free_block_list`, `pthread_mutex_t pool_lock` |
| `new_operation_buffer` | Resize write-ahead doubly linked list: `head_ptr`, `tail_ptr`, `current_consumer_ptr`, `pthread_spinlock_t buffer_lock`, `pause_chasing`, `is_running` |
| `delete_operation` | Delete tracking entry: `char* key`, `uint64_t hash` |
| `delete_operation_buffer` | Growable array of `delete_operation`: `operations*`, `count`, `capacity` (grows in 500-block increments via `realloc`) |
| Config Struct | Key Fields |
|--------------|-----------|
| `sub_hash_table_configuration` | `is_concurrency_enabled`, `bucket_size`, `max_linked_list_chain_length` |
| `hash_table_configuration` | `bucket_size`, `is_concurrency_enabled`, `sub_hash_table_bucket_size`, `max_linked_list_chain_length` |
| `memory_manager_config` | `bucket_size`, `sub_bucket_size`, `pre_allocation_factor`, `allocate_list_pool`, `is_concurrency_enabled` |

### 3.3 `hash_bucket_type_definition.h`

| Type | Description |
|------|-------------|
| `sub_hash_bucket` | `linked_list_head*`, `active_node_count`, `total_node_count`, `is_initialized`, `is_concurrency_enabled`, `max_linked_list_chain_length`, `pthread_rwlock_t sub_hash_bucket_lock` |
| `sub_hash_table_memory_pool` | Array of `sub_hash_bucket`: `sub_hash_buckets_ptr*`, `block_size`, `total_blocks`, `is_initialized`, `is_concurrency_enabled`, `max_linked_list_chain_length` |
| `resizing_buffer` | Holds all resizing state: `new_sub_hash_table_ptr*`, `delete_operation_buffer_ptr*`, `updated_operation_buffer_head*` (linked list), `new_operation_buffer_ptr*` |
| `hash_bucket` | Top-level bucket: `sub_hash_table_ptr*`, `snapshot_sub_hash_table_ptr*`, `resizing_buffer_ptr*`, `pthread_mutex_t resizing_lock`, `node_count`, `is_resizing`, `is_initialized`, `sub_hash_table_config` |
| `hash_table_memory_pool` | Array of `hash_bucket`: `hash_buckets_ptr*`, `block_size`, `total_blocks`, `is_initialized`, `sub_hash_table_config` |

### 3.4 `background_task_manager_type_definitons.h`

| Type | Description |
|------|-------------|
| `background_task_args_t` | `void* task_input_args`, `_Atomic bool kill_signal` |
| `background_task_t` | `int (*background_task)(void*)`, `background_task_args_t* task_args`, `bool is_detached` |
| `background_task_info_t` | `uint32_t task_uuid`, `background_task_t* task`, `pthread_t pthread_id` |

### 3.5 `stats_type_definitions.h`

`data_node_operation_stats` — global counters for successful/failed CRUD ops and per-error-code counters (`error_code_counters[100]`).

---

## 4. Error & Success Codes

### Error Codes (`error_code_definitions.h`)

| Code  | Name         | Description                  |
|-------|--------------|------------------------------|
| 0     | SUCCESS      | Operation completed successfully |
| -1    | ERR_FAILURE  | General/unspecified failure  |
| -11   | ERR_INVALID_ARGUMENT | Invalid argument (e.g., NULL pointer) |
| -12   | ERR_INVALID_CONFIG | Invalid configuration or parameter |
| -20   | ERR_MEMORY_ALLOCATION_FAILED | Memory allocation failed |
| -21   | ERR_RESOURCE_INIT_FAILED | Resource initialization failed |
| -22   | ERR_RESOURCE_CLEANUP_FAILED | Resource cleanup failed |
| -30   | ERR_RW_LOCK_ACQUIRE_FAILED | RW lock acquire failed |
| -31   | ERR_RW_LOCK_RELEASE_FAILED | RW lock release failed |
| -32   | ERR_MUTEX_LOCK_ACQUIRE_FAILED | Mutex lock acquire failed |
| -33   | ERR_MUTEX_LOCK_RELEASE_FAILED | Mutex lock release failed |
| -34   | ERR_GENERIC_LOCK_ACQUIRE_FAILED | Custom lock acquire failed |
| -35   | ERR_GENERIC_LOCK_RELEASE_FAILED | Custom lock release failed |
| -40   | ERR_HASH_COMPUTE_FAILED | Hash computation failure |
| -41   | ERR_INVALID_BUCKET_INDEX | Invalid bucket index |
| -42   | ERR_INVALID_SUB_BUCKET_INDEX | Invalid sub-bucket index |
| -50   | ERR_HASH_TABLE_NOT_INITIALIZED | Hash table not initialized |
| -51   | ERR_HASH_BUCKET_NOT_FOUND | Hash bucket not found |
| -52   | ERR_UNSUPPORTED_HASH_BUCKET_OP | Unsupported hash bucket op |
| -53   | ERR_HASH_BUCKET_FULL | Hash bucket full |
| -54   | ERR_HASH_BUCKET_NOT_INITIALIZED | Hash bucket not initialized |
| -60   | ERR_SUB_HASH_TABLE_NOT_INITIALIZED | Sub-hash table not initialized |
| -61   | ERR_SUB_HASH_BUCKET_NOT_FOUND | Sub-hash bucket not found |
| -62   | ERR_UNSUPPORTED_SUB_HASH_TABLE_OP | Unsupported sub-hash table op |
| -63   | ERR_SUB_HASH_TABLE_RESIZE_FAILED | Sub-hash table resize failed |
| -64   | ERR_SUB_HASH_BUCKET_NOT_INITIALIZED | Sub-hash bucket not initialized |
| -70   | ERR_LINKED_LIST_NODE_NOT_FOUND | Linked list node not found |
| -71   | ERR_LINKED_LIST_NODE_CREATION_FAILED | Linked list node creation failed |
| -72   | ERR_UNSUPPORTED_LINKED_LIST_OP | Unsupported linked list op |
| -80   | ERR_DATA_NODE_NOT_FOUND | Data node not found |
| -81   | ERR_DATA_NODE_UPDATE_FAILED | Data node update failed |
| -82   | ERR_UNSUPPORTED_DATA_NODE_OP | Unsupported data node op |
| -83   | ERR_DATA_NODE_CREATION_FAILED | Data node creation failed |
| -90   | ERR_POOL_NOT_INITIALIZED | Memory pool not initialized |
| -91   | ERR_POOL_EXHAUSTED | Memory pool exhausted |
| -92   | ERR_POOL_CORRUPTION_DETECTED | Memory pool corruption detected |
| -100  | ERR_UNSUPPORTED_PLATFORM | Platform not supported |
| -101  | ERR_THREAD_CREATION_FAILED | Thread creation failed |
| -102  | ERR_MAX_RETRY_EXCEEDED | Max retry attempts exceeded |

### Success Codes (`sucess_code_definitions.h`)

| Code | Name | Description |
|------|------|-------------|
| 0    | SUCCESS | Generic success / node updated |
| 10   | SUCESS_ADDED_NEW_NODE | New node inserted |
| 11   | SUCCESS_ADDED_TO_PENDING_LIST | Added to resize pending buffer |
| 20   | SUCESS_ADDED_NEW_NODE_RESZING_TRIGGERED | New node inserted, resize should be triggered |

---

## 5. Module Reference

### 5.1 `core/key_store` — Public API

**File:** `key_store.c` / `key_store.h`

**Global state:**
- `static uint64_t g_bucket_hash_seed` — random seed for bucket-level hashing, generated at init via `clock_gettime(CLOCK_MONOTONIC)`
- `static uint64_t g_sub_bucket_hash_seed` — random seed for sub-bucket hashing, XOR’d with `0x9e3779b97f4a7c15` for guaranteed distinctness
- `hash_table_memory_pool* g_hash_table_pool` — singleton hash table

**Functions:**

| Function | Signature | Notes |
|----------|-----------|-------|
| `initialise_key_store` | `(hash_table_configuration config, double pre_memory_allocation_factor) → int` | Validates config (bucket sizes must be powers of 2, factor in [0,1]); initializes memory manager + hash table |
| `cleanup_key_store` | `(void) → int` | Calls `cleanup_hash_table` + `cleanup_memory_manager`; resets seed |
| `set_key` | `(key_value_pair* value) → int` | Validates input; hashes key; calls `upsert_node_to_hash_table` |
| `get_key` | `(const char* key, key_value_pair* value_out) → int` | Validates; hashes; calls `get_key_value_from_hash_table` |
| `delete_key` | `(const char* key) → int` | Validates; hashes; calls `delete_key_from_hash_table` |

**Important constraint:** `key_hash == UINT64_MAX` is treated as a hash error (murmur returns `UINT64_MAX` on null key).

---

### 5.2 `hash/hash_functions` — Hashing

**File:** `hash_functions.c` / `hash_functions.h`

| Function | Signature | Notes |
|----------|-----------|-------|
| `hash_function_murmur_64` | `(const char* key, uint64_t seed) → uint64_t` | MurmurHash3 64-bit; returns `UINT64_MAX` on NULL input |

**Implementation details:**
- Block size: 8 bytes; processes in 8-byte blocks then tail bytes
- Mix constants: `0x87c37b91114253d5`, `0x4cf5ad432745937f`
- Finalization avalanche: XOR-shift with `0xff51afd7ed558ccd`, `0xc4ceb9fe1a85ec53`
- Left circular rotation helper: `left_circular_rotate(data, bits, 64)`

---

### 5.3 `hash_table/hash_table_operation` — Hash Table Layer

**File:** `hash_table_operation.c` / `hash_table_operation.h`

| Function | Signature | Notes |
|----------|-----------|-------|
| `create_new_hash_table` | `(hash_table_configuration, hash_table_memory_pool**) → int` | `calloc`s bucket array; eagerly initialises all buckets if concurrency enabled |
| `cleanup_hash_table` | `(hash_table_memory_pool*) → int` | Iterates all buckets → `cleanup_hash_bucket` |
| `upsert_node_to_hash_table` | `(pool*, key_hash, kv_pair*) → int` | Routes to bucket via `key_hash.bucket_hash % total_blocks`; lazy bucket init if not concurrent |
| `get_key_value_from_hash_table` | `(pool*, key_hash, key, kv_pair_out*) → int` | Routes to bucket |
| `delete_key_from_hash_table` | `(pool*, key_hash, key) → int` | Routes to bucket |

**Bucket selection:** `bucket_index = key_hash.bucket_hash % total_blocks` (modulo, NOT bitmask — top-level only).

---

### 5.4 `hash_table/hash_bucket_operation` — Hash Bucket Layer

**File:** `hash_bucket_operation.c` / `hash_bucket_operation.h`

| Function | Signature | Notes |
|----------|-----------|-------|
| `initialise_hash_bucket` | `(hash_bucket*, sub_hash_table_configuration) → int` | Creates sub-hash-table; inits `resizing_lock` (mutex); `snapshot_sub_hash_table_ptr = NULL` |
| `cleanup_hash_bucket` | `(hash_bucket*) → int` | Busy-waits for ongoing resize via `RESIZE_CHECK_STATUS` (code 21 = still resizing); then frees current + snapshot sub-hash-table + resizing buffer; destroys mutex |
| `upsert_node_to_hash_bucket` | `(hash_bucket*, key_hash, kv_pair*) → int` | If not resizing: direct upsert; if code 20 returned → triggers `initialize_hash_bucket_resizing`; if resizing: lock-wrapped delegate |
| `get_key_value_from_hash_bucket` | `(hash_bucket*, key, key_hash, kv_pair_out*) → int` | Direct or resizing-aware lookup |
| `delete_key_from_hash_bucket` | `(hash_bucket*, key, key_hash) → int` | Direct or resizing-aware delete |

**Lock:** `pthread_mutex_t resizing_lock` — guards all resize state transitions. Operations that arrive during resizing acquire this lock before delegating to the resizing-aware variants.

---

### 5.5 `hash_table/resizing/hash_bucket_resizing_operation` — Resize Coordinator

**File:** `hash_bucket_resizing_operation.c` / `hash_bucket_resizing_operation.h`

**Enum:** `hash_bucket_resizing_operation_t` → `RESIZE_INITIALIZE`, `RESIZE_UPSERT_NODE`, `RESIZE_GET_NODE`, `RESIZE_DELETE_NODE`, `RESIZE_CHECK_STATUS`

| Function | Notes |
|----------|-------|
| `initialize_hash_bucket_resizing` | Sets `is_resizing = true`; snapshots `sub_hash_table_ptr → snapshot_sub_hash_table_ptr`; calls `initialize_resizing_buffer`; spawns **detached** `hash_bucket_resize_worker` thread |
| `upsert_node_to_hash_bucket_during_resizing` | If resizing finished while waiting for lock → normal upsert; else calls `_update_node_while_resizing` |
| `get_key_value_from_hash_bucket_during_resizing` | Checks snapshot + resizing buffer; fallback to snapshot sub-hash-table |
| `delete_key_from_hash_bucket_during_resizing` | Checks snapshot; adds to delete buffer or marks as deleted in new-op buffer |
| `check_resize_status` | Returns 21 if `is_resizing == true`, SUCCESS (0) otherwise. Used by `cleanup_hash_bucket` to wait for ongoing resize |

**Private helpers:**
- `_find_node_in_hash_bucket_during_resizing` — checks snapshot sub-hash-table first; if found checks buffer for overwrite; else checks new-op buffer
- `_delete_node_while_resizing` — node in snapshot → `insert_delete_operation_to_resizing_buffer`; node is new → `insert_node_to_new_operation_buffer` with `is_delete_operation = true`
- `_update_node_while_resizing` — node in snapshot → `insert_update_operation_to_resizing_buffer`; node is new → `insert_node_to_new_operation_buffer`

---

### 5.6 `hash_table/resizing/resize_operation` — Background Resize Worker

**File:** `resize_operation.c` / `resize_operation.h`

| Function | Notes |
|----------|-------|
| `hash_bucket_resize_worker` | Entry point for the resize background thread. Doubles `bucket_size`; calls `_perform_hash_bucket_resizing` (up to 3 retries); acquires `resizing_lock`; calls `_finalize_hash_bucket_resizing`; deletes buffer; sets `is_resizing = false`; unlocks |
| `_perform_hash_bucket_resizing` | Creates new sub-hash-table; launches `chase_buffer_worker`; calls `_fillup_new_sub_hash_table`; waits for chase worker; retries on failure |
| `_fillup_new_sub_hash_table` | Iterates all sub-buckets in the **snapshot** sub-hash-table; appends each linked list chain to the new sub-hash-table via `_append_list_nodes_to_sub_hash_table` |
| `_finalize_hash_bucket_resizing` | Commits pending buffer ops to sub-hash-table; swaps `sub_hash_table_ptr` to the new table; frees snapshot |

**Resize strategy:** Doubles the sub-hash-table bucket size on every resize. New `sub_hash_table_configuration` inherits all other config fields.

---

### 5.7 `hash_table/resizing/buffer_operation` — Chase Buffer

**File:** `buffer_operation.c` / `buffer_operation.h`

The chase buffer is a `new_operation_buffer` (doubly-linked list protected by `pthread_spinlock_t`). It records all writes that arrive **during** an active resize. A background "chase worker" thread concurrently migrates these writes to the new sub-hash-table.

**Key data flow:**
1. Writer → `insert_node_to_new_operation_buffer` (inserts at **head**; tail = oldest, never changes)
2. Chase worker reads from **tail → head** (using `current_consumer_ptr`); processes each node via `_commit_data_node_operation`
3. On finish: `wait_for_chase_worker_to_finish` kills worker and invalidates consumer pointer

| Function | Notes |
|----------|-------|
| `chase_buffer_worker` | Runs in background thread; polls for new nodes; commits them to new sub-hash-table; exits on `kill_signal` |
| `initialize_chase_worker` | Resets `current_consumer_ptr = NULL`; spawns **non-detached** chase worker; returns `task_uuid` |
| `wait_for_chase_worker_to_finish` | Sets `kill_signal`; `pthread_join`; invalidates consumer |
| `initialize_resizing_buffer` | `calloc`s `resizing_buffer`; inits `new_operation_buffer` (spinlock) and `delete_operation_buffer` (dynamic array) |
| `insert_node_to_new_operation_buffer` | Creates `data_node` + `double_linked_list_node`; spin-locks and inserts at head; sets tail on first insert |
| `insert_delete_operation_to_resizing_buffer` | Appends `delete_operation` (key copy + hash) to `delete_operation_buffer`; grows capacity by 500 via `realloc` when full |
| `insert_update_operation_to_resizing_buffer` | Creates LLN from kv_pair; inserts at head of `updated_operation_buffer_head` (no locking — called inside resizing lock) |
| `get_node_from_resizing_buffer` | Checks updated-op buffer + optionally current new-op buffer; searches for key |
| `commit_resizing_buffer_operations_to_sub_hash_table` | Applies updates, deletes, and new-op buffer to the target sub-hash-table |
| `delete_resizing_buffer` | Frees updated-op linked list, delete-op array, new-op buffer, and residual new sub-hash-table |

---

### 5.8 `sub_hash_table/sub_hash_table_operation` — Sub-Hash-Table Layer

**File:** `sub_hash_table_operation.c` / `sub_hash_table_operation.h`

| Function | Notes |
|----------|-------|
| `create_new_sub_hash_table` | `calloc`s `sub_hash_table_memory_pool` + bucket array; eager init if concurrency or `earlyInitialize = true` |
| `cleanup_sub_hash_table` | Iterates all sub-buckets → `cleanup_sub_hash_bucket`; frees array |
| `upsert_node_to_sub_hash_table` | Finds bucket via `key_hash.sub_bucket_hash & (total_blocks - 1)` (bitmask — power-of-2 guaranteed); uses a split update-then-add path: first `update_node_in_sub_hash_bucket`, then on `ERR_DATA_NODE_NOT_FOUND` falls back to `add_node_to_sub_hash_bucket` |
| `get_key_store_value_from_sub_hash_table` | Routes to sub-bucket |
| `delete_key_from_sub_hash_table` | Routes to sub-bucket |
| `is_node_in_sub_hash_table` | Existence check without reading value |

**Bucket index:** `key_hash.sub_bucket_hash & (total_blocks - 1)` — bitmasked, requires power-of-2 bucket size.

---

### 5.9 `sub_hash_table/sub_hash_bucket_operation` — Sub-Hash-Bucket Layer

**File:** `sub_hash_bucket_operation.c` / `sub_hash_bucket_operation.h`

**Struct:** `sub_hash_bucket_operation_args` — `{ sub_hash_bucket*, const char* key, composite_key_hash key_hash }`

**Locking model:**
- List operations use `pthread_rwlock_t sub_hash_bucket_lock` via `_lock_wrapper_for_linked_list_node_operation`
  - `GET_LL_NODE` → read lock
  - `INSERT_LL_NODE`, `DELETE_ALL_LL_NODES`, `CLEANUP_DELETED_LL_NODES` → write lock
- Data node mutation uses `pthread_mutex_t lock` (per-node) via `_lock_wrapper_for_data_node_operation`

| Function | Notes |
|----------|-------|
| `initialise_sub_hash_bucket` | Inits `pthread_rwlock_t` if concurrent; zero-initialises counts |
| `cleanup_sub_hash_bucket` | Deletes all LLNs (locked if concurrent); destroys rwlock |
| `update_node_in_sub_hash_bucket` | Find LLN (read lock) → edit value (node mutex) |
| `add_node_to_sub_hash_bucket` | Create `data_node` + LLN → insert (write lock); increment counts; check resize trigger |
| `get_key_store_value_from_sub_hash_bucket` | Find LLN (read lock) → read value (node mutex) |
| `delete_key_from_sub_hash_bucket` | Find LLN (read lock) → soft-delete `data_node` (node mutex); decrement `active_node_count` |
| `is_node_in_sub_hash_bucket` | Find LLN (read lock); returns `SUCCESS` or `ERR_DATA_NODE_NOT_FOUND` |

**Hot-path note:** the current upsert path is optimized for updates, but unique-key inserts still pay a lookup miss before allocation/insertion. This is the main set-latency concern identified in the current v1 roadmap.

**Resize trigger:** `_check_for_resize_condition` first checks `total_node_count` against `max_linked_list_chain_length`; if deleted nodes exist it runs `cleanup_deleted_linked_list_nodes`, updates `total_node_count`, and only returns `SUCESS_ADDED_NEW_NODE_RESZING_TRIGGERED` (20) if the chain is still at or above threshold after cleanup.

---

### 5.10 `data_structures/data_node_operation` — Data Node

**File:** `data_node_operation.c` / `data_node_operation.h`

**Global:** `data_node_operation_stats g_data_node_operation_counters` — tracks successful/failed counts per operation type plus per-error-code counters.

**Enum:** `data_node_operation_t` → `CREATE_NODE`, `DELETE_NODE`, `SOFT_DELETE_NODE`, `UPDATE_NODE`, `READ_NODE`

| Function | Notes |
|----------|-------|
| `create_new_data_node` | Allocates `data_node` with FAM `key[]`; copies key + value; inits mutex if concurrent |
| `delete_data_node` | Frees `data`, destroys mutex if concurrent, frees node |
| `soft_delete_data_node` | Sets `is_deleted = true` only |
| `edit_data_node_value` | `realloc`s `data`; copies new value; updates `data_size` |
| `read_data_node_value` | `malloc`s and copies `data` and `key` into output `key_value_pair` — **caller owns the memory** |

**Memory ownership:** `read_data_node_value` allocates both `kv_pair_out->value` and `kv_pair_out->key`. The caller is responsible for `free()`-ing them.

---

### 5.11 `data_structures/linked_list_operation` — Singly Linked List

**File:** `linked_list_operation.c` / `linked_list_operation.h`

| Function | Notes |
|----------|-------|
| `create_new_linked_list_node` | Allocates from memory pool (`allocate_memory_from_pool`) |
| `insert_linked_list_node` | Head insertion into `**node_header_ptr` |
| `get_data_node_from_linked_list` | Linear scan; `include_soft_deleted` flag; returns first match on `key_hash` + `strcmp(key)` |
| `delete_all_linked_list_nodes` | Traverses and calls `delete_data_node` + frees each LLN |
| `cleanup_deleted_linked_list_nodes` | Frees only `is_deleted == true` nodes; returns deleted count |

**Allocation:** LLN nodes are always allocated from `g_list_pool` (pre-allocated pool) if initialised; fallback to `malloc`.

---

### 5.12 `data_structures/double_linked_list_operation` — Doubly Linked List

**File:** `double_linked_list_operation.c` / `double_linked_list_operation.h`

Used exclusively by the `new_operation_buffer` during resizing.

| Function | Notes |
|----------|-------|
| `create_new_double_linked_list_node` | Allocates node; sets `data_node_ptr`, `key_hash`; `prev/next = NULL` |
| `insert_double_linked_list_node` | Head insertion; sets `next->prev` for the old head |
| `find_data_node_in_double_linked_list` | Linear scan from head |
| `delete_double_linked_list` | Traverses and calls `delete_data_node` + frees each DLLN |

---

### 5.13 `utils/memory_manager` — Memory Manager

**File:** `memory_manager.c` / `memory_manager.h`

**Global state:**
- `static memory_pool g_list_pool` — pool for `linked_list_node`
- `static memory_manager_config g_config`

**Pool:** A flat pre-allocated arena. Blocks are handed out sequentially; freed blocks go into `free_block_list` (a pointer array). Thread-safe via `pthread_mutex_t pool_lock` when concurrency is enabled.

| Function | Notes |
|----------|-------|
| `initialize_memory_manager` | Creates `g_list_pool` sized by `ceil(bucket_size * pre_allocation_factor)` LLN blocks |
| `cleanup_memory_manager` | Frees pool arena + free-block-list; destroys mutex |
| `allocate_memory_from_pool` | Returns from `g_list_pool` or falls back to `malloc(sizeof(linked_list_node))` |
| `allocate_memory` | Thin `malloc` wrapper |
| `callocate_memory` | Thin `calloc` wrapper |
| `reallocate_memory` | Thin `realloc` wrapper with NULL/zero-size safety |
| `free_memory(ptr, is_pool)` | If `is_pool && pool_initialized` → return to pool's `free_block_list`; else `free` |

---

### 5.14 `utils/background_task_manager` — Thread Management

**File:** `background_task_manager.c` / `background_task_manager.h`

**Global state:**
- `background_task_info_t* background_tasks_registry[100]` — static registry
- `pthread_mutex_t registry_lock` — guards registry access
- `int initialised_items_count`, `active_tasks_count`

| Function | Notes |
|----------|-------|
| `initialize_background_function` | Allocates `background_task_args_t` + `background_task_t`; spawns pthread; detaches if `detach_task = true`; registers with UUID if not detached |
| `cleanup_background_task` | Sets `kill_signal = true` atomically; `pthread_join`; removes from registry |

**UUID generation:** Small atomic counter; collision-handled via retry. Max 100 concurrent tracked tasks.

---

### 5.15 `utils/helper_functions` — Utilities

| Function | Notes |
|----------|-------|
| `is_power_of_two(n)` | `n != 0 && (n & (n-1)) == 0` |
| `portable_sleep_ms(ms)` | `usleep(ms * 1000)` on POSIX; `Sleep(ms)` on Windows |

---

## 6. Concurrency Model

### Locking Hierarchy (coarse → fine)

```
hash_bucket.resizing_lock  (pthread_mutex_t)
  └─ sub_hash_bucket.sub_hash_bucket_lock  (pthread_rwlock_t)
       └─ data_node.lock  (pthread_mutex_t)
```

### Resize Flow (concurrent path)

```
upsert detects active_node_count >= max_chain_length
  → returns SUCCESS_ADDED_NEW_NODE_RESIZING_TRIGGERED (20)
  → hash_bucket_operation calls _resizing_lock_wrapper(RESIZE_INITIALIZE)
     → acquire resizing_lock
     → initialize_hash_bucket_resizing:
         - snapshot current sub_hash_table
         - initialize_resizing_buffer (spinlock + LLN pool for new_op_buffer)
         - spawn DETACHED resize worker thread
     → release resizing_lock

Incoming ops while is_resizing == true:
  → acquire resizing_lock
  → delegate to resizing-aware variant (upsert/get/delete _during_resizing)
  → release resizing_lock

Background resize worker:
  → doubles sub_hash_table bucket_size
  → create new sub_hash_table (empty)
  → spawn chase worker (NON-detached, returned UUID)
  → _fillup_new_sub_hash_table from snapshot (main migrate thread)
  → wait_for_chase_worker_to_finish (kills chase, joins)
  → acquire resizing_lock
  → _finalize_hash_bucket_resizing:
      - commit updated-op, delete-op, new-op buffers to new table
      - swap sub_hash_table_ptr → new table
      - free snapshot
  → delete_resizing_buffer
  → is_resizing = false
  → release resizing_lock
```

### Key Invariants
- `snapshot_sub_hash_table_ptr` is read-only during resize (no writers)
- `new_operation_buffer` is spinlock-protected; writers at head, chase worker at tail
- After `is_resizing` becomes `false`, all ops revert to normal paths
- `pthread_rwlock_t` on sub-buckets: reads share, all writes exclusive
- `data_node.lock` (mutex): serialises concurrent updates to the same key's value

---

## 7. Test Suite

### Unit Tests (`tests/for_c/unit_tests/`)

Run via `test_runner.c` using the **Unity** framework.

| Test File | Module Tested |
|-----------|--------------|
| `test_data_node_operation.c` | `data_node_operation` |
| `test_linked_list_operation.c` | `linked_list_operation` |
| `test_sub_hash_bucker_operation.c` | `sub_hash_bucket_operation` |
| `test_sub_hash_table_operation.c` | `sub_hash_table_operation` |
| `test_hash_bucket_operation.c` | `hash_bucket_operation` |
| `test_hash_table_operation.c` | `hash_table_operation` |
| `test_dynamic_resizing.c` | Combined resize flow |
| `test_key_store.c` | Public API (`key_store`) |
| `test_buffer_operation.c` | Chase buffer / resizing buffer |

**Test config used in unit tests:** `bucket_size = 8`, `sub_hash_table_bucket_size = 4`, `max_linked_list_chain_length = 4`, `is_concurrency_enabled = false`

### Integration / Stress Test (`integration_test/concurrency_test.c`)

- `NUM_THREADS = 120`, `NUM_KEYS_PER_THREAD = 150` (18K total ops)
- Each thread sets + gets its own non-overlapping key range
- Measures `set` and `get` latency (nanoseconds via `CLOCK_MONOTONIC`)
- Reports p50/p99 latencies + throughput
- Tracks `race_errors` (atomic) and `resizing_triggered_count` (atomic)
- Key format: `"K%d"` (e.g. `"K42"`)

### Build Targets (Makefile)

| Target | Description |
|--------|-------------|
| `make test` | Build with coverage flags + run unit tests |
| `make valgrind-test` | Build without coverage/sanitizers + run under Valgrind |
| `make coverage` | Generate gcov/lcov report |
| `make coverage-simple` | Generate `.gcov` files without HTML output |
| `make run-concurrency-test` | Build and run the integration concurrency test |
| `make run-ct-valgrind` | Run the integration concurrency test under Valgrind |

**Compiler:** `gcc` with `-Wall -Wextra -g -fno-pie -no-pie -Wa,--noexecstack -lpthread -lm`

---

## 8. Known Design Notes & Caveats

1. **Soft-delete semantics:** Delete operations mark `data_node.is_deleted = true`. Physical removal happens only during `cleanup_deleted_linked_list_nodes`, which is called during resize or explicit cleanup. `active_node_count` tracks live nodes; `total_node_count` includes soft-deleted.

2. **Resize only doubles:** The resize strategy always multiplies `bucket_size × 2`. No shrinking implemented.

3. **Memory pool scope:** Only `linked_list_node` allocations use the pre-allocated pool. `data_node`, `double_linked_list_node`, and all management structs use standard `malloc`/`calloc`.

4. **Memory pool sizing is bucket-based today:** `initialize_memory_manager()` sizes the LLN pool from `ceil(bucket_size * pre_allocation_factor)`. It is not yet sized from expected entry count or benchmark workload shape, which is part of the planned v1 refactor.

5. **`free_memory(ptr, is_pool=false)` for non-pool allocations:** Nearly all calls pass `is_pool = false`; only the memory manager itself uses `is_pool = true` internally via `_free_memory_to_pool`.

6. **`key_hash == 0` treated as invalid** in `_get_hash_table_bucket`, `_get_sub_hash_table_bucket`, and several resizing-buffer entry points. This is a guard but means keys that naturally hash to 0 would fail. The `UINT64_MAX` sentinel is used separately for Murmur error detection.

7. **No WAL / persistence:** v1.0 is fully in-memory. WAL stubs are reserved for v2.0 (mark with `/* WAL: v2.0 */`).

8. **Background task registry capacity:** Hard-coded at `MAX_BACKGROUND_TASKS = 100`. Overflow is not gracefully handled and could cause silent failures in very high-concurrency scenarios with many simultaneous resizes.

9. **Unique-key set path remains miss-heavy:** `upsert_node_to_sub_hash_table()` currently does read/search first and insert second. For workloads dominated by new keys, this adds extra lookup overhead before allocation and write-lock insertion.

10. **Current source search found no active `printf`/`fprintf` traces in `src/keystore`:** older documentation that described pervasive stdout debug logging is now stale, though some files still include `<stdio.h>`.

11. **POSIX sleep dependency remains in hot infrastructure paths:** `buffer_operation.c` uses `usleep(100)` in the chase worker loop, and `hash_bucket_operation.c` also uses `usleep(100)` during resize cleanup polling. Windows portability is therefore incomplete in current checked-in source despite `portable_sleep_ms()` existing.

12. **Memory returned from `read_data_node_value`:** Both `key` and `value` are heap-allocated by the callee. The caller **must** `free()` both. The current integration test now frees both correctly.

---

## 9. v1.0 Checklist

- [x] Two-level hash table with bucket RW-locks
- [x] Per-data-node mutex for value updates
- [x] Fine-grained spinlock chase buffer during resize
- [x] Full CRUD (create/read/update/delete with soft-delete)
- [x] Memory pool for linked list nodes
- [x] Background resize worker (doubles sub-table bucket size)
- [x] Concurrency stress test harness present (120 threads × 150 keys in current checked-in config)
- [x] Unity unit tests across all modules
- [ ] Benchmark parity with prior 1M-operation global-hash-table baseline
- [ ] Staged benchmark campaign at 3M and 5M operations
- [x] Valgrind clean confirmed (149 tests, 0 failures, 630 allocs / 630 frees, 0 errors)
- [ ] `key_hash == 0` guard reviewed (edge case for some key strings)
- [x] Generate 2 hash for hash table and sub hash table to improve the distribution
- [ ] User finer locks for resizing
- [ ] Refactor back ground task manager with lazy memory pool
- [ ] Refactor Memory pool
- [ ] Fast Key lookup mechanism using bloom filter
- [ ] Add Logs using defined functions
- [ ] README.md finalized (updated 2026-03-17 with architecture, concurrency, error codes, checklist)
- [ ] API.md complete (reviewed 2026-03-16)
- [ ] ERROR_CODES.md complete (reviewed 2026-03-16)
- [ ] Tagged v1.0.0

### Current v1 Priority Order (2026-03-17)

1. ~~Fix routing and hash distribution first using dual-hash distribution or a second independently mixed hash view.~~ **DONE** — dual-seed MurmurHash3-64 implemented with `composite_key_hash`.
2. Rework memory-pool sizing and scope around expected entry count and benchmark workload, not only top-level bucket count.
3. Add a bloom filter to reduce negative-path cost on unique-key set traffic.
4. Revisit finer resize locking only after pool sizing and bloom-filter work are in place.
5. Keep background task manager refactor and logging abstraction in the v1 hardening scope.

### Current Benchmark Direction

- Baseline reference: the earlier global-hash-table design already passed a 1M-operation benchmark gracefully.
- Immediate goal: bring the current two-level design back to that 1M-operation level after routing and set-path fixes.
- Follow-up validation: run staged benchmark campaigns at 3M and 5M operations to validate scaling and latency stability.

## 10. v2.0 Planned

- [ ] Write-Ahead Log (WAL) — stubs marked `/* WAL: v2.0 */`
- [ ] Snapshot + checkpoint
- [ ] Crash recovery

---

## 11. Session Log

### 2026-03-23
- Full documentation refresh across all docs (README.md, API.md, ERROR_CODES.md, docs/architecture.md, docs/memory.md, docs/progress.md)
- **Major correction:** Hash function upgraded from MurmurHash3 32-bit to **64-bit** (`hash_function_murmur_64`, `uint64_t`). All docs updated.
- **Major correction:** Dual-seed composite key hashing implemented (`g_bucket_hash_seed` + `g_sub_bucket_hash_seed` with golden-ratio XOR separation). All docs updated.
- Seed generation now uses `clock_gettime(CLOCK_MONOTONIC)` nanosecond resolution instead of `time(NULL)`
- Error sentinel updated from `UINT32_MAX` to `UINT64_MAX` in all docs
- Checklist item "Generate 2 hash for hash table and sub hash table" marked as **DONE**
- Mix constants updated in §5.2: `0x87c37b91114253d5` / `0x4cf5ad432745937f` (block), `0xff51afd7ed558ccd` / `0xc4ceb9fe1a85ec53` (finalization)
- Block size updated from 4 bytes to 8 bytes in hash function docs
- All routing descriptions updated to reference `composite_key_hash.bucket_hash` and `.sub_bucket_hash`
- Priority order updated: dual-hash routing marked as completed; next priority is memory-pool refactor

### 2026-03-17
- Full memory refresh: re-read all source files, tests, and docs
- Documented new `RESIZE_CHECK_STATUS` enum value and `check_resize_status()` function in `hash_bucket_resizing_operation`
- Documented `cleanup_hash_bucket` busy-wait loop for ongoing resize completion
- Added missing types `delete_operation` and `delete_operation_buffer` to §3.1
- Updated concurrency test params: `NUM_THREADS=120`, `NUM_KEYS_PER_THREAD=150` (18K ops, down from 1M)
- Updated `insert_delete_operation_to_resizing_buffer` docs with capacity growth details (500-block increments)
- README.md updated with full architecture, concurrency model, error codes, and v1.0 checklist
- No new error/success codes, no new public API changes, no WAL stubs introduced
- Verified current repo still uses split update-then-add upsert flow in `sub_hash_table_operation.c`, which explains unique-key set miss overhead
- Corrected resize-trigger documentation to match `_check_for_resize_condition()` behavior based on `total_node_count` and deleted-node cleanup
- Verified current integration test frees both `out->value` and `out->key`; previous leak note is resolved
- Verified `tests/for_c/Makefile` includes `run-concurrency-test`, `run-ct-valgrind`, and `coverage-simple`
- Verified current source tree has no active `printf`/`fprintf` traces under `src/keystore`
- Recorded current product scope: v1 is single-node/in-memory/API-node focused; WAL, snapshots, crash recovery, and distribution remain post-v1
- Recorded agreed implementation order: routing/distribution → pool sizing/scope → bloom filter → finer resize locks
- Recorded benchmark direction: recover 1M-op parity first, then stage 3M and 5M runs

### 2026-03-16
- Reviewed and updated all type definitions, error/success codes, and concurrency model for accuracy
- Confirmed all public API signatures and module responsibilities
- Checked README.md, API.md, and ERROR_CODES.md for alignment with implementation
- No major architectural changes detected; documentation and code are in sync
- Checklist, invariants, and caveats remain current for v1.0

### 2026-03-09
- Full codebase review completed (all `.c` and `.h` files in `src/` and `tests/`)
- Documented all modules: types, functions, concurrency model, resize flow
- Identified 10 design notes / caveats
- Created this memory file as the canonical reference for future tasks
