# Distributed Keystore

Distributed Keystore is a fast, reliable, and thread-safe key-value store written in C. It is designed for high concurrency, efficient memory use, and easy integration with other systems. The project uses a modular structure and is tested for correctness and performance under heavy workloads.

## Key Features

- **Thread-Safe and Concurrent**
    - Uses per-bucket read-write locks for safe parallel access.
    - Uses per-node mutexes to ensure safe and consistent read/write operations on individual data nodes
    - Concurrency can be enabled or disabled via configuration.
- **Automatic Resizing**
    - Hash tables and sub-buckets grow automatically as you add more data.
    - Keeps performance high and collisions low, even with millions of keys.
- **Flexible Value Support**
    - Store any C data type: int, float, double, string, bytes, structs, enums, and more.
- **Custom Memory Pool**
    - Fast, efficient memory allocation and reuse for all internal data structures.
    - Falls back to standard malloc if needed.
- **Simple, FFI-Friendly API**
    - Clean C API for easy use in C, C++, or other languages via FFI.
    - Supports binary and structured data, with configurable table and pool sizes.
- **Comprehensive Testing**
    - 90%+ code coverage with unit and integration tests.
    - Stress-tested for thread safety and performance.
- **Detailed Error Handling**
    - All functions return clear error codes for easy debugging.
    - See [ERROR_CODES.md](./ERROR_CODES.md) for details.
- **Built-in Statistics**
    - Query stats at runtime: key counts, memory use, operation counters, and more.
- **Modular and Maintainable**
    - Clean separation of core logic, data structures, memory management, and tests.
    - Easy to extend for new features or data types.
- **Scalable and Proven**
    - Handles thousands of threads and millions of operations with no data loss.

## Data Structure

The keystore is composed of modular components for speed, scalability, and safety:

- **Hash Table:**
    - The top-level structure. Maps keys to hash buckets using a fast hash function.

- **Hash Buckets:**
    - Each bucket points to a sub-hash table for most operations.
    - During resizing, operations are temporarily stored in a pending linked list.

- **Sub-Hash Tables:**
    - Further divide the key space within each bucket, reducing collisions and supporting dynamic resizing.

- **Sub-Hash Buckets:**
    - Each sub-hash bucket contains a linked list of data nodes.
    - If the linked list chain exceeds a set limit, resizing is triggered.
    - Per-sub-hash bucket read-write locks ensure thread safety.

- **Linked Lists:**
    - Used to store data nodes within sub-hash buckets and for pending operations during resizing.

- **Data Nodes:**
    - Store each key-value pair, including the key (string), value (byte array, any C type), value size, and metadata (deletion flag, per-node mutex).
    - Data nodes are soft-deleted and cleaned up during resizing.

- **Memory Pool:**
    - All linked list nodes are allocated from a custom memory pool for fast, efficient memory reuse.
    - Falls back to standard malloc if needed.

**How it works:**
1. The key is hashed to find the correct bucket.
2. The bucket points to a sub-hash table (or, during resizing, a pending list).
3. The sub-hash table or list is searched for the key.
4. Data nodes store the actual key-value pairs, protected by per-node mutex locks for safe concurrent updates.

This design ensures high performance, low collision rates, and safe concurrent access—even with millions of keys and thousands of threads.


## Directory Structure

```
src/
    keystore/
        core/              # Core keystore logic (API in key_store.h)
        data_structures/   # Linked list and data node logic
        hash/              # Hash functions
        hash_table/        # Hash bucket and resizing logic
        sub_hash_table/    # Sub-bucket logic
        type_definitions/  # Type and error code definitions
        utils/             # Memory manager and helpers
examples/
    main.c               # Example usage
tests/
    for_c/
        unit_tests/        # Unit tests for all modules
        integration_test/  # Concurrency and integration tests
```

## Building and Running Tests

This project uses a Makefile for building and testing. Ensure you have `gcc` and `make` installed, and are on a POSIX-compatible system (Linux, macOS, or Windows with MinGW).

### Build All

```sh
make
```

### Run Unit Tests

```sh
make run-unit-tests
```

### Run Concurrency Stress Test

```sh
make run-concurrency-test
```

This will compile and run the concurrency test located in `tests/for_c/integration_test/concurrency_test.c`. The test will report the number of threads, keys per thread, and any missing keys after concurrent set operations.

## Example Output

```
Starting concurrency stress test...
Total threads: 1000
Number of keys per thread: 1000
Key missing after set (bucket-level concurrency): 0
Result: PASS
```

## Customization

- Adjust the number of threads and keys per thread in the concurrency test source file to stress test different scenarios.
- The memory pool pre-allocation factor and bucket size can be configured in the keystore initialization.

## Requirements

- GCC (or compatible C compiler)
- POSIX threads (pthreads)
- Make

## License

MIT

## Author

amrishAK