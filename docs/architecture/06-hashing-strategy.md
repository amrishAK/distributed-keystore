# Hashing Strategy

## Table of Contents
- [1. Overview](#1-overview)
- [2. Hashing Pipeline](#2-hashing-pipeline)
- [3. Dual-Seed Strategy](#3-dual-seed-strategy)
- [4. Special Cases](#4-special-cases)
- [5. Implementation Notes](#5-implementation-notes)

## 1. Overview

The keystore uses **MurmurHash3 (64-bit)** to map keys uniformly across the two-level hash table hierarchy. Each instance maintains two independent seeds: one for top-level bucket selection (`bucket_hash`), one for sub-hash-table routing (`sub_bucket_hash`). Independent seeds prevent correlated hash drift across routing levels.

> **Related:** See [Data Structures In Depth](05-data-structures.md), §2 for how the composite hash is stored in `key_hash` and used for routing. See §4 for Bloom filter hashing details.

## 2. Hashing Pipeline

```
┌─────────────────────────────────────────────────────────────┐
│  Input: Key String + Seed (64-bit)                          │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
            ┌──────────────────────────────┐
            │  MurmurHash3-64 Processing   │
            │  ─────────────────────────   │
            │  • Process 8-byte blocks     │
            │  • Process tail (0–7 bytes)  │
            │  • Avalanche finalization    │
            └──────────────────────────────┘
                           │
                           ▼
          ┌────────────────────────────────┐
          │  64-bit Hash Output            │
          │  (reduced via mask to fit      │
          │   routing dimensions)          │
          └────────────────────────────────┘
```

**Algorithm choice:** MurmurHash3 provides:
- **Speed:** O(1) per key, minimal CPU cost
- **Uniformity:** Low collision rate across range
- **Avalanche:** Small input changes → large output changes, preventing clustering

## 3. Dual-Seed Strategy

Two independent seeds derive two distinct routing hashes from each key, eliminating correlated hash output across the hierarchy:

```
                      Key + Seed₁              Key + Seed₂
                           │                         │
                           ▼                         ▼
                     MurmurHash3                MurmurHash3
                           │                         │
                           ▼                         ▼
                    bucket_hash              sub_bucket_hash
                 (top-level routing)      (second-level routing)
                           │                         │
                           ▼                         ▼
                  [Bucket Index]          [Sub-Bucket Index]
```

**Seed generation:** During keystore initialization, two distinct random 64-bit seeds are generated. Using the same seed twice would correlate output; independent seeds ensure each routing layer receives independent hash entropy.

**Problem solved:** In earlier designs, one hash output was split across both levels (e.g., lower 16 bits for buckets, upper 16 bits for sub-buckets). This created correlation: keys mapping to bucket *i* tended to cluster in sub-buckets with correlated indices. Two independent seeds remove this coupling.

## 4. Special Cases

| Scenario | Behavior | Rationale |
|----------|----------|-----------|
| `NULL` key | Returns `UINT64_MAX` | Error sentinel; prevents null-pointer traversal in lookup paths. |
| Empty string `""` | Hashes normally | Valid key; produces deterministic hash like any other string. |

## 5. Implementation Notes

- Seeds are stored in the keystore instance and reused for all subsequent keys.
- Hash outputs are masked (bitwise AND) to fit routing dimensions (e.g., `bucket_hash & (num_buckets - 1)`).
- No cryptographic security assumed; hashing is strictly for uniform distribution.

> **See also:** [Two-Level Hash Table Architecture](04-two-level-hash-table.md) for routing mechanics, lookup process, and architectural rationale.
