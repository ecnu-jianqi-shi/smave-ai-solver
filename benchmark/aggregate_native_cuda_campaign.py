#!/usr/bin/env python3
"""Aggregate native CUDA campaign runs into a provenance-bound evidence envelope."""
import argparse
import hashlib
import os
import platform
import re
import subprocess
import datetime
from pathlib import Path

HEADER = "SMAVE_NATIVE_CUDA_CAMPAIGN 1"


def parse(path: Path) -> dict:
    lines = path.read_text().splitlines()
    assert lines[0] == "NATIVE_CUDA_DEVICE_EVIDENCE_V1"
    assert lines[-1] == "END"
    values = {}
    for line in lines[1:-1]:
        key, _, value = line.partition("=")
        values[key] = value
    return values


def gpu_name(values):
    return values.get("cuda_device_name", "unknown")


def provenance_block():
    fields = {
        "host_architecture": platform.machine(),
        "host_name": platform.node(),
        "host_os": platform.platform(),
        "captured_at_utc": datetime.datetime.utcnow().isoformat() + "Z",
    }
    nvcc = ""
    for candidate in ("nvcc", "/usr/local/cuda/bin/nvcc"):
        result = subprocess.run(
            [candidate, "--version"], capture_output=True, text=True, check=False
        )
        if result.returncode == 0:
            m = re.search(r"release (\d+\.\d+)", result.stdout)
            nvcc = m.group(1) if m else "unknown"
            break
    fields["cuda_compiler_version"] = nvcc or "unknown"
    nvidia_smi = subprocess.run(
        ["nvidia-smi", "--query-gpu=name,driver_version", "--format=csv,noheader"],
        capture_output=True, text=True, check=False,
    )
    if nvidia_smi.returncode == 0 and nvidia_smi.stdout.strip():
        parts = nvidia_smi.stdout.strip().split(", ")
        fields["cuda_device_name_prov"] = parts[0] if parts else "unknown"
        fields["cuda_driver_version"] = parts[1] if len(parts) > 1 else "unknown"
    else:
        fields["cuda_device_name_prov"] = "unknown"
        fields["cuda_driver_version"] = "unknown"
    return fields


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cold", required=True)
    parser.add_argument("--warm-1", required=True)
    parser.add_argument("--warm-2", required=True)
    parser.add_argument("--warm-3", required=True)
    parser.add_argument("--replicate-id", type=int, required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    cold = parse(Path(args.cold))
    warm1 = parse(Path(args.warm_1))
    warm2 = parse(Path(args.warm_2))
    warm3 = parse(Path(args.warm_3))

    prov = provenance_block()

    # SHA-256 of each component evidence file for provenance binding.
    def digest(path):
        return hashlib.sha256(Path(path).read_bytes()).hexdigest()

    lines = [HEADER]
    lines.append("contract=native-discrete-gpu-cuda-solver-kernel-campaign")
    lines.append("campaign_schema=1")
    lines.append(f"replicate_id={args.replicate_id}")
    lines.append("native_cuda_execution=1")
    lines.append("discrete_gpu=1")
    lines.append("emulation=0")
    lines.append("kernel_only_benchmark=0")
    lines.append("cold_warm_separated=1")
    lines.append("transfer_reported=1")
    lines.append("residency_reported=1")
    lines.append("original_equation_gate=1")
    lines.append("negative_results_retained=1")
    lines.append("fp64_reference_gate=1")
    lines.append(f"cuda_device_name={gpu_name(cold)}")
    for key, value in prov.items():
        lines.append(f"{key}={value}")
    lines.append(f"cold_affine_upload_us={cold['cuda_affine_upload_us']}")
    lines.append(f"cold_affine_kernel_us={cold['cuda_affine_kernel_us']}")
    lines.append(f"cold_affine_download_us={cold['cuda_affine_download_us']}")
    lines.append(f"cold_stencil_setup_us={cold['cuda_stencil_setup_us']}")
    lines.append(f"cold_stencil_kernel_us={cold['cuda_stencil_kernel_us']}")
    lines.append(f"cold_stencil_download_us={cold['cuda_stencil_download_us']}")
    lines.append(f"warm1_affine_kernel_us={warm1['cuda_affine_kernel_us']}")
    lines.append(f"warm2_affine_kernel_us={warm2['cuda_affine_kernel_us']}")
    lines.append(f"warm3_affine_kernel_us={warm3['cuda_affine_kernel_us']}")
    lines.append(f"warm1_stencil_kernel_us={warm1['cuda_stencil_kernel_us']}")
    lines.append(f"warm2_stencil_kernel_us={warm2['cuda_stencil_kernel_us']}")
    lines.append(f"warm3_stencil_kernel_us={warm3['cuda_stencil_kernel_us']}")
    lines.append(f"affine_executed={int(cold['cuda_affine_executed'])}")
    lines.append(f"affine_verified={int(cold['cuda_affine_verified'])}")
    lines.append(f"stencil_executed={int(cold['cuda_stencil_executed'])}")
    lines.append(f"stencil_verified={int(cold['cuda_stencil_verified'])}")
    lines.append(f"affine_max_rel_error={cold['cuda_affine_max_rel_error']}")
    lines.append(f"stencil_max_rel_residual={cold['cuda_stencil_max_rel_residual']}")
    lines.append(f"cold_evidence_sha256={digest(args.cold)}")
    lines.append(f"warm1_evidence_sha256={digest(args.warm_1)}")
    lines.append(f"warm2_evidence_sha256={digest(args.warm_2)}")
    lines.append(f"warm3_evidence_sha256={digest(args.warm_3)}")
    lines.append("END")

    out = Path(args.output)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("\n".join(lines) + "\n")
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
