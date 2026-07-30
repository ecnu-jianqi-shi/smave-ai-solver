#!/bin/sh
set -eu

repository_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
build_directory=${1:-"$repository_root/build/release"}
repetitions=${2:-30}
benchmark_executable="$build_directory/smave_pdebench_ns_incompressible_benchmark"
input_file="$repository_root/benchmark/pdebench/2D/NS_incom/ns_incom_2d_512-0.h5"
learned_operator="$build_directory/pdebench-solver-labels/ns-incompressible.operator"
evidence_directory="$build_directory/parallel-scaling"
run_identifier=$(date -u +%Y%m%dT%H%M%SZ)
report_directory="$evidence_directory/$run_identifier"
plugin_path=$(sed -n 's/^SMAVE_BENCH_HDF5_PLUGIN_PATH:PATH=//p' "$build_directory/CMakeCache.txt")

test -x "$benchmark_executable"
test -f "$input_file"
test -f "$learned_operator"
test -n "$plugin_path"
mkdir -p "$report_directory"

for worker_count in 1 2 4 8 10; do
    HDF5_PLUGIN_PATH="$plugin_path" VECLIB_MAXIMUM_THREADS=1 \
        "$benchmark_executable" "$input_file" \
        "$report_directory/warmup-workers-$worker_count.txt" \
        "$learned_operator" "$worker_count" >/dev/null
done

repetition=1
while [ "$repetition" -le "$repetitions" ]; do
    if [ $((repetition % 2)) -eq 1 ]; then
        worker_order="1 2 4 8 10"
    else
        worker_order="10 8 4 2 1"
    fi
    for worker_count in $worker_order; do
        report_path=$(printf '%s/workers-%s-repeat-%02d.txt' \
            "$report_directory" "$worker_count" "$repetition")
        HDF5_PLUGIN_PATH="$plugin_path" VECLIB_MAXIMUM_THREADS=1 \
            "$benchmark_executable" "$input_file" "$report_path" \
            "$learned_operator" "$worker_count" >/dev/null
    done
    repetition=$((repetition + 1))
done

python3 "$repository_root/benchmark/analyze_ns_parallel_scaling.py" \
    --reports "$report_directory" \
    --evidence "$evidence_directory/evidence.txt" \
    --data "$repository_root/paper/data/ns_parallel_scaling.dat" \
    --repetitions "$repetitions"

printf 'SMAVE_NS_PARALLEL_SCALING_RUN %s\n' "$report_directory"
