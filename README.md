# Distributed Keystore


Distributed Keystore is a high-performance, concurrent, in-memory key-value store written in C (C11). It features a two-level hash table architecture with fine-grained locking, lock-free resize signalling, and a background chase buffer worker for safe concurrent resizing. The design is modular, thoroughly tested, and optimized for both correctness and throughput under heavy workloads.

**Architecture Highlights:**
- Two-level hash table: hash table → hash bucket → sub-hash-table → sub-hash-bucket → linked list → data node
- Fine-grained locking: per-bucket `pthread_rwlock_t`, per-data-node `pthread_mutex_t`
- Lock-free resize signalling: `pthread_spinlock_t` for chase buffer
- Dual-seed MurmurHash3 (64-bit) for independent bucket and sub-bucket routing
- Optional memory pool for `linked_list_node` pre-allocation
- Background chase buffer worker for concurrent resize data migration


## Key Features

- **Thread-Safe and Concurrent**
    - Per-bucket read-write locks (`pthread_rwlock_t`) and per-data-node mutexes (`pthread_mutex_t`)
    - Lock-free chase buffer for concurrent resize
    - Concurrency can be enabled/disabled via configuration
- **Automatic, Safe Resizing**
    - Hash tables and sub-buckets double in size automatically as needed
    - Background resize worker and chase buffer ensure no data loss or race conditions
- **Flexible Value Support**
    - Store any C data type: int, float, double, string, bytes, structs, enums, etc.
- **Custom Memory Pool**
    - Fast, efficient memory pool for all `linked_list_node` allocations
    - Falls back to standard malloc if pool is exhausted
- **Comprehensive Testing**
    - 90%+ code coverage with Unity-based unit tests and integration stress tests
    - Stress-tested for thread safety and performance (120 threads × 150 keys, with p50/p99 latency tracking)
- **Detailed Error Handling**
    - All functions return clear error codes (see [ERROR_CODES.md](./ERROR_CODES.md))
- **Built-in Statistics**
    - Runtime stats: key counts, memory use, operation counters, per-error-code counters
- **Modular and Maintainable**
    - Clean separation of core logic, data structures, memory management, and tests
    - Easy to extend for new features or data types
- **Scalable and Proven**
    - Handles thousands of threads and millions of operations with no data loss


## Architecture & Data Flow

The keystore is built from modular components for speed, scalability, and safety:

- **Hash Table:** Top-level structure, maps keys to hash buckets using MurmurHash3 (64-bit). Each key produces a `composite_key_hash` with separate `bucket_hash` and `sub_bucket_hash` computed from two independent seeds.
- **Hash Buckets:** Each bucket points to a sub-hash table. During resizing, a snapshot and chase buffer are used for safe migration.
- **Sub-Hash Tables:** Further divide the key space within each bucket, reducing collisions and supporting dynamic resizing. Bucket index is a bitmask (`key_hash & (total_blocks - 1)`).
- **Sub-Hash Buckets:** Each contains a linked list of data nodes. Per-bucket `pthread_rwlock_t` ensures thread safety. Resizing is triggered when the chain exceeds a set limit.
- **Linked Lists:** Used for collision chains and for pending operations during resizing. All `linked_list_node` allocations use a custom memory pool.
- **Data Nodes:** Store key-value pairs, including key (string), value (byte array), value size, and metadata (deletion flag, per-node mutex). Soft-deleted nodes are cleaned up during resizing.
- **Chase Buffer:** A doubly-linked list protected by a spinlock, used to record all writes during an active resize. A background chase worker migrates these writes to the new sub-hash-table.
- **Memory Pool:** Pre-allocated arena for `linked_list_node` (thread-safe). Falls back to malloc if exhausted.

**Operation Flow:**
1. Key is hashed twice (MurmurHash3-64 with two independent seeds) to produce a `composite_key_hash` containing `bucket_hash` and `sub_bucket_hash`.
2. `bucket_hash` routes to the correct hash bucket; `sub_bucket_hash` routes within the sub-hash table.
3. Sub-hash table/bucket is searched for the key.
4. Data nodes store the actual key-value pairs, protected by per-node mutex locks for safe concurrent updates.

**Resize Flow:**
- When a sub-hash-bucket's chain exceeds `max_linked_list_chain_length`, a resize is triggered.
- A snapshot of the sub-hash-table is taken, and a background worker migrates data to a new, larger table.
- All new writes during resize are recorded in the chase buffer and migrated by a chase worker.
- After migration, the new table replaces the old, and normal operation resumes.

**Concurrency Model:**
- `hash_bucket.resizing_lock` (pthread_mutex_t) guards all resize state transitions.
- `sub_hash_bucket.sub_hash_bucket_lock` (pthread_rwlock_t) guards linked list structure only (traversal, insert, list-level cleanup).
- `data_node.lock` (pthread_mutex_t) guards per-node value access — acquired **after** the sub-bucket rwlock is released.

The two locks operate in **sequential phases, never held simultaneously**:
- **Phase 1 (rwlock):** acquire → traverse list → capture node pointer → **release**
- **Phase 2 (node mutex):** acquire → check `is_deleted` → read/write value or soft-delete → **release**

Releasing the rwlock before acquiring the node mutex allows threads targeting *different nodes within the same sub-bucket* to run Phase 2 in parallel, beyond what a single rwlock would permit. The `is_deleted` flag (checked under the node mutex) closes the use-after-free race in the window between rwlock release and node mutex acquisition: a concurrent DELETE sets `is_deleted = true` under the node mutex before unlinking the node, so any reader hitting the same node in that window returns `ERR_DATA_NODE_NOT_FOUND` rather than accessing freed memory.

**Key Invariants:**
- `snapshot_sub_hash_table_ptr` is read-only during resize.
- `new_operation_buffer` (chase buffer) is spinlock-protected; writers at head, chase worker at tail.
- After `is_resizing` becomes `false`, all ops revert to normal paths.



## Directory Structure

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
examples/
    main.c              — Example usage

tests/for_c/
    unit_tests/         — Unity-based unit tests (one file per module)
    integration_test/   — Concurrency stress test (2,000 threads × 2,000 keys, 8M ops)
    include/            — Unity test framework source
    Makefile            — Targets: test, valgrind-test, coverage, run-concurrency-test, run-ct-valgrind
```


## Building and Testing

This project uses a Makefile (in `tests/for_c/`) for building and testing. Ensure you have `gcc` and `make` installed, and are on a POSIX-compatible system (Linux, macOS, or Windows with MinGW).

### Run Unit Tests

```sh
make test
```

Builds all keystore sources and the Unity test runner, then executes the unit test suite.

### Run Unit Tests Under Valgrind

```sh
make valgrind-test
```

Builds the test binary without coverage instrumentation and runs it under Valgrind with `--leak-check=full --track-origins=yes`.

### Run Concurrency Stress Test

```sh
make run-concurrency-test
```

Compiles and runs the concurrency stress test in `integration_test/concurrency_test.c`. The test spawns 2,000 threads × 2,000 keys (8M total ops) and reports missing keys, p50/p99 latencies, throughput, and race errors after concurrent set/get operations.

### Run Concurrency Test Under Valgrind

```sh
make run-ct-valgrind
```

### Coverage

```sh
make coverage          # generates .gcov files; set LCOV=1 for HTML report
make coverage-simple   # quick .gcov files only
```

### Clean

```sh
make clean
```

Run `make help` to see all available targets.

## Example Output

```
Starting concurrency stress test...
Test scenario: Bucket-level concurrency with 2000 threads each setting/getting 2000 unique keys.
==== Concurrency Test Report ====
Initialization: bucket_size=1024, sub_bucket_size=1024, max_chain_length=15, concurrency_enabled=true
Total threads: 2000
Number of keys per thread: 2000
Set operations: 4000000, Get operations: 4000000
Total ops: 8000000
Total time: 1.862s
Throughput: 4295904.57 ops/sec
SET latency (ns): avg=3565, p50=1107, p95=3122, p99=9466
GET latency (ns): avg=1130, p50=302, p95=604, p99=4330
Key missing after set (bucket-level concurrency): 0
Number of resizings triggered: 7
Result: PASS
```

## Error and Success Codes

All functions return explicit error or success codes. See [ERROR_CODES.md](./ERROR_CODES.md) for the full list. Common codes include:

| Code  | Name         | Description                  |
|-------|--------------|------------------------------|
| 0     | SUCCESS      | Operation completed successfully |
| -1    | ERR_FAILURE  | General/unspecified failure  |
| -20   | ERR_MEMORY_ALLOCATION_FAILED | Memory allocation failed |
| -30   | ERR_RW_LOCK_ACQUIRE_FAILED | RW lock acquire failed |
| -40   | ERR_HASH_COMPUTE_FAILED | Hash computation failure |
| -50   | ERR_HASH_TABLE_NOT_INITIALIZED | Hash table not initialized |
| -80   | ERR_DATA_NODE_NOT_FOUND | Data node not found |
| 10    | SUCESS_ADDED_NEW_NODE | New node inserted |
| 20    | SUCESS_ADDED_NEW_NODE_RESZING_TRIGGERED | New node inserted, resize triggered |

## Hashing

The keystore uses **MurmurHash3 (64-bit)** with two independent per-instance seeds generated via `clock_gettime(CLOCK_MONOTONIC)`. The two seeds are guaranteed distinct by XOR-ing the second with the golden-ratio constant `0x9e3779b97f4a7c15`. Each key produces a `composite_key_hash`:

- `bucket_hash` — routes to the top-level hash bucket (`bucket_hash % total_buckets`)
- `sub_bucket_hash` — routes within the sub-hash table (`sub_bucket_hash & (sub_buckets - 1)`)

This dual-hash design eliminates the correlation between level-1 and level-2 routing that existed when a single hash was split across both levels, improving key distribution under load.

## Known Design Notes & Caveats

1. **Soft-delete semantics:** Delete operations mark `data_node.is_deleted = true` under the node mutex. Physical removal happens only during `cleanup_deleted_linked_list_nodes` (triggered at resize or chain-length threshold, under a WRITE rwlock). `active_node_count` tracks live nodes; `total_node_count` includes soft-deleted. The `is_deleted` flag also closes the use-after-free race in the window between rwlock release and node mutex acquisition — see Concurrency Model above.
2. **Resize only doubles:** The resize strategy always multiplies `bucket_size × 2`. No shrinking implemented.
3. **Memory pool scope:** Only `linked_list_node` allocations use the pre-allocated pool. `data_node`, `double_linked_list_node`, and management structs use standard `malloc`/`calloc`.
4. **`free_memory` pool flag:** Nearly all callers pass `is_pool = false`; only the memory manager itself uses `is_pool = true` internally via `_free_memory_to_pool`.
5. **`key_hash == 0` treated as invalid:** `_get_hash_table_bucket` and `_get_sub_hash_table_bucket` reject `key_hash == 0`. Keys that naturally hash to 0 would fail. `UINT64_MAX` is the murmur error sentinel.
6. **No WAL / persistence:** v1.0 is fully in-memory. WAL stubs are reserved for v2.0 (marked `/* WAL: v2.0 */`).
7. **Background task registry capacity:** Hard-coded at 100. Overflow may cause silent failures in extreme concurrency scenarios.
8. **Debug traces:** `printf` debug traces are present throughout; should be removed or guarded for production.
9. **Platform notes:** `usleep(100)` in chase worker loop is POSIX only; Windows build uses `Sleep(0)`.
10. **Memory ownership:** `read_data_node_value` allocates both `key` and `value` — caller must `free()` both. The integration test correctly frees `value` but skips `free(key)` (potential minor leak).

## v1.0 Checklist

- [x] Two-level hash table with bucket RW-locks
- [x] Per-data-node mutex for value updates
- [x] Fine-grained spinlock chase buffer during resize
- [x] Full CRUD (create/read/update/delete with soft-delete)
- [x] Memory pool for linked list nodes
- [x] Background resize worker (doubles sub-table bucket size)
- [x] Stress test: 2,000 threads × 2,000 keys — 8M ops, 4.3M ops/s, 7 resizes, 0 data loss
- [x] Unity unit tests across all modules
- [x] Valgrind clean confirmed (630/630 allocs/frees, 0 leaks, 0 errors — unit; 24M/24M — integration)
- [ ] `printf` debug output removed / guarded
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

## License

Apache 2.0 — see [LICENSE](./LICENSE) for full terms.

Copyright 2026 Amrish Arunachalam Kulasekaran

## Author

Amrish Arunachalam Kulasekaran (amrishAK)