# CCF-A Full Review — Round 45

## 1. Report Metadata

- **Mode:** post-revision full scientific, evidence, implementation, and artifact review.
- **Target venue assumption:** IEEE Transactions on Parallel and Distributed Systems.
- **Review date:** 2026-07-27.
- **Method under review:** request-level control-aware family-anchor guard.
- **Evidence status:** development-only; v5 remains an immutable negative result and was not rerun.

## 2. Method Revision

The revised Router keeps the training-selected family anchor but also carries the training-selected global fixed action. For every request, it computes conservative complete-cost upper estimates for both controls using calibrated action cost, pass uncertainty, support extrapolation, and terminal continuation cost. A family anchor is retained only when it is at least 5% better than global fixed and is not severely outside support; otherwise the Router uses global fixed. The same selected control becomes the baseline for deciding whether a fully request-conditioned plan has sufficient gain.

The earlier family-level all-or-nothing guard was retained only as a diagnostic because it collapsed every development fold to one global action and discarded useful family specialization. No support-distance threshold was tuned: the implementation reuses the existing `log(4)` severe-extrapolation boundary.

## 3. Development Evidence

| Fold | Conditioned | Global fixed | Static | Raw family-fixed | Outcome |
| --- | ---: | ---: | ---: | ---: | --- |
| v1 | 1.61425 | 4.37827 | 5.48215 | 1.61425 | retains the strong family policy; beats fixed/static |
| v2 | 1.32429 | 1.32946 | 1.50068 | 1.50282 | beats all three controls |
| v3 | 1.60744 | 1.65858 | 1.83195 | 1.13113 | beats fixed/static but concedes to raw family-fixed |

All folds have zero production failures, zero gate mismatches, zero plan-order mismatches, and zero DP/exhaustive mismatches. The revised policy is therefore robust against the global fixed and static controls in every development fold, while the raw family-fixed control remains stronger on one fold and weaker on another. This is a credible tradeoff rather than universal dominance.

## 4. Scorecard

| Dimension | Score | Confidence | Evidence basis | Deduction and repair condition |
| --- | ---: | ---: | --- | --- |
| Contribution and novelty | 4/5 | 5/5 | Control-aware verified cascade baseline is integrated into the production Router | No untouched result yet |
| Significance and impact | 4/5 | 5/5 | Avoids the exact v5 failure while retaining useful specialization in development | Material unseen gain remains unproven |
| Technical soundness | 5/5 | 5/5 | Conservative complete-cost comparison, support guard, focused unit tests, exact optimizer checks | No visible correctness defect |
| Evidence and evaluation | 4/5 | 5/5 | Three group-disjoint development folds and strong controls | Development data include previously inspected cohorts |
| Clarity and organization | 4/5 | 5/5 | Control hierarchy is explicit: family, global fixed, conditioned plan, terminal continuation | Manuscript not yet synchronized |
| Positioning and related work | 4/5 | 4/5 | Method aligns with algorithm selection and safeguarded learned solvers | Public closest-work refresh remains pending |
| Reproducibility and auditability | 5/5 | 5/5 | v4/v5 retained; tests and diagnostics include global fixed, support, uncertainty, and anchor flags | Final freeze and artifact sync remain pending |
| Ethics and limitations | 5/5 | 5/5 | Negative results and tuning boundary are explicit | No deduction |

**Overall:** 8/10  | **Scholarly Confidence:** 5/5

**Recommendation:** accept

**Verdict:** The revision resolves the v5 methodological asymmetry on development evidence and is eligible for one new untouched run. A correct, non-inferior v6 result raises the paper to 9/10; 10/10 still requires material evidence, closest-work closure, complete artifact/PDF verification, and no unresolved manuscript issue.

## 5. Multi-Reviewer Panel

### Reviewer A — Numerical Methods
- **Score tendency:** 8/10.
- **Confidence:** 5/5.
- **Positive signal:** global fixed is now an explicit original-equation-verified control inside the cascade policy.
- **Negative signal:** one fold still favors raw family-fixed substantially.
- **Score-change condition:** new unseen data must show the guard does not erase useful specialization systematically.

### Reviewer B — Empirical Evaluation
- **Score tendency:** 8/10.
- **Confidence:** 5/5.
- **Positive signal:** conditioned routing beats global fixed and static in all three folds.
- **Negative signal:** gains over global fixed are only 0.4% and 3.1% in v2/v3, and v1 is a tie with raw family-fixed.
- **Score-change condition:** report effect size and controls on one untouched cohort.

### Reviewer C — Systems And Reproducibility
- **Score tendency:** 9/10 on implementation discipline, 8/10 overall.
- **Confidence:** 5/5.
- **Positive signal:** the guard is in `src/routing.cpp`, not only an analysis harness; focused tests cover both fallback and retention.
- **Negative signal:** the final verifier still targets v5 and must be frozen with v6.
- **Score-change condition:** hash source, executable, verifier, manifests, and absent output directory before v6.

### Reviewer D — Clarity And Scope
- **Score tendency:** 8/10.
- **Confidence:** 5/5.
- **Positive signal:** the method now has a simple reviewer-facing safety story.
- **Negative signal:** strict family-level diagnostic and active request-level gate must not be conflated in prose.
- **Score-change condition:** describe the strict gate as an ablated rejected design.

### Panel Synthesis

- **Agreement:** authorize one new unseen v6 run; no further method tuning before it.
- **Disagreement:** empirical reviewers reserve judgment on materiality, while systems review gives stronger credit for fail-closed control selection.
- **Decisive accept axis:** verified control-aware complete-cost routing.
- **Decisive reject axis:** another unseen regression or any post-freeze tuning.
- **Final calibrated stance:** 8/10 accept.

## 6. Concern-To-Action Table

| Priority | Concern | Action | Success condition |
| --- | --- | --- | --- |
| P0 | No untouched evidence for revised method | Freeze v6 using unseen groups excluded from all prior locks | Deterministic selection and zero pre-run measurements |
| P0 | Freeze completeness | Hash source, executable, verifier, manifests, structure audit, and absent output path | One immutable first-run contract |
| P0 | Strong controls | Report conditioned, control-aware anchor, raw family-fixed, global fixed, and static | No selective control omission |
| P1 | Diagnostics | Preserve support, uncertainty, family-anchor, global-fixed, and selected-plan flags | Reviewer can audit every fallback decision |
| P1 | Manuscript synchronization | Update only after v6 review | All numbers map to frozen evidence |

## 7. Score-Change Conditions

| Change | Condition | Likely dimensions | Expected movement |
| --- | --- | --- | --- |
| Raise | v6 is no worse than global fixed/static with zero correctness mismatch | Evidence, impact, reproducibility | +1 overall |
| Raise to 10 | v6 additionally shows material gain or compelling broader effect, closest-work search closes novelty risk, and full artifact/PDF checks pass | All dimensions | +1 further |
| Hold | v6 ties global fixed through abstention | Soundness, reproducibility | remains 8/10 or weak 9/10 depending on breadth |
| Lower | v6 loses materially to global fixed or static | Evidence, significance | -1 or more |
| Fatal | v6 is rerun, overwritten, or used for tuning | Evidence integrity | reject adaptive claim |

## 8. Checks Run

- Built the revised core Router and SuiteSparse evidence executable.
- Passed the focused `cascade-ordering` C++ test, including global-fixed fallback and robust family-anchor retention.
- Ran three group-disjoint five-repetition development folds.
- Confirmed all numerical and exact-optimization contracts in every fold.
- Added control-aware-anchor regret and per-action support/uncertainty/global-fixed diagnostics.

## 9. Output Self-Check

- The score increase is tied to concrete development evidence, not to intent.
- The stronger raw family-fixed result on v3 is retained.
- No unseen-run outcome is predicted or guaranteed.
- The next action is a freeze, not another tuning pass.
