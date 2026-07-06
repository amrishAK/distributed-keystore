# Benchmark Run History

Centralized index of all benchmark executions. Each dated directory contains metadata, metrics snapshots, and references to full JSONL results.

## Runs

| Date | Status | ST Correctness | MT Scaling | Memory | Resizes | Key Findings |
|------|--------|---|---|---|---|---|
| [2026-07-06 (Run 2)](./2026-07-06-run-2/RUN_METADATA.md) | ⚠️ Under Review | ❌ FAIL | ⚠️ 3.70x @ 16T | ❌ High (3.22GB @ 2000T) | ✅ Telemetry Active | ST2-CW-001 still failing; resize telemetry restored |

## How to Use

1. **Review latest run:** See [2026-07-06-run-2/RUN_METADATA.md](./2026-07-06-run-2/RUN_METADATA.md)
2. **Compare metrics:** [2026-07-06-run-2/METRICS_SNAPSHOT.md](./2026-07-06-run-2/METRICS_SNAPSHOT.md)
3. **Access raw results:** See links in each run's metadata file
4. **Track fixes:** Update metadata after addressing blocking issues

## Planned Upcoming Runs

- Post-correctness-fix run (after ST2-CW-001 is resolved)
- Post-resize-telemetry run (after resize capture is verified)
- Profiling run (with perf/Valgrind for lock contention and memory churn)
