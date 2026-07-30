# TPDS Manuscript

This directory contains the modular manuscript targeting **IEEE Transactions on
Parallel and Distributed Systems (TPDS)**.

## Scientific Scope

The paper studies repeated numerical solves. Its central question is whether
heterogeneous classical and learned experts can be fused so
that routing minimizes complete verified runtime while the original equation determines
which result is returned.

The paper's claim surface is limited to:

- reach-weighted complete-cost routing and fixed-cascade ordering;
- typed candidate--corrector--gate--numerical-continuation solver pipelines;
- calibrated per-expert correction-budget propagation;
- exact finite-action joint expert--budget optimization for independent and adjacent
  transition costs, with an NP-completeness boundary;
- request-conditioned joint action cost/acceptance prediction from production traces;
- routing calibration under size, conditioning, and topology shift;
- family-specific original-equation acceptance for algebraic and dynamic systems;
- learned-candidate correction, strict-gate optimization, batching, and heterogeneous
  placement under complete cost;
- paired solver benchmarks, negative results, and reproducible claim extraction.

Repository regression tests may remain for compatibility, but the manuscript and review
priorities follow the solver claims listed above.

Terminology is numerical: `gate` means original-equation acceptance. Rejection continues
from the original request state to the next numerical solver path. The paper calls this
mechanism numerical continuation; `fallback` is retained only in legacy code and metric
field names.

The research scope is solver algorithms, numerical correctness, complete-path cost,
solver-internal parallelism, heterogeneous computation, and reproducible evidence.
Numerical continuation is strictly an in-request solver decision. Every review condition
must map to a solver claim, a numerical-validity contract, or complete-path evidence.

## Structure

- `main.tex`: IEEE Computer Society journal entry point.
- `authors.tex`: author and affiliation block; currently a required placeholder.
- `abstract.tex`: standalone abstract.
- `sections/`: numbered manuscript sections.
- `figures/`: TikZ/PGFPlots figures.
- `data/` and `generated/`: machine-derived plotting data and LaTeX values.
- `references.bib`: bibliography.
- `SUPPLEMENTARY_THEORY.md`: expanded finite-cascade exactness and hardness proofs.
- `CLAIM_EVIDENCE.md`: solver claim-to-evidence ledger.
- `REVIEW.md`: latest scope-corrected submission review.
- `ARTIFACT_SNAPSHOT.md`: frozen local evidence scope and limitations.

## Build

```bash
cd paper
latexmk -pdf -interaction=nonstopmode -halt-on-error main.tex
```

Repository-level checking:

```bash
paper/check.sh
```

## Primary Evidence

- `build/release/pdebench-repeated-timing/evidence.txt`: seven-workload paired
  complete-runtime comparison.
- `build/release/pdebench-order-sensitivity/evidence.txt`: counterbalanced phase-order
  sensitivity.
- `build/release/phase4/family-router-evaluation.txt`: calibrated routing versus a fixed
  expert.
- `build/release/cascade-ordering/evidence.txt`: exact four-stage/24-permutation
  fixed-cascade ordering check.
- `build/release/router-shift/evidence.txt`: gate-passing-expert calibration, cost-rank stability,
  and source-selected regret under a 5×5-to-6×6 size/fingerprint shift.
- `build/release/request-conditioned-joint-route/evidence.txt`,
  `action-observations.tsv`, and `request-conditioned-model.txt`: production-trace-trained
  request-conditioned cost/pass prediction for 12 expert--budget actions, disjoint
  calibration and held-out requests, exact-DP/exhaustive agreement, and production
  original-equation gate checks.
- `build/release/suitesparse-request-conditioned-route-final-heldout-v6-first-run/`:
  unique prefrozen v6 SuiteSparse run over three unseen collection groups and 24
  requests. Control-aware regret is `1.000030794×`, `0.109%` below global fixed and
  `43.1%` below static, with zero correctness, plan-order, or DP mismatch.
- `build/release/suitesparse-control-aware-replay-v5/`: zero-execution replay over
  immutable v5 observations. It switches 0/24 requests and remains `1.718371×` global
  fixed, proving that the current guard does not generally repair the v5 failure.
- `kb/closest-work-refresh-2026-07-27.md`: public-safe dated search showing that
  dynamic/request-conditioned solver selection is prior art and narrowing novelty to
  verified complete-path cascade composition.
- `build/release/complete-cost-decomposition/evidence.txt`: two-family layered candidate,
  correction/runtime-gate, gate-kernel, complete-path decomposition, and the production
  correction-budget frontier.
- `build/release/phase5/` and `build/release/nonlinear-operator/`: held-out learned-
  operator studies.
- `build/release/operator-shared-baseline/evidence.txt`: common candidate/correction/
  strict-gate/numerical-continuation control.
- `build/release/hints-native-baseline/evidence.txt`: official HINTS code, DeepONet,
  pretrained weights, complete 750-case 1D Poisson test set, common original-equation
  residual, and paired complete-path comparison against the default production Router.
- `build/release/gate-architecture/evidence.txt`: strict-gate fusion.
- `build/release/gate-parallel-scaling/evidence.txt`: threaded linear/nonlinear gate
  scaling with decision/residual equivalence.
- `build/release/parallel-scaling/evidence.txt` and `batch-scaling/evidence.txt`:
  complete-path scaling and batch amortization.
- `build/release/data-lock/evidence.txt`: consumed benchmark byte locks and provenance.

## Reproduction

```bash
cmake --preset release
cmake --build --preset release -j
ctest --test-dir build/release --output-on-failure
cmake --build build/release --target reproduce-cascade-ordering
cmake --build build/release --target reproduce-router-shift
cmake --build build/release --target reproduce-router-shift-matrix
cmake --build build/release --target reproduce-calibrated-correction-router
cmake --build build/release --target reproduce-joint-route-scaling-round51
cmake --build build/release --target reproduce-frozen-interaction-prevalence-round52
cmake --build build/release --target reproduce-frozen-transition-attrition-round53
cmake --build build/release --target reproduce-joint-route-budget-shift
cmake --build build/release --target reproduce-request-conditioned-joint-route
cmake --build build/release --target reproduce-suitesparse-request-conditioned-route
cmake --build build/release --target reproduce-complete-cost-decomposition
cmake --build build/release --target reproduce-gate-parallel-scaling
cmake --build build/release --target reproduce-operator-shared-baseline
cmake --build build/release --target reproduce-hints-schedule-baseline
# Requires a pinned official HINTS checkout and compatible PyTorch environment:
cmake --build build/release --target reproduce-hints-native-baseline
benchmark/run_pdebench_repeated_timing.sh build/release 30
benchmark/run_pdebench_order_sensitivity.sh build/release 30
python3 paper/check_evidence.py
python3 paper/check_artifact_manifest.py
paper/check.sh
```

The deterministic core bundle is created and verified with:

```bash
python3 artifact/make_core_repro_bundle.py
python3 artifact/verify_core_repro_bundle.py \
  build/core-repro-bundle/smave-core-repro.tar.gz
```

## Current Review Boundary

The current review evaluates only solver innovation and evidence. The correction-budget
frontier, complete-cost decomposition, size/conditioning/topology routing shifts,
held-out joint expert--budget calibration, request-conditioned cost/pass prediction,
exact interaction-DP state preflight, post-hoc frozen interaction-support shift and
training-plan attrition,
stronger closest-work positioning, and the original-equation acceptance contract
determine scientific movement.

The current results remain primarily measured on one Apple M4 host, which limits
performance generalization but does not change the solver's correctness contract. The
native HINTS rerun additionally requires external public code and a compatible PyTorch
environment; the core archive freezes that evidence instead of installing or executing
the external dependency. Large benchmark payloads are not embedded in the core archive,
and author/disclosure metadata remains unfinished.
