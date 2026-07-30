#!/usr/bin/env python3

import argparse
import hashlib
import json
import subprocess
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path


HEADER = "SMAVE_BENCHMARK_DATA_LOCK"


def hash_file(path: Path) -> tuple[str, str]:
    sha256 = hashlib.sha256()
    md5 = hashlib.md5()
    with path.open("rb") as source:
        while chunk := source.read(16 * 1024 * 1024):
            sha256.update(chunk)
            md5.update(chunk)
    return sha256.hexdigest(), md5.hexdigest()


def read_tsv(path: Path, expected_columns: int) -> list[list[str]]:
    rows = []
    for line_number, line in enumerate(path.read_text().splitlines(), 1):
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) != expected_columns:
            raise ValueError(f"{path}:{line_number}: expected {expected_columns} columns")
        rows.append(fields)
    return rows


def verify_pdebench(root: Path, lock: Path) -> tuple[int, int, int, list[list[str]]]:
    rows = read_tsv(lock, 12)
    total_bytes = 0
    aliases = 0
    for (
        local_path,
        datafile_id,
        official_filename,
        directory_label,
        size_text,
        md5,
        sha256,
        dataset_doi,
        dataset_version,
        license_id,
        license_url,
        source_url,
    ) in rows:
        if not datafile_id.isdigit() or int(datafile_id) <= 0:
            raise ValueError(f"invalid PDEBench datafile id: {datafile_id}")
        if dataset_doi != "10.18419/DARUS-2986" or dataset_version != "8.0":
            raise ValueError(f"unexpected PDEBench dataset identity: {local_path}")
        if license_id != "CC-BY-4.0" or license_url != "https://creativecommons.org/licenses/by/4.0/":
            raise ValueError(f"unexpected PDEBench license: {local_path}")
        expected_url = f"https://darus.uni-stuttgart.de/api/access/datafile/{datafile_id}"
        if source_url != expected_url:
            raise ValueError(f"unexpected PDEBench source URL: {local_path}")
        path = root / local_path
        expected_size = int(size_text)
        if not path.is_file() or path.stat().st_size != expected_size:
            raise ValueError(f"PDEBench size mismatch: {local_path}")
        actual_sha256, actual_md5 = hash_file(path)
        if actual_sha256 != sha256:
            raise ValueError(f"PDEBench SHA-256 mismatch: {local_path}")
        if actual_md5 != md5:
            raise ValueError(f"PDEBench official MD5 mismatch: {local_path}")
        if len(md5) != 32 or len(sha256) != 64 or not directory_label or not official_filename:
            raise ValueError(f"invalid PDEBench lock metadata: {local_path}")
        aliases += int(path.name != official_filename)
        total_bytes += expected_size
    if len(rows) != 7:
        raise ValueError(f"expected 7 PDEBench files, found {len(rows)}")
    return len(rows), total_bytes, aliases, rows


def verify_suitesparse(root: Path, lock: Path) -> tuple[int, int, int, int, list[list[str]]]:
    rows = read_tsv(lock, 10)
    total_bytes = 0
    matrices = 0
    right_hand_sides = 0
    systems = set()
    for (
        role,
        local_path,
        group,
        name,
        size_text,
        sha256,
        detail_url,
        archive_url,
        license_id,
        license_url,
    ) in rows:
        if role not in {"matrix", "rhs"}:
            raise ValueError(f"invalid SuiteSparse role: {role}")
        if license_id != "CC-BY-4.0" or license_url != "https://creativecommons.org/licenses/by/4.0/":
            raise ValueError(f"unexpected SuiteSparse license: {local_path}")
        if detail_url != f"https://sparse.tamu.edu/{group}/{name}":
            raise ValueError(f"unexpected SuiteSparse detail URL: {local_path}")
        if archive_url != f"https://suitesparse-collection-website.herokuapp.com/MM/{group}/{name}.tar.gz":
            raise ValueError(f"unexpected SuiteSparse archive URL: {local_path}")
        path = root / local_path
        expected_size = int(size_text)
        if not path.is_file() or path.stat().st_size != expected_size:
            raise ValueError(f"SuiteSparse size mismatch: {local_path}")
        actual_sha256, _ = hash_file(path)
        if actual_sha256 != sha256:
            raise ValueError(f"SuiteSparse SHA-256 mismatch: {local_path}")
        header = path.open("r", encoding="utf-8", errors="replace").read(4096)
        if "UF Sparse Matrix Collection" not in header or f"name: {group}/{name}" not in header:
            raise ValueError(f"SuiteSparse embedded metadata mismatch: {local_path}")
        systems.add((group, name))
        matrices += int(role == "matrix")
        right_hand_sides += int(role == "rhs")
        total_bytes += expected_size
    if matrices != 66 or right_hand_sides != 6 or len(systems) != 66:
        raise ValueError(
            f"expected 66 systems, 66 matrices, and 6 RHS files; found "
            f"{len(systems)}, {matrices}, and {right_hand_sides}"
        )
    return matrices, right_hand_sides, total_bytes, len(systems), rows


def read_url(url: str) -> bytes:
    result = subprocess.run(
        [
            "curl",
            "--fail",
            "--location",
            "--retry",
            "5",
            "--retry-all-errors",
            "--connect-timeout",
            "15",
            "--max-time",
            "90",
            "--silent",
            "--show-error",
            "--user-agent",
            "smave-data-lock/1",
            url,
        ],
        check=True,
        capture_output=True,
    )
    return result.stdout


def verify_upstream(pde_rows: list[list[str]], suite_rows: list[list[str]]) -> tuple[int, int]:
    dataset_url = (
        "https://darus.uni-stuttgart.de/api/datasets/:persistentId/"
        "?persistentId=doi:10.18419/darus-2986"
    )
    version = json.loads(read_url(dataset_url))["data"]["latestVersion"]
    if (
        version["versionNumber"] != 8
        or version["versionMinorNumber"] != 0
        or version["versionState"] != "RELEASED"
        or version["license"]["name"] != "CC BY 4.0"
    ):
        raise ValueError("PDEBench upstream version or license changed")
    official_files = {
        str(entry["dataFile"]["id"]): (
            entry["dataFile"]["filename"],
            entry.get("directoryLabel", ""),
            str(entry["dataFile"]["filesize"]),
            entry["dataFile"]["checksum"]["value"],
        )
        for entry in version["files"]
    }
    for row in pde_rows:
        _, datafile_id, filename, directory, size, md5, *_ = row
        if official_files.get(datafile_id) != (filename, directory, size, md5):
            raise ValueError(f"PDEBench upstream metadata changed: {datafile_id}")

    systems = {(row[2], row[3], row[6], row[7]) for row in suite_rows}
    about = read_url("https://sparse.tamu.edu/about").decode(errors="replace")
    if "matrices themselves are under the CC-BY 4.0 License" not in about:
        raise ValueError("SuiteSparse collection license statement changed")

    def verify_system(system: tuple[str, str, str, str]) -> None:
        group, name, detail_url, archive_url = system
        page = read_url(detail_url).decode(errors="replace")
        if (
            f"<title>{group}/{name} | SuiteSparse Matrix Collection</title>" not in page
            or archive_url not in page
        ):
            raise ValueError(f"SuiteSparse upstream metadata changed: {group}/{name}")

    with ThreadPoolExecutor(max_workers=4) as executor:
        list(executor.map(verify_system, sorted(systems)))
    return len(official_files), len(systems)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--verify-upstream", action="store_true")
    arguments = parser.parse_args()
    root = arguments.root.resolve()
    lock_root = root / "benchmark/data-lock"
    pde_lock_sha256, _ = hash_file(lock_root / "pdebench.tsv")
    suite_lock_sha256, _ = hash_file(lock_root / "suitesparse.tsv")
    pde_files, pde_bytes, pde_aliases, pde_rows = verify_pdebench(
        root, lock_root / "pdebench.tsv"
    )
    matrices, rhs_files, suite_bytes, systems, suite_rows = verify_suitesparse(
        root, lock_root / "suitesparse.tsv"
    )
    upstream_pde_files = 0
    upstream_suite_pages = 0
    if arguments.verify_upstream:
        upstream_pde_files, upstream_suite_pages = verify_upstream(pde_rows, suite_rows)
    output = arguments.output
    if not output.is_absolute():
        output = root / output
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        "\n".join(
            [
                f"{HEADER} 1",
                "contract=full-local-consumed-data-lock",
                "pdebench_dataset_doi=10.18419/DARUS-2986",
                "pdebench_dataset_version=8.0",
                "pdebench_license=CC-BY-4.0",
                f"pdebench_lock_sha256={pde_lock_sha256}",
                f"pdebench_files={pde_files}",
                f"pdebench_bytes={pde_bytes}",
                f"pdebench_local_filename_aliases={pde_aliases}",
                "suitesparse_collection_doi=10.1145/2049662.2049663",
                "suitesparse_license=CC-BY-4.0",
                f"suitesparse_lock_sha256={suite_lock_sha256}",
                f"suitesparse_systems={systems}",
                f"suitesparse_matrix_files={matrices}",
                f"suitesparse_rhs_files={rhs_files}",
                f"suitesparse_bytes={suite_bytes}",
                "size_verified=1",
                "official_md5_verified=1",
                "sha256_verified=1",
                "embedded_matrix_metadata_verified=1",
                f"upstream_network_verified={int(arguments.verify_upstream)}",
                f"upstream_pdebench_files_seen={upstream_pde_files}",
                f"upstream_suitesparse_pages_verified={upstream_suite_pages}",
                "payloads_redistributed_in_core_bundle=0",
                "public_immutable_mirror=0",
                "independent_reproduction=0",
                "END",
                "",
            ]
        )
    )
    print(f"{HEADER}_CHECK 1 files={pde_files + matrices + rhs_files}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
