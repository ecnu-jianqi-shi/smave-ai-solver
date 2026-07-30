#!/bin/sh
# Native discrete-GPU CUDA campaign for SMAVE solver kernels.
#
# Executes the CUDA affine batch and weighted-Jacobi stencil kernels on a real
# discrete NVIDIA GPU, records cold and warm runs, transfer/residency timings
# and a host provenance block, then verifies the resulting evidence envelope.
#
# Usage:
#   benchmark/run_native_cuda_campaign.sh \
#       --build <build-dir> --output <output-dir> [--replicate-id N]
set -eu

repository_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
build_directory="$repository_root/build/native-cuda"
output_directory="$repository_root/build/native-cuda/campaign"
replicate_id=1

while [ "$#" -gt 0 ]; do
    case "$1" in
        --build) build_directory=$2; shift 2 ;;
        --output) output_directory=$2; shift 2 ;;
        --replicate-id) replicate_id=$2; shift 2 ;;
        *) printf 'unknown argument: %s\n' "$1" >&2; exit 2 ;;
    esac
done

cmake -E rm -rf "$output_directory"
cmake -E make_directory "$output_directory"

if [ -z "${SMAVE_CUDA_TOOLKIT_ROOT:-}" ]; then
    echo "SMAVE_CUDA_TOOLKIT_ROOT must be set (e.g. /usr/local/cuda)" >&2
    exit 2
fi

cmake -S "$repository_root" -B "$build_directory" \
    -DCMAKE_BUILD_TYPE=Release \
    -DSMAVE_CUDA_TOOLKIT_ROOT="$SMAVE_CUDA_TOOLKIT_ROOT" \
    -DCMAKE_CUDA_ARCHITECTURES=native \
    -DSMAVE_CORE_REPRO_BUNDLE=ON

cmake --build "$build_directory" --target smave_cuda_device_evidence

evidence_binary="$build_directory/smave_cuda_device_evidence"

# Cold run: first kernel launch (includes JIT, context, module load)
"$evidence_binary" "$output_directory/cold"
cold_evidence="$output_directory/cold/evidence.txt"

# Warm runs: resident context, repeated kernels
"$evidence_binary" "$output_directory/warm-1"
"$evidence_binary" "$output_directory/warm-2"
"$evidence_binary" "$output_directory/warm-3"

python3 "$repository_root/benchmark/aggregate_native_cuda_campaign.py" \
    --cold "$cold_evidence" \
    --warm-1 "$output_directory/warm-1/evidence.txt" \
    --warm-2 "$output_directory/warm-2/evidence.txt" \
    --warm-3 "$output_directory/warm-3/evidence.txt" \
    --replicate-id "$replicate_id" \
    --output "$output_directory/evidence.txt"

python3 "$repository_root/benchmark/verify_native_cuda_campaign.py" \
    --evidence "$output_directory/evidence.txt"

printf 'SMAVE_NATIVE_CUDA_CAMPAIGN_RUN %s\n' "$output_directory"
