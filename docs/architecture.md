# KeyStore — Architecture Guide

The architecture guide is now split into section-based sub-documents under [docs/architecture/README.md](./architecture/README.md).

## Split Architecture Docs

1. [High-Level Overview](./architecture/01-overview.md)
2. [Directory Structure](./architecture/02-directory-structure.md)
3. [Data Flow — How a Request Travels](./architecture/03-data-flow.md)
4. [Two-Level Hash Table Architecture](./architecture/04-two-level-hash-table.md)
5. [Data Structures In Depth](./architecture/05-data-structures.md)
6. [Hashing Strategy](./architecture/06-hashing-strategy.md)
7. [Concurrency Model](./architecture/07-concurrency-model.md)
8. [Dynamic Resizing Architecture](./architecture/08-dynamic-resizing.md)
9. [Memory Management](./architecture/09-memory-management.md)
10. [Background Task Manager](./architecture/10-background-task-manager.md)
11. [Error Handling Model](./architecture/11-error-handling.md)
12. [Public API Surface](./architecture/13-public-api-surface.md)
13. [Design Decisions & Trade-offs](./architecture/14-design-decisions-trade-offs.md)

## Notes

- The split follows the original section and subsection boundaries from the monolithic guide.
- Segment content has been updated to reflect the current codebase, including Bloom filter behavior, power-of-two bucket validation, current resize trigger logic, and background task manager behavior.
