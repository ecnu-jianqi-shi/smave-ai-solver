# CCF-A Full Review — Round 48

## 1. Report Metadata

- **Review date:** 2026-07-27.
- **Target venue/year/track:** IEEE Transactions on Parallel and Distributed Systems,
  regular research article; generic 2026 journal submission assumptions.
- **Paper title:** *Complete-Cost Expert Fusion for Verified Repeated Numerical Solves*.
- **Input materials reviewed:** complete LaTeX manuscript, rendered 12-page PDF,
  generated macros, claim ledger, bibliography, dated closest-work audit, frozen solver
  evidence, data locks, release tests, deterministic core archive, clean-tree evidence,
  and Round 48 integrity audit.
- **Search basis:** public-safe dated search through 2026-07-27 plus official IEEE,
  publisher, arXiv, PMLR, Crossref, and project metadata.
- **Report file:** `ccfa-review-reports/round-48-full-review.md`.
- **Reviewer mode:** full scientific, writing, format, artifact, and AC review.

## 2. Desk Rejection Assessment

- **Paper length — pass.** The IEEE Computer Society regular-paper limit is 12
  formatted pages; the synchronized PDF has 12 pages.
- **Abstract length — pass.** The abstract is 191 words, below the current 200-word
  regular-paper limit, after preserving the method, v5/v6 contrast, and scope boundary.
- **Topic compatibility — pass with moderate fit risk.** Verified heterogeneous solver
  composition, intra-node parallel verification, batching, and device paths fit TPDS,
  but the strongest evidence remains numerical-solver and single-host rather than
  distributed-system scale.
- **Minimum quality — pass.** The paper has a defined problem, assumptions, exact
  finite optimizer, implementation, broad evidence, limitations, and reproducibility
  package.
- **Policy/anonymity/compliance — uncertain.** The template is appropriate, but author,
  affiliation, correspondence, funding, conflict, and acknowledgment metadata remain
  placeholders. A submission-ready anonymity choice has not been supplied.
- **Prompt injection and hidden manipulation — pass.** No hidden reviewer instruction,
  prompt-injection text, or score manipulation was found.
- **Ethics and reviewability — pass.** No human-subject or private-data issue is present;
  licenses, residual limitations, hardware scope, negative results, and external-code
  boundaries are explicit.
- **Desk rejection risk:** low scientifically, medium administratively until the author
  block and submission mode are completed.

## 3. Paper Summary And Contribution Map

The paper studies repeated numerical solves where classical and learned candidates have
different costs, acceptance probabilities, correction paths, and fallback behavior. It
proposes a typed candidate--corrector--gate--continuation cascade, optimizes
reach-weighted complete path cost rather than candidate time, derives an ordering rule
under order-invariant statistics, and uses an exact dynamic program for finite
expert--budget actions. A production invariant requires original-equation acceptance
before any result returns. Evidence spans classical solvers, public sparse matrices,
PDE-derived workloads, learned operators, HINTS, intra-node scaling, devices, negative
transfer, frozen adverse/favorable final cohorts, and a clean reproduction archive.

- **Claimed problem:** routing and learned arithmetic gains can disappear after
  correction, verification, tracing, rejection, and numerical continuation.
- **Claimed gap:** existing hybrid solvers and selectors do not by themselves provide
  the paper's declared combination of typed complete-path optimization and a mandatory
  original-equation return invariant.
- **Method map:** typed actions; reach-weighted objective; ordering proposition; exact
  finite expert--budget/order optimization; calibrated/request-conditioned controls;
  mandatory gate and terminal solver.
- **Evidence map:** exact-oracle checks, broad regression suites, repeated paired timing,
  control ablations, negative outcomes, immutable v4--v6 sequence, 79-file data lock,
  and deterministic clean extraction.
- **Stated limitations:** single Apple M4 timing host, narrow final cohorts, mixed v5/v6
  validity, weak prediction calibration, non-universal speedups, no independent public
  archive, and incomplete author metadata.

## 4. Search And Related-Work Basis

- **Queries used:** verified solver cascades, neural/classical PDE routing, RHS-aware
  sparse solver selection, unseen-matrix prediction, online/run-local solver adaptation,
  learned preconditioning, sequential solver-parameter regret, and exact finite
  portfolio optimization.
- **Sources searched:** official publisher pages, arXiv, PMLR, OpenReview/ICLR records,
  Crossref, and the project source audit.
- **Closest works found:** HINTS, Greedy PDE Router, Error-Conditioned Neural Solvers,
  Lighthouse, SPECTRA, data-driven multiphysics solver selection, Learning to Relax,
  learned CG preconditioners, and FCG-NO.
- **Unverified related-work risks:** future or newly indexed 2026 work may narrow the
  positioning before submission; no currently located work invalidates the stated
  non-exclusive contribution boundary.
- **Source-quality screening:** pass. Dynamic, RHS-conditioned, unseen-matrix, online,
  and learned solver selection are treated as prior art; the paper makes no first claim.

## 5. Expected Review Outcome

- **Expected outcome:** 9/10, strong accept, not 10/10.
- **Main accept signal:** unusually complete alignment among exact finite optimization,
  production return safety, adverse-result preservation, and inspectable artifact
  evidence.
- **Main reject signal:** external validity remains mixed and narrow: v5 is not repaired,
  v6 beats global fixed by only 0.109%, both final cohorts contain three matrices and 24
  requests, and all authoritative timing is single-host.
- **Confidence:** 5/5.

## 6. Strengths And Weaknesses

### Strengths

- The paper optimizes complete expected execution rather than reporting candidate-only
  speedups, and its cost accounting includes rejection and terminal continuation.
- Exact dynamic programming is independently checked by exhaustive enumeration; the
  fixed-cascade order is checked over all 24 permutations.
- The original-equation gate is a return invariant, making router error a cost/fallback
  issue rather than permission to return an unchecked candidate.
- V4 contract failure, v5 adverse behavior, device regressions, transfer failures, and
  no-common-success cases remain visible instead of being filtered from the narrative.
- The clean artifact now builds from extraction, passes 29/29 CTests, checks frozen v6,
  replays v5 with zero solver execution, rebuilds the paper, and is generated twice
  byte-identically.
- The 191-word abstract, related-work boundary, final-routing table, discussion, and
  conclusion now agree on the narrow v6 success and adverse v5 result.

### Weaknesses

- **Weakness:** the favorable final cohort is not a broad routing validation.
  **Evidence basis:** three v6 matrices, 24 requests, no selected family adaptation or
  interaction transition, and RHS/tolerance controls tie the final anchor.
  **Reviewer deduction:** significance 4/5 rather than 5/5.
  **Required fix:** broader independently prefrozen validation across more matrices,
  hosts, or equation families; this is new scientific work, not a prose repair.
- **Weakness:** the same guard leaves v5 unchanged and 71.8% worse than global fixed.
  **Evidence basis:** zero-execution replay switches 0/24 requests at regret
  `4.4899717598` versus fixed `2.6129235277`.
  **Reviewer deduction:** the method cannot be scored as generally validated.
  **Required fix:** a genuinely general, development-derived safeguard validated on new
  untouched evidence; tuning against v5/v6 is prohibited.
- **Weakness:** TPDS-scale parallel/distributed evidence is limited.
  **Evidence basis:** one Apple M4, intra-node workers, no native distributed-memory or
  multi-node performance evaluation.
  **Reviewer deduction:** venue fit and impact remain below exceptional-best-paper level.
  **Required fix:** independent multi-architecture and distributed-scale evaluation.
- **Weakness:** the submission package is administratively incomplete.
  **Evidence basis:** explicit placeholders in `paper/authors.tex`.
  **Reviewer deduction:** no scientific score deduction, but submission cannot proceed.
  **Required fix:** verified author-provided metadata and an explicit anonymity mode.

## 7. Potentially Missing Related Work

- **Work:** no materially closer unaddressed work identified in the dated audit.
  **Status:** searched.
  **Why relevant:** the paper's contribution boundary depends on acknowledging dynamic,
  RHS-aware, online, and learned numerical solver selection.
  **Overlap:** those capabilities are prior art and are already cited.
  **Needed comparison:** retain the current non-exclusive positioning and refresh the
  search immediately before submission if substantial time passes.

## 8. Claim-Evidence Audit

| Claim | Where stated | Evidence provided | Strength | Reviewer deduction | Required fix |
| --- | --- | --- | --- | --- | --- |
| Complete-cost typed cascade | Abstract, Sections III--V | Objective, implementation, decomposition, ordering and DP checks | Strong | None | Preserve assumptions |
| Every return is gate-controlled | Abstract, design, discussion, conclusion | Runtime invariant and attempt traces | Strong | Not formal hardware/callback verification | Preserve risk boundary |
| V6 is near oracle | Abstract, evaluation, conclusion | Unique prefrozen run and frozen verifier | Strong but narrow | Small-cohort external-validity deduction | New independent evidence only |
| Guard is not a general repair | Abstract, evaluation, discussion | Frozen v5 zero-execution replay | Strong negative evidence | Prevents general safeguard claim | Preserve prominently |
| Parallel speedups | Abstract and evaluation | Paired worker/gate/batch evidence | Strong for one host | No distributed-scale extrapolation | Multi-host evidence for stronger claim |
| Broad solver/runtime coverage | Methodology and evaluation | SuiteSparse, PETSc, MSL, COPS, PDE, operators, HINTS | Strong coverage, heterogeneous contracts | Not all suites support common performance claims | Keep per-suite boundaries |
| Reproducible core package | Claim ledger and artifact docs | Deterministic archive and clean verifier | Strong author-operated evidence | Not public/independent reproduction | Public archive and third-party rerun if claimed |
| Novelty is verified complete-path composition | Introduction, related work, discussion | Dated closest-work audit and exact mechanism | Defensible | Originality 4/5, not first-of-kind certainty | Maintain no-first wording |

## 9. Experiment / Benchmark / Reproducibility Audit

- **Baselines:** workload-specific classical solvers, static/fixed/family controls,
  shared hybrid operator control, external HINTS, PETSc, MSL, and COPS paths are clearly
  separated. Hindsight oracles are labeled non-deployable.
- **Ablations:** candidate, correction, gate, trace, continuation, budget, routing
  features, interactions, order, shared control, worker count, and batch size are
  isolated with negative results retained.
- **Datasets/benchmarks:** evaluated-suite counts and the wider 79-file consumed-data
  lock are distinct; immutable final-cohort contracts prevent silent replacement.
- **Metrics:** complete runtime, regret, acceptance, residual/error, fallback, mismatch,
  confidence intervals, calibration error, and no-common-success counts match the
  direction and scope stated in text.
- **Statistical rigor:** authoritative PDE and scaling claims use 30 paired repetitions,
  counterbalancing, and fixed-seed bootstrap intervals. V6 remains a single prefrozen
  campaign by design and is not assigned a population confidence claim.
- **Robustness/failures:** v4 invalidity, v5 harm, device rejection, topology effects,
  order probes, and solver failures are visible.
- **Implementation detail:** finite actions, budgets, gate conditions, repetitions,
  train/calibration/held-out partitions, and terminal continuation are inspectable.
- **Artifacts:** release build, 29 CTests, evidence checker, manifest, PDF, data lock,
  deterministic archive, clean build, frozen v6 verification, and zero-execution v5
  replay pass in the synchronized pre-panel state.
- **Limitations:** appropriately explicit; no remaining prose inflation was found.

## 10. Multi-Reviewer Panel

### Reviewer A — Method And Soundness

- **Likely score:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** exact finite optimization, explicit assumptions, exhaustive
  oracle checks, and a production return invariant.
- **Main negative signal:** order optimality depends on order-invariant statistics, and
  the learned/calibrated layer is not generally validated.
- **Score-change condition:** 10 requires broader validation without relaxing the model
  boundary or tuning on final cohorts.

### Reviewer B — Evidence And Ablation

- **Likely score:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** broad multi-axis ablations, paired intervals, and retained
  adverse results make causal claims unusually inspectable.
- **Main negative signal:** v5 and v6 show opposite final-cohort outcomes, and the v6
  gain over fixed is only 0.109%.
- **Score-change condition:** independent, prefrozen replication across a broader
  population would be required for 10.

### Reviewer C — Novelty And Positioning

- **Likely score:** 8/10.
- **Confidence:** 4/5.
- **Main positive signal:** verified complete-path optimization and the return invariant
  are a coherent systems contribution.
- **Main negative signal:** most component ideas—solver selection, request features,
  online adaptation, learned preconditioning, and hybrid iteration—are prior art.
- **Score-change condition:** clearer theorem-level novelty or a demonstrated systems
  capability unavailable from prior compositions would increase originality.

### Reviewer D — Writing And Clarity

- **Likely score:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** the abstract is now compliant and the v5/v6 narrative is
  recoverable across abstract, table, discussion, and conclusion.
- **Main negative signal:** dense evidence breadth still makes the 12-page paper
  demanding, and several tables have underfull typography.
- **Score-change condition:** no decision-level writing defect remains; only optional
  compression and typography refinement are available.

### Reviewer E — Ethics And Reproducibility

- **Likely score:** 10/10 for audit discipline, 9/10 overall.
- **Confidence:** 5/5.
- **Main positive signal:** immutable negative evidence, license-aware locks, no solver
  re-execution, deterministic archives, and explicit non-public boundaries.
- **Main negative signal:** the archive is author-operated and local, not persistent or
  independently reproduced.
- **Score-change condition:** public immutable release plus a third-party rerun.

### Reviewer F — Domain Application

- **Likely score:** 8/10.
- **Confidence:** 4/5.
- **Main positive signal:** complete-path accounting matches practical repeated-solve
  deployment better than candidate-only benchmarking.
- **Main negative signal:** one hardware stack and limited final cohorts do not establish
  production transfer across scientific applications.
- **Score-change condition:** independent application deployments and multi-architecture
  evidence.

### Reviewer G — Reproducibility Specialist

- **Likely score:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** the clean extracted tree rebuilds, runs 29/29 tests, checks
  exact analyses, and preserves frozen v5/v6 evidence.
- **Main negative signal:** large external datasets and the official HINTS environment
  are not redistributed or rerun by the core bundle.
- **Score-change condition:** public data/code archival and independently executed
  external baselines.

### Reviewer H — Novice Advocate

- **Likely score:** 8/10.
- **Confidence:** 4/5.
- **Main positive signal:** research questions, contribution map, final-routing table,
  and limitations orient a careful reader.
- **Main negative signal:** the number of solver families, controls, and evidence layers
  creates a steep terminology burden.
- **Score-change condition:** a shorter evidence roadmap or visual taxonomy would help,
  but this is not required for correctness.

### Panel Synthesis

- **Agreement:** strong soundness, exceptional auditability, honest negative results,
  and no claim-evidence defect.
- **Disagreement:** novelty/impact reviewers assign 8 because the component methods are
  established and final-cohort generalization is mixed; reproducibility reviewers assign
  9--10 for artifact discipline.
- **Decisive positive axis:** exact verified complete-path composition with transparent
  adverse evidence and a clean deterministic artifact.
- **Decisive negative axis:** no general repair across v5/v6, tiny improvement over the
  strongest fixed control on v6, and one-host/small-cohort external validity.
- **Unresolved evidence:** independent multi-architecture, multi-cohort, and public
  reproduction; verified author metadata.
- **AC stance:** strong accept at 9/10; 10/10 is not scientifically defensible from the
  current evidence package.

## 11. Concerns Table

| ID | Severity | Concern | Evidence basis | Affected criterion | Fix class | Required action | Owner skill | Score-change condition |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| C1 | Moderate | Mixed final-cohort validity | v5 unchanged and 1.718× fixed; v6 near oracle | Significance, external validity | Experiment | New independently prefrozen validation, without v5/v6 tuning | Future experimental work | Required for 10 |
| C2 | Moderate | Limited TPDS-scale parallel evidence | One M4, intra-node workers, no native multi-node timing | Venue fit, impact | Experiment | Multi-architecture/distributed-scale study | Future experimental work | Required for exceptional score |
| C3 | Minor | Public independent reproduction absent | Author-operated local bundle only | Reproducibility | Reproducibility | Public immutable archive and third-party rerun | Artifact owner | Would strengthen 9 toward 10 |
| C4 | Minor | Administrative metadata incomplete | Placeholder author block | Compliance | Writing/compliance | Supply verified author/disclosure data and anonymity mode | Authors | Required for submission, not scientific score |

## 12. AC / Meta-Review

The panel finds no fatal or major soundness flaw. The manuscript is unusually candid:
the guard succeeds narrowly on v6 and fails to improve v5, and both facts are central.
The artifact repairs discovered during this round—five rather than two CTest matrices,
current data-lock counts, citation metadata, and abstract length—are complete and
validated. The decisive acceptance case is therefore sound method plus exceptional
auditability. The decisive ceiling is scientific breadth, not presentation: the current
evidence does not support a general routing repair or an exceptional TPDS-scale systems
claim. The calibrated decision remains 9/10.

## 13. Quantitative Scores

| Dimension | Score | Confidence | Evidence basis | Deduction / repair condition |
| --- | ---: | ---: | --- | --- |
| Quality | 5/5 | 5/5 | Complete method, implementation, evidence, and limitations | None |
| Clarity | 5/5 | 5/5 | 191-word abstract and synchronized v5/v6 narrative | Optional density reduction only |
| Significance | 4/5 | 5/5 | Broad runtime scope and complete-cost lesson | Mixed final cohorts and one-host evidence; broader independent validation needed |
| Originality | 4/5 | 4/5 | Verified complete-path composition and return invariant | Component solver selection/hybrid methods are prior art |
| Soundness | 5/5 | 5/5 | Exact/exhaustive checks and explicit assumptions | None within declared model |
| Evidence | 5/5 | 5/5 | Paired timing, ablations, failures, and immutable final cohorts | External validity remains bounded, not hidden |
| Reproducibility | 5/5 | 5/5 | Deterministic clean bundle and 29/29 tests | Public/independent rerun still absent |
| Ethics / Limitations | 5/5 | 5/5 | Licenses, risks, negative results, and exclusions are explicit | None |

- **Overall:** 9/10, strong accept.
- **Confidence:** 5/5.
- **Score-change condition:** 10/10 would require new independent scientific evidence
  demonstrating broader generalization and stronger TPDS-scale system validity, plus
  public/independent reproduction. No manuscript-only edit can justify that movement.

## 14. Questions For Authors

1. What verified author, affiliation, funding, conflict, acknowledgment, and
   corresponding-author metadata should replace `paper/authors.tex`?
2. Will the submission use standard single-anonymous or optional double-anonymous
   review, and have repository/artifact identity leaks been handled accordingly?
3. Is there a planned public immutable archive and independent rerun, or should the
   current author-operated boundary remain the submission claim?

## 15. Score Revision Criteria

- **Raising the score would require:** independent prefrozen multi-cohort and
  multi-architecture validation, stronger distributed-scale evidence, and public
  third-party reproduction without tuning on v5/v6.
- **Lowering the score would be triggered by:** hiding v5, inflating the 0.109% v6
  fixed-control gain, calling v6 a general repair, weakening the original-equation gate,
  or overwriting a frozen first-run artifact.
- **Concerns unlikely to change before submission:** mixed external validity and the
  absence of new independent scientific evidence under the current no-new-cohort rule.

## 16. Action Plan And CCFA Handoffs

- **Priority:** P0. **Action:** regenerate manifest, PDF record, deterministic archive,
  and clean-tree evidence after this report. **Owner:** artifact/reproducibility workflow.
  **Input needed:** synchronized source tree. **Expected output:** all checks pass and two
  archives are byte-identical. **Handoff required:** no.
- **Priority:** P0. **Action:** supply verified author and disclosure metadata and choose
  anonymity mode. **Owner:** authors. **Input needed:** real submission metadata.
  **Expected output:** submission-ready `paper/authors.tex`. **Handoff required:** yes.
- **Priority:** Future. **Action:** obtain broader independent validation before claiming
  general routing robustness or exceptional TPDS-scale impact. **Owner:** experimental
  research. **Input needed:** new prefrozen protocol, hardware, and untouched workloads.
  **Expected output:** external-validity evidence. **Handoff required:** yes.

- **Checks run:** Release configure/build; 29/29 CTests; paper evidence; citation-key,
  metadata, and context audit; data-lock verification record; direct LaTeX build;
  deterministic double generation; clean extracted-tree verifier; hidden-instruction and
  path-privacy scans.
- **Checks skipped:** v5/v6 selection-manifest deterministic regeneration because the
  original byte-locked `ssstats.csv` is unavailable locally; frozen payload/output
  contracts remain checked. No solver timing was rerun for v5/v6.
- **Unresolved risks:** author metadata, public/independent reproduction, and scientific
  external validity beyond the frozen cohorts and one timing host.

## Output Self-Check

- Scores, confidence, evidence, and repair conditions are internally consistent.
- Every score below 5/5 states its deduction and movement condition.
- The report preserves the v5 negative result and the 0.109% v6 effect size.
- No acceptance probability, new experiment, author identity, citation, or policy result
  is invented.
- The score remains 9/10 because 10/10 is not supported by the current scientific
  evidence, even after all manuscript and artifact repairs.
