#!/usr/bin/env python3

import argparse
import hashlib
import math
import statistics
import sys
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parent))
from native_external_performance_contract import (
    CAMPAIGN_METRICS,
    parse_envelope,
    require_fields,
)
from verify_native_external_performance import validate


HEADER = "SMAVE_NATIVE_EXTERNAL_PERFORMANCE_CAMPAIGN 1"


def require_exact_schema(
    values: dict[str, str], expected_keys: set[str], path: Path
) -> None:
    missing = sorted(expected_keys - values.keys())
    extra = sorted(values.keys() - expected_keys)
    if missing:
        raise ValueError(f"missing campaign field {missing[0]}: {path}")
    if extra:
        raise ValueError(f"unexpected campaign field {extra[0]}: {path}")


def require_close(key: str, actual: str, expected: float, path: Path) -> None:
    try:
        value = float(actual)
    except ValueError as error:
        raise ValueError(f"invalid campaign metric {key}: {path}") from error
    if not math.isfinite(value) or value <= 0.0:
        raise ValueError(f"expected positive campaign metric {key}: {path}")
    if not math.isclose(value, expected, rel_tol=1.0e-12, abs_tol=1.0e-9):
        raise ValueError(
            f"campaign metric mismatch {key}: {value:.17g} != {expected:.17g}: {path}"
        )


def validate_campaign(
    campaign_path: Path, input_root: Path, expected_replicates: int = 3
) -> dict[str, str]:
    if expected_replicates <= 0:
        raise ValueError("expected replicate count must be positive")
    campaign_path = campaign_path.resolve()
    input_root = input_root.resolve()
    values = parse_envelope(campaign_path, HEADER)
    required = {
        "contract": "three-job-github-hosted-solver-performance-campaign",
        "replicates": str(expected_replicates),
        "provider": "github-hosted",
        "external_provider": "1",
        "performance_evidence": "1",
        "native_external_performance": "1",
        "native_isa_execution": "1",
        "provider_hosted_vm": "1",
        "bare_metal": "0",
        "distinct_workflow_jobs": "1",
        "independent_reproduction": "0",
        "author_operated_workflow": "1",
        "public_immutable_archive": "0",
        "artifact_attestation_requested": "1",
        "artifact_attestation_embedded": "0",
        "source_commit_bound": "1",
        "direction_agnostic_acceptance": "1",
        "performance_threshold_gate": "0",
        "negative_results_retained": "1",
        "claim_scope": "three-github-hosted-x86-64-vm-jobs-self-contained-workloads-only",
        "pdebench_payload_performance": "0",
        "customer_workload_performance": "0",
        "accelerator_performance": "0",
        "numa_performance": "0",
    }
    require_fields(values, required, campaign_path)

    per_replicate_fields = (
        "id",
        "evidence_file",
        "evidence_sha256",
        "collected_at_utc",
        "job",
        "invocation_url",
        "runner_system",
        "runner_machine",
        "cpu_model",
        "logical_cpus",
        "memory_bytes",
        "virtualization",
    )
    base_keys = set(required) | {
        "repository",
        "source_commit_sha",
        "run_id",
        "run_attempt",
        "workflow_ref",
    }
    replicate_keys = {
        f"replicate_{index}.{field}"
        for index in range(1, expected_replicates + 1)
        for field in per_replicate_fields
    }
    metric_keys = {
        f"{metric}.{field}"
        for metric in CAMPAIGN_METRICS
        for field in ("cross_job_median", "cross_job_min", "cross_job_max")
    }
    require_exact_schema(values, base_keys | replicate_keys | metric_keys, campaign_path)

    records = []
    referenced_paths = set()
    for index in range(1, expected_replicates + 1):
        relative = Path(values[f"replicate_{index}.evidence_file"])
        evidence_path = (input_root / relative).resolve()
        try:
            evidence_path.relative_to(input_root)
        except ValueError as error:
            raise ValueError(
                f"replicate evidence path escapes input root: {evidence_path}"
            ) from error
        if not evidence_path.is_file():
            raise ValueError(f"missing replicate evidence: {evidence_path}")
        if evidence_path in referenced_paths:
            raise ValueError(f"duplicate replicate evidence path: {evidence_path}")
        referenced_paths.add(evidence_path)
        replicate = validate(evidence_path, "github-hosted")
        if int(replicate["replicate_id"]) != index:
            raise ValueError(f"replicate index/id mismatch: {evidence_path}")
        if values[f"replicate_{index}.id"] != replicate["replicate_id"]:
            raise ValueError(f"campaign replicate id mismatch: {evidence_path}")
        digest = hashlib.sha256(evidence_path.read_bytes()).hexdigest()
        if values[f"replicate_{index}.evidence_sha256"] != digest:
            raise ValueError(f"campaign replicate digest mismatch: {evidence_path}")
        for campaign_field, replicate_field in (
            ("collected_at_utc", "collected_at_utc"),
            ("job", "job"),
            ("invocation_url", "invocation_url"),
            ("runner_system", "runner_system"),
            ("runner_machine", "runner_machine"),
            ("cpu_model", "cpu_model"),
            ("logical_cpus", "logical_cpus"),
            ("memory_bytes", "memory_bytes"),
            ("virtualization", "virtualization"),
        ):
            if values[f"replicate_{index}.{campaign_field}"] != replicate[replicate_field]:
                raise ValueError(
                    f"campaign replicate metadata mismatch {campaign_field}: {evidence_path}"
                )
        records.append(replicate)

    discovered_paths = {
        path.resolve()
        for path in input_root.glob("*/evidence.txt")
        if path.resolve() != campaign_path
    }
    if discovered_paths != referenced_paths:
        raise ValueError("campaign replicate file set differs from input root")

    shared_keys = (
        "repository",
        "source_commit_sha",
        "run_id",
        "run_attempt",
        "workflow_ref",
    )
    for key in shared_keys:
        observed = {replicate[key] for replicate in records}
        if len(observed) != 1:
            raise ValueError(f"replicate provenance differs: {key}")
        if values[key] != records[0][key]:
            raise ValueError(f"campaign provenance mismatch: {key}")

    for metric in CAMPAIGN_METRICS:
        samples = [float(replicate[metric]) for replicate in records]
        for field, expected in (
            ("cross_job_median", statistics.median(samples)),
            ("cross_job_min", min(samples)),
            ("cross_job_max", max(samples)),
        ):
            key = f"{metric}.{field}"
            require_close(key, values[key], expected, campaign_path)
    return values


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--campaign", type=Path, required=True)
    parser.add_argument("--input-root", type=Path, required=True)
    parser.add_argument("--expected-replicates", type=int, default=3)
    arguments = parser.parse_args()
    values = validate_campaign(
        arguments.campaign,
        arguments.input_root,
        arguments.expected_replicates,
    )
    print(
        "SMAVE_NATIVE_EXTERNAL_PERFORMANCE_CAMPAIGN_CHECK 1 "
        f"replicates={values['replicates']} "
        f"attestation_embedded={values['artifact_attestation_embedded']}"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (KeyError, OSError, ValueError) as error:
        print(f"native external performance campaign verification failed: {error}")
        raise SystemExit(1)
