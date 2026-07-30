#!/usr/bin/env python3

import argparse
import random
import statistics
from pathlib import Path


WORKLOADS = (
    "advection",
    "darcy",
    "burgers",
    "diffusion-sorption",
    "shallow-water",
    "ns-incompressible",
    "cfd-1d",
)
ORDERS = ("classical-first", "smave-first")


def parse_report(path: Path) -> tuple[str, float]:
    values = {}
    for line in path.read_text().splitlines():
        fields = line.split(maxsplit=1)
        if len(fields) == 2:
            values[fields[0]] = fields[1].strip('"')
    if values.get("CROSS_SOLVER_AGREEMENT") != "1":
        raise ValueError(f"{path}: cross-solver agreement failed")
    return values["SOLVER_ORDER"], float(values["SMAVE_VS_CLASSICAL_SPEEDUP"])


def percentile(sorted_values: list[float], probability: float) -> float:
    position = probability * (len(sorted_values) - 1)
    lower_index = int(position)
    upper_index = min(lower_index + 1, len(sorted_values) - 1)
    fraction = position - lower_index
    return (
        sorted_values[lower_index] * (1.0 - fraction)
        + sorted_values[upper_index] * fraction
    )


def bootstrap_interval(values: list[float], seed: int) -> tuple[float, float]:
    generator = random.Random(seed)
    medians = []
    for _ in range(10000):
        medians.append(statistics.median([generator.choice(values) for _ in values]))
    medians.sort()
    return percentile(medians, 0.025), percentile(medians, 0.975)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--reports", type=Path, required=True)
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--repetitions", type=int, required=True)
    arguments = parser.parse_args()

    evidence = [
        "SMAVE_PDEBENCH_ORDER_SENSITIVITY 1",
        "contract=paired-counterbalanced-process-order-sensitivity",
        f"repetitions={arguments.repetitions}",
        f"workloads={len(WORKLOADS)}",
        f"measured_pairs={len(WORKLOADS) * arguments.repetitions}",
        f"measured_reports={len(WORKLOADS) * len(ORDERS) * arguments.repetitions}",
        f"warmup_reports={len(WORKLOADS) * len(ORDERS)}",
        "bootstrap_resamples=10000",
        "bootstrap_seed_base=20260723",
        "within_pair_order=alternated",
    ]
    maximum_shift = 0.0
    median_ratios = []
    intervals_containing_one = 0
    all_measured_speedups_exceed_one = True
    for workload_index, workload in enumerate(WORKLOADS):
        speedups = {order: [] for order in ORDERS}
        for order in ORDERS:
            warmup_path = arguments.reports / f"warmup-{workload}-{order}.txt"
            reported_order, _ = parse_report(warmup_path)
            if reported_order != order:
                raise ValueError(f"{warmup_path}: solver order mismatch")
        for repetition in range(1, arguments.repetitions + 1):
            for order in ORDERS:
                report_path = arguments.reports / (
                    f"{workload}-{order}-repeat-{repetition:02d}.txt"
                )
                reported_order, speedup = parse_report(report_path)
                if reported_order != order:
                    raise ValueError(f"{report_path}: solver order mismatch")
                speedups[order].append(speedup)
                all_measured_speedups_exceed_one = (
                    all_measured_speedups_exceed_one and speedup > 1.0
                )
        first_median = statistics.median(speedups["classical-first"])
        second_median = statistics.median(speedups["smave-first"])
        paired_ratios = [
            second / first
            for first, second in zip(
                speedups["classical-first"], speedups["smave-first"]
            )
        ]
        lower, upper = bootstrap_interval(paired_ratios, 20260723 + workload_index)
        median_ratio = statistics.median(paired_ratios)
        median_ratios.append(median_ratio)
        intervals_containing_one += int(lower <= 1.0 <= upper)
        maximum_shift = max(maximum_shift, abs(median_ratio - 1.0))
        prefix = workload.replace("-", "_")
        evidence.extend(
            [
                f"{prefix}.classical_first_median_speedup={first_median:.17g}",
                f"{prefix}.smave_first_median_speedup={second_median:.17g}",
                f"{prefix}.paired_order_ratio_median={median_ratio:.17g}",
                f"{prefix}.paired_order_ratio_bootstrap_95_lower={lower:.17g}",
                f"{prefix}.paired_order_ratio_bootstrap_95_upper={upper:.17g}",
            ]
        )
    evidence.extend(
        [
            f"minimum_paired_order_ratio_median={min(median_ratios):.17g}",
            f"maximum_paired_order_ratio_median={max(median_ratios):.17g}",
            f"maximum_absolute_median_order_shift={maximum_shift:.17g}",
            f"bootstrap_95_intervals_containing_one={intervals_containing_one}",
            "bootstrap_95_intervals_total=" + str(len(WORKLOADS)),
            "all_measured_speedups_exceed_one="
            + str(int(all_measured_speedups_exceed_one)),
            "END",
        ]
    )
    arguments.evidence.parent.mkdir(parents=True, exist_ok=True)
    arguments.evidence.write_text("\n".join(evidence) + "\n")


if __name__ == "__main__":
    main()
