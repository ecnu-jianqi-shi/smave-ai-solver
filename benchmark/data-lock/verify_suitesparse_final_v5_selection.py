#!/usr/bin/env python3

import argparse
import csv
import hashlib
from pathlib import Path


INDEX_SHA256 = "9bc797ab989331afbc9e0e51236d9145c2ac7f76c0913e9677ae07c4913a2aad"
V5_PAYLOAD_PREFIX = "benchmark/suitesparse/final-heldout-v5/"


def digest(path: Path) -> str:
    result = hashlib.sha256()
    with path.open("rb") as source:
        while chunk := source.read(1024 * 1024):
            result.update(chunk)
    return result.hexdigest()


def read_excluded_groups(path: Path) -> set[str]:
    result = set()
    with path.open(newline="") as source:
        for row in csv.reader(source, delimiter="\t"):
            if (
                row
                and row[0] == "matrix"
                and not row[1].startswith(V5_PAYLOAD_PREFIX)
            ):
                result.add(row[2])
    return result


def metadata_class(row: list[str]) -> str:
    positive_definite = int(row[8]) == 1
    numerically_symmetric = float(row[10]) == 1.0
    if positive_definite and numerically_symmetric:
        return "spd"
    if numerically_symmetric:
        return "symmetric-non-spd"
    return "nonsymmetric"


def eligible_rows(index_path: Path, excluded_groups: set[str]) -> list[dict[str, object]]:
    with index_path.open(newline="") as source:
        rows = list(csv.reader(source))[2:]
    result = []
    for row in rows:
        if len(row) < 12:
            continue
        group, name = row[0], row[1]
        row_count, column_count = int(row[2]), int(row[3])
        description = row[11]
        if (
            group in excluded_groups
            or row_count != column_count
            or not 5001 <= row_count <= 10000
            or int(row[5]) != 1
            or "graph" in description.lower()
            or "duplicate" in description.lower()
        ):
            continue
        result.append(
            {
                "split": "heldout",
                "numeric_class": metadata_class(row),
                "scale_band": "rows-5001-10000",
                "selection_role": "lowest-row-unseen-group",
                "group": group,
                "name": name,
                "rows": row_count,
                "columns": column_count,
                "nonzeros": int(row[4]),
                "kind": description,
            }
        )
    return sorted(result, key=lambda item: (item["rows"], item["name"]))


def select(index_path: Path, excluded_groups: set[str]) -> list[dict[str, object]]:
    candidates = eligible_rows(index_path, excluded_groups)
    selected = []
    selected_groups = set()
    for numeric_class in ("spd", "symmetric-non-spd", "nonsymmetric"):
        found = next(
            row
            for row in candidates
            if row["numeric_class"] == numeric_class
            and row["group"] not in selected_groups
        )
        selected.append(found)
        selected_groups.add(str(found["group"]))
    return selected


def read_frozen(path: Path) -> list[dict[str, object]]:
    with path.open(newline="") as source:
        rows = [row for row in csv.reader(source, delimiter="\t") if row]
    header = [value.removeprefix("# ") for value in rows[0]]
    result = []
    for values in rows[1:]:
        if len(values) != len(header):
            raise ValueError("v5 selection row width changed")
        row = dict(zip(header, values))
        row["rows"] = int(row["rows"])
        row["columns"] = int(row["columns"])
        row["nonzeros"] = int(row["nonzeros"])
        result.append(row)
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--index", type=Path, required=True)
    parser.add_argument("--lock", type=Path, required=True)
    parser.add_argument("--selection", type=Path, required=True)
    arguments = parser.parse_args()
    if digest(arguments.index) != INDEX_SHA256:
        raise ValueError("SuiteSparse source-index digest changed")
    expected = select(arguments.index, read_excluded_groups(arguments.lock))
    frozen = read_frozen(arguments.selection)
    if frozen != expected:
        raise ValueError(f"v5 deterministic selection mismatch: {frozen!r} != {expected!r}")
    print("SMAVE_SUITESPARSE_FINAL_V5_SELECTION 1")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
