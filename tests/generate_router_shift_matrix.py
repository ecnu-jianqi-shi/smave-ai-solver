#!/usr/bin/env python3

import argparse
from pathlib import Path


def write_model(
    path: Path,
    name: str,
    diagonal: float,
    west: float,
    east: float,
    north: float,
    south: float,
) -> None:
    grid = 5
    lines = [f"model {name}"]
    for index in range(1, grid * grid + 1):
        lines.append(f"  parameter Real b{index} = 1.0;")
    for index in range(1, grid * grid + 1):
        lines.append(f"  Real x{index}(start = 0.0);")
    lines.append("equation")
    for row in range(grid):
        for column in range(grid):
            index = row * grid + column + 1
            terms = [f"{diagonal}*x{index}"]
            if column > 0:
                terms.append(f"- {west}*x{index - 1}")
            if column + 1 < grid:
                terms.append(f"- {east}*x{index + 1}")
            if row > 0:
                terms.append(f"- {north}*x{index - grid}")
            if row + 1 < grid:
                terms.append(f"- {south}*x{index + grid}")
            lines.append("  " + " ".join(terms) + f" = b{index};")
    lines.append(f"end {name};")
    path.write_text("\n".join(lines) + "\n")


def write_scenarios(directory: Path) -> None:
    directory.mkdir(parents=True, exist_ok=True)
    for case in range(64):
        lines = []
        for index in range(1, 26):
            value = (
                0.75
                + 0.0125 * case
                + 0.003 * index
                + 0.02 * (((case + index) % 7) - 3)
            )
            lines.append(f"b{index}={value:.10f}")
        (directory / f"case-{case:02d}.conf").write_text(
            "\n".join(lines) + "\n"
        )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    arguments.output.mkdir(parents=True, exist_ok=True)
    write_model(
        arguments.output / "source.mo",
        "RouterShiftSource",
        4.5,
        1.0,
        1.0,
        1.0,
        1.0,
    )
    write_model(
        arguments.output / "conditioning.mo",
        "RouterShiftConditioning",
        3.5,
        1.0,
        1.0,
        1.0,
        1.0,
    )
    write_model(
        arguments.output / "topology.mo",
        "RouterShiftTopology",
        4.5,
        1.35,
        0.65,
        1.0,
        1.0,
    )
    write_scenarios(arguments.output / "scenarios")
    print("SMAVE_ROUTER_SHIFT_MATRIX_INPUTS 1")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
