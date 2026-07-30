# CCF-A Full Review — Round 53

## 1. Report Metadata

- **Review date:** 2026-07-29.
- **Target venue/year/track:** IEEE TPDS-style CCF-A journal article, 2026 review state.
- **Paper title:** *Complete-Cost Expert Fusion for Verified Repeated Numerical Solves*.
- **Input materials reviewed:** final 12-page PDF and LaTeX sources, supplementary
  theory, claim ledger, artifact snapshot, frozen v4/v5/v6 packages, Round 51 exact
  preflight evidence, Round 52 interaction-support audit, Round 53 transition-attrition
  audit, Release build/tests, paper checks, and core-bundle scripts.
- **Search basis:** public-safe keyword/title refresh on arXiv, ACM, and Springer records,
  cross-read with `kb/closest-work-refresh-2026-07-27.md`; no private manuscript text was
  queried.
- **Report file:** `ccfa-review-reports/round-53-full-review.md`.
- **Reviewer mode:** full scientific, systems, theory, evidence, writing, format,
  reproducibility, and AC review.

## 2. Desk Rejection Assessment

- **Paper length:** pass; the synchronized manuscript is exactly 12 pages.
- **Topic compatibility:** pass; verified numerical runtime, routing, parallelism, and
  heterogeneous solver execution fit a TPDS-style systems/numerical-software scope.
- **Minimum quality:** pass; algorithms, proofs, implementation, experiments, negative
  results, and artifacts are inspectable.
- **Policy/anonymity/compliance:** uncertain administratively; verified author,
  affiliation, disclosure, conflict, funding, and anonymity metadata are unavailable
  and are not invented.
- **Prompt injection and hidden manipulation:** pass; no reviewer-directed instruction or
  hidden manipulation was found in manuscript sources.
- **Ethics and reviewability:** pass; no human-subject or sensitive-data study is claimed,
  and benchmark licenses and scope limits are documented.

## 3. Paper Summary And Contribution Map

SMAVE composes classical, learned, and device solver experts into verified request-level
cascades. Selection minimizes reach-weighted complete cost across candidate generation,
correction, original-equation gating, continuation, and terminal fallback. The paper
derives cost-per-acceptance ordering, exact finite dynamic programs for independent and
adjacent-transition costs, and an NP-completeness result for the interaction-aware
decision form. Every returned result must pass a family-specific original-equation gate.

The evidence spans sparse linear systems, dynamic equations, optimization,
PDEBench-derived workloads, held-out learned-operator studies, an official-code HINTS
comparison, parallel gates, device paths, exact property sweeps, and frozen public
SuiteSparse routing. The final routing result remains deliberately mixed: v6 reaches
near-oracle regret but only `0.109%` below fixed, while the frozen v5 current-policy
replay switches no request and remains `1.718370902×` fixed.

Round 53 closes Round 52's only local diagnostic question. All 32 development-supported
GMRES ILU0/ILUT pairs are modeled in every nonsymmetric training request but disappear
at unguarded top-3 selection. Five other unguarded failed-first adjacent transition
identities occur in each version, none development-supported; the exact control-aware
route removes all, leaving zero final candidates and zero conditional timings. This is a
clean explanation of policy exposure, not evidence of an interaction benefit.

## 4. Search And Related-Work Basis

- **Queries used:** public keywords for request-conditioned sparse solver selection,
  online linear-solver adaptation, neural--classical PDE correction, residual-triggered
  hybrids, and unseen-matrix performance prediction.
- **Sources searched:** arXiv primary records, ACM publication records, Springer chapter
  records, and the dated local closest-work refresh.
- **Closest works found:** SPECTRA, data-driven porous-media solver selection, embedding-
  based solver performance prediction, HINTS, the Greedy PDE Router, ANCHOR,
  Error-Conditioned Neural Solvers, and adjacent neural hybrid correction work.
- **Unverified related-work risks:** no exhaustive 2026 field census is claimed; another
  system may combine overlapping components.
- **Source-quality screening status:** pass; only primary publication/preprint records
  informed positioning, and no search result supports a “first” claim.

No newly identified work invalidates the paper's narrowed claim. Solver selection,
request conditioning, online adaptation, residual monitoring, and neural--classical
hybrids remain prior art. The defensible contribution is the declared combination of
typed verified pipelines, reach-weighted complete-path cost, exact bounded optimization,
and mandatory original-equation acceptance.

## 5. Expected Review Outcome

- **Expected outcome:** **strong accept, 9/10**.
- **Main accept signal:** technically coherent verified solver composition with exact
  bounded optimization, exhaustive checks, broad implementation, claim-matched evidence,
  retained negative results, and unusually strong artifact discipline.
- **Main reject/ceiling signal:** practical significance remains single-host and mixed;
  the only favorable final fixed-control delta is `0.109%`, no real interaction is
  calibrated or selected, and no public independent reproduction exists.
- **Confidence:** **5/5**.

Round 53 improves transparency and closes the local attrition concern, but it cannot
raise significance. A 10/10 would still be inflated.

## 6. Strengths And Weaknesses

### Strengths

- **Verified return invariant:** routing may change cost and order but cannot bypass the
  original-equation gate.
- **Complete-cost formulation:** the objective includes candidate, correction, gate,
  continuation, and terminal costs rather than only raw solver latency.
- **Exact bounded optimization:** independent and adjacent-transition planners are
  checked against exhaustive oracles; preflight counts exact states/transitions before
  admission.
- **Evidence discipline:** v4 is retained as invalid, v5 as negative, and v6 as a narrow
  favorable run; the paper does not average away failures.
- **Round 53 diagnosis:** model reconstruction, production-route cross-checking, stage
  attrition, unsupported candidate identities, and zero-execution constraints are all
  machine verified.
- **Reproducibility:** deterministic targets, immutable hashes, generated macros,
  manifest checks, 29 CTests, and clean-tree bundle verification form a coherent chain.
- **Writing and scope:** contribution, prior-art boundary, one-host limits, adjacent-only
  interactions, and missing metadata are explicit.

### Weaknesses

- **Weakness:** final empirical significance over a strong fixed control is small and
  unstable across frozen cohorts.
  **Evidence basis:** v6 is `0.109%` below fixed; v5 current-policy replay is
  `1.718370902×` fixed.
  **Reviewer deduction:** significance remains 4/5 and caps the overall score at 9/10.
  **Required fix:** independently prefrozen, multi-host cohorts with a declared material
  effect threshold and repeatable improvement over strong fixed controls.

- **Weakness:** no real conditional interaction effect is identified.
  **Evidence basis:** all 32 supported pairs vanish at unguarded top-3 selection; five
  different unsupported candidates are removed by the control-aware route; final
  candidates, timings, and calibrations are zero.
  **Reviewer deduction:** interaction theory is sound, but workload-level interaction
  significance is unproven.
  **Required fix:** prefrozen workloads with recurring transition exposure, independent
  conditional timing support, selected calibrated transitions, and held-out benefit.

- **Weakness:** external reproducibility is not demonstrated.
  **Evidence basis:** the archive and clean-tree rerun are author-operated and local.
  **Reviewer deduction:** no technical reproducibility deduction because the package is
  strong, but external confidence and significance cannot reach the maximum.
  **Required fix:** immutable public release and clean rerun by an independent operator.

- **Weakness:** submission metadata remain incomplete.
  **Evidence basis:** author, affiliation, funding, conflict, acknowledgment, disclosure,
  and final anonymity status are placeholders or unverified.
  **Reviewer deduction:** administrative uncertainty only; potential desk risk depends on
  the actual submission track.
  **Required fix:** authors must supply official metadata before submission.

## 7. Potentially Missing Related Work

- **Work:** *NHCS: A Neural Hybrid PDE Solver for Complex Topology* (2026 preprint).
  **Status:** searched.
  **Why relevant:** adjacent neural--classical correction for PDE solves.
  **Overlap:** hybrid correction and iterative numerical completion, but not the same
  request-level verified complete-path selection problem.
  **Needed comparison:** no mandatory experiment for the current claim; cite only if the
  manuscript expands hybrid-correction novelty.

- **Work:** online data-driven linear-solver selection for multiphysics simulations.
  **Status:** searched and already represented in the local closest-work audit.
  **Why relevant:** adapts selection over repeated solves.
  **Overlap:** repeated-system adaptation.
  **Needed comparison:** retain the current distinction between frozen offline routing
  and online adaptation; do not claim superiority.

- **Work:** SPECTRA and embedding-based unseen-matrix performance prediction.
  **Status:** searched; SPECTRA is cited and both are recorded in the local audit.
  **Why relevant:** request/matrix-conditioned sparse-solver choice.
  **Overlap:** directly limits novelty of request-conditioned selection.
  **Needed comparison:** current mixed v5/v6 evidence and conservative novelty wording
  are sufficient; a new baseline is required only for a stronger routing claim.

## 8. Claim-Evidence Audit

| Claim | Where stated | Evidence provided | Strength | Reviewer deduction | Required fix |
| --- | --- | --- | --- | --- | --- |
| Every returned result passes the original-equation gate | Abstract, design, evaluation | Production traces, gate mismatch checks, unit tests | Strong | None | Preserve invariant |
| Complete-cost ordering and exact finite planning | Formulation, theory, ablation | Proofs, exhaustive 24-permutation check, two 256-case sweeps | Strong | None | Keep model assumptions explicit |
| Interaction-aware decision form is NP-complete | Theory | Reduction and 4,096-graph audit | Strong within declared model | None | Do not generalize to arbitrary history |
| Exact preflight rejects oversized cases before DP visits | Method, ablation | Prefrozen Round 51 identities and zero-visit rejections | Strong | Not a wall-time or memory claim | Preserve wording |
| V6 is near oracle and `0.109%` below fixed | Abstract, evaluation, conclusion | Immutable one-shot v6 package | Numerically strong, practically small | Significance deduction | Multi-host material effect |
| Current policy does not generally repair v5 | Abstract, evaluation, limitations | Zero-execution v5 replay, 0/24 switches, `1.718370902×` fixed | Strong negative result | None | Retain prominently |
| Frozen interaction absence is explained | Limitations, claim ledger | Round 52 support audit plus Round 53 attrition audit | Strong diagnostic | No benefit inference | Real conditional campaign |
| Broader performance generalization | Explicitly denied | One-host and cohort limitations | Correctly bounded | None | External replication only |

## 9. Experiment / Benchmark / Reproducibility Audit

- **Baselines:** strong and varied, including fixed/static controls, classical methods,
  shared operator controls, exhaustive oracles, and official HINTS code.
- **Ablations:** objective components, correction budgets, model feature groups, family
  anchors, interaction assumptions, planner exactness, order sensitivity, and gate-only
  scaling are covered.
- **Datasets/benchmarks:** broad in family but uneven in external scale; the decisive
  final routing cohorts remain three matrices and 24 requests each.
- **Metrics:** complete-runtime speedup, bootstrap intervals, regret, Brier/ECE, cost
  error, acceptance, continuation, mismatch, and failure accounting align with claims.
- **Statistical rigor:** paired repeated measurements are handled carefully where
  available; frozen routing cohorts are correctly not treated as population estimates.
- **Robustness/failure cases:** v4 invalidity, v5 failure, no-common-success cases,
  order sensitivity, callback limits, and support shift are retained.
- **Implementation details:** sufficient for local reproduction; Round 53 adds exact
  route-path verification and byte-identical model reconstruction.
- **Artifacts:** Release build, 29/29 CTests, evidence checker, artifact manifest, 12-page
  PDF, deterministic Round 53 rerun, and core-bundle integration pass.
- **Limitations:** complete and unusually explicit; the missing evidence is external,
  not hidden.

## 10. Multi-Reviewer Panel

### Reviewer: Method And Soundness

- **Expertise:** algorithms, solver routing, and formal optimization.
- **Likely score:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** exact algorithms, bounded assumptions, exhaustive checks, and
  mandatory acceptance align.
- **Main negative signal:** no workload-level evidence that calibrated interactions help.
- **Evidence basis:** theory sections, property sweeps, Round 51 preflight, Round 53 zero
  final candidates.
- **Score-change condition:** selected calibrated transitions with held-out benefit.

### Reviewer: Evidence And Experiments

- **Expertise:** empirical systems and benchmarking.
- **Likely score:** 8/10.
- **Confidence:** 5/5.
- **Main positive signal:** broad controls, paired timing, negative-result retention, and
  deterministic evidence extraction.
- **Main negative signal:** the decisive final gain over fixed is only `0.109%` on one
  small, single-host cohort.
- **Evidence basis:** v5/v6 packages, PDEBench/HINTS/operator results, limitations.
- **Score-change condition:** prefrozen multi-host material gains.

### Reviewer: Novelty And Positioning

- **Expertise:** algorithm selection and hybrid numerical methods.
- **Likely score:** 8/10.
- **Confidence:** 4/5.
- **Main positive signal:** the combination of typed verified cascades and complete-path
  exact selection is sharply delimited.
- **Main negative signal:** components and neighboring solver-selection/hybrid ideas are
  established prior art.
- **Evidence basis:** related work, dated closest-work audit, 2026-07-29 public refresh.
- **Score-change condition:** maintain conservative combination-level novelty; lower if
  field-first language appears.

### Reviewer: Writing And Clarity

- **Expertise:** technical communication and manuscript structure.
- **Likely score:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** claims, controls, failures, and limitations are unusually
  explicit within 12 pages.
- **Main negative signal:** density remains high, but no decision-relevant ambiguity
  persists after Round 53.
- **Evidence basis:** final PDF, generated values, claim ledger.
- **Score-change condition:** preserve current compression and terminology.

### Reviewer: Ethics And Reproducibility

- **Expertise:** artifacts, data provenance, and responsible reporting.
- **Likely score:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** immutable data locks, licenses, hashes, no-invention policy,
  deterministic targets, and clean-tree verification.
- **Main negative signal:** public independent execution and official submission metadata
  remain absent.
- **Evidence basis:** data-lock docs, artifact manifest, bundle scripts, metadata files.
- **Score-change condition:** public archive plus third-party rerun and completed metadata.

### Reviewer: Numerical Applications

- **Expertise:** sparse and hybrid numerical solvers.
- **Likely score:** 8/10.
- **Confidence:** 4/5.
- **Main positive signal:** original-equation acceptance and failure accounting make the
  system scientifically safer than latency-only routing.
- **Main negative signal:** hardware, precision, equation-family, and large-real-workload
  transfer remain unmeasured.
- **Evidence basis:** benchmark matrix, limitations, v5/v6 scope.
- **Score-change condition:** cross-platform and larger application campaigns.

### Reviewer: Evidence/Ablation Specialist

- **Expertise:** causal diagnosis and controlled ablation.
- **Likely score:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** Round 53 now distinguishes support, alternative availability,
  top-3 exposure, failed-first adjacency, and final control-aware exposure.
- **Main negative signal:** the diagnosis is necessarily post hoc and cannot supply an
  effect estimate.
- **Evidence basis:** transition/candidate tables and verifier.
- **Score-change condition:** none locally; requires a prefrozen conditional campaign.

### Reviewer: Novice Advocate

- **Expertise:** broad systems reader.
- **Likely score:** 8/10.
- **Confidence:** 4/5.
- **Main positive signal:** the paper clearly explains that verification, not router
  confidence, decides returned results.
- **Main negative signal:** the number of evidence tiers and routing controls is dense.
- **Evidence basis:** architecture figure, abstract, evaluation hierarchy, limitations.
- **Score-change condition:** no extra prose should be added unless space is removed
  elsewhere; the current 12-page presentation is acceptable.

### Reviewer: Area Chair

- **Expertise:** calibrated synthesis across theory, systems, and evidence.
- **Likely score:** 9/10, strong accept.
- **Confidence:** 5/5.
- **Main positive signal:** no central correctness or integrity defect remains, and the
  contribution is reviewable and reproducible.
- **Main negative signal:** external significance is not maximal.
- **Evidence basis:** all reviewer axes and the full artifact chain.
- **Score-change condition:** independent, material, multi-host evidence only.

**Agreement:** theory, implementation integrity, claim discipline, and local
reproducibility are strong; external impact remains the ceiling.

**Disagreement:** evidence-oriented reviewers place more weight on the tiny v6 fixed
delta and one-host scope than method reviewers, but none identifies a rejection-level
correctness defect.

**Decisive positive axis:** verified complete-path composition backed by exact bounded
algorithms and unusually transparent artifacts.

**Decisive negative axis:** no independently reproduced material multi-host gain and no
beneficial selected real interaction.

**Unresolved evidence:** cross-host transfer, recurring transition exposure, conditional
timing, material fixed-control gain, and public independent reproduction.

**AC stance:** strong accept, 9/10.

## 11. Concerns Table

| ID | Severity | Concern | Evidence basis | Affected criterion | Fix class | Required action | Owner skill | Score-change condition |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| R53-1 | Major | External significance is not maximal | V6 `0.109%` below fixed; v5 `1.718370902×` fixed; one host and three-matrix cohorts | Significance | Experiment | Freeze and execute multi-host, group-disjoint cohorts with a material effect threshold | External experiment owner | Required for 10/10 |
| R53-2 | Major | No real selected/calibrated interaction benefit | 32 supported pairs vanish at unguarded top-3; five unsupported candidates are removed; zero timings/calibrations | Significance/evidence | Experiment | Prefreeze recurring transition exposure and conditional timing, then test held-out benefit | External experiment owner | Required for 10/10 interaction claim |
| R53-3 | Moderate | No public independent reproduction | Local deterministic bundle is author-operated | External reproducibility | Reproducibility | Publish immutable archive and obtain independent clean rerun | Artifact owner + external operator | Required for maximal confidence/impact |
| R53-4 | Minor | Submission metadata are incomplete | Placeholder/unverified author and disclosure fields | Compliance | Ethics/limitations | Supply official metadata and confirm anonymity rules | Authors | Prevents administrative desk risk |
| R53-5 | Minor | Full 2026 novelty census is not established | Public-safe search is targeted, not systematic | Originality | Related-work | Preserve no-first wording; run systematic search only if stronger novelty is desired | Literature owner | No score increase under current claim |

The Round 52 concern about missing gate-stage attrition is **closed** by the frozen
Round 53 target and does not remain in this table.

## 12. AC / Meta-Review

The panel agrees that the manuscript is technically sound, carefully scoped, and
artifact-complete for its local claims. Round 53 resolves the only remaining local
diagnostic ambiguity: it identifies the first stage eliminating every supported pair
and separately accounts for all unguarded candidates before the control-aware gate.

This result is negative but valuable. It prevents readers from treating isolated
co-failure support as latent interaction benefit and shows that the final zero-candidate
record is a consequence of policy exposure, not missing bookkeeping. It does not create
a conditional timing estimate or a favorable policy result.

The decisive acceptance axis remains scientific and artifact integrity. The decisive
ceiling remains practical significance. With a `0.109%` favorable fixed delta, a large
negative v5 result, no selected real interaction, one host, and no independent public
rerun, **10/10 would be inflated**. The calibrated AC stance remains **strong accept,
9/10**.

## 13. Quantitative Scores

| Dimension | Score (1-5) | Confidence | Evidence basis | Deduction / repair condition |
| --- | ---: | ---: | --- | --- |
| Quality | 5 | 5 | Coherent theory, implementation, experiments, and artifact | Lower only for a central inconsistency |
| Clarity | 5 | 5 | Dense but explicit 12-page narrative and generated claim ledger | Preserve current wording |
| Significance | 4 | 5 | Broad system, but tiny favorable fixed delta, negative v5, one host, no selected interaction | Multi-host material gain and real interaction evidence required for 5 |
| Originality | 5 | 4 | Conservative combination-level positioning | Lower if direct complete-path overlap is established |
| Soundness | 5 | 5 | Proofs, exact checks, route cross-check, and gate invariants align | Do not broaden assumptions |
| Evidence | 5 | 5 | Claim-matched controls, failures, frozen packages, and Round 53 diagnosis | External impact remains a significance issue |
| Reproducibility | 5 | 5 | Deterministic targets, hashes, manifest, bundle, and 29 tests | Public independent rerun remains external |
| Ethics / Limitations | 5 | 5 | Scope, failure, callback, hardware, interaction, and metadata limits explicit | Complete official metadata |

- **Quality:** 5/5.
- **Clarity:** 5/5.
- **Significance:** 4/5.
- **Originality:** 5/5.
- **Soundness:** 5/5.
- **Evidence:** 5/5.
- **Reproducibility:** 5/5.
- **Ethics / Limitations:** 5/5.
- **Overall:** **9/10, strong accept**.
- **Confidence:** **5/5**.
- **Score-change conditions:** only new, independently prefrozen external evidence can
  raise the calibrated overall score; unsupported interaction-benefit or population
  wording would lower it.

## 14. Questions For Authors

1. What material effect threshold and host diversity will be frozen before the next
   fixed-control campaign?
2. Which development workloads can produce recurring final-plan transition exposure
   without using held-out outcomes for cohort selection?
3. Can an immutable public archive and an independent operator be arranged before any
   externally reproduced performance claim?
4. Which official author, disclosure, funding, conflict, and anonymity fields apply to
   the intended submission state?

## 15. Score Revision Criteria

- **Raising the score would require:** a prefrozen multi-host campaign with material,
  repeatable improvement over strong fixed controls; real selected/calibrated interaction
  transitions with recurring held-out support and benefit; and public independent
  reproduction.
- **Lowering the score would be triggered by:** describing the five unsupported
  candidates as supported or beneficial, using held-out outcomes for tuning, hiding v5,
  broadening interaction exactness, weakening the gate invariant, or asserting universal
  or cross-platform gain.
- **Concerns unlikely to change before submission:** external host diversity,
  independent reproduction, real interaction benefit, and official metadata cannot be
  supplied by further local prose or synthetic property tests.

## 16. Action Plan And CCFA Handoffs

- **Priority 1 — External experiment:** freeze multi-host, collection-group-disjoint
  cohorts, material effect threshold, recurring transition-exposure rule, conditional
  timing rule, and one-shot analysis. **Owner skill:** external experiment owner.
  **Input needed:** hosts, workloads, and execution access. **Expected output:** immutable
  prefrozen campaign and held-out result. **Handoff required:** yes.
- **Priority 2 — Independent artifact:** publish an immutable archive and obtain a clean
  third-party rerun. **Owner skill:** artifact owner plus independent operator.
  **Input needed:** public hosting and external executor. **Expected output:** public
  checksums and independent evidence. **Handoff required:** yes.
- **Priority 3 — Submission metadata:** complete author, affiliation, funding, conflict,
  acknowledgment, disclosure, and anonymity fields. **Owner skill:** authors.
  **Input needed:** verified official metadata. **Expected output:** submission-ready
  front matter. **Handoff required:** no.
- **Priority 4 — Preserve local closure:** keep Round 53 frozen and do not tune against
  its diagnostic. **Owner skill:** artifact owner. **Input needed:** none. **Expected
  output:** unchanged deterministic evidence. **Handoff required:** no.

**Checks run:** Release configure/build; 29/29 CTests; Round 53 target and verifier;
two-run byte-stability comparison; Round 52/53 exact pair-set join; immutable model and
observation hashes; Python syntax checks; paper evidence and artifact manifest checks;
12-page PDF build; undefined-reference/overfull-box scan; citation-key and duplicate-key
audit; public-safe related-work refresh; full integrity and multi-reviewer review.

**Checks skipped:** no numerical solver re-execution for Round 53, no cohort search, no
policy tuning, no conditional timing inference, no external multi-host run, no public
independent rerun, and no claim of an exhaustive 2026 literature census.

**Unresolved risks:** independently reproduced multi-host performance, material fixed
improvement, recurring final-plan interaction exposure, beneficial calibrated
transitions, and verified submission metadata.
