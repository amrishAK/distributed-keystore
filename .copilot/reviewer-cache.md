# Reviewer Cache (distributed-keystore)
<!-- Auto-maintained by C-Code-Reviewer. Do not edit manually. -->

## Language
C (project is C with .c/.h sources and pthread-based concurrency)

## Style Profile
- Naming       : snake_case (functions/variables), lower_snake typedef aliases
- Braces       : K&R (opening brace on same line)
- Indentation  : 4 spaces
- Comments     : // inline and Doxygen-style block headers
- Include order: project headers grouped first, then standard headers
- Consistency  : mostly consistent across touched files

## Architecture
- Core: keystore facade in src/keystore/core
- Hash Table: bucket and resize pipeline in src/keystore/hash_table
- Sub Hash Table: bucket operations split into operation_handlers for data/coordination
- Data Structures: linked list, data node, bloom filter helpers
- Utilities: memory manager, helper functions, background task manager

## Conventions
- Error strategy : integer error/success codes (SUCCESS / ERR_*)
- Ownership model: manual memory management via allocate_memory/free_memory wrappers
- Concurrency    : pthread mutex and rwlock wrappers in operation modules
- Test framework : Unity tests under tests/for_c/unit_tests

## Build Command
wsl -e bash -lc 'cd /mnt/d/Work/distributed-keystore/tests/for_c && make -j2'

## Open Issues
- None tracked

## Resolved Issues
- 2026-07-04: Removed unresolved symbol usage in coordination handler by eliminating calls to missing linked-list APIs.
- 2026-07-04: Restored correctness when bloom filter is disabled by falling back to linked-list lookup path.
- 2026-07-04: Fixed signedness mismatch in resize cleanup deleted-count handoff.
- 2026-07-04: Synced bloom-filter header docs with current check function signature.
