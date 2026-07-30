#!/usr/bin/env python3

import argparse
import gzip
import hashlib
import io
import os
import tarfile
from pathlib import Path


ARCHIVE_ROOT = "smave-core-repro"
NORMALIZED_MTIME = 0
EXCLUDED_PREFIXES = (
    "benchmark/pdebench/",
    "benchmark/suitesparse/",
    "paper/main.aux",
    "paper/main.bbl",
    "paper/main.blg",
    "paper/main.fdb_latexmk",
    "paper/main.fls",
    "paper/main.log",
    "paper/main.out",
)
INCLUDED_SOURCE_PATHS = (
    ".github",
    ".gitignore",
    "CMakeLists.txt",
    "CMakePresets.json",
    "DESIGN.md",
    "GOAL.md",
    "README.md",
    "artifact",
    "benchmark",
    "ccfa-review-reports",
    "cmake",
    "docs",
    "examples",
    "include",
    "initial_swarm.xmf",
    "initial_swarm_swarm_fields.pbin",
    "kb",
    "paper",
    "src",
    "tests",
)
INCLUDED_BUILD_PATHS = (
    "build/release/cascade-ordering/evidence.txt",
    "build/release/router-shift/evidence.txt",
    "build/release/router-shift-matrix/evidence.txt",
    "build/release/calibrated-correction-router/evidence.txt",
    "build/release/joint-route-budget/evidence.txt",
    "build/release/joint-route-scaling-round51",
    "build/release/frozen-interaction-prevalence-round52",
    "build/release/frozen-transition-attrition-round53",
    "build/release/joint-route-budget-shift/evidence.txt",
    "build/release/request-conditioned-joint-route/evidence.txt",
    "build/release/request-conditioned-joint-route/action-observations.tsv",
    "build/release/request-conditioned-joint-route/request-conditioned-model.txt",
    "build/release/suitesparse-request-conditioned-route-final-heldout-v4-first-run",
    "build/release/suitesparse-request-conditioned-route-final-heldout-v4-first-run.console.txt",
    "build/release/suitesparse-request-conditioned-route-final-heldout-v5-first-run",
    "build/release/suitesparse-request-conditioned-route-final-heldout-v5-first-run.console.txt",
    "build/release/suitesparse-request-conditioned-route-final-heldout-v6-first-run",
    "build/release/suitesparse-request-conditioned-route-final-heldout-v6-first-run.console.txt",
    "build/release/suitesparse-control-aware-replay-v5",
    "build/release/suitesparse-control-aware-replay-v6",
    "build/release/complete-cost-decomposition/evidence.txt",
    "build/release/phase4/source-repeat.competition",
    "build/release/phase4/heldout-traces/heldout.competition",
    "build/release/phase5/benchmark-traces/operator-ablation.txt",
    "build/release/nonlinear-operator/benchmark-traces/operator-ablation.txt",
    "build/release/gate-parallel-scaling/evidence.txt",
    "build/release/pdebench-repeated-timing/evidence.txt",
    "build/release/pdebench-repeated-timing/20260724T045454Z",
    "build/release/pdebench-order-sensitivity/evidence.txt",
    "build/release/pdebench-order-sensitivity/20260724T035555Z",
    "build/release/parallel-scaling/evidence.txt",
    "build/release/batch-scaling/evidence.txt",
    "build/release/operator-shared-baseline/evidence.txt",
    "build/release/hints-schedule-baseline/evidence.txt",
    "build/release/hints-native-baseline/evidence.txt",
    "build/release/hints-native-baseline/official/evidence.txt",
    "build/release/hints-native-baseline/official/raw-samples.txt",
    "build/release/hints-native-baseline/smave/evidence.txt",
    "build/release/hints-native-baseline/smave/raw-samples.txt",
    "build/release/hints-native-baseline/workload/contract.txt",
    "build/release/phase5/benchmark-traces/operator-hints-schedule-baseline.txt",
    "build/release/phase5/benchmark-traces/operator-shared-hybrid-baseline.txt",
    "build/release/nonlinear-operator/benchmark-traces/operator-shared-hybrid-baseline.txt",
    "build/native-external-dry-run-output/evidence.txt",
    "build/native-external-dry-run-output/components/gate-parallel/evidence.txt",
    "build/native-external-dry-run-output/components/gate-parallel/raw-samples.txt",
    "build/native-external-dry-run-output/components/risk-adaptive/evidence.txt",
    "build/native-external-dry-run-output/components/risk-adaptive/raw-samples.txt",
    "build/native-external-dry-run-output/components/operator-replication/evidence.txt",
    "build/native-external-dry-run-output/components/operator-replication/performance.txt",
    "build/native-external-dry-run-output/components/operator-replication/operator-statistics.txt",
    "build/portability/linux-arm64/evidence.txt",
    "build/portability/linux-amd64/evidence.txt",
    "build/release/data-lock/evidence.txt",
)
INCLUDED_EXCLUDED_PATHS = (
    "benchmark/suitesparse/small/west0479/west0479.mtx",
    "benchmark/suitesparse/small/nasa2910/nasa2910.mtx",
    "benchmark/suitesparse/final-heldout-v1/laser/laser.mtx",
    "benchmark/suitesparse/final-heldout-v2/M10PI_n1/M10PI_n1.mtx",
    "benchmark/suitesparse/final-heldout-v2/TS/TS.mtx",
    "benchmark/pdebench/files.tsv",
    "benchmark/pdebench/download_full.sh",
    "benchmark/pdebench/download_parallel_resume.sh",
)

def digest_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def add_path(files: set[Path], root: Path, path: Path, force: bool = False) -> None:
    if path.is_file():
        relative = path.relative_to(root).as_posix()
        if force or not any(
            relative == prefix.rstrip("/") or relative.startswith(prefix)
            for prefix in EXCLUDED_PREFIXES
        ):
            files.add(path)
    elif path.is_dir():
        for directory, names, filenames in os.walk(path):
            directory_path = Path(directory)
            names[:] = [
                name
                for name in names
                if not any(
                    (directory_path / name).relative_to(root).as_posix()
                    == prefix.rstrip("/")
                    or (directory_path / name).relative_to(root).as_posix().startswith(prefix)
                    for prefix in EXCLUDED_PREFIXES
                )
                and name != "__pycache__"
            ]
            for filename in filenames:
                candidate = directory_path / filename
                relative = candidate.relative_to(root).as_posix()
                if filename.endswith((".pyc", ".pyo")):
                    continue
                if any(relative == prefix.rstrip("/") or relative.startswith(prefix)
                       for prefix in EXCLUDED_PREFIXES):
                    continue
                files.add(candidate)
    else:
        raise FileNotFoundError(path)


def collect_files(root: Path) -> list[Path]:
    files: set[Path] = set()
    for relative in INCLUDED_SOURCE_PATHS:
        add_path(files, root, root / relative)
    for relative in INCLUDED_BUILD_PATHS:
        add_path(files, root, root / relative)
    for relative in INCLUDED_EXCLUDED_PATHS:
        add_path(files, root, root / relative, force=True)
    return sorted(files, key=lambda path: path.relative_to(root).as_posix())


def file_manifest(root: Path, files: list[Path]) -> bytes:
    lines = ["SMAVE_CORE_REPRO_FILE_MANIFEST 1"]
    for path in files:
        relative = path.relative_to(root).as_posix()
        lines.append(f"{digest_bytes(path.read_bytes())}  {relative}")
    lines.append("END")
    return ("\n".join(lines) + "\n").encode()


def tar_info(name: str, size: int, executable: bool = False) -> tarfile.TarInfo:
    info = tarfile.TarInfo(name)
    info.size = size
    info.mtime = NORMALIZED_MTIME
    info.uid = 0
    info.gid = 0
    info.uname = ""
    info.gname = ""
    info.mode = 0o755 if executable else 0o644
    return info


def archive_bytes(root: Path, files: list[Path], manifest: bytes) -> bytes:
    tar_buffer = io.BytesIO()
    with tarfile.open(fileobj=tar_buffer, mode="w", format=tarfile.GNU_FORMAT) as archive:
        entries = [
            (
                path.relative_to(root).as_posix(),
                path.read_bytes(),
                bool(path.stat().st_mode & 0o111),
            )
            for path in files
        ]
        entries.append(("CORE_REPRO_FILE_MANIFEST.txt", manifest, False))
        for relative, data, executable in sorted(entries):
            archive.addfile(
                tar_info(f"{ARCHIVE_ROOT}/{relative}", len(data), executable),
                io.BytesIO(data),
            )
    gzip_buffer = io.BytesIO()
    with gzip.GzipFile(fileobj=gzip_buffer, mode="wb", filename="", mtime=0) as compressed:
        compressed.write(tar_buffer.getvalue())
    return gzip_buffer.getvalue()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", default="build/core-repro-bundle")
    arguments = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    output = root / arguments.output
    output.mkdir(parents=True, exist_ok=True)
    files = collect_files(root)
    manifest = file_manifest(root, files)
    payload = archive_bytes(root, files, manifest)
    repeated_payload = archive_bytes(root, files, manifest)
    if payload != repeated_payload:
        raise RuntimeError("two normalized archive generations were not byte-identical")
    archive_path = output / "smave-core-repro.tar.gz"
    archive_path.write_bytes(payload)
    archive_sha256 = digest_bytes(payload)
    (output / "smave-core-repro.tar.gz.sha256").write_text(
        f"{archive_sha256}  {archive_path.name}\n"
    )
    (output / "contract.txt").write_text(
        "\n".join(
            [
                "SMAVE_CORE_REPRO_BUNDLE 1",
                "archive_format=tar.gz",
                "normalized_mtime=0",
                "normalized_uid=0",
                "normalized_gid=0",
                "sorted_paths=1",
                "deterministic_generations=2",
                "byte_identical=1",
                f"archive_sha256={archive_sha256}",
                f"source_tree_sha256={digest_bytes(manifest)}",
                f"included_files={len(files) + 1}",
                "included_sparse_system_matrices=5",
                "excluded_pdebench_datasets=1",
                "excluded_suitesparse_benchmark_matrices=61",
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
    print(f"SMAVE_CORE_REPRO_BUNDLE_CREATED 1 sha256={archive_sha256}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
