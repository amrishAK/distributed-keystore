# Distributed Keystore

Distributed Keystore is a high-performance, thread-safe key-value store implemented in C. It is designed for concurrent workloads, efficient memory management, and easy integration with other systems. The project uses a modular architecture, separating core logic, hash bucket management, memory pooling, and testing for maintainability and scalability. With robust error handling and comprehensive testing, it ensures reliability and correctness even under extreme concurrency.

## Key Features

- **Thread-Safe Hash Table**
    - Per-bucket `pthread_rwlock_t` for concurrent read/write operations.
    - Fine-grained locking for high concurrency and minimal contention.
- **Eager Initialization for Concurrency**
    - All buckets and locks are initialized up front in multi-threaded mode, eliminating race conditions.
    - Lazy initialization is used only in single-threaded mode for efficiency.
- **Custom Memory Pool**
    - Efficient allocation and reuse of list and tree nodes via a configurable memory pool.
    - Thread-safe allocation and free operations, with fallback to standard `malloc` if the pool is exhausted.
- **Flexible API**
    - FFI-friendly C API for easy integration with other languages or systems.
    - Supports binary and string data, with configurable bucket size and memory pool parameters.
    - Refer [Api documentation](./API.md)  for more details
- **Comprehensive Testing**
    - Unit tests for all core modules ensure correctness and coverage.
    - Integration and stress tests validate thread safety and performance under extreme concurrency.
- **Robust Error Handling & Diagnostics**
    - Detailed error codes for allocation, locking, and operation failures.
    - Diagnostic output in stress tests to track missing keys and concurrency issues.
- **Modular & Maintainable Design**
    - Clear separation of concerns: core logic, buckets, memory manager, tests.
    - Easy to extend or adapt for new data structures or features.
- **Scalable Performance**
    - Proven to handle thousands of threads and millions of operations with zero data loss.

## Directory Structure

```
examples/                # Example usage (main.c)
src/
    core/                  # Core keystore logic
    bucket/                # Hash bucket and list management
    utils/                 # Memory manager
    hash/                  # Hash functions
tests/
    for_c/
        unit_tests/          # Unit tests for modules
        integration_test/    # Concurrency and integration tests
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