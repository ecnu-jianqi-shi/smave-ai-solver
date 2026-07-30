# CCF-A Full Review — Round 46

## 1. Report Metadata

- **Mode:** post-v6 full scientific, evidence, implementation, writing-risk, and artifact review.
- **Target venue assumption:** IEEE Transactions on Parallel and Distributed Systems.
- **Review date:** 2026-07-27.
- **Official evidence:** unique v6 first run on `nd3k`, `meg4`, and `g7jac020`.
- **Integrity boundary:** v6 was selected, downloaded, numerically classified, source-hashed, executable-hashed, verifier-hashed, and output-path-checked before any performance measurement.

## 2. Official V6 Result

The v6 run completed once in 7,815.53 seconds and passed its frozen verifier. It reports 24/24 production successes, zero production failures, zero original-equation gate mismatches, zero plan-order mismatches, and zero DP/exhaustive mismatches.

Complete-path regret relative to the per-request oracle is:

| Policy | Regret | Relation to conditioned |
| --- | ---: | --- |
| Conditioned control-aware Router | 1.00003079 | reference |
| Control-aware anchor only | 1.00003079 | tie |
| Raw training family-fixed | 1.00003079 | tie |
| Global fixed SuperLU | 1.00112294 | conditioned is 0.109% lower |
| Static profile | 1.75646475 | conditioned is 43.1% lower |
| Raw unguarded conditioned model | 1.04528744 | guard removes a 4.5% excess |

The v6 result is strong evidence of non-inferiority and near-oracle abstention, not evidence of a large adaptive speedup. The policy uses SuperLU on SPD and nonsymmetric requests and the terminal numerical cascade on the symmetric-indefinite matrix. No family adaptation or interaction transition is enabled; the observed gain comes from correct control selection and abstention.

## 3. Evidence Interpretation

The method now has a credible two-part empirical story:

1. v5 demonstrates that an unsupported family anchor can be catastrophically worse than simple controls.
2. v6 demonstrates that the control-aware guard can preserve correctness, avoid the raw conditioned model's 4.5% excess, beat static substantially, and edge global fixed on untouched data.

The evidence does not yet prove that the guard repairs v5 specifically because v5 must not be re-timed. A deterministic offline replay over the frozen v5 observations can answer that question without new solver measurements or method tuning.

## 4. Scorecard

| Dimension | Score | Confidence | Evidence basis | Deduction and repair condition |
| --- | ---: | ---: | --- | --- |
| Contribution and novelty | 4/5 | 5/5 | Control-aware verified complete-cost routing is implemented in the production Router | Closest-work differentiation is not yet refreshed |
| Significance and impact | 4/5 | 5/5 | Near-oracle unseen result and large advantage over static | Improvement over global fixed is only 0.109% |
| Technical soundness | 5/5 | 5/5 | Frozen verifier, exact optimizer oracle, original-equation gate, and focused unit tests | No visible defect |
| Evidence and evaluation | 5/5 | 5/5 | Three development folds, preserved v4 failure, preserved v5 negative result, untouched v6 success, strong controls | Cross-cohort replay remains useful but not required for correctness |
| Clarity and organization | 4/5 | 5/5 | Method and evidence boundary are now conceptually clean | Manuscript still reports v3 and lacks the v4--v6 progression |
| Positioning and related work | 4/5 | 4/5 | Existing framing is plausible | Public closest-work search is stale after the method change |
| Reproducibility and auditability | 5/5 | 5/5 | Complete pre-first-run contract and hashed v6 package | Artifact manifest and core bundle are not yet synchronized |
| Ethics and limitations | 5/5 | 5/5 | Negative and small-effect findings are retained | No deduction |

**Overall:** 9/10  | **Scholarly Confidence:** 5/5

**Recommendation:** strong accept

**Verdict:** v6 satisfies the Round 45 condition for 9/10. A 10/10 review requires the final manuscript and artifact to make the cross-cohort lesson decisive: show, without re-timing, that the guard addresses v5; refresh closest work; synchronize every claim and artifact; and pass the full PDF/bundle/integrity audit. The score must not rise merely by describing the 0.109% fixed-control gain more aggressively.

## 5. Multi-Reviewer Panel

### Reviewer A — Numerical Methods
- **Score tendency:** 9/10.
- **Confidence:** 5/5.
- **Positive signal:** near-oracle complete-path behavior with exact original-equation acceptance.
- **Negative signal:** the selected v6 policy is mostly abstention/control selection rather than a richer adaptive cascade.
- **Score-change condition:** demonstrate the guard's general repair effect across frozen prior observations.

### Reviewer B — Empirical Evaluation
- **Score tendency:** 8/10.
- **Confidence:** 5/5.
- **Positive signal:** untouched non-inferiority against strong controls and a 43.1% reduction relative to static.
- **Negative signal:** only three unseen matrices and a tiny gain over global fixed.
- **Score-change condition:** cross-cohort evidence and precise effect-size framing, not another held-out search.

### Reviewer C — Systems And Reproducibility
- **Score tendency:** 10/10 on audit discipline, 9/10 overall.
- **Confidence:** 5/5.
- **Positive signal:** source, executable, verifier, manifests, audit, and absent output path were frozen before a two-hour first run.
- **Negative signal:** the manuscript and public-facing artifact still point to v3.
- **Score-change condition:** complete synchronized artifact verification.

### Reviewer D — Clarity And Scope
- **Score tendency:** 9/10.
- **Confidence:** 5/5.
- **Positive signal:** v5 failure and v6 repair form a clear problem--reason--action--result narrative.
- **Negative signal:** the paper must distinguish raw conditioned, control-aware anchor, raw family-fixed, fixed, and static without overloading “conditioned.”
- **Score-change condition:** rewrite tables and prose around the control hierarchy.

### Panel Synthesis

- **Agreement:** strong accept; no further unseen cohort should be created.
- **Disagreement:** Reviewer B withholds 9/10 because effect size over fixed is small; Reviewer C gives exceptional credit for evidence integrity.
- **Decisive accept axis:** near-oracle correctness-preserving control selection under a frozen unseen run.
- **Decisive reject axis:** claim inflation or incomplete artifact synchronization.
- **Final calibrated stance:** 9/10 strong accept.

## 6. Concern-To-Action Table

| Priority | Concern | Action | Success condition |
| --- | --- | --- | --- |
| P0 | Cross-cohort repair not shown | Replay v5 from frozen observations without solver re-execution | Guarded counterfactual is reported with immutable inputs and no tuning |
| P0 | Manuscript staleness | Replace v3 routing narrative and macros with v5/v6 progression | Every number maps to an evidence path |
| P0 | Closest-work freshness | Search public algorithm-selection, safeguarded learned-solver, and verified-cascade work | No materially closer unaddressed method remains |
| P0 | Artifact drift | Update verifier, manifest, bundle allowlist, docs, and generated values | Clean reproduction checks pass |
| P1 | Effect-size framing | State 0.109% vs fixed and 43.1% vs static explicitly | No “dominance” or “material speedup” claim |
| P1 | Administrative metadata | Replace author placeholders and complete disclosures | Submission-ready front matter |

## 7. Score-Change Conditions

| Change | Condition | Likely dimensions | Expected movement |
| --- | --- | --- | --- |
| Raise to 10 | Offline v5 repair evidence, current closest-work closure, synchronized manuscript/artifact, clean PDF and bundle, and no unresolved scientific or submission issue | Novelty, clarity, reproducibility, overall | +1 overall |
| Hold at 9 | All synchronization passes but v5 replay is inconclusive or authors remain unavailable | Clarity/reproducibility | strong accept remains |
| Lower | Paper inflates the 0.109% gain or omits v5 negative evidence | Evidence, ethics, clarity | -1 or more |
| Fatal | v6 is overwritten or rerun for selection | Evidence integrity | reject adaptive claim |

## 8. Checks Run

- Verified deterministic v6 selection and all frozen manifest hashes.
- Passed focused control-aware Router tests.
- Confirmed the first-run output path was absent before execution.
- Completed exactly one v6 performance run.
- Passed the frozen CMake evidence verifier.
- Hashed all v6 output files and the console record.

## 9. Output Self-Check

- The score is raised because the exact Round 45 unseen condition was met.
- The tiny fixed-control gain is reported numerically and not inflated.
- Static, fixed, raw family, control-aware anchor, raw conditioned, and final conditioned controls are all retained.
- No additional unseen cohort is recommended.
