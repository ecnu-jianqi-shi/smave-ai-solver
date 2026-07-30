# CCF-A Full Review — Round 49

## 1. Mode

Full scientific, evidence, novelty, writing, format, reproducibility, and AC review.

## 2. Venue And Assumptions

- **Target:** IEEE TPDS-style CCF-A systems/numerical-software article.
- **Paper type:** solver/runtime method with formal finite-model reasoning, empirical
  systems evidence, numerical-correctness contracts, and an executable artifact.
- **Review state:** 2026-07-28 manuscript, 12 pages, after the exact finite-cascade
  proposition, prior-art positioning, and 256-case property sweep were integrated.
- **Administrative assumption:** verified author, affiliation, funding, conflict,
  acknowledgment, corresponding-author, and anonymity metadata remain unavailable and
  are not invented.

## 3. Paper Summary

The paper presents SMAVE, a typed numerical solver runtime that selects candidate,
correction, original-equation gate, numerical-continuation, and terminal-solver paths
by complete expected cost. Its central safety boundary is that routing cannot bypass
family-specific original-equation acceptance. Its strongest new formal result states
that, for a finite action set with order-invariant action and suffix statistics,
positive acceptance probabilities, a cardinality cap, and one action per expert,
cost-per-acceptance sorting followed by a dynamic program yields the globally minimum
expected-cost cascade, including immediate terminal fallback. The implementation
exposes explicit state-cap and 63-expert limits.

The evaluation combines exact oracles, deterministic properties, held-out routing,
complete-cost decompositions, negative transfer, operator and PDE workloads, HINTS,
parallel/batch scaling, portability, and a deterministic core artifact. The final
public SuiteSparse evidence remains mixed: v6 is near oracle but only 0.109% below the
strong fixed control, while frozen v5 remains 1.718x fixed after zero-execution replay.

## 4. Likely Stance And Calibrated Score

- **Likely stance:** strong accept.
- **Overall score:** **9/10**.
- **Scholarly confidence:** **5/5**.
- **Why not 10:** the formal strengthening closes a genuine soundness/originality
  presentation gap, but the strongest unresolved limitation is scientific breadth:
  small one-host final cohorts, an adverse v5 result, a narrow 0.109% v6-vs-fixed
  effect, no multi-architecture/distributed-scale final validation, and no public
  third-party reproduction.
- **Why not 8:** the method is internally exact under explicit assumptions, every
  returned result remains gate-controlled, exact and exhaustive checks are unusually
  strong, negative results are central rather than hidden, and the artifact is deeply
  auditable.

## 5. Quantitative Scorecard

| Dimension | Score | Confidence | Evidence basis | Deduction / score-change condition |
| --- | ---: | ---: | --- | --- |
| Novelty | 4/5 | 5/5 | Verified complete-path composition, exact constrained finite recurrence, and return invariant | `K/p` ordering and solver selection are prior art; 5/5 would require a broader nontrivial theoretical or systems advance beyond this finite-model formalization |
| Soundness | 5/5 | 5/5 | Explicit assumptions, corrected state bound, exhaustive oracle, 256 property cases, state-cap/63-expert boundary, and gate invariant | None within the declared model; any history-dependent/global claim would lower this score |
| Evidence | 5/5 | 5/5 | Paired timing, confidence intervals, exact oracles, ablations, adverse cohorts, shift tests, and retained failures | External validity is bounded but transparently measured rather than hidden |
| Significance | 4/5 | 5/5 | Complete-cost and verification lessons matter across numerical runtimes | Mixed final-cohort behavior and one-host scope; broader independent validation is required for 5/5 |
| Clarity | 5/5 | 5/5 | Contribution boundary, theorem assumptions, negative results, and conclusion are recoverable in 12 pages | Dense evidence inventory remains, but no decision-relevant ambiguity persists |
| Reproducibility | 5/5 | 5/5 | Machine-generated evidence, frozen runs, deterministic archive workflow, 29 CTests, and executable verifiers | Public immutable release and independent rerun remain absent but are explicitly excluded |
| Ethics / Limitations | 5/5 | 5/5 | Data licenses, risk boundaries, negative outcomes, no-first claim, and one-host limits are explicit | None scientifically; administrative disclosures still require verified author input |

**Recommendation:** strong accept.

**Verdict:** the theorem and property sweep strengthen the 9/10 case but do not make
10/10 scientifically defensible. A score increase requires new independent evidence,
not another manuscript-only iteration.

## 6. Desk Checks

| Check | Status | Evidence | Consequence / action |
| --- | --- | --- | --- |
| Paper length | Pass | Direct build is 12 pages | No formatting-based desk risk under the current assumption |
| Topic compatibility | Pass | Numerical solver runtime, routing, parallelism, correctness, and reproducibility | Fits a TPDS-style systems/numerical-software scope |
| Minimum quality | Pass | Complete method, proof, evidence, related work, limitations, and artifact | No minimum-quality risk |
| Citation/reference integrity | Pass | 20 cited keys; no undefined citation; new DOI/context verified | Preserve current prior-art wording |
| Policy/anonymity | Uncertain | Author block and disclosure metadata remain placeholders | Authors must choose anonymity mode and supply verified metadata |
| Prompt manipulation | Pass | No hidden reviewer/LLM instructions found | No action |
| Ethics/reviewability | Pass | Public scientific data, license records, no human subjects, explicit numerical-risk boundary | No ethics desk risk found |

- **Desk rejection risk:** low.
- **Reason:** only administrative metadata/anonymity remains uncertain; scientific and
  formatting checks pass.
- **Can be fixed before review:** yes, with verified author input.

## 7. Top Strengths

1. **Exactness is now stated at the correct boundary.** The proposition covers finite,
   history-independent action statistics with a cardinality and partition constraint,
   includes the empty cascade, states the corrected `(n+1)(k+1)2^m` bound, and exposes
   implementation rejection limits.
2. **The formal result is independently exercised.** One production-path profile and
   256 fixed-seed six-action cases match exhaustive subset-and-permutation enumeration
   with zero maximum gap and preserved expert exclusivity/order.
3. **Correctness is separated from routing quality.** A poor router increases cost or
   continues; it cannot bypass the original-equation gate.
4. **Negative evidence is unusually credible.** V5 remains adverse, v6's favorable
   effect over fixed is reported as 0.109%, and common-success failures, device
   regressions, nonlinear failures, and shift limitations remain visible.
5. **Artifact discipline is exceptional.** Evidence values are generated and checked,
   final cohorts are frozen, v5 replay performs no solver execution, and the core
   verifier rebuilds and checks the claim surface.

## 8. Major Or Fatal Concerns

- **Fatal concerns:** none.
- **Major unresolved scientific concern:** none that invalidates the declared claims.
- **Decision-limiting concern:** the favorable final routing evidence is narrow and not
  a general repair. V6 contains only three matrices and 24 requests on one host and
  improves on fixed by 0.109%; v5 remains substantially worse than fixed.
- **Impact ceiling:** TPDS-scale breadth is not established by one Apple M4, intra-node
  workers, emulated portability builds, and author-operated reproduction.
- **Originality ceiling:** the ratio ordering and component solver-selection/hybrid
  methods are prior art. The defensible novelty lies in verified composition, exact
  constrained finite selection, complete-cost accounting, and auditability—not a new
  universal scheduling law.

## 9. Writing And Presentation Concerns

- The theorem insertion improves scientific recoverability without increasing the
  paper beyond 12 pages.
- Related work now correctly marks ratio ordering as prior art and avoids a first claim.
- The compressed discussion removes duplicated caveats while preserving every
  decision-relevant negative result.
- The evidence density remains high; a reader must track many workloads and routing
  variants. This is a minor accessibility cost, not a correctness defect.
- Author/disclosure placeholders prevent submission readiness but cannot be repaired
  without verified identities.

## 10. Multi-Reviewer Panel

### Reviewer 1 — Best-Justified Accept Case

- **Lens:** strongest defensible contribution and community value.
- **Score tendency:** 9/10.
- **Confidence:** 5/5.
- **Positive signal:** verified complete-path composition is formalized, implemented,
  and supported by exact/exhaustive evidence plus transparent adverse results.
- **Negative signal:** breadth is insufficient for an exceptional 10.
- **Score-change condition:** independent prefrozen validation across additional hosts
  and untouched cohorts.

### Reviewer 2 — Critical Reviewer

- **Lens:** strongest rejection case.
- **Score tendency:** 8/10.
- **Confidence:** 5/5.
- **Positive signal:** no central unsupported claim or hidden negative result.
- **Negative signal:** v6's 0.109% gain over fixed is practically tiny, while v5 fails;
  the full system contribution may be viewed as careful composition rather than a
  decisive performance advance.
- **Fatal concern:** none under the bounded claims.
- **Score-change condition:** downgrade if the paper generalizes v6 or hides v5;
  upgrade with broader independent effect evidence.

### Reviewer 3 — Method And Soundness

- **Lens:** assumptions, proof, algorithm, and control-flow validity.
- **Score tendency:** 9/10.
- **Confidence:** 5/5.
- **Positive signal:** the theorem now states the required history-independence,
  terminal option, corrected state count, cap behavior, and implementation limit.
- **Negative signal:** interaction-aware and history-dependent optimization remains
  outside the proof.
- **Score-change condition:** a stronger theorem would need genuinely new analysis,
  not broader wording.

### Reviewer 4 — Evidence And Experiments

- **Lens:** controls, statistics, ablations, and robustness.
- **Score tendency:** 9/10.
- **Confidence:** 5/5.
- **Positive signal:** exact oracles, property testing, paired timing, intervals,
  shifts, decompositions, and negative cohorts create a strong falsification package.
- **Negative signal:** final held-out cohort sizes and host diversity are small.
- **Score-change condition:** larger prefrozen cohorts and native multi-architecture
  measurements.

### Reviewer 5 — Novelty And Positioning

- **Lens:** closest work and novelty delta.
- **Score tendency:** 8/10.
- **Confidence:** 5/5.
- **Positive signal:** the paper now distinguishes classical ratio ordering from its
  constrained finite recurrence and verified numerical composition.
- **Negative signal:** much of the component machinery is established, and the dynamic
  program is an elementary exact formalization under strong assumptions.
- **Score-change condition:** a 9--10 novelty score would require a deeper theoretical
  result or clearly broader new systems capability.

### Reviewer 6 — Writing And Clarity

- **Lens:** contribution recoverability and claim visibility.
- **Score tendency:** 9/10.
- **Confidence:** 5/5.
- **Positive signal:** assumptions, prior-art boundary, failures, and conclusion align.
- **Negative signal:** many evidence layers create a steep terminology load.
- **Score-change condition:** an optional visual evidence map could improve access, but
  no scientific rewrite is required.

### Reviewer 7 — Ethics And Reproducibility

- **Lens:** auditability, licensing, risk, and responsible claims.
- **Score tendency:** 9/10.
- **Confidence:** 5/5.
- **Positive signal:** frozen evidence, no-execution replay, data locks, explicit
  one-host limits, and no-first wording are exemplary.
- **Negative signal:** no public immutable archive or independent operator exists.
- **Score-change condition:** public release and third-party reproduction.

### Reviewer 8 — Domain Application

- **Lens:** practical numerical-solver validity.
- **Score tendency:** 8/10.
- **Confidence:** 5/5.
- **Positive signal:** original-equation gates, continuation, correction budgets, and
  terminal fallbacks match production numerical concerns.
- **Negative signal:** only selected workloads and one hardware platform establish
  end-to-end value.
- **Score-change condition:** more equation families, scales, precisions, and hardware.

### Reviewer 9 — Evidence/Ablation Specialist

- **Lens:** mechanism isolation.
- **Score tendency:** 9/10.
- **Confidence:** 5/5.
- **Positive signal:** candidate/corrector/gate/continuation decomposition, budget
  sweeps, exhaustive ordering, and the new property sweep isolate the mechanism.
- **Negative signal:** the property sweep validates the finite optimizer, not learned
  calibration or real-workload generalization.
- **Score-change condition:** none for soundness; generalization needs new experiments.

### Reviewer 10 — Reproducibility Specialist

- **Lens:** ability to inspect and rerun the claim surface.
- **Score tendency:** 9/10.
- **Confidence:** 5/5.
- **Positive signal:** deterministic bundle construction, machine evidence, 29 CTests,
  and frozen replay contracts are unusually complete.
- **Negative signal:** large external datasets and official HINTS environment are not
  rerun by the core bundle; reproduction remains author-operated.
- **Score-change condition:** persistent public artifact plus independent report.

### Reviewer 11 — Novice Advocate

- **Lens:** accessibility to a new TPDS reader.
- **Score tendency:** 8/10.
- **Confidence:** 4/5.
- **Positive signal:** the architecture, contribution list, research questions, and
  conclusion orient the reader.
- **Negative signal:** many routing versions, controls, and generated metrics make the
  evaluation hard to absorb on a first pass.
- **Score-change condition:** optional compact evidence taxonomy; not a score gate.

### Panel Synthesis

- **Agreement:** strong soundness, exceptional evidence discipline, honest negative
  results, and no claim-evidence defect.
- **Disagreement:** novelty and domain reviewers remain at 8 because the theory is a
  bounded formalization and final-cohort generalization is mixed; method and
  reproducibility reviewers favor 9.
- **Decisive positive axis:** exact verified complete-path composition with exhaustive
  finite-model checks and transparent failure evidence.
- **Decisive negative axis:** no general routing repair, only 0.109% v6 improvement over
  fixed, one-host/small-cohort validation, and no independent reproduction.
- **Unresolved evidence:** multi-architecture and broader prefrozen validation,
  distributed-scale behavior, public third-party reproduction, and author metadata.
- **AC stance:** **9/10, strong accept**. The formal strengthening improves confidence
  but does not justify 10/10.

## 11. Concern-To-Action Table

| ID | Severity | Concern | Evidence basis | Affected criterion | Required action | Score-impact condition |
| --- | --- | --- | --- | --- | --- | --- |
| C1 | Moderate | Mixed final-cohort validity | v5 remains 1.718x fixed; v6 near oracle but only 0.109% below fixed | Significance, external validity | New independently prefrozen cohorts without v5/v6 tuning | Required for 10 |
| C2 | Moderate | One-host and limited scale | Apple M4 timing, intra-node workers, no native multi-node study | Significance, venue impact | Native multi-architecture and distributed-scale validation | Required for exceptional score |
| C3 | Minor | Public independent reproduction absent | Author-operated deterministic bundle only | Reproducibility | Public immutable artifact and third-party rerun | Would strengthen 9 toward 10 |
| C4 | Minor | Administrative metadata incomplete | Placeholder author/disclosure block | Compliance | Verified author input and anonymity choice | Required for submission, not scientific score |

## 12. AC / Meta-Review

Round 49 closes the remaining manuscript-level theory gap. The ratio ordering is
correctly acknowledged as prior art; the paper's proposition now proves only the
exact constrained finite recurrence it actually implements, with all material
assumptions and rejection limits exposed. The 256-case exhaustive property sweep makes
that result falsifiable beyond a single hand-designed probe. These changes improve the
paper's originality presentation and reinforce soundness.

They do not resolve the decisive ceiling. The public final routing evidence remains
mixed across v5/v6, the favorable v6 effect over fixed is 0.109%, final cohorts are
small and single-host, and no independent public reproduction exists. Therefore the
scientifically calibrated decision remains 9/10. Granting 10/10 would confuse a
manuscript repair with evidence of broader impact.

## 13. Questions For Authors

1. What verified author, affiliation, funding, conflict, acknowledgment, and
   corresponding-author metadata should replace the placeholders?
2. Will the submission use single-anonymous or double-anonymous review, and how will
   repository/artifact identity be handled?
3. Is a public immutable archive and independent rerun planned, or should the current
   author-operated boundary remain the final claim?

## 14. Score Revision Criteria

| Change | Condition | Affected dimensions | Expected movement |
| --- | --- | --- | --- |
| Raise score | Independent prefrozen multi-cohort and multi-architecture evidence showing a repeatable material gain over strong fixed controls | Significance, evidence, domain validity | Potential 9 to 10 |
| Raise score | Public immutable artifact plus successful independent reproduction | Reproducibility, confidence | Strengthens 9; insufficient alone for 10 |
| Lower score | Hide v5, generalize v6, weaken the gate, or overwrite frozen first-run evidence | Soundness, ethics, evidence | 9 to 7 or lower |
| No quick change | Mixed external validity under the current no-new-cohort/no-tuning constraint | Significance | Manuscript-only edits cannot remove the ceiling |

## 15. Recommended Next CCFA Owner

- Artifact/reproducibility workflow: refresh the manifest, deterministic archive,
  sidecar, contract, and clean extracted-tree evidence after Round 49 reports are
  frozen.
- Authors: provide verified administrative metadata and anonymity mode.
- Future experimental work: only a new, independently prefrozen protocol can pursue
  the evidence required for 10/10.

## 16. Checks Run

- Release configure/build and all 29 CTests.
- Focused joint expert--budget evidence reproduction and 256-case property verifier.
- Paper evidence generator/checker.
- Theorem/proof/code-boundary audit.
- Citation DOI and context verification for the prior-art ordering statement.
- Direct 12-page LaTeX rebuild and diagnostic scan.

## 17. Unresolved Or Unverified

- Verified author/disclosure metadata and submission anonymity mode.
- Independent public reproduction and persistent archive identifier.
- Multi-architecture, distributed-scale, and broader prefrozen final-cohort validity.
- The Round 49 deterministic archive refresh is intentionally sequenced after these
  review files are frozen; its result does not affect the scientific score.

## 18. Output Self-Check

- Every score below 5/5 includes an evidence-based deduction and repair condition.
- The panel preserves the adverse v5 result and the narrow 0.109% v6 effect.
- The formal strengthening changes confidence and presentation, not the external
  evidence base.
- No acceptance probability, author identity, new experiment, citation, or consensus
  is invented.
- The final 9/10 stance follows the strongest unresolved concern rather than averaging
  reviewer enthusiasm.
