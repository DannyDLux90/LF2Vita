#!/usr/bin/env python3
"""Analyze LF2 Vita PERF records without modifying the game or importing VitaSDK.

Example:
    python3 tools/analyze_stage_perf_log.py lf2.log --json report.json --csv samples.csv

"load/transition nearby" means temporal overlap, NOT established causation.
"""
from __future__ import annotations

import argparse
import csv
import json
import re
import statistics
from pathlib import Path

STAMP = re.compile(r"^\\[(\\d+) ms\\]")
PERF = re.compile(r"\\bPERF\\s+stage\\s+")
FIELD = re.compile(r"([a-z_]+)=(-?\\d+(?:\\.\\d+)?)")
CDRAM = re.compile(r"\\bcdram=(\\d+)")
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
    "ai_us_per_tu", "objects_us_per_tu", "hits_us_per_tu",
)


def parse_log(text: str, nearby_ms: int = 5000) -> dict:
    """Parse records and tag PERF windows containing preceding texture/act events."""
    perf_rows: list[dict] = []
    events: list[tuple[int, str]] = []
    cdram_values: list[int] = []
    for line in text.splitlines():
        match = STAMP.match(line)
        if match is None:
            continue
        timestamp = int(match.group(1))
        cdram = CDRAM.search(line)
        if cdram:
            cdram_values.append(int(cdram.group(1)))
        if PERF.search(line):
            row = {"time_ms": timestamp}
            for field, raw in FIELD.findall(line):
                row[field] = float(raw)
            if "enemies" in row and "logic_hz" in row and "fps" in row:
                perf_rows.append(row)
        if any(marker in line for marker in TRANSITION_MARKERS):
            kind = "texture_load" if "loading textures char=" in line else "stage_transition"
            events.append((timestamp, kind))
    for row in perf_rows:
        relevant = {kind for stamp, kind in events if 0 <= row["time_ms"] - stamp <= nearby_ms}
        row["texture_load_nearby"] = "texture_load" in relevant
        row["stage_transition_nearby"] = "stage_transition" in relevant
        row["clean_window"] = not relevant
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
            groups[f"{low}-{high if high < 99999 else '+'}"] = summarize(subset)
    slow = sorted(samples, key=lambda r: (r.get("logic_hz", 999), r["time_ms"]))[:8]
    return {"sample_count": len(samples), "cdram_min_bytes": parsed["cdram_min_bytes"],
            "event_count": parsed["event_count"], "event_nearby_ms": parsed["nearby_ms"],
            "by_enemy_count": groups,
            "windows_with_load_or_transition": summarize([r for r in samples if not r["clean_window"]]),
            "windows_without_load_or_transition": summarize([r for r in samples if r["clean_window"]]),
            "slowest_logic_windows": slow}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path, help="LF2 Vita lf2.log")
    parser.add_argument("--nearby-ms", type=int, default=5000,
                        help="lookback interval for load/transition flags (default: 5000)")
    parser.add_argument("--json", type=Path, help="write machine-readable summary")
    parser.add_argument("--csv", type=Path, help="write all individual PERF samples")
    args = parser.parse_args()
    if args.nearby_ms < 0:
        parser.error("--nearby-ms must be nonnegative")
    parsed = parse_log(args.log.read_text(encoding="utf-8", errors="replace"), args.nearby_ms)
    result = report(parsed)
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
