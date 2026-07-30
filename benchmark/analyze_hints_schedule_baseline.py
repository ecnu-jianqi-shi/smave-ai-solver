#!/usr/bin/env python3

import argparse
import math
from pathlib import Path


EXPECTED_HEADER = "SMAVE_OPERATOR_HINTS_SCHEDULE_BASELINE 1"
EXPECTED_CONTRACT = (
    "paired-complete-runtime-hints-alternation-common-latent-operator-"
    "original-gate-fallback"
)


def parse(path: Path) -> dict[str, str]:
    lines = path.read_text().splitlines()
    if not lines or lines[0] != EXPECTED_HEADER or lines[-1] != "END":
        raise ValueError(f"invalid HINTS schedule report: {path}")
    values: dict[str, str] = {}
    for line in lines[1:-1]:
        if "=" not in line:
            raise ValueError(f"invalid HINTS schedule field: {line}")
        key, value = line.split("=", 1)
        values[key] = value
    if values.get("contract") != EXPECTED_CONTRACT:
        raise ValueError("HINTS schedule contract mismatch")
    return values


def portable_report_path(path: Path) -> str:
    parts = path.parts
    if "build" in parts:
        return Path(*parts[parts.index("build") :]).as_posix()
    return path.name


def finite_number(values: dict[str, str], key: str) -> float:
    value = float(values[key])
    if not math.isfinite(value):
        raise ValueError(f"non-finite HINTS schedule field: {key}")
    return value


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--tex", type=Path, required=True)
    arguments = parser.parse_args()

    values = parse(arguments.report)
    attempted = int(values["attempted"])
    accepted = int(values["accepted"])
    fallbacks = int(values["fallbacks"])
    failures = int(values["failures"])
    total_iterations = int(values["total_iterations"])
    learned_corrections = int(values["learned_corrections"])
    if attempted <= 0 or accepted + fallbacks != attempted:
        raise ValueError("HINTS schedule outcome accounting mismatch")
    if failures != 0 or values.get("same_accuracy") != "1":
        raise ValueError("HINTS schedule does not satisfy the common accuracy contract")

    speedup = finite_number(values, "paired_median_speedup")
    lower = finite_number(values, "bootstrap_95_lower")
    upper = finite_number(values, "bootstrap_95_upper")
    operator_speedup = finite_number(
        values, "verified_operator_vs_hints_schedule_paired_median_speedup"
    )
    operator_lower = finite_number(
        values, "verified_operator_vs_hints_schedule_bootstrap_95_lower"
    )
    operator_upper = finite_number(
        values, "verified_operator_vs_hints_schedule_bootstrap_95_upper"
    )
    maximum_error = finite_number(values, "maximum_mixed_qoi_error")
    maximum_gate_difference = finite_number(
        values, "maximum_gate_residual_difference"
    )
    average_iterations = total_iterations / attempted
    average_learned = learned_corrections / attempted

    evidence = [
        "SMAVE_HINTS_SCHEDULE_EVIDENCE 1",
        f"source_report={portable_report_path(arguments.report)}",
        f"contract={values['contract']}",
        f"published_method={values['published_method']}",
        f"published_paper_doi={values['published_paper_doi']}",
        f"official_code_revision={values['official_code_revision']}",
        f"algorithmic_schedule_reimplementation={values['algorithmic_schedule_reimplementation']}",
        f"official_public_code_executed={values['official_public_code_executed']}",
        f"deep_onet_architecture_reproduced={values['deep_onet_architecture_reproduced']}",
        f"official_pretrained_weights_used={values['official_pretrained_weights_used']}",
        f"shared_latent_operator_weights={values['shared_latent_operator_weights']}",
        f"scope={values['scope']}",
        f"evaluation_scenarios={values['evaluation_scenarios']}",
        f"repetitions={values['repetitions']}",
        f"attempted={attempted}",
        f"accepted={accepted}",
        f"fallbacks={fallbacks}",
        f"failures={failures}",
        f"gate_decision_mismatches={values['gate_decision_mismatches']}",
        f"gate_residual_mismatches={values['gate_residual_mismatches']}",
        f"maximum_gate_residual_difference={values['maximum_gate_residual_difference']}",
        f"same_accuracy={values['same_accuracy']}",
        f"linear_matrix_assembly_in_timing={values['linear_matrix_assembly_in_timing']}",
        f"right_hand_side_update_in_timing={values['right_hand_side_update_in_timing']}",
        f"residual_kernel={values['residual_kernel']}",
        f"numerical_method={values['numerical_method']}",
        f"jacobi_weight={values['jacobi_weight']}",
        f"numerical_to_learned_ratio={values['numerical_to_learned_ratio']}",
        f"maximum_iterations={values['maximum_iterations']}",
        f"average_iterations={average_iterations:.17g}",
        f"average_learned_corrections={average_learned:.17g}",
        f"paired_median_speedup={speedup:.17g}",
        f"bootstrap_95_lower={lower:.17g}",
        f"bootstrap_95_upper={upper:.17g}",
        f"verified_operator_vs_hints_schedule_paired_median_speedup={operator_speedup:.17g}",
        f"verified_operator_vs_hints_schedule_bootstrap_95_lower={operator_lower:.17g}",
        f"verified_operator_vs_hints_schedule_bootstrap_95_upper={operator_upper:.17g}",
        f"maximum_mixed_qoi_error={maximum_error:.17g}",
        f"published_full_implementation_claim={values['published_full_implementation_claim']}",
        "negative_result_retained=1",
        f"all_failures_retained={values['all_failures_retained']}",
        "END",
    ]
    arguments.evidence.parent.mkdir(parents=True, exist_ok=True)
    arguments.evidence.write_text("\n".join(evidence) + "\n")

    tex = [
        "% Generated from build/release/hints-schedule-baseline/evidence.txt.",
        f"\\newcommand{{\\HintsScheduleAccepted}}{{{accepted}}}",
        f"\\newcommand{{\\HintsScheduleFallbacks}}{{{fallbacks}}}",
        f"\\newcommand{{\\HintsScheduleSpeedup}}{{{speedup:.3f}}}",
        f"\\newcommand{{\\HintsScheduleLower}}{{{lower:.3f}}}",
        f"\\newcommand{{\\HintsScheduleUpper}}{{{upper:.3f}}}",
        f"\\newcommand{{\\OperatorVsHintsSchedule}}{{{operator_speedup:.3f}}}",
        f"\\newcommand{{\\OperatorVsHintsScheduleLower}}{{{operator_lower:.3f}}}",
        f"\\newcommand{{\\OperatorVsHintsScheduleUpper}}{{{operator_upper:.3f}}}",
        f"\\newcommand{{\\HintsAverageIterations}}{{{average_iterations:.2f}}}",
        f"\\newcommand{{\\HintsAverageLearnedCorrections}}{{{average_learned:.2f}}}",
        f"\\newcommand{{\\HintsGateResidualMismatches}}{{{values['gate_residual_mismatches']}}}",
        f"\\newcommand{{\\HintsMaximumGateResidualDifference}}{{\\num{{{maximum_gate_difference:.3e}}}}}",
    ]
    arguments.tex.parent.mkdir(parents=True, exist_ok=True)
    arguments.tex.write_text("\n".join(tex) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
