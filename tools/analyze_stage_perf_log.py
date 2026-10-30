#!/usr/bin/env python3
"""Analyze LF2 Vita PERF records without modifying the game or importing VitaSDK.

Example:
    python3 tools/analyze_stage_perf_log.py lf2.log --baseline older_lf2.log --json report.json --csv samples.csv

"load/transition nearby" means temporal overlap, NOT established causation.
The last reported CDRAM figure may be stale; detailed profiler fields are
only available in Round-27-or-later logs. Slow-with-low-measured-work is a
diagnostic flag, never proof of a texture-loading stall.
"""
from __future__ import annotations

import argparse
from bisect import bisect_right
import csv
import json
import re
import statistics
from pathlib import Path

STAMP = re.compile(r"^\[(\d+) ms\]")
PERF = re.compile(r"\bPERF\s+stage\s+")
FIELD = re.compile(r"([a-z_]+)=(-?\d+(?:\.\d+)?)")
CDRAM = re.compile(r"\bcdram=(\d+)")
PHASE = re.compile(r"\bphase=(\d+)/(\d+)")
TRANSITION_MARKERS = (
    "stage:transition_snapshot",
    "stage:wave_advance",
    "stage:preload_ready",
    "stage:act_change",
    "stage:section_change",
    "loading textures char=",
)
BINS = ((0, 10), (11, 20), (21, 30), (31, 40), (41, 99999))
METRICS = (
    "fps", "logic_hz", "logic_us_per_tu", "draw_us_per_frame",
    "ai_us_per_tu", "objects_us_per_tu", "hits_us_per_tu", "other_us_per_tu",
)


def parse_log(text: str, nearby_ms: int = 5000) -> dict:
    """Parse records and tag PERF windows containing preceding texture/act events."""
    perf_rows: list[dict] = []
    events: list[tuple[int, str]] = []
    cdram_values: list[int] = []
    cdram_events: list[tuple[int, int]] = []
    for line in text.splitlines():
        match = STAMP.match(line)
        if match is None:
            continue
        timestamp = int(match.group(1))
        cdram = CDRAM.search(line)
        if cdram:
            value = int(cdram.group(1))
            cdram_values.append(value)
            cdram_events.append((timestamp, value))
        if PERF.search(line):
            row = {"time_ms": timestamp}
            for field, raw in FIELD.findall(line):
                row[field] = float(raw)
            phase = PHASE.search(line)
            if phase:
                row["phase_index"], row["phase_total"] = int(phase.group(1)), int(phase.group(2))
                row.pop("phase", None)  # Avoid parsing '13/21' as a bogus float.
            components = ("ai_us_per_tu", "objects_us_per_tu", "hits_us_per_tu")
            if "logic_us_per_tu" in row and all(key in row for key in components):
                remaining = row["logic_us_per_tu"] - sum(row[key] for key in components)
                # A negative remainder indicates overlapping/misaligned timers,
                # not negative work; leave it unreported.
                if remaining >= 0:
                    row["other_us_per_tu"] = round(remaining, 2)
            if "enemies" in row and "logic_hz" in row and "fps" in row:
                perf_rows.append(row)
        if any(marker in line for marker in TRANSITION_MARKERS):
            kind = "texture_load" if "loading textures char=" in line else "stage_transition"
            events.append((timestamp, kind))
    # Indexed lookups avoid scanning every texture event for every PERF sample.
    event_times = {
        kind: sorted(stamp for stamp, observed_kind in events if observed_kind == kind)
        for kind in ("texture_load", "stage_transition")
    }
    cdram_events.sort()
    cdram_times = [stamp for stamp, _ in cdram_events]
    for row in perf_rows:
        for kind, flag in (("texture_load", "texture_load_nearby"),
                           ("stage_transition", "stage_transition_nearby")):
            stamps = event_times[kind]
            index = bisect_right(stamps, row["time_ms"]) - 1
            row[flag] = (index >= 0 and
                         row["time_ms"] - stamps[index] <= nearby_ms)
            row[f"ms_since_{kind}"] = (
                row["time_ms"] - stamps[index] if index >= 0 else None)
        row["clean_window"] = not (
            row["texture_load_nearby"] or row["stage_transition_nearby"])
        index = bisect_right(cdram_times, row["time_ms"]) - 1
        row["last_reported_cdram_bytes"] = cdram_events[index][1] if index >= 0 else None
        # The following are triage indicators, NOT a diagnosis of root cause.
        row["logic_under_25_hz"] = row["logic_hz"] < 25.0
        row["measured_logic_over_budget"] = (
            row.get("logic_us_per_tu", 0.0) > 1000000.0 / 30.0)
        row["slow_with_low_measured_logic_cost"] = (
            row["logic_under_25_hz"] and
            row.get("logic_us_per_tu", float("inf")) < 10000.0)
    return {"samples": perf_rows, "cdram_min_bytes": min(cdram_values) if cdram_values else None,
            "event_count": len(events), "nearby_ms": nearby_ms}


def summarize(rows: list[dict]) -> dict:
    """Summarize sampling intervals; these are not weighted by frames or TUs."""
    result = {"count": len(rows)}
    for field in METRICS:
        values = [row[field] for row in rows if field in row]
        if values:
            result[field] = {"mean": round(statistics.fmean(values), 2),
                             "min": round(min(values), 2),
                             "max": round(max(values), 2)}
    return result


def report(parsed: dict) -> dict:
    samples = parsed["samples"]
    groups = {}
    for low, high in BINS:
        subset = [r for r in samples if low <= r["enemies"] <= high]
        if subset:
            groups[f"{low}-{high}" if high < 99999 else f"{low}+"] = summarize(subset)
    slow = sorted(samples, key=lambda r: (r.get("logic_hz", 999), r["time_ms"]))[:8]
    return {"sample_count": len(samples), "cdram_min_bytes": parsed["cdram_min_bytes"],
            "event_count": parsed["event_count"], "event_nearby_ms": parsed["nearby_ms"],
            "by_enemy_count": groups,
            "clean_windows_by_enemy_count": {
                (f"{low}-{high}" if high < 99999 else f"{low}+"):
                    summarize([r for r in samples if r["clean_window"] and
                               low <= r["enemies"] <= high])
                for low, high in BINS
                if any(r["clean_window"] and low <= r["enemies"] <= high
                       for r in samples)
            },
            "simulation_under_25_hz": summarize(
                [r for r in samples if r["logic_under_25_hz"]]),
            "slow_with_low_measured_logic_cost": summarize(
                [r for r in samples if r["slow_with_low_measured_logic_cost"]]),
            "windows_with_load_or_transition": summarize([r for r in samples if not r["clean_window"]]),
            "windows_with_texture_load": summarize([r for r in samples if r["texture_load_nearby"]]),
            "windows_without_texture_load": summarize([r for r in samples if not r["texture_load_nearby"]]),
            "windows_without_load_or_transition": summarize([r for r in samples if r["clean_window"]]),
            "slowest_logic_windows": slow,
            "slowest_clean_logic_windows": sorted(
                (r for r in samples if r["clean_window"]),
                key=lambda r: (r.get("logic_hz", 999), r["time_ms"]))[:8]}



def compare_runs(baseline: dict, candidate: dict, min_samples: int = 3) -> dict:
    """Exploratory A/B comparison, matched only by enemy count and clean windows.

    NOT a controlled benchmark: input actions, scene, randomness, and stage
    states can differ even when the number of enemies is identical.
    """
    if min_samples < 1:
        raise ValueError("min_samples must be at least 1")
    result = {"method": "same_enemy_bin_clean_windows_only",
              "controlled_benchmark": False,
              "minimum_samples_per_run": min_samples,
              "bins": {}}
    for low, high in BINS:
        key = f"{low}-{high}" if high < 99999 else f"{low}+"
        reference = [row for row in baseline["samples"]
                     if row["clean_window"] and low <= row["enemies"] <= high]
        current = [row for row in candidate["samples"]
                   if row["clean_window"] and low <= row["enemies"] <= high]
        comparison = {"baseline_samples": len(reference),
                      "candidate_samples": len(current),
                      "status": "insufficient_samples"}
        if len(reference) >= min_samples and len(current) >= min_samples:
            comparison["status"] = "exploratory_only"
            comparisons = {}
            for metric in METRICS:
                old = [r[metric] for r in reference if metric in r]
                new = [r[metric] for r in current if metric in r]
                if len(old) < min_samples or len(new) < min_samples:
                    continue
                previous, following = statistics.fmean(old), statistics.fmean(new)
                comparisons[metric] = {
                    "baseline_mean": round(previous, 3),
                    "candidate_mean": round(following, 3),
                    "delta": round(following - previous, 3),
                    "delta_percent": (
                        round(100 * (following - previous) / previous, 2)
                        if previous != 0 else None),
                }
            comparison["metrics"] = comparisons
        result["bins"][key] = comparison
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path, help="LF2 Vita lf2.log")
    parser.add_argument("--nearby-ms", type=int, default=5000,
                        help="lookback interval for load/transition flags (default: 5000)")
    parser.add_argument("--json", type=Path, help="write machine-readable summary")
    parser.add_argument("--csv", type=Path, help="write all individual PERF samples")
    parser.add_argument("--baseline", type=Path, help="optional older Vita log for exploratory A/B comparison")
    parser.add_argument("--min-comparison-samples", type=int, default=3, help="minimum clean samples per bin per run")
    args = parser.parse_args()
    if args.nearby_ms < 0:
        parser.error("--nearby-ms must be nonnegative")
    if args.min_comparison_samples < 1:
        parser.error("--min-comparison-samples must be positive")
    parsed = parse_log(args.log.read_text(encoding="utf-8", errors="replace"), args.nearby_ms)
    result = report(parsed)
    if args.baseline:
        before = parse_log(args.baseline.read_text(encoding="utf-8", errors="replace"),
                           args.nearby_ms)
        result["comparison"] = compare_runs(before, parsed, args.min_comparison_samples)
    summary = json.dumps(result, indent=2, ensure_ascii=False)
    print(summary)
    if args.json:
        args.json.write_text(summary + "\n", encoding="utf-8")
    if args.csv:
        rows = parsed["samples"]
        fields = sorted({k for row in rows for k in row}) if rows else ["time_ms"]
        with args.csv.open("w", encoding="utf-8", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=fields)
            writer.writeheader()
            writer.writerows(rows)


if __name__ == "__main__":
    main()
