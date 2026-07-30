#!/usr/bin/env python3

import math
import statistics
from pathlib import Path


CAMPAIGN_METRICS = (
    "gate_parallel.linear.worker_10.paired_speedup",
    "gate_parallel.nonlinear.worker_10.paired_speedup",
    "risk_adaptive.full_solve_linear.total_speedup",
    "risk_adaptive.full_solve_nonlinear.total_speedup",
    "risk_adaptive.full_solve_scaled_nonlinear.total_speedup",
    "operator_replication.paired_median_speedup",
)


def parse_envelope(path: Path, header) -> dict[str, str]:
    lines = path.read_text().splitlines()
    accepted_headers = (header,) if isinstance(header, str) else tuple(header)
    if not lines or lines[0] not in accepted_headers or lines[-1] != "END":
        raise ValueError(f"invalid evidence envelope: {path}")
    values: dict[str, str] = {}
    for line in lines[1:-1]:
        if "=" not in line:
            raise ValueError(f"invalid evidence field: {path}: {line}")
        key, value = line.split("=", 1)
        if key in values:
            raise ValueError(f"duplicate evidence field: {path}: {key}")
        values[key] = value
    return values


def require_fields(
    values: dict[str, str], expected: dict[str, str], path: Path
) -> None:
    for key, value in expected.items():
        if values.get(key) != value:
            raise ValueError(f"expected {key}={value}: {path}")


def numeric(values: dict[str, str], key: str, path: Path) -> float:
    try:
        value = float(values[key])
    except (KeyError, ValueError) as error:
        raise ValueError(f"invalid numeric field {key}: {path}") from error
    if not math.isfinite(value) or value <= 0.0:
        raise ValueError(f"expected positive {key}: {path}")
    return value


def require_close(path: Path, key: str, actual: float, expected: float) -> None:
    if not math.isclose(actual, expected, rel_tol=1.0e-12, abs_tol=1.0e-9):
        raise ValueError(
            f"raw-summary mismatch {key}: {actual:.17g} != {expected:.17g}: {path}"
        )


def require_exact_keys(
    values: dict[str, str], expected_keys: set[str], path: Path
) -> None:
    missing = sorted(expected_keys - values.keys())
    extra = sorted(values.keys() - expected_keys)
    if missing:
        raise ValueError(f"missing raw sample field {missing[0]}: {path}")
    if extra:
        raise ValueError(f"unexpected raw sample field {extra[0]}: {path}")


def validate_scaling(
    summary_path: Path,
    samples_path: Path,
    summary_header: str,
    samples_header: str,
    summary_contract: str,
    samples_contract: str,
    workers: tuple[int, ...],
    same_host_only: bool,
) -> tuple[dict[str, str], int]:
    summary = parse_envelope(summary_path, summary_header)
    expected_summary = {
        "contract": summary_contract,
        "families": "linear,nonlinear",
        "workers": ",".join(str(worker) for worker in workers),
        "linear.repetitions": "30",
        "nonlinear.repetitions": "30",
        "linear.decision_mismatches": "0",
        "linear.residual_mismatches": "0",
        "nonlinear.decision_mismatches": "0",
        "nonlinear.residual_mismatches": "0",
        "strict_equivalence": "1",
    }
    if same_host_only:
        expected_summary["same_host_only"] = "1"
    require_fields(summary, expected_summary, summary_path)

    samples = parse_envelope(samples_path, samples_header)
    metadata = {
        "contract": samples_contract,
        "families": "linear,nonlinear",
        "workers": ",".join(str(worker) for worker in workers),
        "repetitions": "30",
    }
    require_fields(samples, metadata, samples_path)
    sample_keys = {
        f"{family}.repetition_{repetition}.worker_{worker}.total_us"
        for family in ("linear", "nonlinear")
        for repetition in range(1, 31)
        for worker in workers
    }
    require_exact_keys(samples, set(metadata) | sample_keys, samples_path)
    for family in ("linear", "nonlinear"):
        timings = {
            worker: [
                numeric(
                    samples,
                    f"{family}.repetition_{repetition}.worker_{worker}.total_us",
                    samples_path,
                )
                for repetition in range(1, 31)
            ]
            for worker in workers
        }
        for worker in workers:
            median_key = f"{family}.worker_{worker}.total_median_us"
            speedup_key = f"{family}.worker_{worker}.paired_speedup"
            require_close(
                summary_path,
                median_key,
                numeric(summary, median_key, summary_path),
                statistics.median(timings[worker]),
            )
            require_close(
                summary_path,
                speedup_key,
                numeric(summary, speedup_key, summary_path),
                statistics.median(
                    baseline / current
                    for baseline, current in zip(timings[workers[0]], timings[worker])
                ),
            )
            for interval in ("bootstrap_95_lower", "bootstrap_95_upper"):
                numeric(summary, f"{family}.worker_{worker}.{interval}", summary_path)
    return summary, len(sample_keys)


def validate_risk(
    summary_path: Path, samples_path: Path
) -> tuple[dict[str, str], int]:
    summary = parse_envelope(summary_path, "SMAVE_RISK_ADAPTIVE_GATE 1")
    require_fields(
        summary,
        {
            "offline_strict_equivalence": "1",
            "full_solve_strict_equivalence": "1",
            "operator-linear-100.false_accepts": "0",
            "operator-linear-100.false_rejects": "0",
            "cubic-coupled-nonlinear.false_accepts": "0",
            "cubic-coupled-nonlinear.false_rejects": "0",
            "full-solve-linear.result_mismatches": "0",
            "full-solve-nonlinear.result_mismatches": "0",
            "full-solve-scaled-nonlinear.result_mismatches": "0",
        },
        summary_path,
    )
    samples = parse_envelope(samples_path, "SMAVE_RISK_ADAPTIVE_GATE_SAMPLES 1")
    metadata = {
        "contract": "counterbalanced-strict-adaptive-complete-cost-raw-samples",
        "repetitions": "100",
    }
    require_fields(samples, metadata, samples_path)
    offline_workloads = ("operator-linear-100", "cubic-coupled-nonlinear")
    full_workloads = (
        "full-solve-linear",
        "full-solve-nonlinear",
        "full-solve-scaled-nonlinear",
    )
    sample_keys = {
        f"{workload}.repetition_{repetition}.{field}_us"
        for workload in offline_workloads
        for repetition in range(1, 101)
        for field in ("strict", "adaptive")
    } | {
        f"{workload}.repetition_{repetition}.{field}_us"
        for workload in full_workloads
        for repetition in range(1, 101)
        for field in (
            "strict_total",
            "adaptive_total",
            "strict_gate",
            "adaptive_gate",
        )
    }
    require_exact_keys(samples, set(metadata) | sample_keys, samples_path)

    for workload in offline_workloads:
        strict = [
            numeric(samples, f"{workload}.repetition_{index}.strict_us", samples_path)
            for index in range(1, 101)
        ]
        adaptive = [
            numeric(samples, f"{workload}.repetition_{index}.adaptive_us", samples_path)
            for index in range(1, 101)
        ]
        strict_median = statistics.median(strict)
        adaptive_median = statistics.median(adaptive)
        expected = {
            "strict_median_us": strict_median,
            "adaptive_median_us": adaptive_median,
            "paired_speedup": strict_median / adaptive_median,
        }
        for field, value in expected.items():
            key = f"{workload}.{field}"
            require_close(summary_path, key, numeric(summary, key, summary_path), value)
        numeric(summary, f"{workload}.paired_speedup_ci95_lower", summary_path)

    for workload in full_workloads:
        arrays = {
            field: [
                numeric(
                    samples,
                    f"{workload}.repetition_{index}.{field}_us",
                    samples_path,
                )
                for index in range(1, 101)
            ]
            for field in (
                "strict_total",
                "adaptive_total",
                "strict_gate",
                "adaptive_gate",
            )
        }
        medians = {field: statistics.median(values) for field, values in arrays.items()}
        expected = {
            "strict_total_median_us": medians["strict_total"],
            "adaptive_total_median_us": medians["adaptive_total"],
            "total_speedup": medians["strict_total"] / medians["adaptive_total"],
            "strict_gate_median_us": medians["strict_gate"],
            "adaptive_gate_median_us": medians["adaptive_gate"],
            "gate_speedup": medians["strict_gate"] / medians["adaptive_gate"],
        }
        for field, value in expected.items():
            key = f"{workload}.{field}"
            require_close(summary_path, key, numeric(summary, key, summary_path), value)
        numeric(summary, f"{workload}.total_speedup_ci95_lower", summary_path)
        numeric(summary, f"{workload}.gate_speedup_ci95_lower", summary_path)
    return summary, len(sample_keys)


def require_same(
    left: dict[str, str],
    left_key: str,
    left_path: Path,
    right: dict[str, str],
    right_key: str,
    right_path: Path,
) -> None:
    left_value = numeric(left, left_key, left_path)
    right_value = numeric(right, right_key, right_path)
    require_close(left_path, left_key, left_value, right_value)


def validate_operator(
    evidence_path: Path, performance_path: Path, statistics_path: Path
) -> dict[str, str]:
    evidence = parse_envelope(evidence_path, "SMAVE_OPERATOR_REPLICATION 1")
    require_fields(
        evidence,
        {
            "repetitions": "100",
            "failures": "0",
            "fallbacks": "0",
            "gate_mismatches": "0",
            "same_accuracy": "1",
        },
        evidence_path,
    )
    performance_header = performance_path.read_text().splitlines()[0]
    performance = parse_envelope(
        performance_path,
        ("SMAVE_OPERATOR_BENCHMARK 1", "SMAVE_OPERATOR_BENCHMARK 3"),
    )
    require_fields(
        performance,
        {
            "repetitions": "100",
            "failures": "0",
            "fallbacks": "0",
            "same_accuracy": "1",
        },
        performance_path,
    )
    statistics_values = parse_envelope(statistics_path, "SMAVE_OPERATOR_STATISTICS 1")
    require_fields(
        statistics_values,
        {"repetitions": "100", "bootstrap_resamples": "10000"},
        statistics_path,
    )
    for key in (
        "cold_baseline_us",
        "cold_operator_us",
        "hot_repetitions",
        "hot_baseline_median_us",
        "hot_operator_median_us",
        "runtime_setup_us",
        "operator_setup_us",
        "rss_before_bytes",
        "rss_after_setup_bytes",
        "peak_rss_bytes",
        "paired_median_speedup",
        "bootstrap_95_lower",
        "bootstrap_95_upper",
    ):
        require_same(evidence, key, evidence_path, statistics_values, key, statistics_path)
    require_fields(
        evidence,
        {"stable_speedup": statistics_values["stable_speedup"]},
        evidence_path,
    )
    for key in ("baseline_median_us", "operator_median_us"):
        require_same(performance, key, performance_path, statistics_values, key, statistics_path)
    online_speedup = numeric(performance, "online_speedup", performance_path)
    if performance_header == "SMAVE_OPERATOR_BENCHMARK 3":
        require_close(
            performance_path,
            "online_speedup",
            online_speedup,
            numeric(statistics_values, "paired_median_speedup", statistics_path),
        )
        require_close(
            performance_path,
            "paired_speedup_ci95_lower",
            numeric(performance, "paired_speedup_ci95_lower", performance_path),
            numeric(statistics_values, "bootstrap_95_lower", statistics_path),
        )
        require_close(
            performance_path,
            "paired_speedup_ci95_upper",
            numeric(performance, "paired_speedup_ci95_upper", performance_path),
            numeric(statistics_values, "bootstrap_95_upper", statistics_path),
        )
    else:
        require_close(
            performance_path,
            "online_speedup",
            online_speedup,
            numeric(performance, "baseline_median_us", performance_path)
            / numeric(performance, "operator_median_us", performance_path),
        )
    for key in ("paired_median_speedup", "bootstrap_95_lower", "bootstrap_95_upper"):
        numeric(evidence, key, evidence_path)
    if evidence["stable_speedup"] not in {"0", "1"}:
        raise ValueError(f"invalid stable_speedup: {evidence_path}")
    return evidence


def validate_component_set(paths: dict[str, Path]) -> dict[str, object]:
    parallel, parallel_count = validate_scaling(
        paths["gate_parallel_file"],
        paths["gate_parallel_samples_file"],
        "SMAVE_GATE_PARALLEL_SCALING 1",
        "SMAVE_GATE_PARALLEL_SCALING_SAMPLES 1",
        "paired-fused-original-equation-gate-worker-scaling",
        "paired-fused-original-equation-gate-worker-scaling-raw-samples",
        (1, 2, 4, 8, 10),
        False,
    )
    risk, risk_count = validate_risk(
        paths["risk_adaptive_file"], paths["risk_adaptive_samples_file"]
    )
    operator = validate_operator(
        paths["operator_replication_file"],
        paths["operator_performance_file"],
        paths["operator_statistics_file"],
    )
    selected_metrics = {
        "gate_parallel.linear.worker_10.paired_speedup": parallel[
            "linear.worker_10.paired_speedup"
        ],
        "gate_parallel.nonlinear.worker_10.paired_speedup": parallel[
            "nonlinear.worker_10.paired_speedup"
        ],
        "risk_adaptive.full_solve_linear.total_speedup": risk[
            "full-solve-linear.total_speedup"
        ],
        "risk_adaptive.full_solve_nonlinear.total_speedup": risk[
            "full-solve-nonlinear.total_speedup"
        ],
        "risk_adaptive.full_solve_scaled_nonlinear.total_speedup": risk[
            "full-solve-scaled-nonlinear.total_speedup"
        ],
        "operator_replication.paired_median_speedup": operator[
            "paired_median_speedup"
        ],
    }
    return {
        "raw_sample_values": parallel_count + risk_count,
        "selected_metrics": selected_metrics,
        "operator_stable_speedup": operator["stable_speedup"],
    }
