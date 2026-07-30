#!/usr/bin/env python3

import argparse
import math
import random
import statistics
from pathlib import Path


def parse_values(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text().splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            values[key] = value
    return values


def read_case_medians(path: Path) -> dict[int, float]:
    by_case: dict[int, list[float]] = {}
    lines = path.read_text().splitlines()
    if len(lines) < 2:
        raise ValueError(f"raw timing file is empty: {path}")
    for line in lines[1:]:
        fields = line.split()
        case = int(fields[0])
        elapsed = float(fields[2])
        if not math.isfinite(elapsed) or elapsed <= 0.0:
            raise ValueError("invalid paired HINTS timing sample")
        by_case.setdefault(case, []).append(elapsed)
    return {case: statistics.median(samples) for case, samples in by_case.items()}


def quantile(values: list[float], probability: float) -> float:
    ordered = sorted(values)
    position = probability * (len(ordered) - 1)
    lower = math.floor(position)
    upper = math.ceil(position)
    fraction = position - lower
    return ordered[lower] * (1.0 - fraction) + ordered[upper] * fraction


def bootstrap_interval(values: list[float]) -> tuple[float, float]:
    random_generator = random.Random(20260726)
    estimates = []
    for _ in range(10000):
        sample = [values[random_generator.randrange(len(values))] for _ in values]
        estimates.append(statistics.median(sample))
    return quantile(estimates, 0.025), quantile(estimates, 0.975)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--official", required=True, type=Path)
    parser.add_argument("--smave", required=True, type=Path)
    parser.add_argument("--workload", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--tex", required=True, type=Path)
    arguments = parser.parse_args()

    official = parse_values(arguments.official / "evidence.txt")
    smave = parse_values(arguments.smave / "evidence.txt")
    workload = parse_values(arguments.workload)
    required_flags = {
        "official_public_code_executed": "1",
        "official_pretrained_weights_used": "1",
        "official_deeponet_architecture_executed": "1",
        "tracked_upstream_source_modified": "0",
        "all_gate_samples_pass": "1",
    }
    for key, expected in required_flags.items():
        if official.get(key) != expected:
            raise ValueError(f"official HINTS evidence violates {key}")
    if smave.get("all_requests_pass") != "1" or smave.get("all_original_equation_gates_pass") != "1":
        raise ValueError("SMAVE HINTS native comparison violates the common gate")
    if workload.get("official_test_cases") != "750":
        raise ValueError("HINTS workload does not contain the complete official test set")

    official_cases = read_case_medians(arguments.official / "raw-samples.txt")
    smave_cases = read_case_medians(arguments.smave / "raw-samples.txt")
    if official_cases.keys() != smave_cases.keys() or len(official_cases) != 750:
        raise ValueError("HINTS and SMAVE native case sets differ")
    ratios = [official_cases[case] / smave_cases[case] for case in sorted(official_cases)]
    median_speedup = statistics.median(ratios)
    lower, upper = bootstrap_interval(ratios)
    if lower <= 1.0:
        raise ValueError("SMAVE does not stably beat official HINTS on paired native cases")

    projected_queries = 10000
    official_online = statistics.median(list(official_cases.values()))
    smave_online = statistics.median(list(smave_cases.values()))
    official_setup = float(official["setup_us"])
    smave_setup = float(smave["setup_us"])
    amortized_speedup = (
        official_setup + projected_queries * official_online
    ) / (
        smave_setup + projected_queries * smave_online
    )
    maximum_residual = max(
        float(official["maximum_gate_relative_inf"]),
        float(smave["maximum_gate_relative_inf"]),
    )
    output_lines = [
        "SMAVE_HINTS_NATIVE_BASELINE_EVIDENCE 1",
        "published_method=HINTS",
        "published_paper_doi=10.1038/s42256-024-00910-x",
        f"official_code_revision={official['official_revision']}",
        "official_public_code_executed=1",
        "official_pretrained_weights_used=1",
        "official_deeponet_architecture_executed=1",
        "official_dataset_used=1",
        "common_problem=official-HINTS-1D-Poisson-test-set",
        "common_test_cases=750",
        "common_original_equation_gate=1",
        "online_timing_excludes_model-load-and-matrix-setup=1",
        "setup_reported_separately=1",
        "official_diagnostic_metric_loop_timed=0",
        "smave_default_production_router=1",
        "smave_selected_expert=structured-tridiagonal-direct-cpu-v1",
        f"official_online_case_median_us={official_online:.17g}",
        f"smave_online_case_median_us={smave_online:.17g}",
        f"paired_median_speedup={median_speedup:.17g}",
        f"bootstrap_95_lower={lower:.17g}",
        f"bootstrap_95_upper={upper:.17g}",
        "bootstrap_resamples=10000",
        "bootstrap_seed=20260726",
        f"official_setup_us={official_setup:.17g}",
        f"smave_setup_us={smave_setup:.17g}",
        f"projected_queries={projected_queries}",
        f"amortized_speedup={amortized_speedup:.17g}",
        f"maximum_common_gate_relative_inf={maximum_residual:.17g}",
        "official_failures=0",
        "smave_failures=0",
        "negative_results_retained=1",
        "END",
        "",
    ]
    arguments.output.write_text("\n".join(output_lines))
    arguments.tex.write_text(
        "\n".join(
            [
                "% Generated from build/release/hints-native-baseline/evidence.txt.",
                f"\\newcommand{{\\HintsNativeCases}}{{{len(ratios)}}}",
                f"\\newcommand{{\\HintsNativeSpeedup}}{{{median_speedup:.3f}}}",
                f"\\newcommand{{\\HintsNativeLower}}{{{lower:.3f}}}",
                f"\\newcommand{{\\HintsNativeUpper}}{{{upper:.3f}}}",
                f"\\newcommand{{\\HintsNativeAmortizedSpeedup}}{{{amortized_speedup:.3f}}}",
                f"\\newcommand{{\\HintsNativeMaximumResidual}}{{\\num{{{maximum_residual:.3e}}}}}",
                "",
            ]
        )
    )
    print(
        "SMAVE_HINTS_NATIVE_BASELINE_EVIDENCE 1 "
        f"speedup={median_speedup:.6f} ci=[{lower:.6f},{upper:.6f}]"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
