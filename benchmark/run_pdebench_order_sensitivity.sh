#!/bin/sh
set -eu

repository_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
build_directory=${1:-"$repository_root/build/release"}
repetitions=${2:-30}
evidence_directory="$build_directory/pdebench-order-sensitivity"
run_identifier=$(date -u +%Y%m%dT%H%M%SZ)
report_directory="$evidence_directory/$run_identifier"
label_directory="$build_directory/pdebench-solver-labels"
plugin_path=$(sed -n 's/^SMAVE_BENCH_HDF5_PLUGIN_PATH:PATH=//p' "$build_directory/CMakeCache.txt")

test -n "$plugin_path"
mkdir -p "$report_directory"

run_workload() {
    workload=$1
    order=$2
    output=$3
    case "$workload" in
        advection)
            SMAVE_BENCHMARK_SOLVER_ORDER="$order" \
                "$build_directory/smave_pdebench_advection_benchmark" \
                "$repository_root/benchmark/pdebench/1D/Advection/1D_Advection_Sols_beta0.1.hdf5" \
                "$output" "$label_directory/advection.operator" ;;
        darcy)
            SMAVE_BENCHMARK_SOLVER_ORDER="$order" \
                "$build_directory/smave_pdebench_darcy_benchmark" \
                "$repository_root/benchmark/pdebench/2D/DarcyFlow/2D_DarcyFlow_beta0.01_Train.hdf5" \
                "$output" "$label_directory/darcy.operator" ;;
        burgers)
            SMAVE_BENCHMARK_SOLVER_ORDER="$order" \
                "$build_directory/smave_pdebench_burgers_benchmark" \
                "$repository_root/benchmark/pdebench/1D/Burgers/1D_Burgers_Sols_Nu0.001.hdf5" \
                "$output" "$label_directory/burgers.operator" ;;
        diffusion-sorption)
            SMAVE_BENCHMARK_SOLVER_ORDER="$order" \
                "$build_directory/smave_pdebench_diffusion_sorption_benchmark" \
                "$repository_root/benchmark/pdebench/1D/diffusion-sorption/1D_diff-sorp.h5" \
                "$output" "$label_directory/diffusion-sorption.operator" ;;
        shallow-water)
            SMAVE_BENCHMARK_SOLVER_ORDER="$order" \
                "$build_directory/smave_pdebench_shallow_water_benchmark" \
                "$repository_root/benchmark/pdebench/2D/shallow-water/2D_rdb.h5" \
                "$output" "$label_directory/shallow-water.operator" ;;
        ns-incompressible)
            SMAVE_BENCHMARK_SOLVER_ORDER="$order" HDF5_PLUGIN_PATH="$plugin_path" \
                "$build_directory/smave_pdebench_ns_incompressible_benchmark" \
                "$repository_root/benchmark/pdebench/2D/NS_incom/ns_incom_2d_512-0.h5" \
                "$output" "$label_directory/ns-incompressible.operator" ;;
        cfd-1d)
            SMAVE_BENCHMARK_SOLVER_ORDER="$order" \
                "$build_directory/smave_pdebench_cfd_1d_benchmark" \
                "$repository_root/benchmark/pdebench/1D/CFD/1D_CFD_Rand_Train.hdf5" \
                "$output" ;;
        *)
            printf 'unknown workload: %s\n' "$workload" >&2
            return 2 ;;
    esac
}

workloads="advection darcy burgers diffusion-sorption shallow-water ns-incompressible cfd-1d"

for workload in $workloads; do
    for order in classical-first smave-first; do
        run_workload "$workload" "$order" "$report_directory/warmup-$workload-$order.txt" \
            >/dev/null
    done
done

repetition=1
while [ "$repetition" -le "$repetitions" ]; do
    for workload in $workloads; do
        if [ $((repetition % 2)) -eq 1 ]; then
            orders="classical-first smave-first"
        else
            orders="smave-first classical-first"
        fi
        for order in $orders; do
            report_path=$(printf '%s/%s-%s-repeat-%02d.txt' \
                "$report_directory" "$workload" "$order" "$repetition")
            run_workload "$workload" "$order" "$report_path" >/dev/null
        done
    done
    repetition=$((repetition + 1))
done

python3 "$repository_root/benchmark/analyze_pdebench_order_sensitivity.py" \
    --reports "$report_directory" \
    --evidence "$evidence_directory/evidence.txt" \
    --repetitions "$repetitions"

printf 'SMAVE_PDEBENCH_ORDER_SENSITIVITY_RUN %s\n' "$report_directory"
