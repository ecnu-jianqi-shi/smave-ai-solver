#!/bin/sh
set -eu

cd "$(dirname "$0")"
python3 check_evidence.py
python3 check_artifact_manifest.py
latexmk -pdf -interaction=nonstopmode -halt-on-error main.tex

for required in \
  main.tex authors.tex abstract.tex references.bib \
  generated/gate_scaling_values.tex \
  generated/pde_timing_values.tex \
  generated/order_sensitivity_values.tex \
  generated/hints_native_values.tex \
  generated/joint_route_budget_values.tex \
  generated/joint_route_scaling_values.tex \
  generated/frozen_interaction_prevalence_values.tex \
  generated/frozen_transition_attrition_values.tex \
  generated/joint_route_budget_shift_values.tex \
  sections/01_introduction.tex \
  sections/02_background_related_work.tex \
  sections/03_problem_formulation.tex \
  sections/04_system_design.tex \
  sections/05_verification_aware_fusion.tex \
  sections/06_experimental_methodology.tex \
  sections/07_evaluation.tex \
  sections/08_ablation_analysis.tex \
  sections/09_discussion_limitations.tex \
  sections/10_conclusion.tex \
  sections/11_acknowledgments.tex; do
  test -s "$required"
done

if rg -q 'Undefined control sequence|Citation .* undefined|Reference .* undefined|Overfull \\[hv]box' main.log; then
  echo "paper check failed: unresolved reference or overfull box" >&2
  exit 1
fi

pages="$(sed -n 's/.*Output written on main.pdf (\([0-9][0-9]*\) pages.*/\1/p' main.log | tail -n 1)"
if [ "$pages" != "12" ]; then
  echo "paper check failed: expected 12 pages, found ${pages:-unknown}" >&2
  exit 1
fi

test -s main.pdf
echo "SMAVE_TPDS_PAPER_CHECK 1"
