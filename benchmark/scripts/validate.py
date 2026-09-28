#!/usr/bin/env python3

import argparse
import json
import statistics
import sys
from pathlib import Path


def load_results(path: Path) -> dict[str, list[dict[str, object]]]:
    scenarios: dict[str, list[dict[str, object]]] = {}
    with path.open(encoding="utf-8") as results_file:
        for line_number, line in enumerate(results_file, start=1):
            if not line.strip():
                continue
            try:
                result = json.loads(line)
                test_id = str(result["test_id"])
            except (json.JSONDecodeError, KeyError) as error:
                raise ValueError(f"{path}:{line_number}: invalid benchmark result: {error}") from error
            scenarios.setdefault(test_id, []).append(result)
    return scenarios


def median_throughput(
    scenarios: dict[str, list[dict[str, object]]], test_id: str
) -> float:
    results = scenarios.get(test_id)
    if not results:
        raise ValueError(f"required scenario {test_id} is missing")

    throughputs: list[float] = []
    for result in results:
        if not result.get("verification_passed", False):
            raise ValueError(f"{test_id} reported a verification failure")
        if int(result.get("failure_count", 0)) or int(result.get("missing_count", 0)):
            raise ValueError(f"{test_id} reported failed or missing operations")
        throughputs.append(float(result["throughput_ops_sec"]))
    return statistics.median(throughputs)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Validate release benchmark targets")
    parser.add_argument("single_thread_results", type=Path)
    parser.add_argument("multi_thread_results", type=Path)
    parser.add_argument("--minimum-single-thread-ops", type=float, default=2_000_000)
    parser.add_argument("--minimum-multi-thread-ops", type=float, default=28_000_000)
    parser.add_argument("--minimum-scaling-efficiency", type=float, default=0.90)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        single_thread = load_results(args.single_thread_results)
        multi_thread = load_results(args.multi_thread_results)
        single_ops = median_throughput(single_thread, "ST2-CORE-001")
        one_thread_ops = median_throughput(multi_thread, "MT2-CORE-001")
        sixteen_thread_ops = median_throughput(multi_thread, "MT2-CORE-005")
        thirty_two_thread_ops = median_throughput(multi_thread, "MT2-CORE-006")
    except (OSError, TypeError, ValueError) as error:
        print(f"Benchmark validation failed: {error}", file=sys.stderr)
        return 1

    scaling_efficiency = sixteen_thread_ops / (one_thread_ops * 16.0)
    checks = (
        ("single-thread throughput", single_ops, args.minimum_single_thread_ops),
        ("32-thread throughput", thirty_two_thread_ops, args.minimum_multi_thread_ops),
        ("16-thread scaling efficiency", scaling_efficiency, args.minimum_scaling_efficiency),
    )

    failed = False
    for name, actual, minimum in checks:
        status = "PASS" if actual >= minimum else "FAIL"
        print(f"{status}: {name}: {actual:.2f} (minimum {minimum:.2f})")
        failed |= actual < minimum
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())