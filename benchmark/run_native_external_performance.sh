#!/bin/sh
set -eu

repository_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
provider=local
replicate_id=1
build_directory="$repository_root/build/native-external-performance"
output_directory="$repository_root/build/native-external-performance/local-dry-run"
jobs=2

while [ "$#" -gt 0 ]; do
    case "$1" in
        --provider) provider=$2; shift 2 ;;
        --replicate-id) replicate_id=$2; shift 2 ;;
        --build) build_directory=$2; shift 2 ;;
        --output) output_directory=$2; shift 2 ;;
        --jobs) jobs=$2; shift 2 ;;
        *) printf 'unknown argument: %s\n' "$1" >&2; exit 2 ;;
    esac
done

case "$provider" in
    local|github-hosted) ;;
    *) printf 'invalid provider: %s\n' "$provider" >&2; exit 2 ;;
esac

cmake -E rm -rf "$output_directory"
cmake -E make_directory \
    "$output_directory/components/gate-parallel" \
    "$output_directory/components/risk-adaptive" \
    "$output_directory/components/operator-replication"

if command -v ninja >/dev/null 2>&1; then
    cmake -S "$repository_root" -B "$build_directory" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release -DSMAVE_CORE_REPRO_BUNDLE=ON
else
    cmake -S "$repository_root" -B "$build_directory" \
        -DCMAKE_BUILD_TYPE=Release -DSMAVE_CORE_REPRO_BUNDLE=ON
fi
cmake --build "$build_directory" --parallel "$jobs" --target \
    smave_gate_parallel_scaling_evidence \
    smave_risk_adaptive_gate_evidence smave

"$build_directory/smave_gate_parallel_scaling_evidence" \
    "$repository_root/examples/OperatorPoissonGrid10.mo" \
    "$repository_root/examples/operator-scenarios" \
    "$repository_root/examples/CubicCoupled.mo" \
    "$output_directory/components/gate-parallel/evidence.txt"

"$build_directory/smave_risk_adaptive_gate_evidence" \
    "$repository_root/examples/OperatorPoissonGrid10.mo" \
    "$repository_root/examples/operator-scenarios" \
    "$repository_root/examples/CubicCoupled.mo" \
    "$output_directory/components/risk-adaptive/evidence.txt"

cmake --build "$build_directory" --parallel "$jobs" \
    --target reproduce-operator-replication
cmake -E copy "$build_directory/operator-replication/evidence.txt" \
    "$output_directory/components/operator-replication/evidence.txt"
cmake -E copy "$build_directory/operator-replication/performance.txt" \
    "$output_directory/components/operator-replication/performance.txt"
cmake -E copy \
    "$build_directory/operator-replication/benchmark-traces/operator-statistics.txt" \
    "$output_directory/components/operator-replication/operator-statistics.txt"

python3 "$repository_root/benchmark/collect_native_external_performance.py" \
    --provider "$provider" \
    --replicate-id "$replicate_id" \
    --build-directory "$build_directory" \
    --output "$output_directory/evidence.txt" \
    --gate-parallel "$output_directory/components/gate-parallel/evidence.txt" \
    --gate-parallel-samples "$output_directory/components/gate-parallel/raw-samples.txt" \
    --risk-adaptive "$output_directory/components/risk-adaptive/evidence.txt" \
    --risk-adaptive-samples "$output_directory/components/risk-adaptive/raw-samples.txt" \
    --operator-replication "$output_directory/components/operator-replication/evidence.txt" \
    --operator-performance "$output_directory/components/operator-replication/performance.txt" \
    --operator-statistics "$output_directory/components/operator-replication/operator-statistics.txt"

if [ "$provider" = github-hosted ]; then
    expectation=--expect-github-hosted
else
    expectation=--expect-local
fi
python3 "$repository_root/benchmark/verify_native_external_performance.py" \
    --evidence "$output_directory/evidence.txt" "$expectation"
printf 'SMAVE_NATIVE_EXTERNAL_PERFORMANCE_RUN %s\n' "$output_directory"
