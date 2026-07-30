# CCF-A Full Review — Round 44

## 1. Report Metadata

- **Mode:** post-v5 full scientific, evidence, writing, and artifact review.
- **Target venue assumption:** IEEE Transactions on Parallel and Distributed Systems.
- **Review date:** 2026-07-27.
- **New official evidence:** unique v5 first run on three prefrozen, group-disjoint SuiteSparse matrices.

## 2. Result Summary

The v5 first run completed successfully and passed all numerical contracts: 24/24 production successes, zero production failures, zero original-equation gate mismatches, zero plan-order mismatches, and zero DP/exhaustive mismatches. The evidence therefore strongly supports solver safety and optimizer correctness.

Performance is a decisive negative result. Conditioned regret is `4.48997`, compared with static `2.39314`, global fixed SuperLU `2.61292`, and family-fixed `4.48997`. The calibrated adaptation gate enables zero families, so conditioned routing exactly reduces to the training-selected family anchors. The symmetric-indefinite anchor `gmres-ilu0@100` is rejected on all eight `rail_5177` requests and adds terminal continuation cost. The global fixed SuperLU action succeeds directly.

## 3. Root-Cause Assessment

The current gate controls request-level deviation from each family anchor, but it does not validate the family anchor itself against the global fixed baseline. SPD and symmetric-indefinite each have only one independent calibration group. Their family-specific anchors therefore enter production without the same two-group, no-regression, 5% gain standard imposed on adaptive deviations. This is a methodological asymmetry and the direct source of v5 degradation.

The principled repair is a calibration-supported family-anchor gate:

1. Select the global fixed action and provisional family actions using training data only.
2. For every provisional family action that differs from global fixed, require at least two independent calibration matrices.
3. Require no calibration-group regression and at least 5% equal-group-weighted mean gain over global fixed.
4. Otherwise replace that family anchor with global fixed.
5. Keep held-out data completely excluded from selection and calibration.

This repair would have been motivated independently of v5 by the existing two-group rule for adaptive deviations; v5 demonstrates that the rule must also cover the anchor layer.

## 4. Scorecard

| Dimension | Score | Confidence | Evidence basis | Deduction and repair condition |
| --- | ---: | ---: | --- | --- |
| Contribution and novelty | 4/5 | 5/5 | Complete-cost verified routing remains distinctive | Robust policy selection is incomplete |
| Significance and impact | 3/5 | 5/5 | Important problem, but v5 loses to simple controls | Demonstrate non-inferiority under untouched validation |
| Technical soundness | 4/5 | 5/5 | Numerical contracts pass; anchor-selection asymmetry is real | Apply calibration support to anchor selection |
| Evidence and evaluation | 5/5 | 5/5 | Prefrozen v5 exposes a genuine negative result with strong controls | No deduction for honesty or coverage |
| Clarity and organization | 4/5 | 5/5 | Failure is diagnosable from retained traces | Manuscript must disclose v5 and revised method |
| Positioning and related work | 4/5 | 4/5 | Framing remains coherent | Closest-work refresh remains pending |
| Reproducibility and auditability | 5/5 | 5/5 | v4 failure and v5 negative result are immutable and hashed | Synchronize final artifacts after revision |
| Ethics and limitations | 5/5 | 5/5 | Negative evidence and boundaries are retained | No deduction |

**Overall:** 7/10  | **Scholarly Confidence:** 5/5

**Recommendation:** weak accept

**Verdict:** The paper is correct and unusually auditable, but the current production policy is not empirically competitive. A calibration-supported anchor guard plus group-disjoint development validation can restore 8/10; a new untouched non-inferior result is required for 9/10 or higher.

## 5. Multi-Reviewer Panel

### Reviewer A — Numerical Methods
- **Score tendency:** 8/10.
- **Confidence:** 5/5.
- **Positive signal:** every returned solution passes the original-equation gate.
- **Negative signal:** family-anchor selection has an unsupported generalization step.
- **Score-change condition:** symmetrical calibration rules for anchors and deviations.

### Reviewer B — Empirical Evaluation
- **Score tendency:** 6/10.
- **Confidence:** 5/5.
- **Positive signal:** untouched evidence and strong controls are present.
- **Negative signal:** conditioned routing is `1.87×` the static regret and `1.72×` the fixed regret.
- **Score-change condition:** non-inferiority on a new frozen cohort.

### Reviewer C — Reproducibility
- **Score tendency:** 9/10 for artifact discipline, 7/10 overall.
- **Confidence:** 5/5.
- **Positive signal:** failed and negative runs are preserved rather than overwritten.
- **Negative signal:** the verifier initially lagged model schema v2 and required a post-run mechanical update.
- **Score-change condition:** freeze verifier schema and evidence executable together before the next run.

### Reviewer D — Clarity And Scope
- **Score tendency:** 7/10.
- **Confidence:** 5/5.
- **Positive signal:** the negative result has a precise control-flow explanation.
- **Negative signal:** current manuscript numbers still describe v3.
- **Score-change condition:** synchronize method, v5 result, and limitations honestly.

### Panel Synthesis

- **Agreement:** v5 is a valid negative result and invalidates any current routing-dominance claim.
- **Disagreement:** reviewers differ on whether audit quality offsets empirical weakness.
- **Decisive accept axis:** correctness, exact optimization, and evidence integrity.
- **Decisive reject axis:** unsupported family-anchor specialization.
- **Final calibrated stance:** 7/10 weak accept; revise the anchor gate using development data only.

## 6. Concern-To-Action Table

| Priority | Concern | Action | Success condition |
| --- | --- | --- | --- |
| P0 | Unsupported family anchors | Add two-group/no-regression/5%-gain calibration gate against global fixed | Unsupported families use global fixed |
| P0 | Regression risk | Run all three group-disjoint development folds | Guarded policy is no worse than global fixed on each fold |
| P1 | Model/verifier drift | Verify schema v2 and 15 features in the frozen verifier | Clean evidence verification passes |
| P1 | Manuscript staleness | Report v5 as a negative result and revised guard | No routing-dominance language remains |
| P1 | New unseen evidence | Freeze a cohort excluding v1--v5 after method freeze | One immutable first run tests the revision |

## 7. Score-Change Conditions

| Change | Condition | Likely dimensions | Expected movement |
| --- | --- | --- | --- |
| Raise | Guard is non-inferior to global fixed across all development folds | Soundness, evidence | +1 overall |
| Raise | New untouched cohort is non-inferior to fixed and static with zero mismatch | Impact, evidence, reproducibility | +1 further |
| Raise to 10 | Broad material gain, closest-work audit, final artifact/PDF verification, and no unresolved submission issue | All dimensions | only then 10/10 |
| Lower | Guard is chosen using v5 requests or v5 is rerun | Evidence integrity | reject adaptive claim |

## 8. Output Self-Check

- The overall score is lowered in response to the strongest unresolved empirical concern.
- v5 metrics are treated as negative evidence, not tuned-away noise.
- The proposed repair uses training and calibration only.
- No acceptance probability or unsupported score guarantee is claimed.
