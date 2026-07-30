#!/usr/bin/env python3

import argparse
import hashlib
import math
from pathlib import Path

from analyze_router_shift import parse_report, rank, spearman


def parse_values(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text().splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            values[key] = value
    return values


def finite(value: float, name: str) -> float:
    if not math.isfinite(value):
        raise ValueError(f"non-finite {name}")
    return value


def axis_fields(
    axis: str,
    source_experts: dict[str, dict[str, float]],
    heldout_path: Path,
    evaluation_path: Path,
) -> tuple[list[str], dict[str, float]]:
    evaluation = parse_values(evaluation_path)
    _, heldout_experts = parse_report(heldout_path)
    if set(source_experts) != set(heldout_experts):
        raise ValueError(f"{axis} expert set differs from source")
    names = sorted(source_experts)
    source_passing = {
        name
        for name in names
        if source_experts[name]["empirical_pass_rate"] > 0.0
    }
    heldout_passing = {
        name
        for name in names
        if heldout_experts[name]["empirical_pass_rate"] > 0.0
    }
    common_passing = sorted(source_passing & heldout_passing)
    if len(common_passing) < 2 or not heldout_passing:
        raise ValueError(f"{axis} lacks comparable gate-passing experts")
    source_rank = rank(
        {
            name: source_experts[name]["median_wall_us"]
            for name in common_passing
        }
    )
    heldout_rank = rank(
        {
            name: heldout_experts[name]["median_wall_us"]
            for name in common_passing
        }
    )
    selected = evaluation["calibrated_expert"]
    if selected not in heldout_passing:
        raise ValueError(f"{axis} selected expert does not pass the original equation")
    fastest_cost = min(
        heldout_experts[name]["median_wall_us"] for name in heldout_passing
    )
    selected_regret = heldout_experts[selected]["median_wall_us"] / fastest_cost
    status_changes = sum(
        (name in source_passing) != (name in heldout_passing) for name in names
    )
    maximum_calibration = max(
        heldout_experts[name]["calibration_error"] for name in heldout_passing
    )
    metrics = {
        "embedding_similarity": float(evaluation["embedding_similarity"]),
        "common_cost_rank_spearman": finite(
            spearman(source_rank, heldout_rank),
            f"{axis}.common_cost_rank_spearman",
        ),
        "selected_regret": finite(selected_regret, f"{axis}.selected_regret"),
        "maximum_calibration": finite(
            maximum_calibration, f"{axis}.maximum_calibration"
        ),
        "paired_ci_lower": float(evaluation["paired_speedup_ci95_lower"]),
        "paired_speedup": float(evaluation["paired_median_speedup"]),
        "status_changes": float(status_changes),
        "dangerous_misroutes": float(
            evaluation["calibrated_dangerous_misroutes"]
        ),
        "gate_mismatches": float(evaluation["gate_mismatches"]),
        "safe": float(evaluation["safe"]),
    }
    fields = [
        f"{axis}.shift_axis={axis}",
        f"{axis}.embedding_similarity={metrics['embedding_similarity']:.12g}",
        f"{axis}.source_gate_passing_experts={len(source_passing)}",
        f"{axis}.heldout_gate_passing_experts={len(heldout_passing)}",
        f"{axis}.common_gate_passing_experts={len(common_passing)}",
        f"{axis}.gate_status_changes={status_changes}",
        f"{axis}.common_cost_rank_spearman={metrics['common_cost_rank_spearman']:.12g}",
        f"{axis}.heldout_gate_passing_max_calibration_error={maximum_calibration:.12g}",
        f"{axis}.calibrated_expert={selected}",
        f"{axis}.oracle_expert={evaluation['oracle_expert']}",
        f"{axis}.selected_vs_fastest_gate_passing_median={selected_regret:.12g}",
        f"{axis}.paired_median_speedup={metrics['paired_speedup']:.12g}",
        f"{axis}.paired_speedup_ci95_lower={metrics['paired_ci_lower']:.12g}",
        f"{axis}.paired_speedup_ci95_upper={float(evaluation['paired_speedup_ci95_upper']):.12g}",
        f"{axis}.paired_win_rate={float(evaluation['paired_win_rate']):.12g}",
        f"{axis}.gate_mismatches={evaluation['gate_mismatches']}",
        f"{axis}.dangerous_misroutes={evaluation['calibrated_dangerous_misroutes']}",
        f"{axis}.same_accuracy={evaluation['same_accuracy']}",
        f"{axis}.safe={evaluation['safe']}",
    ]
    return fields, metrics


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--conditioning-competition", type=Path, required=True)
    parser.add_argument("--conditioning-evaluation", type=Path, required=True)
    parser.add_argument("--topology-competition", type=Path, required=True)
    parser.add_argument("--topology-evaluation", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    source_values, source_experts = parse_report(arguments.source)
    conditioning_fields, conditioning = axis_fields(
        "conditioning",
        source_experts,
        arguments.conditioning_competition,
        arguments.conditioning_evaluation,
    )
    topology_fields, topology = axis_fields(
        "topology",
        source_experts,
        arguments.topology_competition,
        arguments.topology_evaluation,
    )
    metrics = (conditioning, topology)
    fields = [
        "SMAVE_ROUTER_SHIFT_MATRIX 1",
        "contract=source-calibrated-complete-cost-router-under-conditioning-and-topology-shift",
        f"source_dataset_id={source_values.get('dataset_id', 'unknown')}",
        "axes=conditioning,topology",
        "grid=5x5",
        "scenarios=64",
        "repetitions_per_axis=20",
        "source_diagonal=4.5",
        "conditioning_diagonal=3.5",
        "topology_west_east_coefficients=1.35,0.65",
        *conditioning_fields,
        *topology_fields,
        f"minimum_paired_speedup_ci95_lower={min(item['paired_ci_lower'] for item in metrics):.12g}",
        f"maximum_structurally_filtered_calibration_error={max(item['maximum_calibration'] for item in metrics):.12g}",
        f"maximum_selected_complete_cost_regret={max(item['selected_regret'] for item in metrics):.12g}",
        f"total_gate_status_changes={int(sum(item['status_changes'] for item in metrics))}",
        f"all_axes_zero_gate_mismatches={int(all(item['gate_mismatches'] == 0 for item in metrics))}",
        f"all_axes_zero_dangerous_misroutes={int(all(item['dangerous_misroutes'] == 0 for item in metrics))}",
        f"all_axes_safe={int(all(item['safe'] == 1 for item in metrics))}",
    ]
    payload = "\n".join(fields) + "\n"
    fields.append(f"report_hash={hashlib.sha256(payload.encode()).hexdigest()[:16]}")
    fields.append("END")
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_text("\n".join(fields) + "\n")
    print("SMAVE_ROUTER_SHIFT_MATRIX_WRITTEN 1")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (KeyError, OSError, ValueError) as error:
        print(f"router shift matrix analysis failed: {error}")
        raise SystemExit(1)
