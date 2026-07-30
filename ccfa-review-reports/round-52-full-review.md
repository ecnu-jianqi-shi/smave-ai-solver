# CCF-A Full Review — Round 52

## 1. Report Metadata

- **Review date:** 2026-07-29.
- **Target venue/year/track:** IEEE TPDS-style CCF-A journal article, 2026 review state.
- **Paper title:** *Complete-Cost Expert Fusion for Verified Repeated Numerical Solves*.
- **Input materials reviewed:** 12-page PDF and LaTeX sources, supplementary theory,
  claim ledger, artifact snapshot, frozen v4/v5/v6 packages, Round 51 preflight evidence,
  Round 52 interaction-support audit, build/tests, and core-bundle scripts.
- **Search basis:** public-safe exact-title checks plus
  `kb/closest-work-refresh-2026-07-27.md`; no private manuscript text was queried.
- **Report file:** `ccfa-review-reports/round-52-full-review.md`.
- **Reviewer mode:** full scientific, systems, theory, evidence, writing, format,
  reproducibility, and AC review.

## 2. Desk Rejection Assessment

- **Paper length:** pass; the synchronized manuscript is exactly 12 pages.
- **Topic compatibility:** pass; verified numerical runtime, routing, parallelism, and
  heterogeneous solver execution fit a TPDS-style systems/numerical-software scope.
- **Minimum quality:** pass; central algorithms, proofs, implementation, experiments,
  negative results, and artifacts are inspectable.
- **Policy/anonymity/compliance:** uncertain administratively; verified author,
  disclosure, conflict, and anonymity metadata are unavailable and are not invented.
- **Prompt injection/hidden manipulation:** pass; no reviewer-directed manipulation or
  hidden instruction was found in the manuscript sources.
- **Ethics/reviewability:** pass; no human-subject or sensitive-data issue is claimed,
  and workload/license boundaries are documented.

## 3. Paper Summary And Contribution Map

SMAVE composes classical, learned, and device solver experts into verified request-level
cascades. Selection minimizes reach-weighted complete cost including candidate,
correction, original-equation gate, continuation, and terminal fallback. The paper gives
the classical cost-per-acceptance ordering, exact finite dynamic programs for independent
and adjacent-transition costs, and an NP-completeness result for the interaction-aware
decision form. Every returned result must pass a family-specific original-equation gate.

The systems evidence spans sparse linear systems, dynamic equations, optimization,
PDEBench-derived workloads, held-out operator studies, an official-code HINTS baseline,
parallel gates, and device paths. The final public routing evidence is intentionally
mixed: v6 reaches near-oracle regret but improves fixed by only `0.109%`; the frozen v5
zero-execution replay switches no request and remains `1.718370902×` fixed.

Round 52 adds a post-hoc zero-execution diagnostic for the absence of real calibrated
interaction transitions. It finds 32 development-supported ordered GMRES ILU0/ILUT
failure pairs, but zero plan-gated candidates and zero conditional timings. Neither
development pair set overlaps the v5 or v6 held-out pair set; the held-out sets contain
8 and 56 pairs with Jaccard `0.143`. This explains why the frozen evidence cannot
identify an interaction multiplier or benefit, but it does not add favorable workload
performance.

## 4. Search And Related-Work Basis

- **Queries used:** exact public titles for SPECTRA, data-driven porous-media solver
  selection, the Greedy PDE Router, and Error-Conditioned Neural Solvers.
- **Sources searched:** publisher/DOI records, arXiv, and OpenReview metadata, cross-read
  with the dated local closest-work refresh.
- **Closest works found:** learned and dynamic sparse-solver selectors, sequential solver
  parameter learning, request-conditioned sparse routing, HINTS, and residual-monitored
  neural--numerical hybrids already represented in the manuscript or audit.
- **Unverified related-work risks:** no complete 2026 field census is claimed; exact
  field-first novelty is deliberately excluded.
- **Source-quality screening:** pass; discovery results were used only to verify public
  metadata and positioning, not to import unpublished claims into the review.

## 5. Expected Review Outcome

- **Expected outcome:** **strong accept, 9/10**.
- **Main accept signal:** a technically coherent verified-composition contribution with
  exact bounded optimization, exhaustive checks, broad implementation, complete-cost
  evaluation, and unusually strong negative-result discipline.
- **Main reject/ceiling signal:** empirical significance remains single-host, small-cohort,
  and mixed; no real calibrated interaction transition is selected, and Round 52 shows
  complete development-to-held-out transition-support shift.
- **Confidence:** **5/5**.

Round 52 does not change the overall score. It improves causal honesty and experiment
planning, not external impact. A defensible 10/10 still requires independently prefrozen,
multi-host, materially favorable evidence with real selected interactions and public
independent reproduction.

## 6. Strengths And Weaknesses

### Strengths

- The paper optimizes the complete verified path rather than raw solver or learned-model
  latency, and the runtime gate remains the final return authority.
- The independent and adjacent-interaction finite models are precisely scoped, proved,
  and matched to production code through independent exhaustive oracles.
- Exact reachable-state preflight converts an exponential-state failure mode into an
  auditable admission decision before any DP-state visit.
- Evaluation retains rejected candidates, terminal continuation, no-common-success
  cases, failed transfer, device negatives, the invalid v4 run, the negative v5 run,
  and the narrow v6 effect.
- Round 52 refuses to reinterpret isolated failures as conditional timing evidence and
  demonstrates the lack of transition-support transfer quantitatively.
- Reproducibility is unusually strong for an author-operated local artifact: frozen
  contracts, input hashes, deterministic targets, generated macros, manifest checks,
  29/29 tests, and a clean-tree bundle verifier align.

### Weakness 1 — External significance ceiling

- **Evidence basis:** all authoritative timing is on one Apple M4; final v5/v6 cohorts
  contain three matrices and 24 requests each; v6 improves fixed by only `0.109%`; v5
  remains strongly negative.
- **Reviewer deduction:** significance remains 4/5 despite full marks elsewhere.
- **Required fix:** independently prefrozen multi-host cohorts with a material,
  repeatable fixed-control gain and no post-outcome cohort search.

### Weakness 2 — No selected real interaction

- **Evidence basis:** zero plan-gated candidates, zero conditional-timing rows, zero
  calibrations, and zero plans with a calibrated transition in both frozen packages.
- **Reviewer deduction:** the interaction contribution is formally and synthetically
  strong but lacks a beneficial real-workload selection event.
- **Required fix:** prefreeze interaction-eligible development/held-out cohorts and
  conditional measurement rules, then report selection, cost, gain, and failures once.

### Weakness 3 — Transition-support shift

- **Evidence basis:** all 32 development-supported pairs are GMRES ILU0/ILUT, while
  development overlap with both held-out pair sets is zero; v5/v6 held-out Jaccard is
  only `0.143`.
- **Reviewer deduction:** no transition population or transfer claim is defensible.
- **Required fix:** design broader collection-group-disjoint development support without
  using held-out statuses for tuning; retain a negative result if support still shifts.

### Weakness 4 — Administrative uncertainty

- **Evidence basis:** author, disclosure, conflict, funding, acknowledgment, and target
  anonymity state are not verifiable from the supplied material.
- **Reviewer deduction:** no scientific-score deduction, but submission readiness cannot
  be certified.
- **Required fix:** authors must complete venue-required metadata outside this review.

## 7. Potentially Missing Related Work

- **Work:** no specific uncited work is asserted as missing.
- **Status:** searched at method level and cross-checked against the dated local refresh.
- **Why relevant:** request-conditioned solver selection, online adaptation, and hybrid
  neural--numerical correction are the closest novelty-collapse risks.
- **Overlap:** these areas predate SMAVE and are already acknowledged.
- **Needed comparison:** preserve the current distinction around typed verified cascades,
  rejected-path complete cost, exact bounded ordering, and original-equation return.

## 8. Claim-Evidence Audit

| Claim | Where stated | Evidence provided | Strength | Reviewer deduction | Required fix |
| --- | --- | --- | --- | --- | --- |
| Verified complete-path composition | Abstract, Sections 1, 3–5 | Production runtime, gate invariant, family probes, ablations | Strong | None | Preserve gate/continuation accounting |
| Exact independent cascade | Section 3, supplementary theory | Proof, production case, 256 exhaustive cases | Strong | None | Do not broaden beyond history-independent statistics |
| Exact adjacent-interaction cascade and NP-completeness | Section 3, supplementary theory | Bellman proof, 256 interaction cases, 4,096 graphs | Strong | None | Retain adjacent-Markov/history-independent boundary |
| Practical interaction-state admission | Sections 4 and 8 | Exact count identity, nine executed profiles, three zero-visit cap rejections | Strong | No timing/memory inference | Add resource curves only under a prefrozen contract |
| V6 public routing effect | Abstract, Sections 7, 9, 10 | Unique prefrozen first run, frozen verifier | Strong but narrow | Significance deduction | Replicate materially across hosts/cohorts |
| V5 guard repair | Abstract, Sections 7, 9 | Deterministic zero-execution replay | Strong negative result | No general repair claim allowed | Preserve the negative result |
| Round 52 interaction prevalence | Methodology and limitations | Immutable observations, pair tables, independent recount | Strong diagnostic | No multiplier/gain inference | Future conditional evidence must be prefrozen |
| Universal or cross-platform acceleration | Explicitly excluded | One-host limitations and device/transfer negatives | Correctly excluded | None | Keep excluded |

## 9. Experiment / Benchmark / Reproducibility Audit

- **Baselines:** classical workload-specific solvers, fixed/static routing, hindsight
  oracles, shared hybrid controls, and official HINTS code are present and bounded.
- **Ablations:** candidate, correction, gate, continuation, order, model complexity,
  worker scaling, batch scaling, and device placement are separated.
- **Datasets/workloads:** broad interface coverage is distinguished from acceleration
  evidence; eight SuiteSparse no-common-success cases remain visible.
- **Metrics:** complete cost, gate success, regret, speedup, confidence intervals,
  calibration error, continuation, and mismatch counts align with stated claims.
- **Statistical rigor:** paired repetitions and fixed-seed intervals are appropriate for
  the reported local effects; the final routing cohorts remain too small for population
  inference and are not presented otherwise.
- **Robustness/failure cases:** negative transfer, v4 invalidity, v5 failure, device
  rejection, topology/order effects, and no interaction selection are retained.
- **Implementation:** public interfaces, source, tests, and deterministic evidence paths
  are available; every successful return remains gate-controlled.
- **Artifact:** Release build and all 29 CTests pass; Round 52 target and independent
  recount pass; the synchronized PDF is 12 pages with no undefined reference or overfull
  box. Final archive hashes are refreshed after report/status synchronization.
- **Limitations:** one host, no independent public rerun, small final cohorts, no real
  selected interaction, and unavailable administrative metadata remain explicit.

## 10. Multi-Reviewer Panel

### Reviewer A — Numerical Algorithms And Theory

- **Likely score:** 9/10; **confidence:** 5/5.
- **Main positive signal:** exact finite models, hardness boundary, exhaustive checks,
  and preflight identity are mutually consistent.
- **Main negative signal:** exactness does not cover longer-memory costs or
  history-dependent acceptance.
- **Score-change condition:** any broader exactness claim lowers soundness; current
  wording avoids it.

### Reviewer B — Systems And Runtime

- **Likely score:** 9/10; **confidence:** 5/5.
- **Main positive signal:** complete-path accounting and gate-controlled fallback are
  implemented across realistic numerical interfaces.
- **Main negative signal:** no multi-host material routing gain or interaction selection.
- **Score-change condition:** prefrozen external deployment evidence is required for 10.

### Reviewer C — Empirical Evaluation And Ablation

- **Likely score:** 8/10; **confidence:** 5/5.
- **Main positive signal:** controls and negative-result retention are unusually strong.
- **Main negative signal:** v6 improves fixed by only `0.109%`, v5 is negative, and
  Round 52 shows zero development-to-held-out transition overlap.
- **Score-change condition:** materially favorable replicated cohorts are required.

### Reviewer D — Novelty And Positioning

- **Likely score:** 9/10; **confidence:** 4/5.
- **Main positive signal:** novelty is placed in verified complete-path composition and
  bounded optimization, not in solver selection or component algorithms.
- **Main negative signal:** a complete 2026 field census is not claimed or available.
- **Score-change condition:** a verified direct overlap in complete verified cascades
  could lower originality; the current public-safe refresh found no such contradiction.

### Reviewer E — Writing, Clarity, And Format

- **Likely score:** 9/10; **confidence:** 5/5.
- **Main positive signal:** central claims, prior-art boundaries, negative results, and
  limitations are visible in the abstract, introduction, discussion, and conclusion.
- **Main negative signal:** density remains high, especially in routing and ablation
  paragraphs, but compression no longer causes page or overfull-box failure.
- **Score-change condition:** expanding diagnostic detail without page budget could
  lower readability; current 12-page state is acceptable.

### Reviewer F — Reproducibility And Integrity

- **Likely score:** 9/10; **confidence:** 5/5.
- **Main positive signal:** immutable input hashes, zero-execution reconstruction,
  evidence-to-macro checks, and clean-tree verification are aligned.
- **Main negative signal:** author-operated local reproduction is not independent public
  reuse.
- **Score-change condition:** immutable public archival release plus third-party rerun
  is required for the external-reproduction gap.

### Reviewer G — Domain Application

- **Likely score:** 8/10; **confidence:** 4/5.
- **Main positive signal:** the paper covers multiple numerical equation families while
  distinguishing interface breadth from speed evidence.
- **Main negative signal:** the strongest final router result is too narrow to establish
  application-level transfer.
- **Score-change condition:** domain-owned, prefrozen repeated workloads on independent
  hosts would increase significance.

### Reviewer H — AC / Meta-Reviewer

- **Likely score:** 9/10; **confidence:** 5/5.
- **Main positive signal:** formal, implementation, evidence-integrity, and writing axes
  all support acceptance.
- **Main negative signal:** significance remains the only decisive ceiling.
- **Score-change condition:** no manuscript-only edit can replace missing external
  favorable evidence.

**Panel synthesis**

- **Agreement:** the paper is technically strong, reviewable, conservative, and
  reproducible; Round 52 is a valid diagnostic rather than a new performance claim.
- **Disagreement:** empirical/domain reviewers remain at 8 while theory, systems,
  novelty, writing, and reproducibility reviewers support 9.
- **Decisive positive axis:** verified composition, exact bounded optimization,
  exhaustive checking, and transparent failures.
- **Decisive negative axis:** external significance and absent real interaction benefit.
- **Unresolved evidence:** multi-host material gain, recurring interaction support,
  selected calibrated transitions, independent public reproduction, and metadata.
- **AC stance:** **9/10, strong accept**.

## 11. Concerns Table

| ID | Severity | Concern | Evidence basis | Affected criterion | Fix class | Required action | Owner skill | Score-change condition |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| R52-1 | Major ceiling | Single-host, tiny final cohorts | One Apple M4; three matrices/24 requests per v5/v6 | Significance | Experiment | Freeze and execute multi-host disjoint cohorts once | External experiment owner | Material repeatable fixed-control gain can raise significance to 5 |
| R52-2 | Major ceiling | No real calibrated interaction selection | Zero candidates, timings, calibrations, and selected transitions | Significance/evidence | Experiment | Prefreeze conditional measurement and interaction-eligible workloads | External experiment owner | Beneficial selected transitions are necessary but not sufficient for 10 |
| R52-3 | Major ceiling | Complete development-to-held-out support shift | 32 development pairs; zero overlap; Jaccard `0.143` across held-out sets | External validity | Experiment | Broaden development support without held-out tuning | External experiment owner | Recurrent held-out support plus material gain can improve score |
| R52-4 | Minor | No gate-stage attrition table | Frozen evidence reports zero candidates but not each eliminated predicate | Clarity/diagnosis | Reproducibility | Optionally freeze a zero-execution attrition report | Artifact owner | Improves diagnosis only; no overall-score change |
| R52-5 | Major ceiling | No public independent rerun | Core bundle is author-operated and local | Reproducibility/significance | Reproducibility | Publish immutable archive and obtain third-party verification | Artifact owner / independent group | Required with external evidence for 10 |
| R52-6 | Minor administrative | Submission metadata unverified | Author/disclosure/anonymity fields unavailable | Compliance | Venue mismatch | Complete official submission metadata | Authors | No scientific score movement |

## 12. AC / Meta-Review

Reviewer consensus supports acceptance. The theory and production optimizer are bounded
correctly; the gate invariant and complete-cost system contribution are coherent; the
evaluation is broad, and its negative outcomes are not hidden. Round 52 further improves
trust by showing that absent interaction selection is not evidence of a favorable latent
effect: development isolated-failure support does not transfer, and no conditional timing
exists.

The decisive acceptance axis is scientific and artifact integrity. The decisive ceiling
is practical significance. Because the favorable final effect is only `0.109%`, the
negative v5 result is large, no real interaction transition is calibrated or selected,
and no independent multi-host reproduction exists, 10/10 would be inflated. The correct
AC stance remains **strong accept, 9/10**.

## 13. Quantitative Scores

| Dimension | Score (1-5) | Confidence | Evidence basis | Deduction / repair condition |
| --- | ---: | ---: | --- | --- |
| Quality | 5 | 5 | Coherent theory, implementation, evidence, and artifact | Lower only for a central inconsistency |
| Clarity | 5 | 5 | Explicit scope and limitation statements; clean 12-page build | Retain current compression and caveats |
| Significance | 4 | 5 | Broad system, but v6 `0.109%`, v5 negative, one host, no selected interaction | Multi-host material gain and real interaction evidence required for 5 |
| Originality | 5 | 4 | Conservative positioning around complete verified cascades and bounded optimization | Lower if direct prior complete-path overlap is verified |
| Soundness | 5 | 5 | Proofs, exhaustive checks, preflight, and gate invariants align | Do not broaden assumptions |
| Evidence | 5 | 5 | Claim-matched experiments, controls, failures, frozen packages, Round 52 diagnostic | External impact is separated into significance |
| Reproducibility | 5 | 5 | Deterministic targets, hashes, manifest, bundle, 29 tests | Public independent reproduction remains externally unresolved |
| Ethics / Limitations | 5 | 5 | One-host, cohort, negative, callback, interaction, and metadata limits explicit | Preserve current exclusions |

- **Overall:** **9/10**.
- **Confidence:** **5/5**.
- **Score-change conditions:** only independently prefrozen external evidence can raise
  the calibrated overall score; unsupported interaction-benefit or population wording
  would lower it.

## 14. Questions For Authors

1. What prefrozen development design could supply recurring transition support across
   collection groups without consulting held-out statuses?
2. Which frozen eligibility predicate removes each of the 32 supported development
   transitions, and can that attrition be reported without changing policy?
3. What material effect threshold and host diversity would be declared before a future
   fixed-control replication?
4. Can an immutable public archive and independent operator be arranged before claiming
   externally reproduced performance?

## 15. Score Revision Criteria

- **Raising the score would require:** a prefrozen multi-host campaign with material
  repeatable improvement over strong fixed controls, real selected/calibrated interaction
  transitions on recurring held-out support, and public independent reproduction.
- **Lowering the score would be triggered by:** presenting isolated failure support as
  conditional cost evidence, using held-out outcomes for tuning, hiding v5, broadening
  interaction exactness, or asserting universal/cross-platform gain.
- **Concerns unlikely to change before submission:** external host diversity,
  independent reproduction, and real interaction benefit cannot be manufactured through
  prose, proofs, or additional synthetic property cases.

## 16. Action Plan And CCFA Handoffs

- **Priority 1 — External experiment:** freeze multi-host, collection-group-disjoint
  cohorts, effect threshold, conditional timing rule, and one-shot analysis. **Owner:**
  external experiment owner. **Handoff required:** yes, only when new evidence exists.
- **Priority 2 — Independent artifact:** publish an immutable archive and obtain a clean
  third-party rerun. **Owner:** artifact owner plus independent group. **Handoff required:**
  yes.
- **Priority 3 — Optional diagnosis:** freeze a zero-execution gate-stage attrition table
  for the 32 development pairs without changing policy. **Owner:** artifact owner.
  **Handoff required:** no; this cannot raise the overall score.
- **Priority 4 — Submission metadata:** complete official author, disclosure, conflict,
  funding, and anonymity fields. **Owner:** authors. **Handoff required:** no.

**Checks run:** Release full build; 29/29 CTests; focused Round 52 target; independent
TSV recount; immutable input hashes; Python syntax checks; evidence checker; 12-page PDF
build; undefined-reference/overfull-box scan; citation-key and duplicate-key checks;
full integrity and multi-reviewer review.

**Checks skipped:** no new numerical solver execution, no cohort search, no policy
tuning, no conditional timing inference, and no claim of a complete 2026 literature
census.

**Unresolved risks:** independently reproduced multi-host performance, material fixed
improvement, recurring held-out interaction support, beneficial selected transitions,
and verified submission metadata.
