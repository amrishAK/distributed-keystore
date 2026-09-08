#!/usr/bin/env python3
"""Generate V2 benchmark HTML report from JSONL output."""

from __future__ import annotations

import argparse
import html
import json
from collections import defaultdict
from pathlib import Path


def load_results(path: Path) -> list[dict]:
    rows: list[dict] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line:
            continue
        rows.append(json.loads(line))
    return rows


def parse_timeline(raw: str | None) -> list[dict]:
    if not raw:
        return []
    slices: list[dict] = []
    for part in raw.split("|"):
        cols = part.split(":")
        if len(cols) != 5:
            continue
        slices.append(
            {
                "slice": int(cols[0]),
                "ops": int(cols[1]),
                "p99": int(cols[2]),
                "resize_delta": int(cols[3]),
                "max_chain_depth": int(cols[4]),
            }
        )
    return slices


def fmt_float(value: float, digits: int = 2) -> str:
    return f"{value:,.{digits}f}"


def fmt_int(value: int | float | None) -> str:
    if value is None:
        return "n/a"
    return f"{int(value):,}"


def scenario_groups(rows: list[dict]) -> dict[str, list[dict]]:
    groups: dict[str, list[dict]] = defaultdict(list)
    for row in rows:
        groups[str(row.get("test_id", "unknown"))].append(row)
    for _, items in groups.items():
        items.sort(key=lambda item: int(item.get("run_index", 0)))
    return groups


def one_row_per_scenario(rows: list[dict]) -> list[dict]:
    grouped = scenario_groups(rows)
    merged: list[dict] = []
    for _, runs in grouped.items():
        merged.append(runs[0])
    return sorted(merged, key=lambda item: str(item.get("test_id", "")))


def render_scenario_table(title: str, scenarios: list[dict], max_mean: float) -> str:
    parts: list[str] = [f"<h2>{html.escape(title)}</h2>"]
    if not scenarios:
        parts.append('<p class="muted">No scenarios in this section.</p>')
        return "\n".join(parts)

    parts.append("<table>")
    parts.append(
        "<thead><tr><th>Scenario</th><th>Variant</th><th>Distribution</th><th>Value</th><th>Keyspace</th><th>Mean ops/s</th><th>Stdev</th><th>CV</th><th>Latency p50/p95/p99 (ns)</th><th>Status</th></tr></thead><tbody>"
    )

    for row in scenarios:
        mean = float(row.get("mean_ops_sec", 0.0))
        stdev = float(row.get("stdev_ops_sec", 0.0))
        cv = float(row.get("cv_ops_sec", 0.0))
        ratio = 0.0 if max_mean <= 0.0 else max(2.0, (mean / max_mean) * 100.0)
        status_ok = bool(row.get("verification_passed", False)) and int(row.get("failure_count", 0)) == 0 and int(row.get("missing_count", 0)) == 0

        parts.append("<tr>")
        parts.append(f"<td><strong>{html.escape(str(row.get('test_id', '')))}</strong><div class='muted'>{html.escape(str(row.get('title', '')))}</div></td>")
        parts.append(f"<td>{html.escape(str(row.get('variant', '')))}</td>")
        parts.append(f"<td>{html.escape(str(row.get('distribution', '')))}</td>")
        parts.append(f"<td>{fmt_int(row.get('value_size_bytes'))} B</td>")
        parts.append(f"<td>{fmt_int(row.get('keyspace_size'))}</td>")
        parts.append(f"<td>{fmt_float(mean)}<div class='bar'><div class='fill' style='width:{ratio:.1f}%'></div></div></td>")
        parts.append(f"<td>{fmt_float(stdev)}</td>")
        parts.append(f"<td>{fmt_float(cv, 4)}</td>")
        parts.append(
            f"<td>{fmt_int(row.get('latency_p50_ns'))} / {fmt_int(row.get('latency_p95_ns'))} / {fmt_int(row.get('latency_p99_ns'))}</td>"
        )
        parts.append(f"<td class='{"ok" if status_ok else "bad"}'>{'PASS' if status_ok else 'CHECK'}</td>")
        parts.append("</tr>")

    parts.append("</tbody></table>")
    return "\n".join(parts)


def render_cold_warm_pairs(scenarios: list[dict]) -> str:
    by_signature: dict[tuple, dict[str, dict]] = defaultdict(dict)
    for row in scenarios:
        signature = (
            row.get("category"),
            row.get("distribution"),
            int(row.get("value_size_bytes", 0)),
            int(row.get("keyspace_size", 0)),
        )
        by_signature[signature][str(row.get("variant", ""))] = row

    parts: list[str] = ["<h2>Cold vs Warm Comparisons</h2>"]
    parts.append("<table>")
    parts.append("<thead><tr><th>Signature</th><th>Cold Mean</th><th>Warm Mean</th><th>Delta</th></tr></thead><tbody>")
    for signature, variants in sorted(by_signature.items(), key=lambda item: str(item[0])):
        cold = variants.get("cold")
        warm = variants.get("warm")
        if cold is None or warm is None:
            continue

        cold_mean = float(cold.get("mean_ops_sec", 0.0))
        warm_mean = float(warm.get("mean_ops_sec", 0.0))
        delta = warm_mean - cold_mean
        sign = "+" if delta >= 0.0 else ""

        parts.append("<tr>")
        parts.append(f"<td>{html.escape(str(signature))}</td>")
        parts.append(f"<td>{fmt_float(cold_mean)}</td>")
        parts.append(f"<td>{fmt_float(warm_mean)}</td>")
        parts.append(f"<td>{sign}{fmt_float(delta)}</td>")
        parts.append("</tr>")
    parts.append("</tbody></table>")
    return "\n".join(parts)


def render_resize_diagnostics(scenarios: list[dict]) -> str:
    parts: list[str] = ["<h2>Section C: Resize Diagnostics</h2>"]
    if not scenarios:
        parts.append('<p class="muted">No resize scenarios found.</p>')
        return "\n".join(parts)

    parts.append("<table>")
    parts.append(
        "<thead><tr><th>Scenario</th><th>Resize Count</th><th>Max Chain Depth</th><th>Timeline (slice:ops:p99:resize_delta:max_chain)</th></tr></thead><tbody>"
    )
    for row in scenarios:
        timeline = parse_timeline(str(row.get("resize_events_timeline", "")))
        encoded = " | ".join(
            f"{s['slice']}:{s['ops']}:{s['p99']}:{s['resize_delta']}:{s['max_chain_depth']}"
            + ("*" if s["resize_delta"] > 0 else "")
            for s in timeline
        )
        parts.append("<tr>")
        parts.append(f"<td>{html.escape(str(row.get('test_id', '')))}</td>")
        parts.append(f"<td>{fmt_int(row.get('resize_count'))}</td>")
        parts.append(f"<td>{fmt_int(row.get('max_chain_depth'))}</td>")
        parts.append(f"<td>{html.escape(encoded if encoded else 'n/a')}</td>")
        parts.append("</tr>")
    parts.append("</tbody></table>")
    parts.append('<p class="muted">Slices marked with * indicate resize_count delta > 0.</p>')
    return "\n".join(parts)


def render_memory_diagnostics(scenarios: list[dict]) -> str:
    parts: list[str] = ["<h2>Section D: Memory Diagnostics</h2>"]
    if not scenarios:
        parts.append('<p class="muted">No memory scenarios found.</p>')
        return "\n".join(parts)

    parts.append("<table>")
    parts.append(
        "<thead><tr><th>Scenario</th><th>Peak RSS (KB)</th><th>RSS Delta (KB)</th></tr></thead><tbody>"
    )
    for row in scenarios:
        parts.append("<tr>")
        parts.append(f"<td>{html.escape(str(row.get('test_id', '')))}</td>")
        parts.append(f"<td>{fmt_int(row.get('peak_rss_kb'))}</td>")
        parts.append(f"<td>{fmt_int(row.get('rss_delta_kb'))}</td>")
        parts.append("</tr>")
    parts.append("</tbody></table>")
    return "\n".join(parts)


def render_multi_thread_table(title: str, scenarios: list[dict], max_mean: float) -> str:
    parts: list[str] = [f"<h2>{html.escape(title)}</h2>"]
    if not scenarios:
        parts.append('<p class="muted">No scenarios in this section.</p>')
        return "\n".join(parts)

    parts.append("<table>")
    parts.append(
        "<thead><tr><th>Scenario</th><th>Threads</th><th>Variant</th><th>Distribution</th><th>Workload</th><th>Value</th><th>Keyspace</th><th>Mean ops/s</th><th>Stdev</th><th>CV</th><th>Latency p50/p95/p99 (ns)</th><th>Status</th></tr></thead><tbody>"
    )

    for row in scenarios:
        mean = float(row.get("mean_ops_sec", 0.0))
        stdev = float(row.get("stdev_ops_sec", 0.0))
        cv = float(row.get("cv_ops_sec", 0.0))
        ratio = 0.0 if max_mean <= 0.0 else max(2.0, (mean / max_mean) * 100.0)
        status_ok = bool(row.get("verification_passed", False)) and int(row.get("failure_count", 0)) == 0 and int(row.get("missing_count", 0)) == 0

        parts.append("<tr>")
        parts.append(f"<td><strong>{html.escape(str(row.get('test_id', '')))}</strong><div class='muted'>{html.escape(str(row.get('title', '')))}</div></td>")
        parts.append(f"<td>{fmt_int(row.get('threads'))}</td>")
        parts.append(f"<td>{html.escape(str(row.get('variant', '')))}</td>")
        parts.append(f"<td>{html.escape(str(row.get('key_distribution', row.get('distribution', ''))))}</td>")
        parts.append(f"<td>{html.escape(str(row.get('workload', '')))}</td>")
        parts.append(f"<td>{fmt_int(row.get('value_size_bytes'))} B</td>")
        parts.append(f"<td>{fmt_int(row.get('keyspace_size'))}</td>")
        parts.append(f"<td>{fmt_float(mean)}<div class='bar'><div class='fill' style='width:{ratio:.1f}%'></div></div></td>")
        parts.append(f"<td>{fmt_float(stdev)}</td>")
        parts.append(f"<td>{fmt_float(cv, 4)}</td>")
        parts.append(
            f"<td>{fmt_int(row.get('latency_p50_ns'))} / {fmt_int(row.get('latency_p95_ns'))} / {fmt_int(row.get('latency_p99_ns'))}</td>"
        )
        parts.append(f"<td class='{'ok' if status_ok else 'bad'}'>{'PASS' if status_ok else 'CHECK'}</td>")
        parts.append("</tr>")

    parts.append("</tbody></table>")
    return "\n".join(parts)


def build_multi_thread_html(rows: list[dict]) -> str:
    scenarios = one_row_per_scenario(rows)
    max_mean = max((float(row.get("mean_ops_sec", 0.0)) for row in scenarios), default=0.0)

    issue_count = sum(
        1
        for row in rows
        if (not bool(row.get("verification_passed", False)))
        or int(row.get("failure_count", 0)) > 0
        or int(row.get("missing_count", 0)) > 0
    )

    parts: list[str] = []
    parts.append("<!doctype html><html lang='en'><head><meta charset='utf-8'>")
    parts.append("<meta name='viewport' content='width=device-width, initial-scale=1'>")
    parts.append("<title>KeyStore Multi-Thread V2 Report</title>")
    parts.append("<style>")
    parts.append("body{font-family:Georgia,serif;background:#f4f2ed;color:#1d1a17;margin:24px;}")
    parts.append("h1,h2{margin:0 0 10px 0;} .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:12px;margin:16px 0 20px;}")
    parts.append(".card{background:#fff;border:1px solid #ddd;padding:12px;border-radius:8px;} .label{font-size:12px;text-transform:uppercase;color:#666;} .value{font-size:24px;font-weight:700;}")
    parts.append("table{width:100%;border-collapse:collapse;margin:10px 0 24px;background:#fff;border:1px solid #ddd;} th,td{padding:9px 10px;border-bottom:1px solid #eee;vertical-align:top;text-align:left;}")
    parts.append("th{font-size:12px;text-transform:uppercase;color:#555;background:#faf8f3;} .muted{font-size:12px;color:#666;} .ok{color:#166534;font-weight:700;} .bad{color:#9f1239;font-weight:700;}")
    parts.append(".bar{height:8px;background:#ece8dc;border-radius:999px;margin-top:6px;min-width:120px;} .fill{height:100%;background:linear-gradient(90deg,#0f766e,#14b8a6);border-radius:999px;}")
    parts.append("</style></head><body>")
    parts.append("<h1>KeyStore Multi-Thread Benchmark Report (V2)</h1>")
    parts.append("<p class='muted'>Fixed-duration multi-thread runs with repeated measurements, synchronized starts, correctness checks, and family-separated reporting.</p>")
    parts.append("<div class='grid'>")
    parts.append(f"<div class='card'><div class='label'>Scenario Count</div><div class='value'>{len(scenarios)}</div></div>")
    parts.append(f"<div class='card'><div class='label'>Run Count</div><div class='value'>{len(rows)}</div></div>")
    parts.append(f"<div class='card'><div class='label'>Issues</div><div class='value'>{issue_count}</div></div>")
    parts.append("</div>")

    family_titles = {
        "core_scaling": "Core Scaling",
        "workload_mix": "Workload Mix",
        "resize_stress": "Resize Stress",
        "oversubscription": "Oversubscription",
    }
    for family_key in ("core_scaling", "workload_mix", "resize_stress", "oversubscription"):
        family_rows = [row for row in scenarios if str(row.get("family", row.get("category", ""))) == family_key]
        parts.append(render_multi_thread_table(family_titles[family_key], family_rows, max_mean))

    resize_rows = [row for row in scenarios if str(row.get("family", row.get("category", ""))) == "resize_stress"]
    parts.append(render_cold_warm_pairs(scenarios))
    parts.append(render_resize_diagnostics(resize_rows))
    parts.append(render_memory_diagnostics(scenarios))
    parts.append("</body></html>")
    return "\n".join(parts)


def build_html(rows: list[dict]) -> str:
    if rows and any("family" in row for row in rows):
        return build_multi_thread_html(rows)

    scenarios = one_row_per_scenario(rows)
    max_mean = max((float(row.get("mean_ops_sec", 0.0)) for row in scenarios), default=0.0)

    micro = [row for row in scenarios if str(row.get("category", "")) == "micro"]
    realistic = [row for row in scenarios if str(row.get("category", "")) == "realistic"]
    resize = [row for row in scenarios if str(row.get("category", "")) == "resize"]
    memory = [row for row in scenarios if str(row.get("category", "")) == "memory"]

    issue_count = sum(
        1
        for row in rows
        if (not bool(row.get("verification_passed", False)))
        or int(row.get("failure_count", 0)) > 0
        or int(row.get("missing_count", 0)) > 0
    )

    parts: list[str] = []
    parts.append("<!doctype html><html lang='en'><head><meta charset='utf-8'>")
    parts.append("<meta name='viewport' content='width=device-width, initial-scale=1'>")
    parts.append("<title>KeyStore Single-Thread V2 Report</title>")
    parts.append("<style>")
    parts.append("body{font-family:Georgia,serif;background:#f4f2ed;color:#1d1a17;margin:24px;}")
    parts.append("h1,h2{margin:0 0 10px 0;} .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:12px;margin:16px 0 20px;}")
    parts.append(".card{background:#fff;border:1px solid #ddd;padding:12px;border-radius:8px;} .label{font-size:12px;text-transform:uppercase;color:#666;} .value{font-size:24px;font-weight:700;}")
    parts.append("table{width:100%;border-collapse:collapse;margin:10px 0 24px;background:#fff;border:1px solid #ddd;} th,td{padding:9px 10px;border-bottom:1px solid #eee;vertical-align:top;text-align:left;}")
    parts.append("th{font-size:12px;text-transform:uppercase;color:#555;background:#faf8f3;} .muted{font-size:12px;color:#666;} .ok{color:#166534;font-weight:700;} .bad{color:#9f1239;font-weight:700;}")
    parts.append(".bar{height:8px;background:#ece8dc;border-radius:999px;margin-top:6px;min-width:120px;} .fill{height:100%;background:linear-gradient(90deg,#0f766e,#14b8a6);border-radius:999px;}")
    parts.append("</style></head><body>")
    parts.append("<h1>KeyStore Single-Thread Benchmark Report (V2)</h1>")
    parts.append("<p class='muted'>Fixed-duration single-thread runs with 1s warmup, 3s measurement, 5 repetitions, and correctness checks.</p>")
    parts.append("<div class='grid'>")
    parts.append(f"<div class='card'><div class='label'>Scenario Count</div><div class='value'>{len(scenarios)}</div></div>")
    parts.append(f"<div class='card'><div class='label'>Run Count</div><div class='value'>{len(rows)}</div></div>")
    parts.append(f"<div class='card'><div class='label'>Issues</div><div class='value'>{issue_count}</div></div>")
    parts.append("</div>")

    parts.append("<h2>Section A: Microbenchmarks</h2>")
    parts.append(render_scenario_table("Microbenchmarks", micro, max_mean))

    parts.append("<h2>Section B: Realistic Workload Benchmarks</h2>")
    parts.append(render_scenario_table("Realistic Workloads", realistic, max_mean))
    parts.append(render_cold_warm_pairs(realistic))

    parts.append(render_resize_diagnostics(resize))
    parts.append(render_memory_diagnostics(memory))

    parts.append("</body></html>")
    return "\n".join(parts)


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate V2 HTML report from benchmark JSONL results.")
    parser.add_argument("--input", required=True, help="Path to JSONL results")
    parser.add_argument("--output", required=True, help="Path to HTML report")
    args = parser.parse_args()

    rows = load_results(Path(args.input))
    Path(args.output).write_text(build_html(rows), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())