#!/bin/sh
set -eu

repository_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
build_directory=${1:-"$repository_root/build/release"}
repetitions=${2:-30}
evidence_directory="$build_directory/pdebench-repeated-timing"
run_identifier=$(date -u +%Y%m%dT%H%M%SZ)
report_directory="$evidence_directory/$run_identifier"
label_directory="$build_directory/pdebench-solver-labels"
plugin_path=$(sed -n 's/^SMAVE_BENCH_HDF5_PLUGIN_PATH:PATH=//p' "$build_directory/CMakeCache.txt")

test -n "$plugin_path"
mkdir -p "$report_directory"

run_workload() {
    workload=$1
    output=$2
    case "$workload" in
        advection)
            "$build_directory/smave_pdebench_advection_benchmark" \
                "$repository_root/benchmark/pdebench/1D/Advection/1D_Advection_Sols_beta0.1.hdf5" \
                "$output" "$label_directory/advection.operator" ;;
        darcy)
            "$build_directory/smave_pdebench_darcy_benchmark" \
                "$repository_root/benchmark/pdebench/2D/DarcyFlow/2D_DarcyFlow_beta0.01_Train.hdf5" \
                "$output" "$label_directory/darcy.operator" ;;
        burgers)
            "$build_directory/smave_pdebench_burgers_benchmark" \
                "$repository_root/benchmark/pdebench/1D/Burgers/1D_Burgers_Sols_Nu0.001.hdf5" \
                "$output" "$label_directory/burgers.operator" ;;
        diffusion-sorption)
            "$build_directory/smave_pdebench_diffusion_sorption_benchmark" \
                "$repository_root/benchmark/pdebench/1D/diffusion-sorption/1D_diff-sorp.h5" \
                "$output" "$label_directory/diffusion-sorption.operator" ;;
        shallow-water)
            "$build_directory/smave_pdebench_shallow_water_benchmark" \
                "$repository_root/benchmark/pdebench/2D/shallow-water/2D_rdb.h5" \
                "$output" "$label_directory/shallow-water.operator" ;;
        ns-incompressible)
            HDF5_PLUGIN_PATH="$plugin_path" \
                "$build_directory/smave_pdebench_ns_incompressible_benchmark" \
                "$repository_root/benchmark/pdebench/2D/NS_incom/ns_incom_2d_512-0.h5" \
                "$output" "$label_directory/ns-incompressible.operator" ;;
        cfd-1d)
            "$build_directory/smave_pdebench_cfd_1d_benchmark" \
                "$repository_root/benchmark/pdebench/1D/CFD/1D_CFD_Rand_Train.hdf5" \
                "$output" ;;
        *)
            printf 'unknown workload: %s\n' "$workload" >&2
            return 2 ;;
    esac
}

for workload in advection darcy burgers diffusion-sorption shallow-water ns-incompressible cfd-1d; do
    run_workload "$workload" "$report_directory/warmup-$workload.txt" >/dev/null
done

repetition=1
while [ "$repetition" -le "$repetitions" ]; do
    if [ $((repetition % 2)) -eq 1 ]; then
        workload_order="advection darcy burgers diffusion-sorption shallow-water ns-incompressible cfd-1d"
    else
        workload_order="cfd-1d ns-incompressible shallow-water diffusion-sorption burgers darcy advection"
    fi
    for workload in $workload_order; do
        report_path=$(printf '%s/%s-repeat-%02d.txt' \
            "$report_directory" "$workload" "$repetition")
        run_workload "$workload" "$report_path" >/dev/null
    done
    repetition=$((repetition + 1))
done

python3 "$repository_root/benchmark/analyze_pdebench_repeated_timing.py" \
    --reports "$report_directory" \
    --evidence "$evidence_directory/evidence.txt" \
    --data "$repository_root/paper/data/pdebench_repeated_timing.dat" \
    --repetitions "$repetitions"

printf 'SMAVE_PDEBENCH_REPEATED_TIMING_RUN %s\n' "$report_directory"
