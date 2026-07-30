#!/usr/bin/env python3

from pathlib import Path


FAMILIES = (("linear", "Linear"), ("nonlinear", "Nonlinear"))
WORKERS = ((1, "One"), (2, "Two"), (4, "Four"), (8, "Eight"))


def parse_evidence(path: Path) -> dict[str, str]:
    values = {}
    for line in path.read_text().splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            values[key] = value
    return values


def main() -> None:
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--tex", type=Path, required=True)
    arguments = parser.parse_args()

    values = parse_evidence(arguments.evidence)
    expected = {
        "contract": "process-isolated-fused-original-equation-gate-worker-scaling",
        "process_model": "posix-fork-pipe",
        "input_transport": "fork-inherited-copy-on-write",
        "result_transport": "pipe-summary",
        "fresh_processes_per_measurement": "1",
        "process_launch_and_wait_timed": "1",
        "request_serialization_timed": "0",
        "network_transport": "0",
        "same_host_only": "1",
        "families": "linear,nonlinear",
        "workers": "1,2,4,8",
        "warmup_configurations_per_family": "4",
        "measured_configurations_per_family": "120",
        "child_processes_per_family": "465",
        "total_child_processes": "930",
        "measured_gate_evaluations_per_family": "249600",
        "total_measured_gate_evaluations": "499200",
        "bootstrap_resamples": "10000",
        "bootstrap_seed": "20260724",
        "strict_equivalence": "1",
    }
    for key, value in expected.items():
        if values.get(key) != value:
            raise ValueError(f"expected {key}={value}")

    lines = [
        "% Generated from build/release/gate-process-scaling/evidence.txt.",
    ]
    for family, command_family in FAMILIES:
        if values.get(f"{family}.requests_per_repetition") != "2080":
            raise ValueError(f"{family}: request count changed")
        if values.get(f"{family}.repetitions") != "30":
            raise ValueError(f"{family}: repetition count changed")
        if values.get(f"{family}.decision_mismatches") != "0":
            raise ValueError(f"{family}: decision mismatch")
        if values.get(f"{family}.residual_mismatches") != "0":
            raise ValueError(f"{family}: residual mismatch")
        for worker, command_worker in WORKERS:
            speedup = float(values[f"{family}.worker_{worker}.paired_speedup"])
            lines.append(
                f"\\newcommand{{\\Process{command_family}{command_worker}}}"
                f"{{{speedup:.3f}}}"
            )
        if family == "linear" and float(
            values["linear.worker_8.bootstrap_95_lower"]
        ) <= 1.0:
            raise ValueError("linear process scaling lower bound no longer exceeds one")
        if family == "nonlinear" and float(
            values["nonlinear.worker_8.bootstrap_95_upper"]
        ) >= 1.0:
            raise ValueError("nonlinear process regression upper bound no longer below one")
        lines.extend(
            [
                f"\\newcommand{{\\Process{command_family}EightShort}}"
                f"{{{float(values[f'{family}.worker_8.paired_speedup']):.2f}}}",
                f"\\newcommand{{\\Process{command_family}EightLower}}"
                f"{{{float(values[f'{family}.worker_8.bootstrap_95_lower']):.3f}}}",
                f"\\newcommand{{\\Process{command_family}EightUpper}}"
                f"{{{float(values[f'{family}.worker_8.bootstrap_95_upper']):.3f}}}",
            ]
        )
    arguments.tex.parent.mkdir(parents=True, exist_ok=True)
    arguments.tex.write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
