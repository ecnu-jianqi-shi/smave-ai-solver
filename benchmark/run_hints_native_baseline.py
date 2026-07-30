#!/usr/bin/env python3

import argparse
import hashlib
import importlib.util
import logging
import math
import os
import pickle
import statistics
import subprocess
import sys
import time
from pathlib import Path


OFFICIAL_REVISION = "0c8b712f81ed08bdf27c3a215f8edb99910f5e2f"


def percentile(values: list[float], probability: float) -> float:
    ordered = sorted(values)
    position = probability * (len(ordered) - 1)
    lower = math.floor(position)
    upper = math.ceil(position)
    fraction = position - lower
    return ordered[lower] * (1.0 - fraction) + ordered[upper] * fraction


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--upstream", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--repetitions", type=int, default=5)
    parser.add_argument("--warmups", type=int, default=3)
    arguments = parser.parse_args()
    if arguments.repetitions <= 0 or arguments.warmups < 0:
        raise ValueError("invalid HINTS timing counts")

    upstream = arguments.upstream.resolve()
    output = arguments.output.resolve()
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

    numpy_root = upstream / "HINTS_numpy"
    config_path = numpy_root / "example_configs/configs_1D_Poisson_HINTS_Jacobi.py"
    os.chdir(numpy_root)
    spec = importlib.util.spec_from_file_location("configs", config_path)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load official HINTS configuration")
    configs = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(configs)
    configs.SHOW_DATA_CREATION_IMAGES = False
    sys.modules["configs"] = configs

    import numpy as np
    import torch

    torch.set_num_threads(1)
    try:
        torch.set_num_interop_threads(1)
    except RuntimeError:
        pass
    original_load = torch.load

    def compatible_load(*args, **kwargs):
        kwargs.setdefault("weights_only", False)
        return original_load(*args, **kwargs)

    torch.load = compatible_load
    sys.path.insert(0, str(numpy_root))
    from iterative_solver import DeepONetSolver, IterativeSolver, NumericalSolver
    from utils import logger

    logger.setLevel(logging.ERROR)
    data_path = numpy_root / "data/processed_data_1P.pkl"
    with data_path.open("rb") as handle:
        data = pickle.load(handle)
    case_count = int(data["k_test"].shape[0])
    if case_count != 750 or data["f_test"].shape != (case_count, 31):
        raise ValueError("unexpected HINTS 1D Poisson test-set shape")

    model_path = numpy_root / "models" / configs.MODEL_NAME
    initial_coefficient = np.asarray(data["k_test"][0], dtype=np.float64)
    initial_forcing = np.asarray(data["f_test"][0], dtype=np.float64)
    setup_started = time.perf_counter_ns()
    solver = IterativeSolver(
        initial_coefficient,
        initial_forcing,
        np.zeros_like(initial_forcing),
        logger,
        str(model_path),
    )
    setup_us = (time.perf_counter_ns() - setup_started) / 1000.0

    numerical_updates = configs.NUM_OF_ITERATIONS - (
        configs.NUM_OF_ITERATIONS // configs.NUMERICAL_TO_DON_RATIO
    )
    learned_updates = configs.NUM_OF_ITERATIONS // configs.NUMERICAL_TO_DON_RATIO

    def prepare_case(index: int) -> None:
        solver.k_func = np.asarray(data["k_test"][index], dtype=np.float64)
        solver.f_func = np.asarray(data["f_test"][index], dtype=np.float64)
        solver.u_init = np.zeros_like(solver.f_func)
        solver.assemble()

    def solve_once():
        approximation = solver.u_init.copy()
        started = time.perf_counter_ns()
        for iteration in range(configs.NUM_OF_ITERATIONS):
            if (iteration + 1) % configs.NUMERICAL_TO_DON_RATIO == 0:
                approximation = DeepONetSolver.iterate_once(solver, approximation)
            else:
                approximation = NumericalSolver.iterate_once(solver, approximation)
        elapsed_us = (time.perf_counter_ns() - started) / 1000.0
        return approximation, elapsed_us

    prepare_case(0)
    for _ in range(arguments.warmups):
        solve_once()

    output.mkdir(parents=True, exist_ok=True)
    raw_path = output / "raw-samples.txt"
    case_medians: list[float] = []
    all_samples: list[float] = []
    failures = 0
    maximum_gate_residual = 0.0
    maximum_solution_error = 0.0
    with raw_path.open("w") as raw:
        raw.write("case repetition elapsed_us gate_relative_inf relative_solution_error\n")
        for case in range(case_count):
            prepare_case(case)
            samples: list[float] = []
            final_approximation = None
            for repetition in range(arguments.repetitions):
                final_approximation, elapsed_us = solve_once()
                interior = final_approximation[1:-1]
                residual = solver.stiff_p @ interior - solver.f_p
                gate_residual = float(
                    np.max(np.abs(residual)) /
                    max(1.0, float(np.max(np.abs(solver.f_p))))
                )
                expected = np.asarray(data["u_test"][case], dtype=np.float64)
                solution_error = float(
                    np.max(np.abs(final_approximation - expected)) /
                    max(1.0, float(np.max(np.abs(expected))))
                )
                if not math.isfinite(elapsed_us) or elapsed_us <= 0.0:
                    raise ValueError("invalid official HINTS timing sample")
                if gate_residual > 1.0e-10:
                    failures += 1
                maximum_gate_residual = max(maximum_gate_residual, gate_residual)
                maximum_solution_error = max(maximum_solution_error, solution_error)
                samples.append(elapsed_us)
                all_samples.append(elapsed_us)
                raw.write(
                    f"{case} {repetition} {elapsed_us:.17g} "
                    f"{gate_residual:.17g} {solution_error:.17g}\n"
                )
            case_medians.append(statistics.median(samples))

    source_digest = hashlib.sha256()
    for relative in [
        "HINTS_numpy/iterative_solver.py",
        "HINTS_numpy/deeponet.py",
        "HINTS_numpy/utils.py",
        "HINTS_numpy/example_configs/configs_1D_Poisson_HINTS_Jacobi.py",
        "HINTS_numpy/data/processed_data_1P.pkl",
        "HINTS_numpy/models/training_2022-05-30_1P/branch_model",
        "HINTS_numpy/models/training_2022-05-30_1P/trunk_model",
        "HINTS_numpy/models/training_2022-05-30_1P/bias",
    ]:
        path = upstream / relative
        source_digest.update(relative.encode())
        source_digest.update(b"\0")
        source_digest.update(path.read_bytes())
        source_digest.update(b"\0")

    evidence = output / "evidence.txt"
    evidence.write_text(
        "\n".join(
            [
                "SMAVE_HINTS_OFFICIAL_NATIVE 1",
                f"official_revision={revision}",
                "official_public_code_executed=1",
                "official_pretrained_weights_used=1",
                "official_deeponet_architecture_executed=1",
                "official_dataset_used=1",
                "problem=1D-Poisson-HINTS-Jacobi",
                f"upstream_payload_sha256={source_digest.hexdigest()}",
                f"python={sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}",
                f"torch={torch.__version__}",
                "torch_intraop_threads=1",
                "torch_interop_threads=1",
                "compatibility_adaptation=torch-load-weights-only-false",
                "tracked_upstream_source_modified=0",
                "diagnostic_metric_loop_timed=0",
                "plotting_timed=0",
                "model_load_and_matrix_setup_timed_separately=1",
                f"setup_us={setup_us:.17g}",
                f"test_cases={case_count}",
                f"repetitions={arguments.repetitions}",
                f"warmups={arguments.warmups}",
                f"iterations={configs.NUM_OF_ITERATIONS}",
                f"numerical_to_deeponet_ratio={configs.NUMERICAL_TO_DON_RATIO}",
                f"jacobi_weight={float(configs.OMEGA):.17g}",
                f"numerical_updates_per_solve={numerical_updates}",
                f"deeponet_updates_per_solve={learned_updates}",
                f"online_case_median_us={statistics.median(case_medians):.17g}",
                f"online_case_p90_us={percentile(case_medians, 0.9):.17g}",
                f"online_sample_median_us={statistics.median(all_samples):.17g}",
                f"maximum_gate_relative_inf={maximum_gate_residual:.17g}",
                f"maximum_relative_solution_error={maximum_solution_error:.17g}",
                f"failed_gate_samples={failures}",
                f"all_gate_samples_pass={int(failures == 0)}",
                "END",
                "",
            ]
        )
    )
    print(
        "SMAVE_HINTS_OFFICIAL_NATIVE 1 "
        f"cases={case_count} median_us={statistics.median(case_medians):.6f} "
        f"failures={failures}"
    )
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
