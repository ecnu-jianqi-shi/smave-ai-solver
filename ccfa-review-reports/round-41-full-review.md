# CCF-A Full Review — Round 41

## 1. Report Metadata

- **Mode:** full scientific, writing, format, and artifact review.
- **Target venue assumption:** IEEE Transactions on Parallel and Distributed Systems.
- **Review date:** 2026-07-27.
- **Manuscript:** `paper/main.tex` and its included sections.
- **Official evidence basis:** the preserved SuiteSparse v3 first-run evidence; it was not regenerated or overwritten.
- **New evidence basis:** interaction-aware development runs in newly created `build/release/` directories.
- **Scope:** solver innovation, numerical correctness, complete-path cost, request-conditioned routing, correction, original-equation gate, and numerical continuation only.

## 2. Desk And Scope Assessment

No desk-rejection condition is visible. The manuscript remains a numerical-solver paper. A gate rejection followed by another numerical path is numerical continuation inside one solve request; it is not service failover, host switching, process isolation, or high availability. Repository-wide scope checks found none of those infrastructure claims in the active goal, knowledge base, manuscript, or Round 40 review.

The official manuscript numbers remain unchanged because the new experiments are development-only and the method is not frozen.

## 3. Paper Summary

The paper models heterogeneous repeated solves as verified expert cascades. It accounts for candidate, correction, transfer, original-equation verification, rejected attempts, numerical continuation, and terminal solver cost. It derives fixed-statistic ordering, implements exact bounded expert--budget optimization, and requires every returned solution to pass a family-specific original-equation numerical contract.

## 4. Round 41 Method And Evidence Audit

Round 41 added pairwise conditional action-cost calibration and an exact interaction-aware cascade optimizer whose state contains the used experts, previous action, and selected depth. Pair selection uses only failed adjacent transitions in training plans; held-out requests never select or calibrate transitions. Empty interaction tables reduce exactly to the independent-cost dynamic program.

The review found and fixed a method-consistency defect: the interaction-aware dynamic program used conditional upper multipliers, but the subsequent family-anchor abstention guard evaluated the chosen plan with independent conservative costs. The guard now applies the same conditional upper multipliers. Routing unit tests and the solve-service unit test pass after the fix.

Direct decision-level ablation shows that the new component is not yet an empirical contribution:

| Development evidence | Calibrations | Plans changed versus independent DP | Conditioned without interactions | Conditioned with interactions | Family-fixed |
| --- | ---: | ---: | ---: | ---: | ---: |
| v1 run2, before guard consistency fix | 4 | 1/48 | 2.8740496 | 2.8740913 | 2.8245134 |
| v1 guarded run1 | 2 | 0/48 | 2.7797623 | 2.7797623 | 1.8894580 |
| v2 run1 | 0 | 0/48 by degenerate control | 1.1975785 | 1.1975785 | 1.1975785 |
| v3 run1 | 0 | 0/48 by degenerate control | 1.1125842 | 1.1125842 | 1.1146169 |

The first v1 ablation changed one plan but slightly increased complete-path cost. After the guard fix, learned transitions were unreachable in all held-out plans. Folds v2 and v3 had no failed adjacent transition supported by at least two training matrices. Thus pairwise conditional optimization is technically sound but currently sparse, unstable, and non-decisive.

All reported development runs retain zero DP/exhaustive mismatch, zero production failure, zero original-equation gate mismatch, and zero plan-order mismatch. These results support correctness, not superiority.

## 5. Quantitative Scorecard

| Dimension | Score | Confidence | Evidence basis | Deduction and repair condition |
| --- | ---: | ---: | --- | --- |
| Contribution and novelty | 4/5 | 5/5 | Complete-cost verified cascades and exact bounded routing remain nontrivial | Interaction-aware extension has no demonstrated decision benefit; require reachable, stable interaction effects on unseen groups |
| Significance and impact | 4/5 | 5/5 | Solver selection under numerical correctness is important | Current strongest adaptive result still does not stably dominate simple family-fixed routing |
| Technical soundness | 4/5 | 5/5 | Exact DP/exhaustive agreement and zero gate/order mismatch | Guard inconsistency was real but is fixed; pair calibration has only two-matrix support in v1 and no support in two folds |
| Evidence and evaluation | 3/5 | 5/5 | Three group-disjoint development folds, strong controls, retained negative results | Three timing repetitions permit large run-to-run control drift; use paired/interleaved terminal and action measurements, then repeat whole runs |
| Clarity and organization | 4/5 | 5/5 | Manuscript states the official negative routing result and qualified claims | Do not add interaction-positive language until the decision ablation becomes positive |
| Positioning and related work | 4/5 | 4/5 | Algorithm-selection and learned-solver context are present | No new public novelty search was needed for this implementation-only round; closest conditional cascade literature remains a submission-time audit item |
| Reproducibility and auditability | 5/5 | 5/5 | First runs preserved, new directories unique, raw conditional observations written | Add direct interaction decision counters to any future frozen artifact verifier |
| Ethics and limitations | 5/5 | 5/5 | No human data or safety deployment claim; negative results are explicit | Continue restricting conclusions to observed hardware and workloads |

**Overall:** 8/10. **Recommendation:** accept. **Confidence:** 5/5.

The score does not increase because the new interaction mechanism does not improve a reachable held-out decision and timing variance is large enough to change the apparent family-fixed margin. Full score is not supported.

## 6. Major Concerns

### C1 — Conditional interactions are mostly unreachable

- **Severity:** major for any interaction-aware contribution claim.
- **Evidence:** only v1 learns calibrations; the guarded run uses none in held-out plans, while v2/v3 learn none.
- **Consequence:** the optimizer is an implemented capability, not a validated source of routing gain.
- **Repair condition:** collect interaction evidence for training-selected transitions with broader independent-matrix support, or remove interaction improvement from the claimed contribution while retaining the optimizer as a limitation-aware extension.

### C2 — Cost measurement is not paired tightly enough

- **Severity:** major for adaptive-versus-fixed comparisons.
- **Evidence:** v1 family-fixed regret varies from 2.82 in earlier runs to 1.89 in the guarded run; model logic alone cannot explain that control drift.
- **Consequence:** three repetitions and separately timed terminal solves do not isolate action-to-terminal ratios from temporal load and cache drift.
- **Repair condition:** interleave each action attempt with its same-request terminal reference in a counterbalanced block, compute per-pair ratios, and aggregate at matrix level; retain multiple whole-run replications.

### C3 — Absolute prediction tails remain uncontrolled

- **Severity:** major for a full-score robust-routing claim.
- **Evidence:** development p95/max relative errors remain very large and vary materially between runs.
- **Consequence:** point-prediction quality is insufficient to justify context-sensitive deviations from stable anchors.
- **Repair condition:** train and evaluate matrix-level paired log-ratios with group-aware uncertainty, then show materially lower cross-fold p95/max without sacrificing complete-path control performance.

## 7. Writing And Presentation Review

The manuscript should not be rewritten around the new interaction optimizer. Its current official statement—that adaptive routing loses to fixed SuperLU and family-fixed routing—is still the correct decision-level result. Future prose may describe conditional costs only after a frozen unseen evaluation demonstrates that they alter reachable plans beneficially.

Terminology remains solver-specific: `gate` means original-equation numerical acceptance; `fallback` is a legacy field name for terminal numerical continuation. No infrastructure language should be introduced.

## 8. Multi-Reviewer Panel

### Reviewer A — Numerical Algorithms

- **Likely score:** 8/10.
- **Confidence:** 5/5.
- **Positive signal:** exact interaction-aware optimization and original-equation correctness are sound.
- **Negative signal:** calibrated interactions do not produce a beneficial reachable decision.
- **Score-change condition:** stable unseen complete-cost gain with zero numerical mismatch.

### Reviewer B — Empirical Methodology

- **Likely score:** 7/10.
- **Confidence:** 5/5.
- **Positive signal:** negative runs and raw observations are preserved.
- **Negative signal:** timing drift changes strong-control margins and overwhelms small routing gains.
- **Score-change condition:** counterbalanced paired ratios and repeated whole-run confidence intervals.

### Reviewer C — Systems And Reproducibility

- **Likely score:** 8/10.
- **Confidence:** 5/5.
- **Positive signal:** unique experiment directories, explicit development-only status, and executable contracts.
- **Negative signal:** no new frozen unseen result and no verifier yet requires interaction decision counters.
- **Score-change condition:** freeze only after method stability and extend the artifact verifier.

### Reviewer D — Clarity And Scope

- **Likely score:** 8/10.
- **Confidence:** 5/5.
- **Positive signal:** the paper's solver objective and numerical continuation semantics are recoverable.
- **Negative signal:** interaction language would overstate the present evidence if added now.
- **Score-change condition:** align every new sentence with a frozen claim--evidence row.

### Panel Synthesis

- **Agreement:** numerical correctness and auditability are strong; interaction superiority is unproven.
- **Disagreement:** Reviewer B would lower the stance until timing stability improves, while the other reviewers retain accept because the official paper already reports the negative routing result.
- **Decisive accept axis:** verified complete-cost formulation, exact optimization, and honest evidence.
- **Decisive reject axis for 9–10/10:** no stable adaptive advantage over strong controls under reliable paired measurement.
- **Unresolved evidence:** matrix-level effective sample size, timing tails, reachable interactions, and a genuinely unseen frozen evaluation.
- **AC stance:** 8/10 accept, confidence 5/5; do not freeze v4.

## 9. Concern-To-Action Table

| Priority | Concern | Action | Success condition |
| --- | --- | --- | --- |
| P0 | Temporal drift in relative costs | Interleave same-request action and terminal measurements with counterbalanced order | Whole-run control margins and paired ratio tails become stable |
| P0 | Sparse interaction support | Report reachability and decision-change counters; broaden training-group support without held-out selection | Interactions change and improve at least one held-out plan across supported folds |
| P0 | Large matrix-level tails | Fit and calibrate group-level paired log-ratios | Material p95/max reduction on all development folds |
| P1 | No new decision-level evidence | After method stability, create a unique unseen data lock and exactly one first run | Adaptive policy is no worse than both fixed controls with zero correctness mismatch |

## 10. Score-Change Conditions

| Change | Condition | Likely dimensions | Expected movement |
| --- | --- | --- | --- |
| Raise | Stable counterbalanced paired-cost evidence plus unseen superiority over fixed and family-fixed controls | Evidence, soundness, impact | +1 overall |
| Raise | New unseen result, strong external learned/hybrid control, and submission-complete artifact | Novelty, evidence, reproducibility | +1 overall; 10 still requires an exceptional contribution case |
| Lower | Interaction-positive prose without reachable-plan evidence | Soundness, clarity | -1 overall |
| Lower | Reusing or overwriting an inspected held-out first run | Evidence, reproducibility | fatal to the empirical claim |

## 11. Checks Run

- Built `smave_suitesparse_request_conditioned_route_evidence`, `smave_tests`, and `smave_solve_service_unit` in Release mode.
- Passed `SMAVE_TEST_FILTER=cascade-ordering build/release/smave_tests`.
- Passed `ctest --test-dir build/release -R '^smave_solve_service_unit$'`.
- Ran new v1/v2/v3 interaction-aware development directories and an additional guarded v1 run.
- Confirmed zero DP/exhaustive, production failure, gate, and plan-order mismatches in the cited evidence.
- Confirmed the official v3 first-run directory was not used as an output.

## 12. Unresolved Or Unverified

- No new frozen unseen cohort has been selected or run.
- No paper, PDF, generated value, or artifact manifest has been updated.
- Full Release `smave_unit` was not rerun in this round because its existing macOS FMI `dlopen` path can stall; focused routing and solve-service tests passed.
- Related-work novelty was not re-searched because this round changes empirical method mechanics rather than the manuscript's novelty claim.

## 13. Output Self-Check

- Scores, evidence, and recommendation are consistent.
- Every score of 3/5 has a concrete deduction and repair condition.
- New negative interaction results are retained.
- No infrastructure requirement is introduced.
- No full-score or method-stability claim is made.
