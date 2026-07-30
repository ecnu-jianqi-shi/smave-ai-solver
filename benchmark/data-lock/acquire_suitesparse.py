#!/usr/bin/env python3

import argparse
import hashlib
import shutil
import subprocess
import tarfile
import tempfile
from pathlib import Path, PurePosixPath

from verify_data_lock import read_tsv


def digest_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while chunk := source.read(16 * 1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--name", required=True)
    parser.add_argument("--force", action="store_true")
    arguments = parser.parse_args()
    root = arguments.root.resolve()
    rows = [
        row
        for row in read_tsv(root / "benchmark/data-lock/suitesparse.tsv", 10)
        if row[3] == arguments.name
    ]
    if not rows:
        raise ValueError(f"unknown locked SuiteSparse system: {arguments.name}")
    archive_urls = {row[7] for row in rows}
    if len(archive_urls) != 1:
        raise ValueError("locked system has inconsistent archive URLs")
    archive_url = archive_urls.pop()
    expected = {
        PurePosixPath(row[1]).name: (root / row[1], int(row[4]), row[5])
        for row in rows
    }
    if not arguments.force and all(
        destination.is_file()
        and destination.stat().st_size == expected_size
        and digest_file(destination) == expected_sha256
        for destination, expected_size, expected_sha256 in expected.values()
    ):
        print(f"SMAVE_SUITESPARSE_ACQUIRE 1 name={arguments.name} files={len(expected)} cached=1")
        return 0
    staged = Path(tempfile.mkdtemp(prefix=f"smave-{arguments.name}-"))
    try:
        archive_path = staged / "asset.tar.gz"
        subprocess.run(
            [
                "curl",
                "--fail",
                "--location",
                "--retry",
                "8",
                "--retry-all-errors",
                "--connect-timeout",
                "30",
                "--output",
                str(archive_path),
                archive_url,
            ],
            check=True,
        )
        found = set()
        staged_files = {}
        with tarfile.open(archive_path, mode="r:gz") as archive:
            for member in archive.getmembers():
                if not member.isfile():
                    continue
                name = PurePosixPath(member.name).name
                if name not in expected:
                    continue
                staged_file = staged / name
                with archive.extractfile(member) as source, staged_file.open("wb") as output:
                    shutil.copyfileobj(source, output, 16 * 1024 * 1024)
                _, expected_size, expected_sha256 = expected[name]
                if (
                    staged_file.stat().st_size != expected_size
                    or digest_file(staged_file) != expected_sha256
                ):
                    raise ValueError(f"locked SuiteSparse content mismatch: {name}")
                staged_files[name] = staged_file
                found.add(name)
        if found != set(expected):
            raise ValueError("archive does not contain the complete locked file set")
        for name, staged_file in staged_files.items():
            destination, _, _ = expected[name]
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.move(staged_file, destination)
    finally:
        shutil.rmtree(staged)
    print(f"SMAVE_SUITESPARSE_ACQUIRE 1 name={arguments.name} files={len(found)} cached=0")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
