| Metric | Single-Thread | Multi-Thread (Best) |
|--------|---|---|
| **Throughput** | ST2-CORE-003-run-2: 814,645.85 ops/s | MT2-CORE-005-run-3: 1,591,390.00 ops/s (16T balanced cold) |
| **Latency p50** | ST2-CORE-003-run-2: 919 ns | MT2-CORE-002-run-1: 4,183 ns |
| **Latency p99** | ST2-CORE-001-run-5: 3,469 ns | MT2-CORE-005-run-3: 25,408 ns |
| **Memory Peak** | ~937,520 KB RSS (~915 MB) | 1,110,836 KB @ 16T; 3,375,184 KB @ 2000T |
| **Scaling Factor (1T->16T)** | N/A | **3.70x** (412,336 -> 1,524,112 mean ops/s) |
| **Correctness** | ❌ FAIL (`ST2-CW-001`, 10066 missing) | ✅ PASS (verification passed in core/mix suites) |
| **Resize Detection** | ✅ PASS (`ST2-RSZ-*` non-zero counts) | ✅ PASS (`MT2-RSZ-*` non-zero counts) |

## Status Flags

| Dimension | Status | Notes |
|-----------|--------|-------|
| Release-Ready | ❌ NO | Correctness blocker remains (`ST2-CW-001`) |
| Scaling Acceptable | ⚠️ PARTIAL | 3.70x @ 16T, then flattening/decline |
| Memory Efficiency | ❌ POOR | Multi-thread oversubscription still >3 GB |
| Resize Telemetry | ✅ IMPROVED | Dedicated resize tests now emit counts and timelines |
