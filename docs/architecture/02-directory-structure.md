# Directory Structure

The distributed keystore uses a **layered, modular architecture** organized into functional domains. The design emphasizes separation of concerns: the two-level hash table (level-1 global + level-2 sub-tables) sits at the core, surrounded by utilities and data structures.

> **See also:** [04-two-level-hash-table.md](04-two-level-hash-table.md), [07-concurrency-model.md](07-concurrency-model.md)

## Table of Contents
- [1. Module Structure](#1-module-structure)
- [2. Module Overview](#2-module-overview)
- [3. Key Notes](#3-key-notes)

## 1. Module Structure
|
|-- core/                          PUBLIC API layer — user-facing interface
|   |-- key_store.h
|   `-- key_store.c
|
|-- hash/                          INTERNAL — hashing algorithms & strategy
|   |-- hash_functions.h
|   `-- hash_functions.c
|
|-- hash_table/                    INTERNAL — level-1 (global) hash table
|   |-- hash_table_operation.h/c   core table operations
|   |-- hash_bucket_operation.h/c  single bucket logic
|   `-- resizing/
|       |-- resize_operation.h/c   orchestrate dynamic resizing
|       |-- hash_bucket_resizing_operation.h/c
|       `-- buffer_operation.h/c   temporary resizing buffers
|
|-- sub_hash_table/                INTERNAL — level-2 (sub) hash tables
|   |-- sub_hash_table_operation.h/c
|   |-- sub_hash_bucket_operation.h/c
|   `-- operation_handlers/        lock coordination & data paths
|       |-- coordination_handler.h/c
|       `-- data_handler.h/c
|
|-- data_structures/               INTERNAL — primitive data structures
|   |-- data_node_operation.h/c
|   |-- linked_list_operation.h/c
|   |-- double_linked_list_operation.h/c
|   `-- bloom_filter_operation.h/c  (active; production code)
|
|-- type_definitions/              INTERNAL — all struct, enum, & constant definitions
|   |-- custom_type_definitions.h
|   |-- hash_bucket_type_definition.h
|   |-- config_type_definitions.h
|   |-- error_code_definitions.h
|   |-- sucess_code_definitions.h
|   `-- background_task_manager_type_definitons.h
|
`-- utils/                         INTERNAL — cross-cutting infrastructure
    |-- memory_manager.h/c         centralized heap allocation
    |-- background_task_manager.h/c cleanup & async tasks
    `-- helper_functions.h/c       shared utilities (is_power_of_two, etc.)
```

## 2. Module Overview

| Module | Visibility | Purpose | Maturity |
|--------|------------|---------|----------|
| `core/` | PUBLIC | Entry point; exposes keystore operations | ✓ Stable |
| `hash/` | Internal | Consistent hashing for key distribution | ✓ Stable |
| `hash_table/` | Internal | Global level-1 table; manages buckets & resizing | ✓ Stable |
| `sub_hash_table/` | Internal | Per-bucket level-2 tables; lock coordination | ✓ Stable |
| `data_structures/` | Internal | Primitives: nodes, lists, Bloom filter | ✓ Stable |
| `type_definitions/` | Internal | Centralized type declarations | ✓ Stable |
| `utils/` | Internal | Memory, threading helpers, background tasks | ✓ Stable |

## 3. Key Notes

### Active vs. Future Scope
- **Active code:** `bloom_filter_operation.*` is production code, not reserved for future work.
- **Resizing:** Dynamic resizing logic in `hash_table/resizing/` is fully implemented and tested.

### Design Patterns
- **Lock coordination:** `operation_handlers/` sits in the hot path for sub-hash-bucket operations. It multiplexes between lock-wrapped (concurrent) and non-lock-wrapped (single-threaded fallback) execution paths.
- **Type centralization:** All type definitions live in one directory (`type_definitions/`) to prevent circular includes and simplify enum/constant discovery.

### Implementation Details
- `helper_functions.*` includes utilities like `is_power_of_two()` and `portable_sleep_ms()` for platform portability.
- Some legacy resize paths still call `usleep()` directly; this is a candidate for future unification via `helper_functions`.
