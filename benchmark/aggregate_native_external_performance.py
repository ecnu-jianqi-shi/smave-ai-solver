#!/usr/bin/env python3

import argparse
import hashlib
import statistics
import sys
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parent))
from native_external_performance_contract import CAMPAIGN_METRICS
from verify_native_external_performance import validate


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--expected-replicates", type=int, default=3)
    arguments = parser.parse_args()
    output_path = arguments.output.resolve()
    evidence_paths = sorted(
        path
        for path in arguments.input_root.glob("*/evidence.txt")
        if path.resolve() != output_path
    )
    if len(evidence_paths) != arguments.expected_replicates:
        raise ValueError(
            f"expected {arguments.expected_replicates} replicates, found {len(evidence_paths)}"
        )
    replicate_records = sorted(
        (
            (path, validate(path, "github-hosted"))
            for path in evidence_paths
        ),
        key=lambda record: int(record[1]["replicate_id"]),
    )
    evidence_paths = [record[0] for record in replicate_records]
    replicates = [record[1] for record in replicate_records]
    shared_keys = (
        "repository",
        "source_commit_sha",
        "run_id",
        "run_attempt",
        "workflow_ref",
    )
    for key in shared_keys:
        if len({values[key] for values in replicates}) != 1:
            raise ValueError(f"replicate provenance differs: {key}")
    replicate_ids = [int(values["replicate_id"]) for values in replicates]
    expected_ids = list(range(1, arguments.expected_replicates + 1))
    if replicate_ids != expected_ids:
        raise ValueError(f"expected replicate ids {expected_ids}, found {replicate_ids}")
    fields = [
        "SMAVE_NATIVE_EXTERNAL_PERFORMANCE_CAMPAIGN 1",
        "contract=three-job-github-hosted-solver-performance-campaign",
        f"replicates={len(replicates)}",
        "provider=github-hosted",
        "external_provider=1",
        "performance_evidence=1",
        "native_external_performance=1",
        "native_isa_execution=1",
        "provider_hosted_vm=1",
        "bare_metal=0",
        "distinct_workflow_jobs=1",
        "independent_reproduction=0",
        "author_operated_workflow=1",
        "public_immutable_archive=0",
        "artifact_attestation_requested=1",
        "artifact_attestation_embedded=0",
        "source_commit_bound=1",
        "direction_agnostic_acceptance=1",
        "performance_threshold_gate=0",
        "negative_results_retained=1",
        f"repository={replicates[0]['repository']}",
        f"source_commit_sha={replicates[0]['source_commit_sha']}",
        f"run_id={replicates[0]['run_id']}",
        f"run_attempt={replicates[0]['run_attempt']}",
        f"workflow_ref={replicates[0]['workflow_ref']}",
    ]
    for index, (path, values) in enumerate(zip(evidence_paths, replicates), start=1):
        fields.extend(
            [
                f"replicate_{index}.id={values['replicate_id']}",
                f"replicate_{index}.evidence_file={path.resolve().relative_to(arguments.input_root.resolve()).as_posix()}",
                f"replicate_{index}.evidence_sha256={hashlib.sha256(path.read_bytes()).hexdigest()}",
                f"replicate_{index}.collected_at_utc={values['collected_at_utc']}",
                f"replicate_{index}.job={values['job']}",
                f"replicate_{index}.invocation_url={values['invocation_url']}",
                f"replicate_{index}.runner_system={values['runner_system']}",
                f"replicate_{index}.runner_machine={values['runner_machine']}",
                f"replicate_{index}.cpu_model={values['cpu_model']}",
                f"replicate_{index}.logical_cpus={values['logical_cpus']}",
                f"replicate_{index}.memory_bytes={values['memory_bytes']}",
                f"replicate_{index}.virtualization={values['virtualization']}",
            ]
        )
    for metric in CAMPAIGN_METRICS:
        samples = [float(values[metric]) for values in replicates]
        fields.extend(
            [
                f"{metric}.cross_job_median={statistics.median(samples):.17g}",
                f"{metric}.cross_job_min={min(samples):.17g}",
                f"{metric}.cross_job_max={max(samples):.17g}",
            ]
        )
    fields.extend(
        [
            "claim_scope=three-github-hosted-x86-64-vm-jobs-self-contained-workloads-only",
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
        "SMAVE_NATIVE_EXTERNAL_PERFORMANCE_CAMPAIGN 1 "
        f"replicates={len(replicates)}"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (KeyError, OSError, ValueError) as error:
        print(f"native external performance aggregation failed: {error}")
        raise SystemExit(1)
