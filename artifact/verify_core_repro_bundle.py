#!/usr/bin/env python3

import argparse
import hashlib
import platform
import shutil
import subprocess
import tarfile
import tempfile
from pathlib import Path


def run(command: list[str], cwd: Path) -> None:
    print("+", " ".join(command), flush=True)
    subprocess.run(command, cwd=cwd, check=True)


def parse_file_manifest(path: Path) -> dict[str, str]:
    lines = path.read_text().splitlines()
    if not lines or lines[0] != "SMAVE_CORE_REPRO_FILE_MANIFEST 1" or lines[-1] != "END":
        raise ValueError("invalid core reproduction file manifest")
    values = {}
    for line in lines[1:-1]:
        digest, relative = line.split("  ", 1)
        values[relative] = digest
    return values


def verify_files(root: Path) -> None:
    manifest_path = root / "CORE_REPRO_FILE_MANIFEST.txt"
    expected = parse_file_manifest(manifest_path)
    actual = {
        path.relative_to(root).as_posix()
        for path in root.rglob("*")
        if path.is_file() and path != manifest_path
    }
    if actual != set(expected):
        raise ValueError("extracted file set differs from the internal manifest")
    for relative, digest in expected.items():
        actual_digest = hashlib.sha256((root / relative).read_bytes()).hexdigest()
        if actual_digest != digest:
            raise ValueError(f"checksum mismatch: {relative}")


def verify_archive_metadata(archive: tarfile.TarFile) -> None:
    names = [member.name for member in archive.getmembers()]
    if names != sorted(names):
        raise ValueError("archive members are not sorted")
    for member in archive.getmembers():
        path = Path(member.name)
        if path.is_absolute() or ".." in path.parts:
            raise ValueError(f"unsafe archive member: {member.name}")
        if member.uid != 0 or member.gid != 0 or member.mtime != 0:
            raise ValueError(f"non-normalized archive metadata: {member.name}")
        if not member.isfile():
            raise ValueError(f"unexpected non-file archive member: {member.name}")


def command_version(command: list[str]) -> str:
    result = subprocess.run(command, check=True, text=True, capture_output=True)
    return (result.stdout or result.stderr).splitlines()[0]


def require_test_count(build: Path, expected: int) -> None:
    result = subprocess.run(
        ["ctest", "--test-dir", str(build), "-N"],
        check=True,
        text=True,
        capture_output=True,
    )
    if f"Total Tests: {expected}" not in result.stdout:
        raise ValueError(f"expected {expected} CTests in the extracted tree")


def require_same_file(left: Path, right: Path) -> None:
    if left.read_bytes() != right.read_bytes():
        raise ValueError(f"deterministic replay mismatch: {left} != {right}")


def restore_frozen_analysis_inputs(root: Path, build: Path) -> None:
    for relative in (
        "phase4/source-repeat.competition",
        "phase4/heldout-traces/heldout.competition",
        "phase5/benchmark-traces/operator-ablation.txt",
        "nonlinear-operator/benchmark-traces/operator-ablation.txt",
    ):
        source = root / "build/release" / relative
        target = build / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)


def snapshot_frozen_paper_derivatives(root: Path) -> dict[Path, bytes]:
    relatives = (
        "paper/data/operator_speedups.dat",
        "paper/generated/operator_shared_baseline_values.tex",
        "paper/generated/hints_schedule_values.tex",
        "paper/generated/hints_native_values.tex",
    )
    return {root / relative: (root / relative).read_bytes() for relative in relatives}


def restore_frozen_paper_derivatives(snapshot: dict[Path, bytes]) -> None:
    for path, content in snapshot.items():
        path.write_bytes(content)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("archive", type=Path)
    parser.add_argument("--evidence", type=Path)
    parser.add_argument("--keep", action="store_true")
    parser.add_argument("--skip-pdf", action="store_true")
    arguments = parser.parse_args()
    archive = arguments.archive.resolve()
    sidecar = archive.with_name(archive.name + ".sha256")
    expected_archive_digest = sidecar.read_text().split()[0]
    actual_archive_digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    if actual_archive_digest != expected_archive_digest:
        raise ValueError("archive checksum does not match sidecar")
    temporary = Path(tempfile.mkdtemp(prefix="smave-core-repro-"))
    try:
        with tarfile.open(archive, "r:gz") as source:
            verify_archive_metadata(source)
            source.extractall(temporary)
        root = temporary / "smave-core-repro"
        verify_files(root)
        frozen_paper_derivatives = snapshot_frozen_paper_derivatives(root)
        build = root / "build/core-repro-release"
        run([
            "cmake", "-S", ".", "-B", str(build),
            "-DCMAKE_BUILD_TYPE=Release", "-DSMAVE_CORE_REPRO_BUNDLE=ON",
        ], root)
        run(["cmake", "--build", str(build), "--parallel", "4"], root)
        require_test_count(build, 29)
        run(["ctest", "--test-dir", str(build), "--output-on-failure"], root)
        restore_frozen_analysis_inputs(root, build)
        run(["cmake", "--build", str(build), "--target", "reproduce-cascade-ordering"], root)
        run(["cmake", "--build", str(build), "--target", "reproduce-router-shift"], root)
        run([
            "cmake", "--build", str(build), "--target",
            "reproduce-router-shift-matrix",
        ], root)
        run([
            "cmake", "--build", str(build), "--target",
            "reproduce-calibrated-correction-router",
        ], root)
        run([
            "cmake", "--build", str(build), "--target",
            "reproduce-joint-route-budget",
        ], root)
        run([
            "cmake", "--build", str(build), "--target",
            "reproduce-joint-route-scaling-round51",
        ], root)
        run([
            "cmake", "--build", str(build), "--target",
            "reproduce-frozen-interaction-prevalence-round52",
        ], root)
        run([
            "cmake", "--build", str(build), "--target",
            "reproduce-frozen-transition-attrition-round53",
        ], root)
        run([
            "cmake", "--build", str(build), "--target",
            "reproduce-joint-route-budget-shift",
        ], root)
        run([
            "cmake", "--build", str(build), "--target",
            "reproduce-request-conditioned-joint-route",
        ], root)
        run([
            "cmake",
            "-DEVIDENCE="
            + str(
                root
                / "build/release/suitesparse-request-conditioned-route-final-heldout-v6-first-run/evidence.txt"
            ),
            "-P", "tests/verify_suitesparse_request_conditioned_route.cmake",
        ], root)
        replay_directory = build / "suitesparse-control-aware-replay-v5"
        run([
            "python3", "benchmark/replay_control_aware_anchor.py",
            "--evidence-dir",
            "build/release/suitesparse-request-conditioned-route-final-heldout-v5-first-run",
            "--heldout-lock", "benchmark/data-lock/suitesparse-final-heldout-v5.tsv",
            "--routing-source", "src/routing.cpp",
            "--output", str(replay_directory / "evidence.txt"),
            "--decisions", str(replay_directory / "decisions.tsv"),
        ], root)
        require_same_file(
            replay_directory / "evidence.txt",
            root / "build/release/suitesparse-control-aware-replay-v5/evidence.txt",
        )
        require_same_file(
            replay_directory / "decisions.tsv",
            root / "build/release/suitesparse-control-aware-replay-v5/decisions.tsv",
        )
        run([
            "cmake", "--build", str(build), "--target",
            "reproduce-complete-cost-decomposition",
        ], root)
        run([
            "cmake", "--build", str(build), "--target",
            "reproduce-hints-schedule-baseline",
        ], root)
        run([
            "cmake",
            f"-DEVIDENCE={root / 'build/release/hints-native-baseline/evidence.txt'}",
            "-P", "tests/verify_hints_native_baseline.cmake",
        ], root)
        run([
            "python3", "benchmark/verify_native_external_performance.py",
            "--evidence", "build/native-external-dry-run-output/evidence.txt",
            "--expect-local",
        ], root)
        run([
            "python3", "benchmark/check_native_external_performance_campaign_contract.py",
            "--local-root", "build/native-external-dry-run-output",
        ], root)
        restore_frozen_paper_derivatives(frozen_paper_derivatives)
        run(["python3", "paper/check_evidence.py"], root)
        run(["python3", "paper/check_artifact_manifest.py"], root)
        if not arguments.skip_pdf and shutil.which("latexmk"):
            run(["paper/check.sh"], root)
        evidence_path = (arguments.evidence or archive.with_name("clean-tree-evidence.txt")).resolve()
        evidence_path.parent.mkdir(parents=True, exist_ok=True)
        evidence_path.write_text(
            "\n".join(
                [
                    "SMAVE_CORE_REPRO_CLEAN_TREE 1",
                    f"archive_sha256={actual_archive_digest}",
                    "archive_manifest_verified=1",
                    "release_configure=1",
                    "release_build=1",
                    "ctest_passed=29",
                    "ctest_total=29",
                    "cascade_ordering_exhaustive_check=1",
                    "router_shift_matrix_check=1",
                    "calibrated_correction_router_check=1",
                    "joint_route_budget_check=1",
                    "joint_route_scaling_check=1",
                    "frozen_interaction_prevalence_check=1",
                    "frozen_transition_attrition_check=1",
                    "joint_route_budget_shift_check=1",
                    "request_conditioned_joint_route_check=1",
                    "suitesparse_v6_frozen_evidence_check=1",
                    "suitesparse_v5_zero_execution_replay_check=1",
                    "suitesparse_solver_reexecution=0",
                    "hints_schedule_baseline_check=1",
                    "hints_native_frozen_evidence_check=1",
                    "native_external_performance_local_check=1",
                    "native_external_performance_campaign_contract_check=1",
                    "paper_evidence_check=1",
                    "paper_artifact_manifest_check=1",
                    f"paper_pdf_rebuilt={int(not arguments.skip_pdf and shutil.which('latexmk') is not None)}",
                    f"host_system={platform.system()}",
                    f"host_machine={platform.machine()}",
                    f"python={platform.python_version()}",
                    f"cmake={command_version(['cmake', '--version'])}",
                    "author_operated=1",
                    "public_archive=0",
                    "independent_reproduction=0",
                    "native_external_performance=0",
                    "external_public_code_required=1",
                    "core_bundle_rerun=0",
                    "END",
                    "",
                ]
            )
        )
        print(f"SMAVE_CORE_REPRO_VERIFIED 1 sha256={actual_archive_digest}")
        print(f"evidence={evidence_path}")
        if arguments.keep:
            print(f"extracted_tree={temporary}")
            temporary = None
        return 0
    finally:
        if temporary is not None:
            shutil.rmtree(temporary)


if __name__ == "__main__":
    raise SystemExit(main())
