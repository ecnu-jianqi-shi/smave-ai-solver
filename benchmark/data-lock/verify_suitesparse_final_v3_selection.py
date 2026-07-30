#!/usr/bin/env python3

import argparse
import csv
import hashlib
from pathlib import Path


INDEX_SHA256 = "9bc797ab989331afbc9e0e51236d9145c2ac7f76c0913e9677ae07c4913a2aad"


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
                and "/final-heldout-v3/" not in row[1]
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
            or not 512 <= row_count <= 5000
            or int(row[5]) != 1
            or "graph" in description.lower()
            or "duplicate" in description.lower()
        ):
            continue
        result.append(
            {
                "numeric_class": metadata_class(row),
                "group": group,
                "name": name,
                "rows": row_count,
                "columns": column_count,
                "nonzeros": int(row[4]),
                "kind": description,
                "scale_band": "rows-512-2047"
                if row_count <= 2047
                else "rows-2048-5000",
            }
        )
    return sorted(result, key=lambda row: (row["rows"], row["name"]))


def select(index_path: Path, excluded_groups: set[str]) -> list[dict[str, object]]:
    candidates = eligible_rows(index_path, excluded_groups)
    selected = []
    selected_groups = set()
    for numeric_class in ("spd", "symmetric-non-spd", "nonsymmetric"):
        class_rows = [
            row for row in candidates if row["numeric_class"] == numeric_class
        ]
        missing_bands = []
        for scale_band, role in (
            ("rows-512-2047", "primary-small"),
            ("rows-2048-5000", "primary-large"),
        ):
            found = next(
                (
                    row
                    for row in class_rows
                    if row["scale_band"] == scale_band
                    and row["group"] not in selected_groups
                ),
                None,
            )
            if found is None:
                missing_bands.append(scale_band)
                continue
            selected.append({**found, "selection_role": role})
            selected_groups.add(found["group"])
        for scale_band in missing_bands:
            found = next(
                row for row in class_rows if row["group"] not in selected_groups
            )
            selected.append(
                {
                    **found,
                    "selection_role": "missing-small-substitute"
                    if scale_band == "rows-512-2047"
                    else "missing-large-substitute",
                }
            )
            selected_groups.add(found["group"])
    return sorted(
        selected,
        key=lambda row: (
            ("spd", "symmetric-non-spd", "nonsymmetric").index(
                str(row["numeric_class"])
            ),
            row["rows"],
        ),
    )


def read_frozen(path: Path) -> list[dict[str, object]]:
    with path.open(newline="") as source:
        rows = [row for row in csv.reader(source, delimiter="\t") if row]
    header = [value.removeprefix("# ") for value in rows[0]]
    result = []
    for values in rows[1:]:
        if len(values) != len(header):
            raise ValueError("v3 selection row width changed")
        row = dict(zip(header, values))
        row["rows"] = int(row["rows"])
        row["columns"] = int(row["columns"])
        row["nonzeros"] = int(row["nonzeros"])
        result.append(row)
    return result


def comparable(row: dict[str, object]) -> dict[str, object]:
    return {
        "split": "heldout",
        "numeric_class": row["numeric_class"],
        "scale_band": row["scale_band"],
        "selection_role": row["selection_role"],
        "group": row["group"],
        "name": row["name"],
        "rows": row["rows"],
        "columns": row["columns"],
        "nonzeros": row["nonzeros"],
        "kind": row["kind"],
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--index", type=Path, required=True)
    parser.add_argument("--lock", type=Path, required=True)
    parser.add_argument("--selection", type=Path, required=True)
    arguments = parser.parse_args()
    if digest(arguments.index) != INDEX_SHA256:
        raise ValueError("SuiteSparse source-index digest changed")
    expected = [
        comparable(row)
        for row in select(
            arguments.index, read_excluded_groups(arguments.lock)
        )
    ]
    frozen = [comparable(row) for row in read_frozen(arguments.selection)]
    if frozen != expected:
        raise ValueError(f"v3 deterministic selection mismatch: {frozen!r} != {expected!r}")
    print("SMAVE_SUITESPARSE_FINAL_V3_SELECTION 1")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
