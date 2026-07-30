#!/usr/bin/env python3

import argparse
import hashlib
import math
import re
from pathlib import Path


EXPERT_PATTERN = re.compile(r'^EXPERT "([^"]+)" (.*)$')


def parse_report(path: Path) -> tuple[dict[str, str], dict[str, dict[str, float]]]:
    values: dict[str, str] = {}
    experts: dict[str, dict[str, float]] = {}
    for line in path.read_text().splitlines():
        if not line or line == "END":
            continue
        match = EXPERT_PATTERN.match(line)
        if match:
            fields: dict[str, float] = {}
            for item in match.group(2).split():
                key, raw_value = item.split("=", 1)
                fields[key] = float(raw_value)
            experts[match.group(1)] = fields
        elif "=" in line:
            key, value = line.split("=", 1)
            values[key] = value
    if not experts or "winner" not in values:
        raise ValueError(f"incomplete competition report: {path}")
    return values, experts


def rank(values: dict[str, float]) -> dict[str, int]:
    ordered = sorted(values, key=lambda name: (values[name], name))
    return {name: index for index, name in enumerate(ordered)}


def spearman(left: dict[str, int], right: dict[str, int]) -> float:
    names = sorted(left)
    if len(names) < 2:
        return 1.0
    numerator = sum((left[name] - right[name]) ** 2 for name in names)
    count = len(names)
    return 1.0 - 6.0 * numerator / (count * (count * count - 1))


def finite(value: float, name: str) -> float:
    if not math.isfinite(value):
        raise ValueError(f"non-finite {name}")
    return value


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--heldout", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()

    source_values, source = parse_report(arguments.source)
    heldout_values, heldout = parse_report(arguments.heldout)
    if set(source) != set(heldout):
        raise ValueError("source and held-out expert sets differ")

    names = sorted(source)
    source_gate_passing = {
        name for name in names if source[name]["empirical_pass_rate"] > 0.0
    }
    heldout_gate_passing = {
        name for name in names if heldout[name]["empirical_pass_rate"] > 0.0
    }
    if not source_gate_passing or not heldout_gate_passing:
        raise ValueError("no accepted expert family in shift reports")

    source_cost_rank = rank({name: source[name]["median_wall_us"] for name in names})
    heldout_cost_rank = rank({name: heldout[name]["median_wall_us"] for name in names})
    source_gate_passing_rank = rank(
        {
            name: source[name]["median_wall_us"]
            for name in sorted(source_gate_passing)
        }
    )
    heldout_gate_passing_rank = rank(
        {
            name: heldout[name]["median_wall_us"]
            for name in sorted(source_gate_passing)
        }
    )
    source_winner = source_values["winner"]
    heldout_winner = heldout_values["winner"]
    source_winner_holdout_cost = heldout[source_winner]["median_wall_us"]
    heldout_best_gate_passing_cost = min(
        heldout[name]["median_wall_us"] for name in heldout_gate_passing
    )
    source_winner_regret = (
        source_winner_holdout_cost / heldout_best_gate_passing_cost
    )
    source_all_calibration = [source[name]["calibration_error"] for name in names]
    heldout_all_calibration = [heldout[name]["calibration_error"] for name in names]
    source_gate_passing_calibration = [
        source[name]["calibration_error"] for name in source_gate_passing
    ]
    heldout_gate_passing_calibration = [
        heldout[name]["calibration_error"] for name in heldout_gate_passing
    ]
    max_cost_shift = max(
        abs(heldout[name]["median_wall_us"] / source[name]["median_wall_us"] - 1.0)
        for name in names
    )
    gate_status_mismatches = sum(
        (source[name]["empirical_pass_rate"] > 0.0)
        != (heldout[name]["empirical_pass_rate"] > 0.0)
        for name in names
    )

    fields = [
        "SMAVE_ROUTER_SHIFT_ANALYSIS 1",
        "contract=source-5x5-to-heldout-6x6-complete-cost-shift",
        f"source_dataset_id={source_values.get('dataset_id', 'unknown')}",
        f"heldout_dataset_id={heldout_values.get('dataset_id', 'unknown')}",
        "shift_axis=problem-size-and-family-fingerprint",
        f"expert_count={len(names)}",
        f"source_gate_passing_expert_count={len(source_gate_passing)}",
        f"heldout_gate_passing_expert_count={len(heldout_gate_passing)}",
        f"source_winner={source_winner}",
        f"heldout_winner={heldout_winner}",
        f"winner_preserved={int(source_winner == heldout_winner)}",
        f"gate_status_mismatches={gate_status_mismatches}",
        f"all_cost_rank_spearman={finite(spearman(source_cost_rank, heldout_cost_rank), 'all_cost_rank_spearman'):.12g}",
        f"gate_passing_cost_rank_spearman={finite(spearman(source_gate_passing_rank, heldout_gate_passing_rank), 'gate_passing_cost_rank_spearman'):.12g}",
        f"max_relative_median_cost_shift={finite(max_cost_shift, 'max_relative_median_cost_shift'):.12g}",
        f"source_selected_holdout_vs_fastest_gate_passing_median={finite(source_winner_regret, 'source_selected_holdout_vs_fastest_gate_passing_median'):.12g}",
        f"source_all_mean_calibration_error={statistics_mean(source_all_calibration):.12g}",
        f"heldout_all_mean_calibration_error={statistics_mean(heldout_all_calibration):.12g}",
        f"source_all_max_calibration_error={max(source_all_calibration):.12g}",
        f"heldout_all_max_calibration_error={max(heldout_all_calibration):.12g}",
        f"source_gate_passing_mean_calibration_error={statistics_mean(source_gate_passing_calibration):.12g}",
        f"heldout_gate_passing_mean_calibration_error={statistics_mean(heldout_gate_passing_calibration):.12g}",
        f"source_gate_passing_max_calibration_error={max(source_gate_passing_calibration):.12g}",
        f"heldout_gate_passing_max_calibration_error={max(heldout_gate_passing_calibration):.12g}",
        f"source_selected_holdout_median_us={source_winner_holdout_cost:.12g}",
        f"heldout_best_gate_passing_cost_us={heldout_best_gate_passing_cost:.12g}",
    ]
    payload = "\n".join(fields) + "\n"
    fields.append(f"report_hash={hashlib.sha256(payload.encode()).hexdigest()[:16]}")
    fields.append("END")
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_text("\n".join(fields) + "\n")
    print("SMAVE_ROUTER_SHIFT_ANALYSIS_WRITTEN 1")
    return 0


def statistics_mean(values: list[float]) -> float:
    return sum(values) / len(values)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError) as error:
        print(f"router shift analysis failed: {error}")
        raise SystemExit(1)
