# CCF-A Full Review — Round 47

## 1. Report Metadata

- **Mode:** full scientific, evidence, implementation, and writing-risk review after frozen-observation v5 replay.
- **Target venue assumption:** IEEE Transactions on Parallel and Distributed Systems.
- **Review date:** 2026-07-27.
- **New evidence:** deterministic counterfactual replay of the current control-aware anchor rule over the immutable v5 observations.
- **Integrity boundary:** the replay reads recorded observations and frozen model artifacts only; it performs zero solver executions and does not alter the official v5 or v6 first-run directories.

## 2. Replay Result

The replay exactly reproduces the official v5 global-fixed regret of `2.6129235277454428` and training-family regret of `4.4899717598139777`. The current control-aware rule switches zero of 24 requests, so its counterfactual regret is also `4.4899717598139777`, or `1.7183709022238944` times global fixed. Input, routing-source, output, and decision hashes are recorded in `build/release/suitesparse-control-aware-replay-v5/evidence.txt`.

This is a negative generalization result. The guard does **not** repair v5. On `aft01`, the family and global anchors are already the same SuperLU action, so no anchor comparison can change the decision. On `rail_5177`, the family model predicts a sufficiently favorable complete-cost upper bound for GMRES-ILU0 and remains within its support threshold, yet some realized requests are harmful. The rule therefore retains the family anchor on every request.

The defensible empirical claim is consequently narrower than Round 46 proposed:

1. v5 exposes a failure of family-anchor selection that the current guard does not retrospectively fix.
2. v6 shows one untouched cohort where the frozen guard achieves near-oracle regret, removes the raw conditioned policy's 4.5% excess, reduces regret by 43.1% relative to static, and improves by 0.109% relative to global fixed.
3. Together, the cohorts establish a useful abstention mechanism with limited demonstrated external validity, not a generally validated repair for unsupported routing.

## 3. Quantitative Scorecard

| Dimension | Score | Confidence | Evidence basis | Deduction and repair condition |
| --- | ---: | ---: | --- | --- |
| Contribution and novelty | 4/5 | 5/5 | Verified complete-path routing plus an auditable control-aware abstention rule | Closest-work differentiation remains stale |
| Significance and impact | 4/5 | 5/5 | Near-oracle v6 behavior and a large static-control reduction | V6 beats global fixed by only 0.109%, and v5 remains adverse |
| Technical soundness | 5/5 | 5/5 | Frozen verifier, exact optimizer oracle, original-equation gate, and replay equivalence | No visible correctness defect |
| Evidence and evaluation | 5/5 | 5/5 | Preserved v4 contract failure, v5 negative result, v6 untouched success, and zero-execution replay | External validity is limited but transparently measured |
| Clarity and organization | 4/5 | 5/5 | The final narrative boundary is now identifiable | Manuscript still reports obsolete v3 evidence |
| Positioning and related work | 4/5 | 4/5 | Existing framing is plausible | Public closest-work search must reflect the guard and negative result |
| Reproducibility and auditability | 5/5 | 5/5 | Immutable source evidence and replay hashes permit exact checking | Public artifact and bundle remain unsynchronized |
| Ethics and limitations | 5/5 | 5/5 | The failed repair is retained without selection or re-timing | No deduction |

**Overall:** 9/10  | **Scholarly Confidence:** 5/5

**Recommendation:** strong accept

**Verdict:** the score remains 9/10. The negative replay strengthens integrity and falsifiability but invalidates the proposed “v5 failure/v6 repair” generalization. A defensible 10/10 now requires current closest-work closure, a manuscript that foregrounds the failed v5 replay and narrow v6 success, a synchronized artifact, a clean PDF/bundle/integrity audit, and no unresolved scientific or submission defect. No further cohort search or policy tuning is permitted in this revision loop.

## 4. Multi-Reviewer Panel

### Reviewer A — Numerical Methods
- **Score tendency:** 9/10.
- **Confidence:** 5/5.
- **Positive signal:** exact acceptance semantics and transparent counterfactual evaluation.
- **Negative signal:** the guard does not address within-support model miscalibration on v5.
- **Score-change condition:** present this limitation explicitly and avoid a general safeguard claim.

### Reviewer B — Empirical Evaluation
- **Score tendency:** 8/10.
- **Confidence:** 5/5.
- **Positive signal:** unusually strong preservation of adverse results and frozen controls.
- **Negative signal:** one adverse and one favorable final cohort do not establish broad guard generalization.
- **Score-change condition:** synchronize all cohort results and state effect sizes and sample scope exactly.

### Reviewer C — Systems And Reproducibility
- **Score tendency:** 10/10 on audit discipline, 9/10 overall.
- **Confidence:** 5/5.
- **Positive signal:** replay equivalence, immutable evidence, content hashes, and zero solver re-execution.
- **Negative signal:** replay artifacts are not yet included in public manifests and documentation.
- **Score-change condition:** complete clean-bundle reproduction and manifest verification.

### Reviewer D — Clarity And Scope
- **Score tendency:** 9/10.
- **Confidence:** 5/5.
- **Positive signal:** the negative replay creates a precise scope boundary.
- **Negative signal:** calling v6 a “repair” would now be materially misleading.
- **Score-change condition:** use “v6 held-out success under the frozen guard,” not “general repair.”

### AC / Meta-Review

The panel agrees on strong acceptance and on withholding 10/10 until the manuscript and artifact are synchronized. Reviewer B assigns the largest penalty because the external-validity evidence is mixed; Reviewer C assigns exceptional credit for auditability. The decisive positive axis is a correctness-preserving, fully frozen evaluation with near-oracle v6 behavior. The decisive negative axis is limited generalization: the same frozen guard leaves v5 unchanged and 71.8% worse than global fixed. This negative result must be treated as a primary finding, not buried as an implementation footnote.

## 5. Concern-To-Action Table

| Priority | Concern | Evidence | Action | Success condition |
| --- | --- | --- | --- | --- |
| P0 | Manuscript has an obsolete v3 narrative | Current paper sources | Rewrite around v5 adverse replay and v6 narrow success | Every numerical claim maps to frozen evidence |
| P0 | Potential repair-claim inflation | Zero v5 switches; regret ratio 1.718 | Remove “repairs v5” and general safeguard language | Scope and limitation are explicit in abstract, evaluation, and conclusion |
| P0 | Closest-work freshness | Method changed after prior search | Run public-safe algorithm-selection and safeguarded solver-routing search | No materially closer unaddressed work remains |
| P0 | Artifact drift | Replay and v4--v6 files absent from release contracts | Update docs, manifests, verifier, and bundle allowlists | Clean artifact checks pass |
| P1 | Control hierarchy is easy to conflate | Six routing/control variants | Define raw conditioned, family-fixed, control-aware, global fixed, static, and final policy once | Tables and prose use consistent names |
| P1 | Administrative metadata unavailable | `paper/authors.tex` remains incomplete | Obtain author and disclosure data externally | Submission front matter is complete |

## 6. Score-Change Conditions

| Change | Condition | Expected movement |
| --- | --- | --- |
| Raise to 10 | Current closest-work closure, fully candid v5/v6 manuscript, synchronized artifact, clean PDF/bundle/integrity checks, and no unresolved scientific defect | +1 overall |
| Hold at 9 | Technical synchronization passes but administrative metadata remains unavailable | Strong accept remains |
| Lower | The paper implies general repair, hides the v5 replay, or inflates the 0.109% fixed-control gain | -1 or more |
| Fatal | Any first-run directory is overwritten, a new cohort is searched, or the method is tuned against v5/v6 | Reject adaptive generalization claim |

## 7. Checks Run

- Ran `benchmark/replay_control_aware_anchor.py` over frozen v5 artifacts.
- Confirmed exact reproduction of official global-fixed and family-fixed regrets.
- Confirmed `requests_switched_from_training_family=0` and `solver_reexecution=0`.
- Inspected per-request decisions and retained immutable output hashes.
- Did not execute any solver or modify any official first-run directory.

## 8. Unresolved Or Unverified

- Public closest-work freshness after the control-aware method change.
- Manuscript/PDF consistency with v4--v6 and replay evidence.
- Artifact manifest, clean-bundle, and broad test verification.
- Author identities, affiliations, acknowledgments, and disclosures.

## 9. Output Self-Check

- The negative replay is stated as a primary result.
- The 71.8% v5 excess and 0.109% v6 gain are not softened or inflated.
- No new experiment, policy change, cohort, citation, or author metadata is invented.
- The score remains calibrated at 9/10 pending complete synchronization and audit.
