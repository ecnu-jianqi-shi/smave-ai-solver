#!/usr/bin/env python3

import argparse
import hashlib
import math
from pathlib import Path


def parse(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    lines = path.read_text().splitlines()
    if not lines or lines[0] != "SMAVE_OPERATOR_ABLATION 1" or lines[-1] != "END":
        raise ValueError(f"invalid operator ablation report: {path}")
    for line in lines[1:-1]:
        if "=" not in line:
            raise ValueError(f"invalid field in {path}: {line}")
        key, value = line.split("=", 1)
        values[key] = value
    return values


def number(values: dict[str, str], key: str) -> float:
    value = float(values[key])
    if not math.isfinite(value) or value < 0.0:
        raise ValueError(f"invalid non-negative value {key}")
    return value


def add_family(fields: list[str], family: str, values: dict[str, str]) -> None:
    candidate = number(values, "raw_candidate_median_us")
    correction_runtime_gate = number(values, "correction_and_runtime_gate_median_us")
    fused_gate = number(values, "fused_original_gate_median_us")
    full = number(values, "full_verified_median_us")
    classic = number(values, "classic_median_us")
    best_budget = int(number(values, "production_corrector_best_budget"))
    minimum_full_acceptance_budget = int(
        number(values, "production_corrector_minimum_full_acceptance_budget")
    )
    best_complete = number(values, "production_corrector_best_complete_median_us")
    budget0_complete = number(values, "production_budget.0.complete_median_us")
    budget2_complete = number(values, "production_budget.2.complete_median_us")
    sweep_failures = sum(
        int(number(values, f"production_budget.{budget}.failures"))
        for budget in (0, 1, 2, 4, 8, 16, 32)
    )
    if full <= 0.0 or classic <= 0.0:
        raise ValueError(f"missing positive full/classic timing for {family}")
    closure_residual = full - candidate - correction_runtime_gate
    fields.extend(
        [
            f"{family}.classic_median_us={classic:.12g}",
            f"{family}.raw_candidate_median_us={candidate:.12g}",
            f"{family}.correction_runtime_gate_median_us={correction_runtime_gate:.12g}",
            f"{family}.fused_gate_median_us={fused_gate:.12g}",
            f"{family}.full_verified_median_us={full:.12g}",
            f"{family}.layer_closure_residual_median_us={closure_residual:.12g}",
            f"{family}.candidate_share={candidate / full:.12g}",
            f"{family}.correction_runtime_gate_share={correction_runtime_gate / full:.12g}",
            f"{family}.fused_gate_share={fused_gate / full:.12g}",
            f"{family}.layer_closure_residual_share={closure_residual / full:.12g}",
            f"{family}.full_over_classic_speedup={classic / full:.12g}",
            f"{family}.full_acceptance_rate={values['full_verified_acceptance_rate']}",
            f"{family}.fallbacks={values['fallbacks']}",
            f"{family}.failures={values['failures']}",
            f"{family}.external_corrector_acceptance_rate={values['external_corrector_acceptance_rate']}",
            f"{family}.external_corrector_median_us={number(values, 'external_corrector_median_us'):.12g}",
            f"{family}.correction_runtime_dominant={int(correction_runtime_gate / full > 0.9)}",
            f"{family}.production_corrector_best_budget={best_budget}",
            f"{family}.production_corrector_minimum_full_acceptance_budget={minimum_full_acceptance_budget}",
            f"{family}.production_corrector_best_complete_median_us={best_complete:.12g}",
            f"{family}.production_corrector_best_complete_over_classic={best_complete / classic:.12g}",
            f"{family}.production_corrector_budget0_acceptance_rate={values['production_budget.0.acceptance_rate']}",
            f"{family}.production_corrector_budget1_acceptance_rate={values['production_budget.1.acceptance_rate']}",
            f"{family}.production_corrector_budget2_acceptance_rate={values['production_budget.2.acceptance_rate']}",
            f"{family}.production_corrector_budget32_acceptance_rate={values['production_budget.32.acceptance_rate']}",
            f"{family}.production_corrector_budget2_vs_budget0_complete_ratio={budget2_complete / budget0_complete:.12g}",
            f"{family}.production_corrector_sweep_failures={sweep_failures}",
        ]
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--linear", type=Path, required=True)
    parser.add_argument("--nonlinear", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    linear = parse(arguments.linear)
    nonlinear = parse(arguments.nonlinear)
    fields = [
        "SMAVE_COMPLETE_COST_DECOMPOSITION 1",
        "contract=two-family-layered-complete-runtime-breakdown",
        "families=linear,nonlinear",
        "component_measurements=raw-candidate,correction-runtime-gate,fused-gate,full-verified,classic,production-corrector-budget-frontier",
        "share_semantics=layer-measurements-normalized-by-full-path;not-independent-additive-timings",
    ]
    add_family(fields, "linear", linear)
    add_family(fields, "nonlinear", nonlinear)
    payload = "\n".join(fields) + "\n"
    fields.append(f"report_hash={hashlib.sha256(payload.encode()).hexdigest()[:16]}")
    fields.append("END")
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_text("\n".join(fields) + "\n")
    print("SMAVE_COMPLETE_COST_DECOMPOSITION_WRITTEN 1")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, KeyError, ValueError) as error:
        print(f"complete cost decomposition failed: {error}")
        raise SystemExit(1)
