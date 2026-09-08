# Architecture Documentation

This directory contains detailed architectural documentation for KeyStore organized by concern. **Start with your use case:**

## For First-Time Readers
1. [01-overview.md](./01-overview.md) — System shape, components, responsibilities
2. [03-data-flow.md](./03-data-flow.md) — How requests flow through the system
3. [07-concurrency-model.md](./07-concurrency-model.md) — Lock hierarchy and thread safety

## For Performance Tuning
- [04-two-level-hash-table.md](./04-two-level-hash-table.md) — Why two levels, routing mechanics
- [06-hashing-strategy.md](./06-hashing-strategy.md) — Hash algorithm, seed strategy, distribution
- [08-dynamic-resizing.md](./08-dynamic-resizing.md) — When and how tables resize

## For Developers & Maintainers
- [02-directory-structure.md](./02-directory-structure.md) — Module layout and organization
- [05-data-structures.md](./05-data-structures.md) — Type definitions and memory layout
- [09-memory-management.md](./09-memory-management.md) — Allocation, pools, ownership rules
- [10-background-task-manager.md](./10-background-task-manager.md) — Background worker, resize coordination

## Document Roadmap

| Doc | Purpose | Audience | Length |
|-----|---------|----------|--------|
| 01-overview | System context and responsibility matrix | Everyone | ~60 lines |
| 02-directory-structure | Module organization, import structure | Developers | ~80 lines |
| 03-data-flow | Request flow, operation sequences, state changes | Everyone | ~150 lines |
| 04-two-level-hash-table | Two-level design, routing, collision handling | Operators, Perf tuners | ~120 lines |
| 05-data-structures | Struct definitions, memory layout | Developers | ~150 lines |
| 06-hashing-strategy | MurmurHash3, dual-seed design | Perf tuners | ~100 lines |
| 07-concurrency-model | Lock hierarchy, two-phase protocol, safety invariants | Developers, Reviewers | ~200 lines |
| 08-dynamic-resizing | Resize triggers, chase buffer, background worker | Developers | ~120 lines |
| 09-memory-management | Pools, allocation lifecycle, ownership | Developers, Maintainers | ~150 lines |
| 10-background-task-manager | Worker threads, task registry, lifecycle | Developers | ~100 lines |
| 11-design-decisions-trade-offs | Design rationale, known limitations | Architects, Reviewers | ~180 lines |

## Consolidation Notes (v1.1+ Roadmap)

Future versions will consolidate these 11 docs into 4-5 focused guides:
- **01-system-overview**: Merge 01 + 02 (context + structure)
- **02-data-flow-and-routing**: Merge 03 + 04 + 08 (operations + mechanics)
- **03-concurrency-and-locks**: Keep 07 (expand if needed)
- **04-memory-and-allocation**: Keep 09 (expand if needed)
- **05-hashing-and-performance**: Merge 06 + tuning advice

For now, **this README serves as the navigation hub**. Use it to find the right document for your question.

---

**See also:**
- [docs/DESIGN_DECISIONS.md](../DESIGN_DECISIONS.md) — Design choices, tradeoffs, known limitations
- [API.md](../../API.md) — Public API reference with examples
- [docs/progress.md](../progress.md) — v1.0 status, roadmap, known issues
