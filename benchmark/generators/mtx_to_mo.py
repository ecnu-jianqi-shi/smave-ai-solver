#!/usr/bin/env python3
"""
Convert a Matrix Market (.mtx) sparse matrix into a SMAVE Modelica-subset .mo file.

The resulting model has one Real unknown per matrix row and one linear equation per row:
    sum_j A[i,j] * x_j = b[i]

If no RHS file is provided, b[i] = 1.0.

Usage:
    python3 mtx_to_mo.py input.mtx output.mo [--rhs rhs.txt] [--model-name NAME]

This is the recommended way to ingest SuiteSparse / Matrix Market matrices into
SMAVE's `smave compile` path without writing a custom frontend.

Limitations:
  - Only coordinate (COO) general and symmetric Matrix Market formats are parsed.
  - The generated .mo file is O(nnz) in size; for very large matrices consider
    direct CSR assembly through tests/large_sparse_evidence.cpp patterns.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path


def parse_mtx(path: Path):
    rows = []
    cols = []
    vals = []
    n = m = 0
    symmetric = False
    with path.open() as handle:
        header = handle.readline().split()
        if header[0] != "%%MatrixMarket":
            raise ValueError(f"not a Matrix Market file: {path}")
        if "symmetric" in header:
            symmetric = True
        # skip comments
        line = handle.readline()
        while line.startswith("%"):
            line = handle.readline()
        n, m, nnz = (int(x) for x in line.split())
        for raw in handle:
            parts = raw.split()
            if len(parts) == 2:
                i, j = int(parts[0]), int(parts[1])
                v = 1.0
            else:
                i, j, v = int(parts[0]), int(parts[1]), float(parts[2])
            rows.append(i - 1)
            cols.append(j - 1)
            vals.append(v)
            if symmetric and i != j:
                rows.append(j - 1)
                cols.append(i - 1)
                vals.append(v)
    return n, m, rows, cols, vals


def parse_rhs(path: Path, n: int):
    b = [1.0] * n
    with path.open() as handle:
        for i, line in enumerate(handle):
            line = line.strip()
            if not line or line.startswith("%"):
                continue
            if i >= n:
                break
            try:
                b[i] = float(line.split()[0])
            except (ValueError, IndexError):
                pass
    return b


def write_mo(out: Path, model_name: str, n: int, rows, cols, vals, b):
    # group by row
    by_row = {}
    for r, c, v in zip(rows, cols, vals):
        by_row.setdefault(r, []).append((c, v))
    lines = [f"model {model_name}"]
    for i in range(n):
        lines.append(f"  Real x{i + 1}(start=0.0, nominal=1.0);")
    lines.append("equation")
    for i in range(n):
        terms = []
        for c, v in sorted(by_row.get(i, []), key=lambda t: t[0]):
            sign = "+" if v >= 0 else ""
            terms.append(f"{sign}{v}*x{c + 1}")
        body = " ".join(terms).lstrip("+").strip()
        if not body:
            body = "0.0"
        lines.append(f"  {body} = {b[i]};")
    lines.append(f"end {model_name};")
    out.write_text("\n".join(lines) + "\n")


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("input", type=Path, help="input .mtx file")
    p.add_argument("output", type=Path, help="output .mo file")
    p.add_argument("--rhs", type=Path, help="optional RHS file (one number per line)")
    p.add_argument("--model-name", default=None, help="Modelica model name")
    args = p.parse_args()
    name = args.model_name or args.output.stem
    n, m, rows, cols, vals = parse_mtx(args.input)
    if n != m:
        print(f"warning: matrix is non-square ({n}×{m}); SMAVE requires square systems", file=sys.stderr)
    b = parse_rhs(args.rhs, n) if args.rhs else [1.0] * n
    write_mo(args.output, name, n, rows, cols, vals, b)
    print(f"wrote {args.output} with {n} unknowns and {len(rows)} nonzeros")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
