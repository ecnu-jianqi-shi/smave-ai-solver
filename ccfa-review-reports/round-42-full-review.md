# CCF-A Full Review — Round 42

## 1. Report Metadata

- **Mode:** full scientific, writing, format, and artifact review.
- **Target venue assumption:** IEEE Transactions on Parallel and Distributed Systems.
- **Review date:** 2026-07-27.
- **Manuscript:** `paper/main.tex` and included sections; manuscript numbers remain the preserved official v3 values.
- **New evidence:** five-repetition, same-request action/terminal counterbalanced development folds plus a repeated v3 run.
- **Scope:** numerical solver routing, correction, original-equation acceptance, numerical continuation, complete-path cost, and reproducibility only.

## 2. Desk And Scope Assessment

No desk-rejection or scope mismatch is visible. The project remains a numerical-solver contribution. No high-availability, host-failover, process-isolation, or security-switch requirement is part of the method or score boundary.

No official v3 first-run directory, paper-generated value, PDF, or artifact manifest was overwritten.

## 3. Method Advance Since Round 41

Round 42 implements three solver-method corrections:

1. **Paired counterbalanced cost collection.** Every action repetition and its terminal numerical reference now occur in one same-request block with forward/reverse rotation. Each action training row uses the terminal cost from the same repetition rather than a separately timed request median.
2. **Calibration-level family adaptation gate.** A numeric family may deviate from its training-selected family anchor only when at least two independent calibration matrices are present, no calibration group becomes slower, and the equally weighted group mean improves by at least 5%. Held-out requests never select this gate.
3. **Decision-relevant prediction diagnostics.** The evidence retains the all-action error distribution and separately reports errors for final-plan actions and family anchors.

The interaction-aware optimizer and its guard remain technically correct, but conditional transitions change no final plan in the stable runs. It is therefore not treated as the empirical source of gain.

## 4. Development Evidence

### Five-Repetition Group-Robust Folds

| Fold | Enabled adaptive families | Conditioned | Fixed | Family-fixed | Relation to controls |
| --- | ---: | ---: | ---: | ---: | --- |
| v1 | 0 | 2.8293495 | 4.1071704 | 2.8293495 | ties family-fixed; beats fixed |
| v2 | 0 | 1.1877562 | 1.1936588 | 1.1877562 | ties family-fixed; beats fixed |
| v3 run1 | 1 (`spd`) | 1.1257769 | 1.5240221 | 1.1279073 | beats both controls |
| v3 run2 | 1 (`spd`) | 1.1196837 | 1.5260184 | 1.1222919 | repeats the same qualitative result |

All cited runs have zero DP/exhaustive mismatch, zero production failure, zero original-equation gate mismatch, and zero plan-order mismatch. The v3 improvement over family-fixed is only approximately 0.19--0.23%, so it is evidence of a reproducible direction, not a practically large effect.

### Prediction-Tail Interpretation

The v3 run2 all-action p95 and maximum relative errors are 352.64 and 667.52. The worst rows are unselected `pcg-ic0` actions on `bibd_81_2`, where the model overpredicts roughly 600-fold; these are conservative errors and do not enter the final plan.

For the 48 actions actually selected by final plans, median, p95, and maximum relative errors are 0.715, 2.775, and 3.612. The largest selected underpredictions occur on terminal or family-anchor direct paths, including `heart2` and `sts4098`; the family adaptation gate retains those anchors rather than using the unreliable contextual deviation. Thus raw action tails remain poor, but their decision effect is bounded by explicit abstention.

## 5. Quantitative Scorecard

| Dimension | Score | Confidence | Evidence basis | Deduction and repair condition |
| --- | ---: | ---: | --- | --- |
| Contribution and novelty | 4/5 | 5/5 | Complete-path verified routing plus calibration-gated family adaptation | Positive adaptive effect is confined to one numeric family and is very small |
| Significance and impact | 4/5 | 5/5 | Solver selection under original-equation correctness is decision-relevant | Development evidence does not yet show a material unseen gain |
| Technical soundness | 5/5 | 5/5 | Exact optimizer checks, consistent conditional guard, matrix-level family gate, zero correctness mismatch | No remaining correctness defect is visible in the reviewed routing path |
| Evidence and evaluation | 4/5 | 5/5 | Three group-disjoint folds, five paired repetitions, repeated v3 run, fixed controls | All positive evidence is development-only; no untouched post-freeze cohort exists yet |
| Clarity and organization | 4/5 | 5/5 | Official negative result remains explicit and scope is stable | The manuscript must not attribute gain to interactions |
| Positioning and related work | 4/5 | 4/5 | Existing algorithm-selection and learned-solver framing remains appropriate | Submission-time closest-work refresh is still required after the method is frozen |
| Reproducibility and auditability | 5/5 | 5/5 | Unique directories, raw paired references, calibration table, plan diagnostics, retained negatives | Frozen verifier must be updated only after a new official evidence package exists |
| Ethics and limitations | 5/5 | 5/5 | No sensitive data; hardware and workload limits are explicit | Continue reporting the small effect size and abstention rate |

**Overall:** 8/10. **Recommendation:** accept. **Confidence:** 5/5.

The paper does not move to 9/10 because the new method has not been tested on a genuinely untouched, post-freeze cohort. The current development result is strong enough to freeze the method, not strong enough to upgrade the manuscript claim.

## 6. Major Concerns

### C1 — Positive effect size is very small

- **Evidence:** v3 improves over family-fixed by roughly 0.19--0.23%; v1/v2 deliberately abstain and tie.
- **Risk:** ordinary machine-load variation could erase the practical difference even though the policy choice repeats.
- **Repair condition:** one untouched first run must reproduce non-inferiority to both controls; any positive claim must report absolute and relative effect sizes.

### C2 — Adaptation is sparse

- **Evidence:** only `spd` is enabled in v3; all families are anchor-only in v1/v2.
- **Risk:** the learned router may be valuable mainly as a calibrated abstention mechanism rather than a broadly superior selector.
- **Repair condition:** state this boundary honestly unless the unseen cohort enables additional families with group-level support.

### C3 — Raw cost tails remain large

- **Evidence:** all-action maximum error remains above 600 on v3 run2.
- **Risk:** unsupported families or new matrices could expose underprediction not covered by the current family gate.
- **Repair condition:** preserve decision-relevant diagnostics and require the unseen run to report selected-action and anchor tails, not only aggregate all-action error.

## 7. Multi-Reviewer Panel

### Reviewer A — Numerical Algorithms

- **Likely score:** 8/10.
- **Confidence:** 5/5.
- **Positive signal:** exact routing and numerical acceptance contracts are sound.
- **Negative signal:** adaptation is useful in only one observed family.
- **Score-change condition:** unseen non-inferiority with at least one supported adaptive gain.

### Reviewer B — Empirical Methodology

- **Likely score:** 8/10.
- **Confidence:** 5/5.
- **Positive signal:** same-repetition terminal pairing and five repetitions directly address temporal drift.
- **Negative signal:** effect size remains too small for a strong empirical claim.
- **Score-change condition:** untouched first-run evidence with preserved controls and diagnostics.

### Reviewer C — Systems And Reproducibility

- **Likely score:** 9/10 tendency if the next freeze succeeds; currently 8/10.
- **Confidence:** 5/5.
- **Positive signal:** development directories, failed methods, and raw evidence are all preserved.
- **Negative signal:** artifact verifier and manuscript still describe the older official method.
- **Score-change condition:** freeze once, run once, then synchronize code, evidence, paper, and manifest.

### Reviewer D — Clarity And Scope

- **Likely score:** 8/10.
- **Confidence:** 5/5.
- **Positive signal:** solver-only scope and numerical-continuation terminology are stable.
- **Negative signal:** interaction-aware language would misidentify the source of the stable result.
- **Score-change condition:** describe the contribution as calibration-gated family adaptation with interaction modeling retained as a negative/limited extension.

### Panel Synthesis

- **Agreement:** method correctness and development stability are adequate for a freeze.
- **Disagreement:** Reviewer C is closest to 9/10 because of artifact discipline; the other reviewers require untouched evidence first.
- **Decisive positive axis:** exact verified routing plus matrix-group calibration gate.
- **Decisive negative axis:** no post-freeze unseen result and a very small adaptive gain.
- **Unresolved evidence:** performance on genuinely uninspected matrix groups.
- **AC stance:** 8/10 accept; authorize one v4 freeze and exactly one first run, with no further method tuning after the lock.

## 8. Concern-To-Action Table

| Priority | Concern | Action | Success condition |
| --- | --- | --- | --- |
| P0 | No untouched evidence | Audit uninspected SuiteSparse groups and create a unique v4 lock before timing | Selection and payload hashes fixed before any action measurement |
| P0 | Freeze integrity | Run exactly one new v4 first-run directory and never overwrite it | Directory, manifests, raw traces, and evidence remain immutable |
| P0 | Decision-level claim | Compare conditioned, fixed, and family-fixed complete-path cost with all correctness contracts | Conditioned is no worse than both controls; any gain is reported without inflation |
| P1 | Tail risk | Preserve selected-action, anchor, and all-action diagnostics in the frozen artifact | Reviewer can distinguish harmful underprediction from unused conservative overprediction |
| P1 | Manuscript synchronization | Update paper, generated values, verifier, PDF, and manifest only after reviewing the first run | Every manuscript number maps to frozen evidence |

## 9. Score-Change Conditions

| Change | Condition | Likely dimensions | Expected movement |
| --- | --- | --- | --- |
| Raise | New untouched first run is no worse than fixed and family-fixed, with zero numerical mismatch | Evidence, impact, reproducibility | +1 overall |
| Raise | Unseen run additionally shows material gain or broader supported adaptation, and closest-work/artifact audits pass | Novelty, impact, evidence | possible +1 further |
| Hold | Unseen run ties controls through abstention and remains numerically correct | Soundness, reproducibility | stays 8/10 unless contribution framing is strengthened honestly |
| Lower | Unseen run loses to a strong control or the gate decision changes incorrectly | Evidence, soundness | -1 or more |
| Fatal | Any inspected held-out run is overwritten or used for method tuning | Evidence integrity | reject empirical claim |

## 10. Checks Run

- Built the evidence target and routing tests in Release mode.
- Passed `SMAVE_TEST_FILTER=cascade-ordering build/release/smave_tests` after each routing change.
- Ran five-repetition v1/v2/v3 group-disjoint development folds.
- Repeated the positive v3 fold in a new directory.
- Confirmed zero DP/exhaustive, production failure, gate, and plan-order mismatches.
- Inspected worst all-action and selected-action prediction diagnostics.
- Confirmed no output path targeted the official v3 first-run directory.

## 11. Unresolved Or Unverified

- No v4 data lock or first run exists yet.
- No current paper number reflects the new method.
- Full broad `smave_unit` remains outside this focused routing round because of the known macOS FMI `dlopen` delay; focused tests pass.
- No new public related-work search was performed in this implementation-focused round.

## 12. Output Self-Check

- The score remains evidence-grounded and is not increased for development-only results.
- Negative interaction findings and small effect sizes are retained.
- The freeze authorization is conditional on a genuinely untouched selection process.
- No infrastructure contribution or requirement is introduced.
