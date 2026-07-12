# Benchmark Run: 2026-07-06 (Run 2)

**Date Run:** 2026-07-06  
**Status:** Under Review  

## Test Coverage

- **Single-thread:** V2 suite (core, correctness, distribution, resize, memory)
- **Multi-thread:** Core scaling (1T-32T), workload mixes, distribution, resize, oversubscription
- **Distribution patterns:** Uniform, Zipf, bursty
- **Value sizes:** 64B, 1KB

## Key Findings Summary

### Critical Issues

1. **Correctness blocker remains:** `ST2-CW-001` still fails all 5 repetitions with `verification_passed=false`, `failure_count=10066`, `missing_count=10066`.

2. **Scaling ceiling unchanged:** Multi-thread scaling reaches only about **3.70x** from 1T to 16T (`412,336` to `1,524,112` mean ops/sec), then declines at 32T.

3. **Memory footprint remains high at scale:** Multi-thread oversubscription peaks near **3.22 GB RSS** (`3,375,184 KB`).

### Improvements Since Previous Run

- **Resize telemetry is now active:** Dedicated resize scenarios report non-zero `resize_count` in both ST and MT suites.
- **Resize timeline populated:** `resize_events_timeline` now contains event data in stress scenarios.

## Difference vs Previous Run (2026-07-06)

- **Correctness:** No change. `ST2-CW-001` still fails with 10066 missing keys.
- **Resize observability:** Improved. Previous run reported broken/zeroed resize telemetry; this run records non-zero resize counts and timelines.
- **Scaling:** Slightly lower at 16T (3.70x vs 3.73x previously), with the same early plateau pattern.
- **Memory profile:** Still high under oversubscription; peak RSS now explicitly observed at 3,375,184 KB in 2000T runs.
- **Overall status:** Still under review, but with better visibility into resize behavior than the prior run.

### Performance Observations

- **Core scaling:** Good gain through 4T; flattening starts by 8T and is clear by 16T+.
- **Skew sensitivity:** Zipf workloads still show strong throughput collapse and tail-latency inflation.
- **Read-heavy penalty:** Read-dominant mixes remain slower than balanced/write-heavy mixes.

## Next Actions

### Blocking (Must Fix Before Release)

- [ ] Debug `ST2-CW-001` deterministic key loss path (10066 missing)
- [ ] Validate root cause and fix for cold correctness path before rerun
- [ ] Re-check report aggregation and comparison pairing in generated summaries

### Important (Quality/Performance)

- [ ] Profile lock contention under Zipf workloads (8T/16T)
- [ ] Run memory attribution to separate allocator churn vs structural overhead
- [ ] Add per-phase timing around resize path to explain scaling plateau

### Nice-to-Have

- [ ] Add latency histograms by workload family
- [ ] Track p99 and max latency regressions as release gates
- [ ] Add controlled 64T/128T stability run after correctness fix

## Results Location

- **Single-thread JSONL:** `benchmark/results/single_thread_results.jsonl`
- **Multi-thread JSONL:** `benchmark/results/multi_thread_results.jsonl`
- **HTML reports:** `benchmark/results/t_single.html`, `benchmark/results/t_multi.html`

## Reference Documents

- Main summary: [docs/benchmark-architecture-summary.md](../../docs/benchmark-architecture-summary.md)
- Architecture docs: [docs/architecture/](../../docs/architecture/)
- Benchmark design: [benchmark/docs/BENCHMARK.md](../docs/BENCHMARK.md)
