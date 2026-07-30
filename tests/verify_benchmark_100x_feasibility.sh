#!/bin/sh
set -eu

directory=${1:?benchmark directory is required}
output=${2:?output path is required}

mkdir -p "$(dirname "$output")"
temporary="${output}.tmp.$$"
trap 'rm -f "$temporary"' EXIT

extract() {
    awk -v key="$2" '$1 == key { print $2; found = 1; exit } END { if (!found) exit 2 }' "$1"
}

blocked=0
{
    echo "SMAVE_PDEBENCH_100X_FEASIBILITY 1"
    for specification in \
        "ADVECTION:advection-summary.txt:SMAVE_GATE_SECONDS" \
        "DARCY:darcy-summary.txt:ACCELERATE_BAND_CHOLESKY_GATE_SECONDS" \
        "BURGERS:burgers-summary.txt:SMAVE_GATE_SECONDS" \
        "DIFFUSION_SORPTION:diffusion-sorption-summary.txt:SMAVE_GATE_SECONDS" \
        "SHALLOW_WATER:shallow-water-summary.txt:SMAVE_GATE_SECONDS" \
        "NS_INCOMPRESSIBLE:ns-incompressible-summary.txt:SMAVE_GATE_SECONDS" \
        "CFD_1D:cfd-1d-summary.txt:SMAVE_GATE_SECONDS"
    do
        family=${specification%%:*}
        remainder=${specification#*:}
        filename=${remainder%%:*}
        gate_key=${remainder#*:}
        report="$directory/$filename"
        classical=$(extract "$report" CLASSICAL_SOLVE_SECONDS)
        gate=$(extract "$report" "$gate_key")
        budget=$(awk -v value="$classical" 'BEGIN { printf "%.17g", value / 100.0 }')
        ratio=$(awk -v gate="$gate" -v budget="$budget" \
            'BEGIN { printf "%.17g", gate / budget }')
        exceeds=$(awk -v gate="$gate" -v budget="$budget" \
            'BEGIN { print (gate > budget ? 1 : 0) }')
        echo "${family}_CLASSICAL_SECONDS $classical"
        echo "${family}_100X_BUDGET_SECONDS $budget"
        echo "${family}_CURRENT_GATE_SECONDS $gate"
        echo "${family}_GATE_VS_100X_BUDGET $ratio"
        echo "${family}_ZERO_KERNEL_100X_FEASIBLE $((1 - exceeds))"
        if [ "$exceeds" -eq 1 ]; then
            blocked=$((blocked + 1))
        fi
    done
    echo "ZERO_KERNEL_100X_BLOCKED_CASES $blocked"
    echo "ZERO_KERNEL_ALL_100X_FEASIBLE $([ "$blocked" -eq 0 ] && echo 1 || echo 0)"
    echo "END"
} > "$temporary"

mv "$temporary" "$output"
trap - EXIT
cat "$output"

if [ "$blocked" -ne 0 ]; then
    echo "current mandatory residual gates exceed the complete 100x budget in $blocked case(s)" >&2
    exit 2
fi
