#!/usr/bin/env python3

import argparse
import datetime
import hashlib
import json
import math
import os
import platform
import re
import shutil
import subprocess
from pathlib import Path

from native_external_performance_contract import validate_component_set


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


def require_positive(values: dict[str, str], keys: list[str], path: Path) -> None:
    for key in keys:
        try:
            value = float(values[key])
        except (KeyError, ValueError) as error:
            raise ValueError(f"invalid numeric field {key}: {path}") from error
        if not math.isfinite(value) or value <= 0.0:
            raise ValueError(f"expected positive {key}: {path}")


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def relative(path: Path, root: Path) -> str:
    try:
        return path.resolve().relative_to(root.resolve()).as_posix()
    except ValueError as error:
        raise ValueError(f"component is outside output directory: {path}") from error


def first_line(command: list[str]) -> str:
    try:
        result = subprocess.run(
            command, check=True, text=True, capture_output=True
        )
    except (OSError, subprocess.CalledProcessError):
        return "unavailable"
    output = result.stdout or result.stderr
    return output.splitlines()[0] if output.splitlines() else "unavailable"


def cpu_model() -> str:
    cpuinfo = Path("/proc/cpuinfo")
    if cpuinfo.exists():
        for line in cpuinfo.read_text(errors="replace").splitlines():
            if line.lower().startswith("model name") and ":" in line:
                return line.split(":", 1)[1].strip()
    if platform.system() == "Darwin":
        return first_line(["sysctl", "-n", "machdep.cpu.brand_string"])
    return platform.processor() or "unavailable"


def memory_bytes() -> str:
    meminfo = Path("/proc/meminfo")
    if meminfo.exists():
        for line in meminfo.read_text().splitlines():
            if line.startswith("MemTotal:"):
                return str(int(line.split()[1]) * 1024)
    if platform.system() == "Darwin":
        return first_line(["sysctl", "-n", "hw.memsize"])
    return "unavailable"


def virtualization() -> str:
    detector = shutil.which("systemd-detect-virt")
    if detector:
        result = subprocess.run(
            [detector], text=True, capture_output=True, check=False
        )
        value = result.stdout.strip()
        if value:
            return value
    product = Path("/sys/class/dmi/id/product_name")
    if product.exists():
        return product.read_text(errors="replace").strip() or "unknown"
    return "unknown"


def clean(value: str) -> str:
    return " ".join(value.replace("=", ":").split()) or "unavailable"


def github_context() -> dict[str, str]:
    required = {
        "GITHUB_ACTIONS": "true",
        "RUNNER_ENVIRONMENT": "github-hosted",
        "RUNNER_OS": "Linux",
        "RUNNER_ARCH": "X64",
    }
    for key, value in required.items():
        if os.environ.get(key) != value:
            raise ValueError(f"GitHub-hosted contract requires {key}={value}")
    names = (
        "GITHUB_REPOSITORY",
        "GITHUB_SHA",
        "GITHUB_RUN_ID",
        "GITHUB_RUN_ATTEMPT",
        "GITHUB_JOB",
        "GITHUB_WORKFLOW_REF",
        "GITHUB_SERVER_URL",
    )
    values = {name: os.environ.get(name, "") for name in names}
    for name, value in values.items():
        if not value:
            raise ValueError(f"GitHub-hosted contract requires {name}")
    if not re.fullmatch(r"[0-9a-fA-F]{40}", values["GITHUB_SHA"]):
        raise ValueError("GITHUB_SHA is not a full commit identifier")
    if platform.machine().lower() not in {"x86_64", "amd64"}:
        raise ValueError("GitHub-hosted campaign requires reported x86-64")
    event_visibility = "unknown"
    event_path = os.environ.get("GITHUB_EVENT_PATH")
    if event_path and Path(event_path).exists():
        try:
            event = json.loads(Path(event_path).read_text())
            event_visibility = event.get("repository", {}).get("visibility", "unknown")
        except (OSError, ValueError):
            event_visibility = "unknown"
    return {
        "provider": "github-hosted",
        "external_provider": "1",
        "performance_evidence": "1",
        "native_external_performance": "1",
        "native_isa_execution": "1",
        "provider_hosted_vm": "1",
        "bare_metal": "0",
        "artifact_attestation_requested": "1",
        "source_commit_bound": "1",
        "repository": values["GITHUB_REPOSITORY"],
        "repository_visibility": event_visibility,
        "source_commit_sha": values["GITHUB_SHA"].lower(),
        "run_id": values["GITHUB_RUN_ID"],
        "run_attempt": values["GITHUB_RUN_ATTEMPT"],
        "job": values["GITHUB_JOB"],
        "workflow_ref": values["GITHUB_WORKFLOW_REF"],
        "invocation_url": (
            f"{values['GITHUB_SERVER_URL']}/{values['GITHUB_REPOSITORY']}"
            f"/actions/runs/{values['GITHUB_RUN_ID']}"
            f"/attempts/{values['GITHUB_RUN_ATTEMPT']}"
        ),
    }


def local_context() -> dict[str, str]:
    return {
        "provider": "local",
        "external_provider": "0",
        "performance_evidence": "0",
        "native_external_performance": "0",
        "native_isa_execution": "1",
        "provider_hosted_vm": "0",
        "bare_metal": "0",
        "artifact_attestation_requested": "0",
        "source_commit_bound": "0",
        "repository": "local-unpublished-snapshot",
        "repository_visibility": "unknown",
        "source_commit_sha": "unavailable-no-remote-commit",
        "run_id": "local",
        "run_attempt": "1",
        "job": "local-dry-run",
        "workflow_ref": "unavailable",
        "invocation_url": "unavailable",
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--provider", choices=("local", "github-hosted"), required=True)
    parser.add_argument("--replicate-id", type=int, required=True)
    parser.add_argument("--build-directory", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--gate-parallel", type=Path, required=True)
    parser.add_argument("--gate-parallel-samples", type=Path, required=True)
    parser.add_argument("--risk-adaptive", type=Path, required=True)
    parser.add_argument("--risk-adaptive-samples", type=Path, required=True)
    parser.add_argument("--operator-replication", type=Path, required=True)
    parser.add_argument("--operator-performance", type=Path, required=True)
    parser.add_argument("--operator-statistics", type=Path, required=True)
    arguments = parser.parse_args()
    if arguments.replicate_id <= 0:
        raise ValueError("replicate id must be positive")

    gate_parallel = parse_envelope(
        arguments.gate_parallel, "SMAVE_GATE_PARALLEL_SCALING 1"
    )
    require_fields(
        gate_parallel,
        {
            "contract": "paired-fused-original-equation-gate-worker-scaling",
            "families": "linear,nonlinear",
            "workers": "1,2,4,8,10",
            "linear.repetitions": "30",
            "nonlinear.repetitions": "30",
            "linear.decision_mismatches": "0",
            "linear.residual_mismatches": "0",
            "nonlinear.decision_mismatches": "0",
            "nonlinear.residual_mismatches": "0",
            "strict_equivalence": "1",
        },
        arguments.gate_parallel,
    )
    require_positive(
        gate_parallel,
        [
            f"{family}.worker_{worker}.{field}"
            for family in ("linear", "nonlinear")
            for worker in (1, 2, 4, 8, 10)
            for field in (
                "total_median_us",
                "paired_speedup",
                "bootstrap_95_lower",
                "bootstrap_95_upper",
            )
        ],
        arguments.gate_parallel,
    )
    gate_parallel_samples = parse_envelope(
        arguments.gate_parallel_samples,
        "SMAVE_GATE_PARALLEL_SCALING_SAMPLES 1",
    )
    require_fields(
        gate_parallel_samples,
        {
            "contract": "paired-fused-original-equation-gate-worker-scaling-raw-samples",
            "families": "linear,nonlinear",
            "workers": "1,2,4,8,10",
            "repetitions": "30",
        },
        arguments.gate_parallel_samples,
    )

    risk = parse_envelope(arguments.risk_adaptive, "SMAVE_RISK_ADAPTIVE_GATE 1")
    require_fields(
        risk,
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
        arguments.risk_adaptive,
    )
    risk_metrics = [
        f"{workload}.{field}"
        for workload in (
            "full-solve-linear",
            "full-solve-nonlinear",
            "full-solve-scaled-nonlinear",
        )
        for field in (
            "strict_total_median_us",
            "adaptive_total_median_us",
            "total_speedup",
            "total_speedup_ci95_lower",
            "strict_gate_median_us",
            "adaptive_gate_median_us",
            "gate_speedup",
            "gate_speedup_ci95_lower",
        )
    ]
    require_positive(risk, risk_metrics, arguments.risk_adaptive)
    risk_samples = parse_envelope(
        arguments.risk_adaptive_samples, "SMAVE_RISK_ADAPTIVE_GATE_SAMPLES 1"
    )
    require_fields(
        risk_samples,
        {
            "contract": "counterbalanced-strict-adaptive-complete-cost-raw-samples",
            "repetitions": "100",
        },
        arguments.risk_adaptive_samples,
    )

    operator = parse_envelope(
        arguments.operator_replication, "SMAVE_OPERATOR_REPLICATION 1"
    )
    require_fields(
        operator,
        {
            "repetitions": "100",
            "failures": "0",
            "fallbacks": "0",
            "gate_mismatches": "0",
            "same_accuracy": "1",
        },
        arguments.operator_replication,
    )
    require_positive(
        operator,
        ("paired_median_speedup", "bootstrap_95_lower", "bootstrap_95_upper"),
        arguments.operator_replication,
    )
    operator_performance = parse_envelope(
        arguments.operator_performance,
        ("SMAVE_OPERATOR_BENCHMARK 1", "SMAVE_OPERATOR_BENCHMARK 3"),
    )
    require_fields(
        operator_performance,
        {
            "repetitions": "100",
            "failures": "0",
            "fallbacks": "0",
            "same_accuracy": "1",
        },
        arguments.operator_performance,
    )
    operator_statistics = parse_envelope(
        arguments.operator_statistics, "SMAVE_OPERATOR_STATISTICS 1"
    )
    require_fields(
        operator_statistics,
        {"repetitions": "100", "bootstrap_resamples": "10000"},
        arguments.operator_statistics,
    )

    output_root = arguments.output.parent
    component_paths = {
        "gate_parallel_file": arguments.gate_parallel,
        "gate_parallel_samples_file": arguments.gate_parallel_samples,
        "risk_adaptive_file": arguments.risk_adaptive,
        "risk_adaptive_samples_file": arguments.risk_adaptive_samples,
        "operator_replication_file": arguments.operator_replication,
        "operator_performance_file": arguments.operator_performance,
        "operator_statistics_file": arguments.operator_statistics,
    }
    component_validation = validate_component_set(component_paths)
    context = github_context() if arguments.provider == "github-hosted" else local_context()
    fields = [
        "SMAVE_NATIVE_EXTERNAL_PERFORMANCE 1",
        "contract=provenance-bound-native-hosted-solver-performance-campaign",
        "campaign_schema=2",
        f"provider={context['provider']}",
        f"replicate_id={arguments.replicate_id}",
        f"collected_at_utc={datetime.datetime.now(datetime.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')}",
        f"external_provider={context['external_provider']}",
        f"performance_evidence={context['performance_evidence']}",
        f"native_external_performance={context['native_external_performance']}",
        f"native_isa_execution={context['native_isa_execution']}",
        f"provider_hosted_vm={context['provider_hosted_vm']}",
        f"bare_metal={context['bare_metal']}",
        "independent_reproduction=0",
        "author_operated_workflow=1",
        "public_immutable_archive=0",
        f"artifact_attestation_requested={context['artifact_attestation_requested']}",
        f"source_commit_bound={context['source_commit_bound']}",
        f"repository={clean(context['repository'])}",
        f"repository_visibility={clean(context['repository_visibility'])}",
        f"source_commit_sha={clean(context['source_commit_sha'])}",
        f"run_id={clean(context['run_id'])}",
        f"run_attempt={clean(context['run_attempt'])}",
        f"job={clean(context['job'])}",
        f"workflow_ref={clean(context['workflow_ref'])}",
        f"invocation_url={clean(context['invocation_url'])}",
        f"runner_system={clean(platform.system())}",
        f"runner_release={clean(platform.release())}",
        f"runner_machine={clean(platform.machine())}",
        f"cpu_model={clean(cpu_model())}",
        f"logical_cpus={os.cpu_count() or 0}",
        f"memory_bytes={clean(memory_bytes())}",
        f"virtualization={clean(virtualization())}",
        f"cxx_compiler={clean(first_line(['c++', '--version']))}",
        f"cmake={clean(first_line(['cmake', '--version']))}",
        f"ninja={clean(first_line(['ninja', '--version']))}",
        f"python={clean(platform.python_version())}",
        f"build_directory={clean(str(arguments.build_directory))}",
        "build_type=Release",
        "benchmark_scope=self-contained-solver-pipeline-fixtures",
        "component_evidence_sets=3",
        "raw_sample_sets=2",
        f"raw_sample_values={component_validation['raw_sample_values']}",
        "direction_agnostic_acceptance=1",
        "performance_threshold_gate=0",
        "negative_results_retained=1",
        "correctness_required=1",
    ]
    for key, path in component_paths.items():
        fields.append(f"{key}={relative(path, output_root)}")
        fields.append(f"{key.removesuffix('_file')}_sha256={digest(path)}")
    selected_metrics = dict(component_validation["selected_metrics"])
    selected_metrics["operator_replication.stable_speedup"] = component_validation[
        "operator_stable_speedup"
    ]
    fields.extend(f"{key}={value}" for key, value in selected_metrics.items())
    fields.extend(
        [
            "claim_scope=native-provider-hosted-vm-self-contained-workloads-only",
            "pdebench_payload_performance=0",
            "customer_workload_performance=0",
            "accelerator_performance=0",
            "numa_performance=0",
            "END",
            "",
        ]
    )
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_text("\n".join(fields))
    print(
        "SMAVE_NATIVE_EXTERNAL_PERFORMANCE_COLLECTED 1 "
        f"provider={context['provider']} external={context['external_provider']}"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (KeyError, OSError, ValueError) as error:
        print(f"native external performance collection failed: {error}")
        raise SystemExit(1)
