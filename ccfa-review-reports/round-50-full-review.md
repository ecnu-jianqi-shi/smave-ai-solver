# CCF-A Full Review — Round 50

## 1. Mode

Full scientific, theory, systems, evidence, novelty, writing, reproducibility, and AC
review.

## 2. Venue And Assumptions

- **Target:** IEEE TPDS-style CCF-A systems/numerical-software article.
- **Paper type:** verified numerical runtime with formal finite optimization,
  implementation, systems evaluation, and executable artifact.
- **Review state:** 2026-07-28, 12-page main paper plus supplementary theory artifact.
- **Administrative boundary:** author/disclosure/anonymity metadata remain unavailable
  and are not invented.

## 3. Paper Summary

SMAVE selects candidate, correction, original-equation gate, numerical-continuation,
and terminal-solver paths by complete expected cost. The independent finite model uses
the classical `K/p` ordering and an exact subset/budget DP. The new Round 50 result
extends exactness to adjacent transition-cost multipliers through a Bellman state over
used experts and the immediately previous action, proves the interaction-aware rational
decision problem NP-complete even for uniform costs/probabilities and binary
multipliers, and justifies exponential dependence. Deterministic evidence matches
exhaustive enumeration on 256 independent cases, 256 interaction cases, and all 4,096
directed four-vertex graphs used by the reduction.

Empirical solver evidence remains unchanged and deliberately mixed. V6 reaches
near-oracle regret but improves on fixed by only 0.109%; frozen v5 remains 1.718x
fixed under zero-execution replay. The paper retains one-host, small-cohort, device,
transfer, nonlinear, and public-reproduction limits.

## 4. Likely Stance And Calibrated Score

- **Likely stance:** strong accept.
- **Overall:** **9/10**.
- **Scholarly confidence:** **5/5**.
- **Round 50 movement:** originality rises from 4/5 to **5/5** because the paper now
  has a nontrivial exact interaction model, a tight complexity boundary, an explicit
  NP-completeness result, and executable reduction evidence while acknowledging prior
  transition-aware sequencing.
- **Why not 10:** significance/external validity remains 4/5. The theory explains and
  validates the optimizer but does not create new multi-host, multi-cohort, distributed,
  or independent reproduction evidence.

## 5. Quantitative Scorecard

| Dimension | Score | Confidence | Evidence basis | Deduction / score-change condition |
| --- | ---: | ---: | --- | --- |
| Novelty | 5/5 | 5/5 | Exact adjacent-cost cascade DP, tight state/transition bounds, NP-completeness under restricted parameters, and verified-composition framing | Preserve the no-first boundary; overclaiming arbitrary interactions would reduce the score |
| Soundness | 5/5 | 5/5 | Proof sketch, full supplement, code-state correspondence, exhaustive property cases, and graph reduction audit | None within adjacent-Markov costs and history-independent acceptance |
| Evidence | 5/5 | 5/5 | Exact oracles, paired timing, intervals, shifts, ablations, failures, frozen cohorts, and deterministic evidence | Broader validity is a significance limit, not a hidden evidence defect |
| Significance | 4/5 | 5/5 | General complete-cost lesson and theoretically justified optimizer | One-host/small final cohorts and narrow v6-vs-fixed effect; independent broad validation required for 5/5 |
| Clarity | 5/5 | 5/5 | Unified theorem, synchronized abstract/limitations, supplementary details, and 12-page main paper | Evidence density remains high but decision boundaries are recoverable |
| Reproducibility | 5/5 | 5/5 | Machine evidence, exhaustive deterministic properties, frozen replays, 29 CTests, and deterministic bundle workflow | Public third-party reproduction remains absent |
| Ethics / Limitations | 5/5 | 5/5 | Prior-art acknowledgment, negative results, licenses, numerical-risk boundary, and explicit exclusions | Administrative disclosures still need verified author input |

**Recommendation:** strong accept.

**Verdict:** Round 50 exhausts the remaining manuscript/theory repair path. A 10/10
decision now requires broader independent scientific evidence rather than stronger
wording, another proof, or additional synthetic property cases.

## 6. Desk Checks

| Check | Status | Evidence | Consequence / action |
| --- | --- | --- | --- |
| Length | Pass | 12-page main PDF | Supplement carries expanded proofs without hiding assumptions |
| Topic fit | Pass | Numerical runtime, exact optimization, parallelism, correctness, and artifact | Strong TPDS-style fit |
| Minimum quality | Pass | Complete formulation, theory, implementation, evaluation, limitations, and references | No quality desk risk |
| Citation integrity | Pass | 21 cited keys; ordering and transition prior art verified | Preserve current attribution |
| Policy/anonymity | Uncertain | Author/disclosure placeholders | Requires author input before submission |
| Hidden manipulation | Pass | No reviewer/LLM instructions found | No action |
| Ethics/reviewability | Pass | Public scientific data, license locks, no human subjects, explicit risk boundary | No ethics desk risk found |

- **Desk rejection risk:** low.
- **Only unresolved desk item:** verified author metadata and anonymity choice.

## 7. Top Strengths

1. **Nontrivial theory now matches the implementation.** The adjacent-interaction
   recurrence exactly captures the production optimizer's previous-action state.
2. **The exponential cost is explained, not merely observed.** NP-completeness holds
   with one action per expert, uniform `K,p`, and multipliers only in `{1,2}`.
3. **The proof is executable.** Every four-vertex directed graph satisfies the exact
   Hamiltonian threshold correspondence, while 256 random interaction cases match a
   separate exhaustive subset/permutation oracle.
4. **Prior art is handled correctly.** Ratio ordering, transition-aware sequencing,
   and dynamic programming are acknowledged; novelty rests on the verified cascade
   model and its exact/hardness boundary.
5. **Negative systems evidence remains central.** The stronger theory does not hide
   v5, inflate the 0.109% v6 effect, or imply broad interaction selection in v6.

## 8. Major Or Fatal Concerns

- **Fatal:** none.
- **Major soundness concern:** none within the declared model.
- **Decisive score ceiling:** external validity. Final public routing still comprises
  three matrices and 24 requests per v5/v6 cohort on one Apple M4, with opposite
  outcomes and no third-party reproduction.
- **Practical-effect ceiling:** v6 is only 0.109% below fixed; the interaction theorem
  improves method depth but does not demonstrate a material interaction-driven gain on
  the final public cohort, where no transition is selected.

## 9. Writing And Presentation

- The main paper keeps a complete proof sketch and all assumptions; the supplement
  expands rather than repairs missing logic.
- Removing the fusion figure is acceptable because its exact values and interpretation
  remain in the text and generated evidence.
- The abstract now foregrounds exact independent/interaction models and NP-completeness
  without displacing the adverse v5 result or one-host limitation.
- The manuscript is dense but coherent; no further compression or theory insertion is
  recommended before submission.

## 10. Multi-Reviewer Panel

### Best-Justified Reviewer

- **Score:** 9/10; **confidence:** 5/5.
- **Positive:** exact verified composition now has both algorithmic and complexity
  depth, plus exceptional falsification evidence.
- **Negative:** external validation prevents an exceptional overall score.

### Critical Reviewer

- **Score:** 8/10; **confidence:** 5/5.
- **Positive:** no unsupported central theorem or hidden adverse evidence.
- **Negative:** interaction theory is not accompanied by a favorable final-cohort
  interaction result; v6 uses zero calibrated transitions.
- **Fatal concern:** none under the bounded claims.

### Method/Soundness Reviewer

- **Score:** 10/10 tendency; **confidence:** 5/5.
- **Positive:** exact state sufficiency, induction, tight bounds, restricted NP-hardness,
  and implementation tests align.
- **Negative:** longer-memory and history-dependent acceptance remain outside scope.

### Evidence/Experiment Reviewer

- **Score:** 9/10; **confidence:** 5/5.
- **Positive:** deterministic exhaustive properties complement paired systems evidence
  and frozen negative cohorts.
- **Negative:** synthetic exactness evidence cannot establish deployment prevalence or
  practical benefit of interactions.

### Novelty/Positioning Reviewer

- **Score:** 9/10; **confidence:** 5/5.
- **Positive:** the exact restricted interaction model and hardness theorem create a
  clear nontrivial delta beyond component composition.
- **Negative:** transition-aware testing and DP are known; novelty must remain model-
  specific rather than universal.

### Writing/Clarity Reviewer

- **Score:** 9/10; **confidence:** 5/5.
- **Positive:** unified proposition and supplement make assumptions recoverable.
- **Negative:** large evidence inventory remains demanding for first-time readers.

### Ethics/Reproducibility Reviewer

- **Score:** 9/10; **confidence:** 5/5.
- **Positive:** frozen runs, deterministic reduction audit, data locks, no-first claims,
  and negative evidence are exemplary.
- **Negative:** no public immutable independent reproduction.

### Domain Application Reviewer

- **Score:** 8/10; **confidence:** 5/5.
- **Positive:** cache/reuse/destructive-state transition costs are plausible solver
  phenomena and the gate boundary is operationally relevant.
- **Negative:** final public routing does not select an interaction transition and
  broader hardware/workload prevalence is unknown.

### Evidence/Ablation Specialist

- **Score:** 9/10; **confidence:** 5/5.
- **Positive:** 126 plan changes show the interaction path is nondegenerate; all random
  cases and graph reductions have zero logical mismatch.
- **Negative:** no new real-workload causal gain is established.

### Reproducibility Specialist

- **Score:** 9/10; **confidence:** 5/5.
- **Positive:** the proof contract is directly executable in the core evidence target.
- **Negative:** large datasets and official external environments remain outside the
  compact bundle.

### Novice Advocate

- **Score:** 8/10; **confidence:** 4/5.
- **Positive:** abstract and unified theorem now expose the key insight early.
- **Negative:** v4/v5/v6, multiple controls, and many solver families still require a
  careful second pass.

### Panel Synthesis

- **Agreement:** originality and soundness materially improve; no reviewer finds a
  theorem defect or claim-evidence mismatch.
- **Disagreement:** method reviewers see near-perfect theoretical execution, while
  critical/domain reviewers discount the absence of interaction-driven final-cohort
  benefit and broader validation.
- **Decisive positive axis:** exact/hard verified cascade optimization plus unusually
  strong executable and negative evidence.
- **Decisive negative axis:** one-host small cohorts, adverse v5, tiny v6-vs-fixed gain,
  no selected final interaction, and no independent reproduction.
- **AC stance:** **9/10, strong accept**. Originality is now 5/5; significance remains
  4/5 and controls the overall decision.

## 11. Concern-To-Action Table

| ID | Severity | Concern | Evidence | Required action | Score condition |
| --- | --- | --- | --- | --- | --- |
| C1 | Moderate | Mixed final-cohort validity | v5 adverse; v6 only 0.109% below fixed | New independently prefrozen cohorts | Required for 10 |
| C2 | Moderate | Interaction benefit not shown in final cohort | Zero calibrated transitions selected in v6 | Untouched workloads where transition effects are present and prefrozen | Required to convert theory into broad impact |
| C3 | Moderate | One-host/limited scale | Apple M4 and intra-node evidence | Native multi-architecture/distributed validation | Required for exceptional TPDS impact |
| C4 | Minor | No independent public reproduction | Author-operated bundle | Persistent archive and third-party report | Strengthens 9; insufficient alone for 10 |
| C5 | Minor | Administrative metadata missing | Placeholder author block | Verified author input | Submission requirement only |

## 12. AC / Meta-Review

Round 50 converts the prior implementation-specific theory repair into a genuine
algorithmic contribution. The paper now identifies the exact state needed for adjacent
transition costs, proves global optimality, tightens the independent state bound, and
shows NP-completeness under strikingly restricted parameters. The executable property
suite makes both the Bellman recurrence and Hamiltonian reduction auditable. Together
with correct prior-art positioning, this supports a 5/5 originality assessment.

The overall score nevertheless remains 9/10. The stronger theory does not change the
external evidence: v5 fails, v6's gain over fixed is tiny, final cohorts are small and
single-host, no interaction is selected in v6, and reproduction is not independent.
Awarding 10/10 would therefore conflate theoretical completeness with demonstrated
broad systems significance.

## 13. Score Revision Criteria

| Change | Condition | Affected dimensions | Movement |
| --- | --- | --- | --- |
| Raise to 10 | Independently prefrozen multi-host cohorts show repeatable material gains over strong fixed controls, including real interaction-selected cases | Significance, domain validity, evidence | 9 to 10 possible |
| Strengthen 9 | Public immutable archive and independent clean rerun | Reproducibility | Confidence/stance strengthened |
| Lower | Generalize to arbitrary history, hide v5, inflate 0.109%, or overwrite frozen evidence | Soundness, ethics, evidence | 9 to 7 or lower |
| No quick change | Further manuscript-only proofs or synthetic cases without broader evidence | Significance | No overall movement expected |

## 14. Recommended Next Owner

- Artifact workflow: refresh manifest and deterministic Round 50 bundle after reports.
- Authors: provide verified administrative metadata and anonymity mode.
- Future experimental work: define a new independent prefrozen protocol only if the
  no-new-cohort restriction is lifted.

## 15. Checks Run

- Release configure/build and 29/29 CTests.
- Independent and interaction-aware focused evidence reproduction.
- 256+256 property cases and 4,096 directed graph reduction cases.
- Citation metadata/context audit.
- Evidence checker and direct 12-page LaTeX build with zero diagnostics.

## 16. Unresolved Or Unverified

- Multi-host, larger-cohort, distributed, and interaction-selected public validation.
- Independent persistent reproduction.
- Verified author/disclosure/anonymity metadata.
- Post-review deterministic archive refresh, intentionally sequenced after this report.

## 17. Output Self-Check

- Every score below 5/5 states the exact deduction and movement condition.
- Originality movement is tied to formal results and executable evidence, not rhetoric.
- The panel does not use synthetic exactness to imply real-world interaction prevalence.
- The final stance follows the strongest unresolved significance concern.
