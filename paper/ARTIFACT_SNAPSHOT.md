# Reviewed Artifact Snapshot

This file records the local solver artifact used for the 2026-07-29 Round 53
CCF-A review. It is a reproducibility aid, not a public archival identifier or an
independent reproduction.

## Host and Toolchain

- Host: Apple M4, arm64.
- Operating system: macOS 26.5.2, Darwin 25.5.0.
- Compiler: Apple Clang 21.0.0.
- Build system: CMake 4.3.2, Release configuration.
- Analysis runtime: Python 3.9.6.
- Manuscript toolchain: Latexmk 4.87 and TeX Live 2026 pdfTeX.
- CPU numerical backend: Apple Accelerate where selected by the benchmark.

## Solver Evidence Frozen for Review

- Repeated PDE run:
  `build/release/pdebench-repeated-timing/20260724T045454Z`.
- Repeated PDE contract: 30 paired independent runs for each of seven workloads,
  210 measured reports plus seven warm-up reports.
- Order-sensitivity run:
  `build/release/pdebench-order-sensitivity/20260724T035555Z`.
- Order-sensitivity contract: 30 matched classical-first/SMAVE-first pairs for each of
  seven workloads, 420 measured reports plus 14 warm-up reports.
- Cascade-ordering evidence: `build/release/cascade-ordering/evidence.txt`; four fixed
  eligible stages, all 24 permutations, terminal continuation cost 10, and exact optimum
  agreement at order `c,b,d,a`.
- Calibrated router evidence:
  `build/release/phase4/family-router-evaluation.txt`.
- Router size/fingerprint-shift evidence: `build/release/router-shift/evidence.txt`.
- Router conditioning/topology-shift evidence:
  `build/release/router-shift-matrix/evidence.txt`.
- Calibrated correction-budget propagation evidence:
  `build/release/calibrated-correction-router/evidence.txt`.
- Exact joint expert--budget routing evidence:
  `build/release/joint-route-budget/evidence.txt`; the production-path case and 256
  deterministic six-action property cases match an independent exhaustive oracle with
  zero maximum gap. A second 256-case sweep covers six actions and all 24 eligible
  adjacent transitions, changes 126 independent plans, and also has zero maximum gap.
  All 4,096 directed four-vertex graphs match the Hamiltonian-path threshold used by
  the NP-completeness reduction. Exactness requires adjacent-Markov costs,
  history-independent acceptance, a sufficient state cap, and at most 63 experts.
- Prefrozen exact-DP tractability evidence:
  `build/release/joint-route-scaling-round51/evidence.txt`; the deployed 12-action shape
  visits 49 states/204 recursive transitions, the largest executed 32-action stress
  profile visits 158,209 states/1,412,192 transitions, and exact preflight rejects the
  44-action/1,227,425-state profile before any DP-state visit under the one-million-state cap.
  This planning-only study executes no numerical solver and makes no timing claim.
- Post-hoc frozen interaction-support audit:
  `build/release/frozen-interaction-prevalence-round52/evidence.txt`; 32 ordered
  development-supported isolated-failure pairs produce zero plan-gated candidates and
  zero conditional timings, with zero overlap into either held-out pair set. This is a
  zero-execution support-shift diagnosis, not conditional calibration or a gain claim.
- Frozen transition-attrition audit:
  `build/release/frozen-transition-attrition-round53/evidence.txt`; both frozen models
  reconstruct byte-for-byte, all 32 supported pairs disappear at unguarded top-3
  selection, five different unsupported candidates occur per version, and the exact
  control-aware route leaves zero final candidates. The audit reads training observations
  only, executes no solver, and infers no timing, multiplier, or gain.
- Held-out joint expert--budget shift evidence:
  `build/release/joint-route-budget-shift/evidence.txt`; two 12-variable nonlinear
  families, 32 training and 32 disjoint held-out scenarios per family, selected budgets
  `2` and `4`, maximum held-out regret `1.000×`, and zero gate mismatches.
- Request-conditioned joint expert--budget evidence:
  `build/release/request-conditioned-joint-route/evidence.txt`; three nonlinear
  families, 12 actions, 192/96/192 training/calibration/held-out requests, frozen model
  parameters, 5,760 action observations, conditioned regret `1.178×` versus `1.451×`
  for both controls, and zero production gate mismatch.
- Public SuiteSparse final-routing evidence:
  `build/release/suitesparse-request-conditioned-route-final-heldout-v6-first-run/`;
  6/4/3 train/calibration/held-out matrices, three unseen collection groups, 19
  candidate actions plus a terminal predictor, five repetitions per compatible action,
  conditioned/static/fixed/family-fixed regret `1.000030794×`/`1.756464745×`/
  `1.001122945×`/`1.000030794×`, 24/24 production successes, eight terminal
  continuations, and zero gate/order/DP mismatch. No family specialization, family
  adaptation, or interaction transition is selected.
- Frozen-observation v5 replay:
  `build/release/suitesparse-control-aware-replay-v5/`; the current guard switches
  0/24 requests and remains `4.489971760×`, or `1.718370902×` global fixed, with
  `solver_reexecution=0`. V5 and v6 selection, payload, final, and pre-first-run
  contracts are frozen under `benchmark/data-lock/`; neither first-run directory may
  be overwritten.
- Two-family layered bottleneck evidence:
  `build/release/complete-cost-decomposition/evidence.txt`; it also records the rotated
  production correction-budget sweep over `0,1,2,4,8,16,32`.
- Linear/nonlinear operator evidence: `build/release/phase5/` and
  `build/release/nonlinear-operator/`.
- Shared operator-control evidence:
  `build/release/operator-shared-baseline/evidence.txt`.
- Native HINTS evidence: `build/release/hints-native-baseline/evidence.txt`, with
  official implementation evidence and raw samples under `official/`, production
  Router evidence and raw samples under `smave/`, and the exact exported 750-case
  workload contract under `workload/`.
- Threaded gate evidence: `build/release/gate-parallel-scaling/evidence.txt`.
- Complete-path and batch evidence: `build/release/parallel-scaling/evidence.txt` and
  `build/release/batch-scaling/evidence.txt`.
- Consumed-data lock: `build/release/data-lock/evidence.txt`.

## Solver Evidence Boundary

- This snapshot freezes solver mechanisms, numerical acceptance, routing, complete-path
  cost, performance, and reproducibility evidence.
- Repository regression tests outside that evidence set do not define the manuscript's
  scientific priorities.

## Portability Boundary

- Ubuntu 24.04 ARM64 and emulated x86-64 environments pass the correctness suite.
- These runs establish build/API/test portability only.
- All authoritative manuscript timing remains from one Apple M4 system.
- The GitHub-hosted native-performance workflow is implemented, but no hosted run
  artifact is frozen in this snapshot; local dry-run flags remain `performance_evidence=0` and
  `native_external_performance=0`.

## Deterministic Core Bundle

- Creation: `python3 artifact/make_core_repro_bundle.py`.
- Verification: `python3 artifact/verify_core_repro_bundle.py
  build/core-repro-bundle/smave-core-repro.tar.gz`.
- Contract: normalized path order, mtime, UID, and GID; two in-process generations must
  be byte-identical; an internal manifest hashes every file.
- Clean extraction configures and builds Release, requires 29/29 CTests, reruns the
  exhaustive cascade-ordering, conditioning/topology shift, calibrated-budget,
  held-out joint expert--budget, request-conditioned 12-action, and prefrozen exact-DP
  scaling targets, restores the
  frozen size-shift/decomposition inputs, reruns those analyses, verifies the local-only
  native-performance package and synthetic campaign contract, restores the reviewed
  manuscript's frozen generated values after local timing probes, checks paper
  evidence/manifest, and rebuilds the PDF.
- Included test data: `west0479`, `nasa2910`, `laser`, `M10PI_n1`, and `TS`, the
  Matrix Market fixtures used by CTest.
- Excluded data: approximately 47 GB PDEBench and 5.1 GB non-fixture SuiteSparse data.
- The official HINTS checkout and PyTorch environment are external dependencies. The
  core bundle freezes their evidence and raw timing samples but does not install or
  execute them (`external_public_code_required=1`, `core_bundle_rerun=0`).
- Machine record: `build/core-repro-bundle/clean-tree-evidence.txt`; clean verifier passed
  29/29 CTests and all downstream checks.
- Deterministic archive: `build/core-repro-bundle/smave-core-repro.tar.gz`; two
  generated archives were byte-identical, with the final digest recorded in the
  external `.sha256` sidecar and clean-tree evidence. The bundle includes five CTest
  matrices and excludes 61 non-fixture SuiteSparse matrices.
- The current validation includes the native HINTS verifier, correction-budget frontier,
  exhaustive 24-permutation ordering check, 256 independent and 256 interaction
  property cases, 4,096 hardness-reduction graphs, frozen v6 route verifier,
  deterministic zero-execution v5 replay, the Round 52 frozen interaction-support
  audit, the Round 53 frozen transition-attrition audit, 29/29 CTests, and paper checks.
- The synchronized PDF has 12 pages and 324,260 bytes; SHA-256 is
  `5caeea636b505e2aeb668cc6805b0c15579b2910e0c3ee475caff2cfa2e08542`.

## Reproduction Commands

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
cmake --build build/release --target reproduce-hints-native-baseline
benchmark/run_pdebench_repeated_timing.sh build/release 30
benchmark/run_pdebench_order_sensitivity.sh build/release 30
paper/check.sh
python3 artifact/make_core_repro_bundle.py
python3 artifact/verify_core_repro_bundle.py \
  build/core-repro-bundle/smave-core-repro.tar.gz
```

## Remaining Solver-Relevant Limits

- The native HINTS comparison covers one official 1D Poisson configuration; broader
  HINTS configurations and additional published hybrid solvers remain open.
- Routing calibration covers size, conditioning, and topology shifts in evaluated sparse
  families, not equation-family, precision, or hardware transfer.
- Native CUDA/discrete-GPU, NUMA, additional architectures, and customer workloads
  remain unmeasured.
- Public archival release and third-party reruns remain reproducibility work, not
  solver contributions or score gates.
- Author, affiliation, funding, conflict, and acknowledgment metadata remain placeholders.
