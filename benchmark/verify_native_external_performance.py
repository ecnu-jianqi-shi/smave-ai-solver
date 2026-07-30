#!/usr/bin/env python3

import argparse
import hashlib
import math
import re
from pathlib import Path
from typing import Optional

from native_external_performance_contract import validate_component_set


HEADER = "SMAVE_NATIVE_EXTERNAL_PERFORMANCE 1"


def parse(path: Path) -> dict[str, str]:
    lines = path.read_text().splitlines()
    if not lines or lines[0] != HEADER or lines[-1] != "END":
        raise ValueError("invalid native external performance envelope")
    values: dict[str, str] = {}
    for line in lines[1:-1]:
        if "=" not in line:
            raise ValueError(f"invalid evidence field: {line}")
        key, value = line.split("=", 1)
        if key in values:
            raise ValueError(f"duplicate evidence field: {key}")
        values[key] = value
    return values


def require(values: dict[str, str], expected: dict[str, str]) -> None:
    for key, value in expected.items():
        if values.get(key) != value:
            raise ValueError(f"expected {key}={value}")


def validate(path: Path, expected_provider: Optional[str] = None) -> dict[str, str]:
    values = parse(path)
    require(
        values,
        {
            "contract": "provenance-bound-native-hosted-solver-performance-campaign",
            "campaign_schema": "2",
            "independent_reproduction": "0",
            "author_operated_workflow": "1",
            "public_immutable_archive": "0",
            "build_type": "Release",
            "benchmark_scope": "self-contained-solver-pipeline-fixtures",
            "component_evidence_sets": "3",
            "raw_sample_sets": "2",
            "raw_sample_values": "1900",
            "direction_agnostic_acceptance": "1",
            "performance_threshold_gate": "0",
            "negative_results_retained": "1",
            "correctness_required": "1",
            "claim_scope": "native-provider-hosted-vm-self-contained-workloads-only",
            "pdebench_payload_performance": "0",
            "customer_workload_performance": "0",
            "accelerator_performance": "0",
            "numa_performance": "0",
        },
    )
    if expected_provider is not None and values.get("provider") != expected_provider:
        raise ValueError(f"expected provider={expected_provider}")
    if values.get("provider") == "local":
        require(
            values,
            {
                "external_provider": "0",
                "performance_evidence": "0",
                "native_external_performance": "0",
                "provider_hosted_vm": "0",
                "artifact_attestation_requested": "0",
                "source_commit_bound": "0",
            },
        )
    elif values.get("provider") == "github-hosted":
        require(
            values,
            {
                "external_provider": "1",
                "performance_evidence": "1",
                "native_external_performance": "1",
                "native_isa_execution": "1",
                "provider_hosted_vm": "1",
                "bare_metal": "0",
                "artifact_attestation_requested": "1",
                "source_commit_bound": "1",
                "runner_system": "Linux",
            },
        )
        if values.get("runner_machine", "").lower() not in {"x86_64", "amd64"}:
            raise ValueError("GitHub-hosted runner is not x86-64")
        if not re.fullmatch(r"[0-9a-f]{40}", values.get("source_commit_sha", "")):
            raise ValueError("invalid hosted source commit")
        for key in (
            "repository",
            "run_id",
            "run_attempt",
            "job",
            "workflow_ref",
            "invocation_url",
        ):
            if values.get(key, "").startswith("unavailable") or not values.get(key):
                raise ValueError(f"missing hosted provenance: {key}")
    else:
        raise ValueError("unknown provider")

    root = path.parent
    file_keys = (
        "gate_parallel_file",
        "gate_parallel_samples_file",
        "risk_adaptive_file",
        "risk_adaptive_samples_file",
        "operator_replication_file",
        "operator_performance_file",
        "operator_statistics_file",
    )
    component_paths = {}
    for key in file_keys:
        component = (root / values[key]).resolve()
        try:
            component.relative_to(root.resolve())
        except ValueError as error:
            raise ValueError(f"component path escapes evidence root: {component}") from error
        if not component.is_file():
            raise ValueError(f"missing component file: {component}")
        digest = hashlib.sha256(component.read_bytes()).hexdigest()
        digest_key = f"{key.removesuffix('_file')}_sha256"
        if values.get(digest_key) != digest:
            raise ValueError(f"component digest mismatch: {component}")
        component_paths[key] = component
    component_validation = validate_component_set(component_paths)
    if str(component_validation["raw_sample_values"]) != values["raw_sample_values"]:
        raise ValueError("raw sample count mismatch")
    for key, expected in component_validation["selected_metrics"].items():
        try:
            value = float(values[key])
            component_value = float(expected)
        except (KeyError, ValueError) as error:
            raise ValueError(f"invalid performance metric: {key}") from error
        if not math.isfinite(value) or value <= 0.0:
            raise ValueError(f"invalid performance metric: {key}")
        if not math.isclose(value, component_value, rel_tol=1.0e-12, abs_tol=1.0e-9):
            raise ValueError(f"campaign-component metric mismatch: {key}")
    if values.get("operator_replication.stable_speedup") != component_validation[
        "operator_stable_speedup"
    ]:
        raise ValueError("invalid operator stable-speedup status")
    if int(values.get("replicate_id", "0")) <= 0:
        raise ValueError("invalid replicate id")
    return values


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--evidence", type=Path, required=True)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--expect-local", action="store_true")
    group.add_argument("--expect-github-hosted", action="store_true")
    arguments = parser.parse_args()
    provider = "local" if arguments.expect_local else "github-hosted"
    values = validate(arguments.evidence, provider)
    print(
        "SMAVE_NATIVE_EXTERNAL_PERFORMANCE_CHECK 1 "
        f"provider={provider} external={values['external_provider']}"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (KeyError, OSError, ValueError) as error:
        print(f"native external performance verification failed: {error}")
        raise SystemExit(1)
