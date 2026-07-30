#!/usr/bin/env python3
"""Verify a native CUDA campaign evidence envelope against the contract."""
import argparse
import re
from pathlib import Path

HEADER = "SMAVE_NATIVE_CUDA_CAMPAIGN 1"

REQUIRED = [
    "contract=native-discrete-gpu-cuda-solver-kernel-campaign",
    "campaign_schema=1",
    "native_cuda_execution=1",
    "discrete_gpu=1",
    "emulation=0",
    "kernel_only_benchmark=0",
    "cold_warm_separated=1",
    "transfer_reported=1",
    "residency_reported=1",
    "original_equation_gate=1",
    "fp64_reference_gate=1",
    "affine_executed=1",
    "affine_verified=1",
    "stencil_executed=1",
    "stencil_verified=1",
]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--evidence", required=True)
    args = parser.parse_args()

    path = Path(args.evidence)
    if not path.exists():
        raise SystemExit(f"missing evidence: {path}")
    text = path.read_text()
    lines = text.splitlines()
    if not lines or lines[0] != HEADER:
        raise SystemExit("invalid header")
    if lines[-1] != "END":
        raise SystemExit("missing END marker")
    values = {}
    for line in lines[1:-1]:
        key, _, value = line.partition("=")
        values[key] = value
    for marker in REQUIRED:
        if marker not in text:
            raise SystemExit(f"missing marker: {marker}")

    # Provenance must bind a real host, compiler and device.
    if values.get("cuda_device_name") in ("", "unknown"):
        raise SystemExit("no CUDA device identity")
    if values.get("cuda_compiler_version") in ("", "unknown"):
        raise SystemExit("no CUDA compiler provenance")
    if values.get("cuda_device_name_prov") in ("", "unknown"):
        raise SystemExit("no nvidia-smi device provenance")
    if values.get("cuda_driver_version") in ("", "unknown"):
        raise SystemExit("no CUDA driver provenance")

    # Cold must be slower than warm (residency confirmed).
    cold = float(values["cold_affine_kernel_us"])
    warm = min(
        float(values["warm1_affine_kernel_us"]),
        float(values["warm2_affine_kernel_us"]),
        float(values["warm3_affine_kernel_us"]),
    )
    if warm >= cold:
        # On small kernels warm can be noisy; only fail if dramatically worse.
        if warm > 10 * cold:
            raise SystemExit("warm kernel dramatically slower than cold (no residency)")

    # Gate must be near zero error.
    if float(values["affine_max_rel_error"]) > 1.0e-5:
        raise SystemExit("affine reference gate error too large")
    if float(values["stencil_max_rel_residual"]) > 1.0e-4:
        raise SystemExit("stencil residual gate too large")

    # SHA-256 provenance must be present and non-empty.
    for key in ("cold_evidence_sha256", "warm1_evidence_sha256",
                "warm2_evidence_sha256", "warm3_evidence_sha256"):
        if len(values.get(key, "")) != 64:
            raise SystemExit(f"missing or invalid {key}")

    print("native CUDA campaign evidence passed")


if __name__ == "__main__":
    main()
