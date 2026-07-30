# CCF-A Full Review — Round 40

## 1. Report Metadata

- **Mode:** full scientific, writing, format, and artifact review.
- **Target venue assumption:** IEEE Transactions on Parallel and Distributed Systems.
- **Review date:** 2026-07-27.
- **Manuscript:** `paper/main.tex` and its included sections.
- **Official evidence basis:** the preserved SuiteSparse v3 first-run evidence reviewed in
  Round 39; it was not regenerated or overwritten.
- **New evidence basis:** three `development_only=1`, matrix-ID-disjoint and
  collection-group-disjoint robust-routing folds under `build/release/`.
- **Scope:** numerical solver innovation, numerical correctness, complete-path cost,
  solver-internal parallelism, heterogeneous placement, baselines, and reproducibility.
- **Scoring boundary:** every deduction or repair condition must map to a solver claim,
  numerical-validity contract, complete-path experiment, or reproducibility requirement.
  Gate rejection followed by another numerical path is numerical continuation within one
  solve request.

## 2. Desk Rejection Assessment

No new desk-rejection condition is visible in the manuscript. The paper remains technically
coherent, uses a consistent original-equation acceptance contract, exposes its negative v3
result, and preserves the official first-run evidence.

Round 40 does not promote the development folds to manuscript evidence. They were created
after inspection of the earlier cohorts, are explicitly marked development-only, and therefore
can guide method design but cannot support a new held-out claim.

## 3. Paper Summary And Contribution Map

The paper studies heterogeneous repeated numerical solves as complete-cost expert cascades.
It contributes:

1. reach-weighted complete-cost accounting over candidate, transfer, correction, original-
   equation acceptance, rejection, and numerical continuation;
2. a cost-per-acceptance ordering result for fixed eligible actions with order-invariant
   statistics;
3. exact bounded dynamic programming for expert--budget selection with at most one budget per
   expert;
4. a typed runtime in which every successful result satisfies a family-specific original-
   equation numerical contract;
5. paired runtime, public workload, negative-result, and artifact evidence.

The official v3 adaptive-routing result remains negative: conditioned routing barely improves
the static profile and loses to fixed SuperLU and a training-selected family-fixed policy.

## 4. Round 40 Method Advance

Round 40 improved the development implementation without changing the official paper claim:

- added action-specific one-sided cost-error bounds;
- added finite-sample pass-probability uncertainty;
- added action-specific feature-support bounds and extrapolation penalties;
- added a minimum robust gain threshold before deviating from a family anchor;
- allowed the terminal numerical cascade to be selected as a fair family-fixed anchor;
- added contract tests for OOD abstention, robust in-support gain, terminal anchors, invalid
  uncertainty/support fields, and invalid gain thresholds;
- parameterized development splits while preserving matrix-ID and collection-group
  disjointness;
- recorded the robust method contract in machine evidence.

These changes improve method clarity and baseline fairness. They do not yet establish a
decision-level performance advance.

## 5. Development Fold Results

| Fold | Conditioned | Static | Fixed | Family-Fixed | Cost p95 / max | Interaction / order |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| v1 held out | 3.801120 | 5.959689 | 4.822510 | **1.821011** | 35.796921 / 44.598274 | 0.523768 / 0.145295 |
| v2 held out | 1.390884 | 1.430348 | **1.307550** | 1.483069 | 15.974420 / 149.361217 | 0.425810 / 0.430987 |
| v3 held out | 1.142979 | 2.057882 | 1.692246 | **1.128559** | 754.534800 / 2075.944812 | 0.773575 / 0.395810 |

All three folds have 48/48 production successes, zero production failures, zero original-gate
mismatches, zero plan-order mismatches, and zero DP/exhaustive mismatches. Numerical
continuation counts are 26, 24, and 16.

The cross-fold conclusion is negative:

1. conditioned routing loses substantially to family-fixed on v1;
2. it loses to the global fixed action on v2;
3. it remains slightly worse than family-fixed on v3;
4. prediction tails remain severe and are catastrophic on v3;
5. measured action interactions and order effects remain too large for an independence-based
   decision claim.

Therefore no v4 unseen collection may be frozen from this method state.

## 6. Likely Stance And Calibration

- **Overall score:** **8/10, accept**.
- **Confidence:** **5/5**.
- **Score movement from Round 39:** none.

The score does not rise because the new robust route is not stable against strong fixed
controls. The score does not fall because the official manuscript has not hidden or replaced
its negative v3 result, all new folds are labeled development-only, and the numerical
correctness contract remains intact.

## 7. Quantitative Scorecard

| Dimension | Score | Confidence | Evidence basis | Deduction | Repair condition |
| --- | ---: | ---: | --- | --- | --- |
| Novelty | 4/5 | 5/5 | Complete-cost cascade formulation, exact expert--budget DP, verified fusion | Current contextual route is not a stable advance over strong fixed controls | Add a genuinely interaction- or relative-cost-aware solver-selection method with new unseen gains |
| Soundness | 4/5 | 5/5 | Zero gate/DP/order mismatches and explicit assumptions | Action-independence and absolute-cost prediction are contradicted by large measured deltas/tails | Model conditional action cost or prove competitive rankings are invariant under measured interactions |
| Evidence | 4/5 | 5/5 | Frozen official v3 plus three disjoint development folds | One-device timing, small per-family matrix coverage, no successful new unseen method result | Freeze a new family-balanced public set only after stable development cross-validation |
| Significance | 4/5 | 5/5 | Broad typed solver runtime and complete-cost evidence | Adaptive routing does not yet beat the strongest simple controls | Show decision-relevant gains over fixed and family-fixed baselines under symmetric accounting |
| Reproducibility | 5/5 | 5/5 | Machine reports, first-run preservation, data locks, 29/29 tests | No scientific-score deduction | Preserve append-only evidence and later synchronize the artifact manifest after method freeze |
| Writing | 4/5 | 5/5 | Claims and negative results are explicit | Some method surface remains large relative to the decisive routing result | Compress secondary system breadth and foreground the exact solver-selection limitation |

## 8. Top Strengths

1. **Numerical authority remains clear.** Learned or speculative candidates never replace the
   original-equation residual, constraint, consistency, branch, or discretization-defect gate.
2. **Complete cost remains symmetric.** Failed attempts and all later numerical paths are
   charged rather than hidden.
3. **Negative results remain visible.** Neither the official v3 loss nor the three development
   fold losses are converted into a positive claim.
4. **Baseline fairness improved.** A terminal numerical cascade can now be a family-fixed
   anchor when it is truly the lowest training complete cost.
5. **Implementation regressions are controlled.** Release builds and all 29 CTest cases pass.

## 9. Major And Minor Concerns

### Major Concern 1 — Absolute Action Models Do Not Transfer Reliably

- **Severity:** major.
- **Evidence:** conditioned regret is 3.801120 on v1 while family-fixed is 1.821011; v3 cost
  p95/max error is 754.534800/2075.944812.
- **Affected criterion:** novelty, soundness, evidence, significance.
- **Root cause:** the model predicts absolute microsecond costs from very few independent
  matrices per family; repeated RHS/tolerance requests do not replace matrix-level coverage.
- **Required fix:** use paired relative-to-anchor targets, hierarchical family pooling, or a
  method whose competitive deviation is bounded without assuming accurate absolute tails.

### Major Concern 2 — Action Independence Is Empirically Violated

- **Severity:** major.
- **Evidence:** maximum interaction deltas are 0.523768, 0.425810, and 0.773575; maximum order
  deltas are 0.145295, 0.430987, and 0.395810.
- **Affected criterion:** soundness and novelty.
- **Root cause:** setup, caches, allocation, and backend state make later action costs depend on
  the preceding action and order.
- **Required fix:** collect pairwise/sequence observations and optimize a conditional cascade,
  or show that competitive plan rankings are invariant despite measured interactions.

### Major Concern 3 — Anchor Selection Is Itself Unstable

- **Severity:** major.
- **Evidence:** nonsymmetric resolves to the terminal cascade in every development fold, while
  symmetric-indefinite changes among Accelerate QR, GMRES@100, and SuperLU.
- **Affected criterion:** evidence and significance.
- **Root cause:** family-fixed selection uses few independent matrices and noisy absolute time.
- **Required fix:** define a pooled or uncertainty-aware anchor-selection contract and validate
  it across group-disjoint folds before using it as the abstention target.

### Minor Concern — Terminology And Scope

The repository now correctly defines gate rejection as numerical continuation. Future score
conditions and method backlogs must remain traceable to the solver claims and their evidence.

## 10. Claim–Evidence Audit

| Claim | Status | Evidence | Review judgment |
| --- | --- | --- | --- |
| Every successful production result passes the original numerical contract | Supported | 48/48 successes and zero gate mismatch in all three folds | Strong |
| Exact bounded DP matches exhaustive optimization under its model | Supported | Zero DP/exhaustive mismatch in all folds | Strong but conditional on the model |
| Support/tail-aware abstention stabilizes contextual routing | Not supported | Loses to family-fixed on v1 and v3, loses to fixed on v2 | Must remain development-only |
| Absolute action statistics are order-invariant | Contradicted as a broad empirical claim | Interaction/order deltas up to 0.773575/0.430987 | Require explicit limitation or new model |
| Current method justifies a new unseen v4 freeze | Not supported | Cross-fold control losses and prediction tails | Do not freeze |

## 11. Experiment, Baseline, And Reproducibility Audit

- Matrix IDs are disjoint across training, calibration, and held-out in every development fold.
- Collection groups are disjoint across splits.
- Held-out classes remain balanced at two SPD, two symmetric-indefinite, and two nonsymmetric
  matrices per fold.
- Fold v1/v2 calibration was expanded by moving the full `Bai` collection group, because the
  original split lacked calibration observations for one eligible direct action.
- Failed development directories were preserved rather than overwritten.
- The official v3 first-run directory was not touched.
- No new official data lock, payload lock, structure audit, or paper number was created.
- The current paper artifact manifest is intentionally not synchronized during unstable method
  development.

## 12. Writing And Presentation Review

The manuscript already presents the official negative routing result honestly. No major prose
rewrite is justified until a method-level advance exists. The main writing action is scope
discipline:

1. keep the paper centered on solver selection, numerical correctness, and complete-path cost;
2. describe gate rejection only as numerical continuation;
3. require every added claim or limitation to affect the solver method or its evidence;
4. retain the fixed and family-fixed losses at their first decision-level occurrence;
5. if an interaction-aware method is added, revise the ordering theorem boundary before adding
   positive empirical language.

## 13. Multi-Reviewer Panel

### Reviewer A

- **Lens:** numerical algorithms and optimization.
- **Score tendency:** 8/10.
- **Confidence:** 5/5.
- **Positive signal:** exact DP and original-equation verification remain sound.
- **Negative signal:** the route optimizes independent action estimates even though interaction
  and order effects are large.
- **Score-change condition:** a conditional cascade optimizer with new unseen superiority over
  fixed controls.

### Reviewer B

- **Lens:** machine learning and distribution shift.
- **Score tendency:** 7–8/10.
- **Confidence:** 5/5.
- **Positive signal:** matrix/group-disjoint development folds and retained negative results.
- **Negative signal:** repeated request samples over few matrices create effective-sample-size
  inflation; absolute cost tails remain uncontrolled.
- **Score-change condition:** matrix-level hierarchical calibration or robust paired-relative
  learning validated across more independent groups.

### Reviewer C

- **Lens:** systems evidence and reproducibility.
- **Score tendency:** 8/10.
- **Confidence:** 5/5.
- **Positive signal:** complete-cost accounting, preserved first runs, and 29/29 tests.
- **Negative signal:** no new decision-level method result and no cross-architecture evidence.
- **Score-change condition:** a newly frozen public evaluation after the method passes development
  folds; cross-hardware evidence is secondary, not a substitute for the algorithmic result.

### Panel Synthesis

- **Agreement:** the paper remains acceptable and unusually auditable.
- **Disagreement:** Reviewer B is more concerned that matrix-level sample size is too small for
  the current contextual model.
- **Decisive accept axis:** numerical correctness, honest complete-cost accounting, and artifact
  discipline.
- **Decisive reject axis for 9–10/10:** no stable solver-selection gain over strong controls.
- **Unresolved evidence:** interaction-aware routing and larger independent matrix coverage.
- **Final calibrated stance:** **8/10, accept, confidence 5/5**.

## 14. Concern-To-Action Table

| Priority | Concern | Action | Success condition |
| --- | --- | --- | --- |
| P0 | Action interactions invalidate independent costs | Collect conditional pair/sequence costs and implement interaction-aware optimization | Competitive plan ranking remains correct or improves on all development folds |
| P0 | Absolute cost tails dominate decisions | Use paired relative/hierarchical targets with matrix-level uncertainty | Materially lower p95/max and stable control competition |
| P0 | Anchor selection drifts | Add uncertainty-aware pooled anchor selection | Same anchor contract remains competitive across group-disjoint folds |
| P1 | Independent matrix coverage is small | Expand seen development matrices before any new freeze | Stable results across multiple family-balanced cohorts |
| P1 | External learned control remains weak | Add a strong hybrid learned-solver or learned-preconditioner selector | Symmetric complete-cost comparison on public workloads |

## 15. AC / Meta-Review

Round 40 confirms that the project is a numerical solver paper. Its scientific contribution is
determined by solver-selection innovation, numerical correctness, complete-path cost, and the
strength of the evidence connecting them.

The robust abstention implementation is a useful engineering and methodological cleanup, but
the three-fold evidence rejects it as the next headline contribution. A score increase requires
a new solver-selection idea that addresses relative costs and conditional action interactions,
not more artifact polish or peripheral engineering breadth.

The correct decision remains **8/10, accept**. Freezing v4 now would convert inspected
development choices into a nominally unseen result and would not solve the cross-fold failure.

## 16. Score-Change Conditions

| Change | Condition | Expected movement |
| --- | --- | --- |
| Raise to 9/10 | New interaction- or relative-cost-aware method competes with both fixed and family-fixed on a uniquely frozen public set, with zero gate mismatch | +1 overall possible |
| Raise evidence dimension | More independent matrices, another architecture, and a strong external selector without method-level gain | +0.5 dimension; overall likely unchanged |
| Lower score | Hide the negative folds, overwrite first-run evidence, leak held-out groups, weaken the original gate, or use asymmetric complete-cost accounting | -1 or fatal depending on scope |
| No score change | More wording, peripheral engineering, or artifact polish without a new decision-level solver result | none |

## 17. Questions For Authors

1. Can pairwise conditional costs be collected for the small set of actions that appear in
   competitive top-$k$ plans rather than exhaustively for all 19 actions?
2. Can the optimizer use relative-to-anchor cost and pass deltas so hardware load and matrix
   family scale cancel more effectively?
3. What matrix-level effective sample size is required before a family-specific action model is
   allowed to deviate from a pooled anchor?
4. Do the measured interaction deltas change competitive plan rankings, or only realized costs?
5. Can a worst-case speculative-overhead budget bound regret relative to the anchor while still
   allowing successful cheap candidates to win?

## 18. Checks Run

- Release full build: pass.
- Full Release CTest: **29/29 pass**.
- Targeted `smave_unit`: pass after robust-contract and terminal-anchor tests.
- Three development folds: complete, each 48/48 success and zero gate/DP/order mismatch.
- Official v3 first-run directory: not regenerated or overwritten.
- Paper/PDF/manifest synchronization: intentionally not run because no method was frozen.

## 19. Unresolved Or Unverified

- Interaction-aware decision optimization.
- Stable relative-cost prediction under matrix-family shift.
- Larger independent matrix coverage per family.
- New uniquely frozen v4 held-out evidence.
- Stronger external hybrid learned-solver or learned-preconditioner selector.
- Cross-architecture complete-cost timing.
- Current-year venue policy and final author metadata.

## 20. Recommended Next Owner

- **Primary:** solver methods and experimental design for conditional interaction costs,
  paired-relative targets, and uncertainty-aware anchors.
- **Secondary:** development evidence generation on additional seen public matrix groups.
- **Artifact:** preserve current first runs; do not synchronize the paper bundle until a method
  survives cross-validation.
- **Writing:** no broad rewrite; only maintain solver-only terminology and claim discipline.

## 21. Output Self-Check

- Criterion scores, overall score, and confidence are separated.
- Every score deduction has an evidence basis and repair condition.
- Development folds are not represented as unseen manuscript evidence.
- No experiment, baseline, result, citation, or acceptance probability is invented.
- Negative results and failed prototypes remain explicit.
- Every score condition maps to a solver claim or its evidence.
