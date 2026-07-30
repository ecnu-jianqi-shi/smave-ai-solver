#!/usr/bin/env python3

import argparse
import hashlib
import pickle
import subprocess
from pathlib import Path


OFFICIAL_REVISION = "0c8b712f81ed08bdf27c3a215f8edb99910f5e2f"
TEST_COUNT = 750


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_model(path: Path) -> None:
    node_count = 31
    inverse_dx_squared = 900.0
    lines = ["model HintsPoisson1D"]
    for index in range(node_count):
        lines.append(f"  parameter Real k{index} = 1.0;")
    for index in range(1, node_count - 1):
        lines.append(f"  parameter Real f{index} = 1.0;")
    for index in range(1, node_count - 1):
        lines.append(f"  Real u{index}(start = 0.0);")
    lines.append("equation")
    for index in range(1, node_count - 1):
        terms = [
            f"({inverse_dx_squared:.17g}*(k{index}+0.5*(k{index - 1}+k{index + 1})))*u{index}"
        ]
        if index > 1:
            terms.append(
                f"(-0.5*{inverse_dx_squared:.17g}*(k{index - 1}+k{index}))*u{index - 1}"
            )
        if index < node_count - 2:
            terms.append(
                f"(-0.5*{inverse_dx_squared:.17g}*(k{index}+k{index + 1}))*u{index + 1}"
            )
        lines.append("  " + " + ".join(terms) + f" = f{index};")
    lines.append("end HintsPoisson1D;")
    path.write_text("\n".join(lines) + "\n")


def write_scenario(path: Path, coefficient, forcing) -> None:
    values = [
        *(f"k{index}={float(coefficient[index]):.17g}" for index in range(31)),
        *(f"f{index}={float(forcing[index]):.17g}" for index in range(1, 30)),
    ]
    path.write_text("\n".join(values) + "\n")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--upstream", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    arguments = parser.parse_args()

    upstream = arguments.upstream.resolve()
    numpy_root = upstream / "HINTS_numpy"
    revision = subprocess.run(
        ["git", "-C", str(upstream), "rev-parse", "HEAD"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()
    if revision != OFFICIAL_REVISION:
        raise ValueError(f"expected HINTS revision {OFFICIAL_REVISION}, observed {revision}")
    tracked_changes = subprocess.run(
        ["git", "-C", str(upstream), "status", "--short", "--untracked-files=no"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()
    if tracked_changes:
        raise ValueError("HINTS checkout has modified tracked files")

    data_path = numpy_root / "data/processed_data_1P.pkl"
    config_path = numpy_root / "example_configs/configs_1D_Poisson_HINTS_Jacobi.py"
    required = [
        data_path,
        config_path,
        numpy_root / "iterative_solver.py",
        numpy_root / "deeponet.py",
        numpy_root / "models/training_2022-05-30_1P/branch_model",
        numpy_root / "models/training_2022-05-30_1P/trunk_model",
        numpy_root / "models/training_2022-05-30_1P/bias",
    ]
    for path in required:
        if not path.is_file():
            raise FileNotFoundError(path)

    with data_path.open("rb") as handle:
        data = pickle.load(handle)
    if data["k_test"].shape != (TEST_COUNT, 31) or data["f_test"].shape != (TEST_COUNT, 31):
        raise ValueError("unexpected HINTS 1D Poisson test-set shape")

    output = arguments.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    scenario_directory = output / "scenarios"
    scenario_directory.mkdir(parents=True, exist_ok=True)
    for path in scenario_directory.glob("*.conf"):
        path.unlink()
    write_model(output / "HintsPoisson1D.mo")
    for index in range(TEST_COUNT):
        write_scenario(
            scenario_directory / f"case-{index:03d}.conf",
            data["k_test"][index],
            data["f_test"][index],
        )

    digest = hashlib.sha256()
    for path in sorted(required):
        digest.update(path.relative_to(upstream).as_posix().encode())
        digest.update(b"\0")
        digest.update(path.read_bytes())
        digest.update(b"\0")
    (output / "contract.txt").write_text(
        "\n".join(
            [
                "SMAVE_HINTS_NATIVE_WORKLOAD 1",
                f"official_revision={revision}",
                "problem=official-HINTS-1D-Poisson-test-set",
                "official_test_cases=750",
                "nodes=31",
                "interior_unknowns=29",
                "discretization=official-HINTS-utils.poisson",
                f"upstream_payload_sha256={digest.hexdigest()}",
                f"official_data_sha256={sha256(data_path)}",
                f"official_config_sha256={sha256(config_path)}",
                "scenario_export_lossless_fp64=1",
                "END",
                "",
            ]
        )
    )
    print(f"SMAVE_HINTS_NATIVE_WORKLOAD 1 cases={TEST_COUNT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
