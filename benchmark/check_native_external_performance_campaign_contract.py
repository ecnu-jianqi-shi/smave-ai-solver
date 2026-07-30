#!/usr/bin/env python3

import argparse
import hashlib
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parent))
from verify_native_external_performance import validate
from verify_native_external_performance_campaign import validate_campaign


def replace_fields(path: Path, replacements: dict[str, str]) -> None:
    lines = []
    for line in path.read_text().splitlines():
        if "=" in line:
            key = line.split("=", 1)[0]
            if key in replacements:
                line = f"{key}={replacements[key]}"
        lines.append(line)
    path.write_text("\n".join(lines) + "\n")


def synthetic_replicate(source: Path, target: Path, replicate: int) -> None:
    shutil.copytree(source, target)
    replace_fields(
        target / "evidence.txt",
        {
            "provider": "github-hosted",
            "replicate_id": str(replicate),
            "external_provider": "1",
            "performance_evidence": "1",
            "native_external_performance": "1",
            "provider_hosted_vm": "1",
            "artifact_attestation_requested": "1",
            "source_commit_bound": "1",
            "repository": "synthetic-contract-only/smave",
            "repository_visibility": "private",
            "source_commit_sha": "0123456789abcdef0123456789abcdef01234567",
            "run_id": "424242",
            "run_attempt": "1",
            "job": "measure",
            "workflow_ref": (
                "synthetic-contract-only/smave/.github/workflows/"
                "native-external-performance.yml@refs/heads/main"
            ),
            "invocation_url": (
                "https://github.invalid/synthetic-contract-only/smave/"
                "actions/runs/424242/attempts/1"
            ),
            "runner_system": "Linux",
            "runner_release": "synthetic-contract-only",
            "runner_machine": "x86_64",
            "cpu_model": f"SYNTHETIC-CONTRACT-X86-64-{replicate}",
            "virtualization": "synthetic-contract-only",
        },
    )
    validate(target / "evidence.txt", "github-hosted")


def expect_rejection(
    campaign: Path, input_root: Path, expected_fragment: str
) -> None:
    try:
        validate_campaign(campaign, input_root, 3)
    except ValueError as error:
        if expected_fragment not in str(error):
            raise ValueError(
                f"unexpected rejection for {campaign.name}: {error}"
            ) from error
        return
    raise ValueError(f"campaign tamper unexpectedly passed: {campaign.name}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--local-root",
        type=Path,
        default=Path("build/native-external-dry-run-output"),
    )
    arguments = parser.parse_args()
    local_root = arguments.local_root.resolve()
    validate(local_root / "evidence.txt", "local")
    script_directory = Path(__file__).resolve().parent

    with tempfile.TemporaryDirectory(prefix="smave-native-campaign-contract-") as temporary:
        root = Path(temporary)
        input_root = root / "input"
        input_root.mkdir()
        for replicate in range(1, 4):
            synthetic_replicate(
                local_root,
                input_root / f"native-external-replicate-{replicate}",
                replicate,
            )
        campaign = root / "summary/evidence.txt"
        subprocess.run(
            [
                sys.executable,
                str(script_directory / "aggregate_native_external_performance.py"),
                "--input-root",
                str(input_root),
                "--output",
                str(campaign),
                "--expected-replicates",
                "3",
            ],
            check=True,
            text=True,
            capture_output=True,
        )
        validate_campaign(campaign, input_root, 3)

        metric_tamper = root / "metric-tamper.txt"
        shutil.copy2(campaign, metric_tamper)
        metric_key = (
            "gate_parallel.linear.worker_10.paired_speedup.cross_job_median"
        )
        metric_value = next(
            line.split("=", 1)[1]
            for line in metric_tamper.read_text().splitlines()
            if line.startswith(f"{metric_key}=")
        )
        replace_fields(metric_tamper, {metric_key: str(float(metric_value) + 0.25)})
        expect_rejection(metric_tamper, input_root, "campaign metric mismatch")

        digest_tamper = root / "digest-tamper.txt"
        shutil.copy2(campaign, digest_tamper)
        replace_fields(digest_tamper, {"replicate_2.evidence_sha256": "0" * 64})
        expect_rejection(digest_tamper, input_root, "campaign replicate digest mismatch")

        schema_tamper = root / "schema-tamper.txt"
        lines = campaign.read_text().splitlines()
        lines.insert(-1, "unexpected_claim=1")
        schema_tamper.write_text("\n".join(lines) + "\n")
        expect_rejection(schema_tamper, input_root, "unexpected campaign field")

        provenance_tamper = root / "provenance-tamper.txt"
        shutil.copy2(campaign, provenance_tamper)
        third_evidence = input_root / "native-external-replicate-3/evidence.txt"
        replace_fields(third_evidence, {"run_id": "999999"})
        replace_fields(
            provenance_tamper,
            {
                "replicate_3.evidence_sha256": hashlib.sha256(
                    third_evidence.read_bytes()
                ).hexdigest()
            },
        )
        expect_rejection(provenance_tamper, input_root, "replicate provenance differs")

    print(
        "SMAVE_NATIVE_EXTERNAL_PERFORMANCE_CAMPAIGN_CONTRACT_CHECK 1 "
        "synthetic=1 negative_cases=4 external_evidence=0"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (KeyError, OSError, subprocess.CalledProcessError, ValueError) as error:
        print(f"native external performance campaign contract check failed: {error}")
        raise SystemExit(1)
