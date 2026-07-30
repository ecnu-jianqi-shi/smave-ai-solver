# CCF-A Full Review — Round 38

## 1. Report Metadata

- **Review date:** 2026-07-27.
- **Target venue/year/track:** IEEE Transactions on Parallel and Distributed Systems,
  regular paper; current-year official venue guide was not available in the local
  skill tree.
- **Paper:** *Complete-Cost Expert Fusion for Verified Repeated Numerical Solves*.
- **Input materials reviewed:** modular 12-page manuscript under `paper/`; current
  solver implementation; public SuiteSparse request-conditioned route evidence;
  generated evidence macros; claim/evidence ledger; artifact snapshot and manifest;
  Round 35--37 review history; Release build and test records.
- **Search basis:** public-safe closest-work search inherited from Rounds 35--36;
  no new method claim was introduced in this round, so no private manuscript text was
  used in a public query.
- **Report file:** `ccfa-review-reports/round-38-full-review.md`.
- **Reviewer mode:** full scientific, writing, format, artifact, and AC synthesis.
- **Scope authority:** solver mechanisms, numerical acceptance, complete-path cost,
  routing, solver-internal parallelism, baselines, and reproducible evidence only.
  Deployment and operations architecture do not contribute to the score.

## 2. Desk Rejection Assessment

- **Paper length:** pass. The local build is 12 pages and the manuscript is complete
  enough for substantive review.
- **Topic compatibility:** pass. The paper addresses solver composition, numerical
  verification, and parallel numerical workloads relevant to TPDS.
- **Minimum quality:** pass. The method, implementation contract, evaluation protocol,
  negative results, and limitations are inspectable.
- **Policy/anonymity/compliance:** uncertain. The local manuscript contains unfinished
  author/disclosure metadata, and current-year official venue policy was not available
  locally; these are submission checks rather than scientific deductions here.
- **Prompt injection and hidden manipulation detection:** pass. No manuscript text,
  artifact, or review history was treated as an instruction to inflate the score.
- **Ethics and reviewability:** pass. The work makes explicit correctness and scope
  boundaries and does not claim universal solver or hardware dominance.

## 3. Paper Summary And Contribution Map

The paper treats repeated numerical solves as selection over typed pipelines rather
than selection of a single kernel. A pipeline may generate a candidate, apply a
correction, recompute an acceptance quantity from the supplied original equation, and
continue to another numerical path after rejection. The reach-weighted complete-cost
objective charges candidate work, correction, gate, transfer, and continuation. Under
order-invariant stage statistics, the paper derives a cost-per-acceptance ordering rule;
for finite expert--budget actions, it uses an exact bounded dynamic program to choose a
subset, budget, and order. The numerical gate, not the learned candidate or router,
determines whether a result is returned.

The contribution map is coherent:

1. complete-cost formulation and ordering/DP results (`paper/sections/03_problem_formulation.tex`);
2. role-constrained candidate--corrector--original-equation-gate--continuation execution
   (`paper/sections/04_system_design.tex`, `paper/sections/05_verification_aware_fusion.tex`);
3. production execution of PCG, GMRES, sparse ILUT/ILU(0), and direct actions under
   budgets and a common gate;
4. broad solver evidence spanning PDEBench-derived workloads, operator studies, HINTS,
   gates, scaling, complete cost, and a public matrix-ID-disjoint SuiteSparse route.

The new public SuiteSparse experiment is decision-relevant and honestly negative. It
uses 6/4/4 matrix-ID-disjoint train/calibration/held-out matrices, 48/32/32 requests,
19 modeled PCG/GMRES/direct actions, and 3,312 raw action observations. On 32 held-out
requests, conditioned regret is `1.540301`, better than the static profile's `1.884023`
but worse than fixed SuperLU's `1.323105` (lower is better). All 32 production requests
succeed, with 16 terminal numerical continuations and zero gate, plan-order, or
DP/exhaustive mismatches. The same experiment reports cost median/p95/max relative
errors of `0.797138`/`4.541954`/`7.169880`, pass Brier/ECE of
`0.422069`/`0.424017`, and maximum action calibration error `0.999056`.

The stated limitations are materially aligned with the evidence: one Apple M4 timing
platform, one-dimensional HINTS coverage, no Greedy PDE Router reproduction, a bounded
order-invariant policy, and the public route's loss to fixed SuperLU
(`paper/sections/09_discussion_limitations.tex:15-40`).

## 4. Search And Related-Work Basis

- **Queries used:** HINTS neural operator relaxation PDE solver; Greedy PDE Router;
  FCG-NO; learned preconditioners; numerical algorithm selection and solver portfolios;
  complete-cost solver selection; official TPDS author guidance.
- **Sources searched:** public primary or authoritative pages and papers recorded in
  the Round 35--36 review basis, including publisher/proceedings pages, arXiv or
  project pages where appropriate, and the manuscript's cited primary literature.
- **Closest works found:** HINTS for neural--relaxation coupling; Greedy PDE Router
  for iteration-level hybrid action selection; FCG-NO and learned preconditioners for
  learned components inside Krylov methods; Rice/SATzilla-style algorithm selection and
  portfolios for feature-to-solver choice.
- **Positioning assessment:** the manuscript distinguishes iteration-level hybrid
  policies and component learning from request-level typed pipeline selection with
  complete verified cost and mandatory original-equation acceptance
  (`paper/sections/02_background_related_work.tex:35-72`).
- **Unverified related-work risks:** no exhaustive census of solver-selection work
  published after the inherited search date; no new external method was added in this
  round. This limits confidence in the breadth of the novelty claim but does not create
  a demonstrated overlap.
- **Source-quality screening status:** adequate for the inherited positioning; current
  venue policy remains unverified because the expected local TPDS guide was absent.

## 5. Expected Review Outcome

- **Expected outcome:** **8/10, accept**.
- **Main accept signal:** the paper offers a technically coherent verified solver
  composition contract, exact finite-action optimization, production execution, and an
  unusually explicit complete-cost/negative-result evidence package.
- **Main reject signal:** the public action-selection result does not beat the strongest
  fixed baseline, and its predicted cost/pass statistics are too poorly calibrated to
  support a strong claim that request-conditioned learning is reliable.
- **Confidence:** 5/5. The source, implementation, frozen evidence, and prior review
  history are available; the missing current-year venue guide affects format certainty,
  not the scientific assessment.

## 6. Strengths And Weaknesses

### Strengths

- **Verified return invariant:** candidate quality can affect cost and continuation but
  cannot bypass the original residual, constraint, consistency, or defect gate
  (`paper/sections/01_introduction.tex:39-44`, `paper/sections/05_verification_aware_fusion.tex:24-70`).
- **Complete-cost framing:** the objective charges the stages that commonly erase a
  candidate's apparent kernel advantage, and the evaluation separates gate-only from
  complete-path scaling (`paper/sections/03_problem_formulation.tex`,
  `paper/sections/07_evaluation.tex:243-287`).
- **Algorithmic exactness within its model:** exact DP/exhaustive agreement is zero in
  the public route, and the same selected rejection--continuation--acceptance path is
  checked in production (`paper/sections/06_experimental_methodology.tex:106-124`).
- **Evidence honesty:** the paper keeps the nonlinear regression, failed device paths,
  transfer failures, order sensitivity, public fixed-SuperLU loss, and prediction-tail
  failures instead of reporting only aggregate wins.
- **Reproducibility:** the frozen evidence files, generated macros, claim ledger,
  artifact checks, clean build, and 29/29 CTests provide a strong audit trail.

### Weaknesses

- **Weakness:** the strongest public policy comparison is negative for the learned
  route. The conditioned route improves a static profile but loses to fixed SuperLU at
  `1.540` versus `1.323` regret.
  **Evidence basis:** `build/release/suitesparse-request-conditioned-route/evidence.txt`;
  `paper/abstract.tex:14-19`; `paper/sections/07_evaluation.tex:145-157`.
  **Reviewer deduction:** the paper supports verified solver composition and adaptive
  improvement over a weak static policy, but not robust superiority over a strong fixed
  sparse-direct policy.
  **Required fix:** retain the negative result prominently and frame public routing as
  a bounded feasibility/diagnostic result, not as a demonstrated portfolio win.

- **Weakness:** prediction quality is not adequate for a confident learned routing
  claim. Cost p95/max relative errors are `4.542`/`7.170`; Brier/ECE are
  `0.422`/`0.424`; maximum action calibration error is `0.999`.
  **Evidence basis:** public route evidence and generated macros.
  **Reviewer deduction:** mean or median prediction quality does not characterize the
  tail risk that directly determines cascade order and complete cost.
  **Required fix:** either add uncertainty/tail-aware selection or narrow the claim to
  feature-conditioned routing that is useful relative to a static profile but not yet
  reliable against strong fixed controls.

- **Weakness:** the public portfolio is still narrow: 14 matrices from one positive-
  definite SuiteSparse source, only 4 held-out matrix IDs, a 5,000-row eligibility
  limit, and a 512-row built-in direct threshold.
  **Evidence basis:** `evidence.txt` fields `matrix_class_source`, split counts,
  `matrix_row_limit`, and `built_in_direct_row_limit`.
  **Reviewer deduction:** the route result is a valuable public stress test but does not
  establish transfer to nonsymmetric, indefinite, larger, or structurally different
  sparse systems.
  **Required fix:** report this boundary next to the result and, for a higher score,
  add family-balanced public matrices with the same complete trace/gate contract.

- **Weakness:** the exact policy assumes order-invariant action statistics while the
  public probes measure interaction/order deltas of `0.022386`/`0.008578`.
  **Evidence basis:** `paper/sections/07_evaluation.tex:155-157` and
  `action-interactions.tsv`.
  **Reviewer deduction:** the DP is exact only for the declared bounded model, not for
  the measured stateful execution environment.
  **Required fix:** keep the assumption explicit and either add a bounded interaction
  correction or show that the observed deltas do not change the decision-level ranking.

## 7. Potentially Missing Related Work

### HINTS, Greedy PDE Router, And FCG-NO

- **Work:** HINTS, Greedy PDE Router, and FCG-NO.
- **Status:** searched and already user-provided in the manuscript/review basis.
- **Why relevant:** they combine neural components with classical iterative numerical
  methods at different routing or correction granularities.
- **Overlap:** hybrid numerical execution and learned action selection.
- **Needed comparison:** preserve the manuscript's axis distinction—iteration-level
  action policy or learned preconditioner versus request-level typed pipeline, complete
  cost, and original-equation acceptance. No claim that the present route beats these
  methods is justified by the current evidence.

### Classical Algorithm Selection And Portfolios

- **Work:** Rice-style algorithm selection, SATzilla-style portfolios, and numerical
  solver portfolio systems.
- **Status:** searched and cited; exact post-search census unverified.
- **Why relevant:** they provide the closest conceptual precedent for feature-based
  solver choice and regret evaluation.
- **Overlap:** feature-to-action selection and portfolio comparison.
- **Needed comparison:** explain that the differentiating object is a verified
  candidate--correction--gate--continuation pipeline with reach-weighted cost, not generic
  algorithm selection. The public fixed-SuperLU loss means this distinction is a method
  contribution, not evidence of universal portfolio dominance.

## 8. Claim-Evidence Audit

| Claim | Where stated | Evidence provided | Strength | Reviewer deduction | Required fix |
| --- | --- | --- | --- | --- | --- |
| Complete-cost selection must charge candidate, correction, gate, and continuation | `paper/sections/01_introduction.tex:32-44`; `paper/sections/03_problem_formulation.tex` | Formal objective, decomposition, complete-path measurements | Strong | None material | Keep complete-cost language adjacent to all speedup claims |
| Exact finite-action policy matches exhaustive optimization | `paper/abstract.tex:8-10`; `paper/sections/07_evaluation.tex:140-143,153-157` | Zero DP/exhaustive mismatch in controlled and public route evidence | Strong within assumptions | Order interaction makes the environment only approximately order-invariant | State “exact under the declared action-statistics model” everywhere |
| Original equations determine acceptance | `paper/abstract.tex:11-12`; `paper/sections/05_verification_aware_fusion.tex:24-70` | Independent original-matrix residual/gate checks; zero public gate mismatch | Strong | Gate validity is conditional on the supplied equation/callback | Retain existing callback-risk limitation |
| Request-conditioned routing improves static routing | `paper/abstract.tex:14-18`; `paper/sections/07_evaluation.tex:145-153` | Public regret `1.540` versus static `1.884` | Supported but modest | It does not improve fixed SuperLU at `1.323` | Keep the strong-fixed-baseline clause in abstract and conclusion |
| Request-conditioned routing is calibrated | `paper/sections/07_evaluation.tex:149-152` | Brier/ECE and tail errors are reported | Mixed/negative | `0.999` maximum action calibration error is poor | Call calibration weak and add tail-aware control before broad claims |
| Verified expert fusion accelerates qualified repeated workloads | `paper/abstract.tex:20-32`; `paper/sections/07_evaluation.tex` | Seven PDEBench-derived comparisons, operator studies, HINTS, gates, scaling | Supported with workload qualification | One host and heterogeneous workload-specific baselines limit generalization | Preserve qualified—not universal—wording |
| Hardware placement/device paths are part of the scientific result | `paper/abstract.tex:30-31`; `paper/sections/07_evaluation.tex` | Device and transfer failures plus placement probes | Supported as boundary analysis | Native performance is one-host and device-specific | Do not generalize beyond measured placement |

## 9. Experiment / Benchmark / Reproducibility Audit

- **Baselines:** strong in breadth—structured direct, PCG, GMRES-ILU/ILUT, external
  model solvers, shared learned hybrid, HINTS, and fixed controls. The public route's
  fixed SuperLU result is particularly important and should remain the primary negative
  policy comparison.
- **Ablations:** feature-group ablations, correction/gate/continuation controls, routing
  shifts, strict-gate versus complete-path studies, order sensitivity, and device
  failures are present. A tail-aware predictor/control ablation is missing.
- **Datasets and splits:** matrix-ID-disjoint public split and disjoint held-out routes
  are a clear strength. The SuiteSparse public route remains one matrix class, with four
  held-out IDs and bounded row eligibility; this is the main external-validity limit.
- **Metrics:** regret, complete runtime, cost error, Brier/ECE, gate mismatch,
  numerical continuation, DP/exhaustive agreement, and interaction/order deltas are
  decision-relevant. Lower regret and prediction-tail interpretation should be stated
  explicitly wherever the numbers appear.
- **Statistical rigor:** paired repetitions and bootstrap contracts are strong for the
  timing suites. The 32 held-out public requests are sufficient to expose failure modes
  but too small to support broad population claims.
- **Robustness and failure cases:** strong. The manuscript retains nonlinear regression,
  failed device candidates, transfer failures, no-common-success cases, fallback
  continuation, and public routing loss to fixed SuperLU.
- **Implementation details:** strong. Production action budgets, CSR sparse ILUT,
  terminal numerical continuation, original-equation gate, and trace fields are
  inspectable.
- **Artifacts and reproducibility:** strong. `paper/check_evidence.py`,
  `paper/check_artifact_manifest.py`, the Release build, CTests, and the public route
  reproduction target provide auditable locks. Current PDF metadata must be synchronized
  after this review round.
- **Limitations:** unusually candid and aligned with the data. The paper should not
  add infrastructure or operations milestones; the unresolved items are solver
  prediction tails, family breadth, action interaction, and performance portability.

## 10. Multi-Reviewer Panel

### Reviewer R1 — Numerical Algorithms

- **Expertise:** Krylov/direct methods, numerical stability, solver contracts.
- **Likely score:** 8/10.
- **Confidence:** 5/5.
- **Main positive signal:** the original-equation gate and executable PCG/GMRES/direct
  actions make the pipeline a real numerical solver composition rather than a latency
  wrapper.
- **Main negative signal:** the public route loses to fixed SuperLU and the DP assumes
  order-invariant statistics despite measured interactions.
- **Evidence basis:** public evidence, `paper/sections/03_problem_formulation.tex`,
  and `paper/sections/05_verification_aware_fusion.tex`.
- **Score-change condition:** raise only after an interaction-aware or tail-aware route
  competes with the strong fixed baseline on family-balanced public matrices; lower if
  any gate mismatch or hidden acceptance error appears.

### Reviewer R2 — Parallel Solver Systems

- **Expertise:** complete-path parallel runtime and performance measurement.
- **Likely score:** 8/10.
- **Confidence:** 5/5.
- **Main positive signal:** gate-only versus complete-path scaling is separated, and
  paired order probes prevent a simple dispatch artifact from carrying the result.
- **Main negative signal:** authoritative timing is one Apple M4 stack, and the public
  route does not show decision-level superiority over a strong fixed sparse-direct path.
- **Evidence basis:** `paper/sections/06_experimental_methodology.tex:20-28,126-138` and
  `paper/sections/07_evaluation.tex`.
- **Score-change condition:** raise with cross-platform or stronger public solver
  evidence; lower if complete-cost accounting or baseline setup costs are asymmetric.

### Reviewer R3 — Scientific Machine Learning

- **Expertise:** learned operators, calibration, distribution shift.
- **Likely score:** 7--8/10.
- **Confidence:** 5/5.
- **Main positive signal:** learned candidates are subordinate to numerical correction
  and the original gate, and the paper reports model shifts and negative transfer.
- **Main negative signal:** public cost tails and calibration are poor enough that the
  learned selector can be brittle under request shift.
- **Evidence basis:** public route error/calibration fields and
  `paper/sections/07_evaluation.tex:145-157`.
- **Score-change condition:** raise with calibrated uncertainty or conservative
  tail-aware action selection; lower if the manuscript hides the current calibration
  failures or implies learned routing beats fixed SuperLU.

### Reviewer R4 — Novelty And Positioning

- **Expertise:** algorithm portfolios, mixtures of experts, hybrid numerical methods.
- **Likely score:** 8/10.
- **Confidence:** 4/5.
- **Main positive signal:** the manuscript clearly separates request-level typed pipeline
  selection from iteration-level hybrid routers and learned preconditioners.
- **Main negative signal:** the formulation combines recognizable portfolio, cascade,
  correction, and verification ideas; novelty depends on their precise co-design and
  numerical contract rather than any isolated component.
- **Evidence basis:** `paper/sections/02_background_related_work.tex:35-72` and the
  contribution list in `paper/sections/01_introduction.tex:55-90`.
- **Score-change condition:** raise with a sharper theorem/algorithmic distinction or a
  decisive public comparison; lower if a close prior system already provides the same
  reach-weighted verified pipeline.

### Reviewer R5 — Evidence And Ablation

- **Expertise:** benchmark design, controls, statistical inference.
- **Likely score:** 8/10.
- **Confidence:** 5/5.
- **Main positive signal:** complete-path cost, negative outcomes, production traces,
  gate outcomes, and public held-out regret are all reported.
- **Main negative signal:** 32 public held-out requests and one matrix family cannot
  support a broad claim about public sparse solver portfolios.
- **Evidence basis:** `paper/sections/06_experimental_methodology.tex:113-124` and the
  public evidence file.
- **Score-change condition:** raise with family-balanced public matrices and uncertainty
  controls; lower if a rerun changes the frozen numbers or exposes leakage.

### Reviewer R6 — Reproducibility And Artifacts

- **Expertise:** deterministic artifacts, build/test auditability.
- **Likely score:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** evidence, manifests, generated values, Release build, 29/29
  CTests, and the public reproduction target are mutually inspectable.
- **Main negative signal:** large external benchmark payloads and official HINTS runtime
  dependencies remain outside the core bundle, so a clean rerun is not entirely
  self-contained.
- **Evidence basis:** `paper/ARTIFACT_SNAPSHOT.md:86-112` and the artifact scripts.
- **Score-change condition:** raise only with a fully self-contained public route or
  precise external-data lock; lower if manifest/PDF hashes are stale.

### Reviewer R7 — Writing And Clarity

- **Expertise:** technical exposition and reviewer-facing claim discipline.
- **Likely score:** 8/10.
- **Confidence:** 5/5.
- **Main positive signal:** gate/continuation terminology is defined numerically, and the
  abstract now states the public fixed-SuperLU negative result.
- **Main negative signal:** the manuscript is dense and contains many parallel evidence
  suites; the distinction between “improves static” and “beats the strongest fixed
  action” must remain visually immediate.
- **Evidence basis:** `paper/abstract.tex:14-32` and `paper/sections/07_evaluation.tex:135-163`.
- **Score-change condition:** raise with no major change needed beyond local compression;
  lower if summary text drops the fixed-baseline caveat.

### Reviewer R8 — AC / Venue Fit

- **Expertise:** TPDS-level systems and numerical-computing significance.
- **Likely score:** 8/10.
- **Confidence:** 4/5 because current-year venue guidance was unavailable locally.
- **Main positive signal:** the work connects numerical validity to complete runtime and
  parallel verification, a reasonable TPDS systems contribution.
- **Main negative signal:** the strongest public adaptive-routing result is diagnostic
  rather than dominant, and cross-platform performance remains open.
- **Evidence basis:** full manuscript, especially Sections 6--10.
- **Score-change condition:** raise with a solver-only decision-level win against strong
  controls; lower if venue policy requires evidence or page constraints not reflected in
  the local manuscript.

### Panel Synthesis

- **Agreement:** the verified numerical contract, complete-cost accounting, production
  execution, and artifact discipline are genuine strengths.
- **Disagreement:** R3 and R5 treat the public learned-route result as a substantial
  evidence weakness; R1/R2/R6 give more weight to the method and broad non-routing
  evidence.
- **Decisive positive axis:** solver composition with mandatory original-equation
  acceptance and complete-path evidence.
- **Decisive negative axis:** no demonstrated adaptive-policy advantage over fixed
  SuperLU on the public sparse portfolio, combined with poor prediction tails.
- **Unresolved evidence:** family-balanced larger sparse portfolio, tail-aware policy,
  interaction-aware optimization, and cross-platform native timing.
- **AC stance:** **8/10, accept**, conditional on preserving the negative result and
  narrowing the routing claim to the evidence actually established.

## 11. Concerns Table

| ID | Severity | Concern | Evidence basis | Affected criterion | Fix class | Required action | Owner skill | Score-change condition |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| R38-C1 | major | Public conditioned routing loses to fixed SuperLU | `evidence.txt`; abstract and evaluation | Evidence, significance | experiment/writing | Keep `1.540/1.884/1.323` visible and state lower regret is better | ccf-paper-writer / solver experiments | Raise only after a strong-control public win or a narrower, fully supported claim |
| R38-C2 | major | Cost and pass prediction tails are poor | `evidence.txt`: p95/max, Brier/ECE, max calibration | Soundness, evidence | method/soundness | Add tail-aware uncertainty/control or explicitly bound the learned-policy claim | solver modeling | Raise after worst-case route regret improves without weakening the gate |
| R38-C3 | moderate | SuiteSparse portfolio is narrow and class-limited | `evidence.txt`: 6/4/4 split, one source class, row limits | Evidence, significance | experiment | Add family-balanced public matrices and preserve complete traces | solver experiments | Raise after held-out public breadth changes the decision-level result |
| R38-C4 | moderate | DP model assumes order-invariant actions despite measured deltas | `action-interactions.tsv`; evaluation lines 155--157 | Soundness | method/soundness | Add bounded interaction correction or report ranking stability under measured deltas | solver methods | Raise after interaction-aware route is evaluated; lower if deltas alter decisions materially |
| R38-C5 | minor | Dense evidence suite and fixed-baseline caveat can be missed | abstract/evaluation density | Clarity | writing/compression | Keep the strong-fixed-baseline sentence adjacent to public route numbers | ccf-paper-writer | Lower if the caveat is removed or abstract implies universal dominance |
| R38-C6 | minor | Current PDF snapshot/manifest metadata need synchronization | `paper/ARTIFACT_SNAPSHOT.md`, `paper/ARTIFACT_MANIFEST.txt` | Reproducibility | reproducibility | Update review date, PDF size/hash, and manifest date consistently | artifact audit | Lower if final checks fail or hashes disagree |

## 12. AC / Meta-Review

The paper should be evaluated as a solver-method paper, not as a deployment or service
governance paper. Its strongest contribution is the explicit composition of candidate,
correction, original-equation acceptance, and numerical continuation under a complete
cost objective. The public SuiteSparse experiment improves the paper because it is
matrix-ID-disjoint, production-backed, and honest about losing to fixed SuperLU; it does
not justify an upgrade to a top score. The result is an above-average, technically
credible paper with a meaningful method and unusually strong auditability, but the
adaptive routing claim remains bounded by calibration tails, portfolio breadth, and
action interactions.

- **Reviewer consensus:** accept-leaning, with no fatal correctness issue found.
- **Reviewer disagreement:** how much the public fixed-baseline loss should discount
  the broader composition contribution.
- **Decisive acceptance axis:** numerical validity plus complete-path solver evidence.
- **Decisive rejection axis:** would be triggered by hidden gate mismatch, asymmetric
  baseline accounting, leakage, or overclaiming the learned route; none is observed.
- **AC stance:** **8/10, accept, confidence 5/5**.
- **Discussion risks:** reviewers may read the many positive PDE/operator speedups as a
  universal routing claim unless the public SuperLU negative and one-host boundary stay
  in the abstract, evaluation, discussion, and conclusion.

## 13. Quantitative Scores

| Dimension | Score (1-5) | Confidence (1-5) | Evidence basis | Deduction / score-change condition |
| --- | ---: | ---: | --- | --- |
| Novelty | 4 | 4 | Typed candidate--corrector--gate--continuation composition, reach-weighted cost, and exact bounded expert--budget optimization | Components have recognizable portfolio/hybrid precedents; raise with a sharper decisive algorithmic distinction or stronger closest-method comparison |
| Soundness | 4 | 5 | Exact DP/exhaustive checks, production gates, budget execution, and retained order-interaction measurement | Exactness is conditional on order-invariant statistics; raise after interaction-aware validation or lower if measured interactions change decisions |
| Evidence | 4 | 5 | Seven PDEBench-derived suites, operator/HINTS studies, public SuiteSparse traces, negatives, artifacts | Public adaptive route loses to fixed SuperLU and has weak calibration; raise after strong-control public evidence or tail-aware route |
| Significance | 4 | 4 | Complete verified cost addresses a real repeated-solve systems problem | Workload qualification and one-host timing limit broad impact; raise with family-balanced public decision-level wins |
| Clarity | 4 | 5 | Clear definitions and scope, but dense multi-suite narrative | Keep the fixed-SuperLU negative adjacent to all public routing claims; otherwise no major repair |
| Reproducibility | 5 | 5 | Frozen evidence, generated values, manifests, Release build, 29/29 CTests, and public reproduction target | Core bundle excludes large/external dependencies; raise only with fully self-contained public route or precise locks |
| Ethics / Limitations | 5 | 5 | Explicit callback, portability, workload, HINTS, order, and public-route limitations | No material deduction; preserve solver-only scope and negative results |

**Overall:** **8/10**  | **Scholarly Confidence:** **5/5**

**Recommendation:** accept

**Verdict:** The paper is technically credible and reviewable, but the current evidence
supports a bounded solver-composition contribution rather than a robust adaptive-routing
win. A 9/10 stance would require the routing evidence to compete with strong fixed
controls or a materially stronger explanation/control of prediction tails. A 7/10 or
lower stance would be warranted if the manuscript overclaims learned-route superiority,
if complete costs are found asymmetric, or if any original-equation gate mismatch is
uncovered.

## 14. Questions For Authors

1. Can the public SuiteSparse result report per-matrix and per-request route choices,
   so readers can distinguish useful feature conditioning from a small number of large
   timing outliers?
2. Does a conservative cost upper confidence bound or calibrated abstention change the
   `1.540` conditioned regret without changing the original-equation gate or terminal
   continuation contract?
3. How often do the measured `0.022386` interaction and `0.008578` order deltas change
   the selected plan, rather than only its realized time?
4. Are all fixed SuperLU and conditioned actions charged with identical setup,
   factorization, gate, trace, and continuation accounting for every held-out request?

## 15. Score Revision Criteria

### Raising the score would require

- A family-balanced, matrix-ID-disjoint public sparse portfolio in which an interaction-
  or tail-aware conditioned route competes with the strongest fixed baseline under the
  same complete cost and original-equation gate.
- A calibrated uncertainty or conservative action policy that reduces p95/max cost
  error and worst-case route regret without weakening acceptance.
- A measured analysis showing that action interactions do not change decisions, or a
  bounded interaction-aware extension to the optimizer.

### Lowering the score would be triggered by

- Any gate mismatch, erroneous acceptance, hidden continuation, or omitted setup/gate/
  trace cost in the public or existing complete-path evidence.
- A stale or inconsistent generated macro, artifact manifest, or PDF snapshot after the
  final synchronization.
- Wording that implies the conditioned public route beats fixed SuperLU, is universally
  optimal, or transfers across hardware/families without evidence.
- A reanalysis showing that the public result depends on matrix leakage, an unfair
  baseline, or non-reproducible timing outliers.

### Concerns unlikely to change before submission

- One-host performance portability.
- One-dimensional official HINTS coverage and no Greedy PDE Router execution.
- Large external benchmark payloads remaining outside the deterministic core bundle.

## 16. Action Plan And CCFA Handoffs

| Priority | Action | Owner skill | Input needed | Expected output | Handoff required |
| --- | --- | --- | --- | --- | --- |
| P0 | Synchronize Round 38 review date, PDF size/hash, and artifact manifest | ccf-integrity-auditor | Current `paper/main.pdf` and frozen run IDs | Consistent snapshot and passing manifest check | No |
| P0 | Preserve the public SuperLU negative and calibration-tail numbers in all summaries | ccf-paper-writer | Current generated SuiteSparse macros | Solver-only abstract, conclusion, README, and claim ledger | No |
| P1 | Evaluate tail-aware or interaction-aware route control | solver experiments | Frozen public traces and action interactions | New regret/calibration/decision-stability evidence | No |
| P1 | Expand public matrices by family while preserving matrix-ID disjointness | solver experiments | Public sparse matrix set and complete-cost contract | Family-balanced route evaluation | No |
| P2 | Verify current-year TPDS formatting/anonymity policy before submission | author | Official venue instructions | Final compliance pass | Yes |

- **Checks run:** public SuiteSparse evidence inspection; manuscript claim/evidence
  audit; Round 35--37 history comparison; scope-term scan; review-standard and rubric
  checks; current PDF size/hash inspection.
- **Checks skipped:** no new public-web search; no new solver experiment; no native
  HINTS rerun; no Greedy PDE Router execution; no current-year official TPDS policy
  verification because the local guide was unavailable.
- **Unresolved risks:** calibration tails, strong-fixed-baseline competition, public
  matrix-family breadth, order interaction, one-host timing portability, and final
  submission metadata.

## Output Self-Check

- Section order follows the standard full-review contract.
- Criterion scores, overall score, confidence, reviewer tendencies, and AC stance are
  internally consistent.
- No non-solver operations item is used as a contribution, deduction, score condition,
  or future-work requirement.
- The public negative result is preserved exactly: conditioned `1.540301`, static
  `1.884023`, fixed SuperLU `1.323105`; lower regret is better.
- Every score below 5 includes a concrete deduction and a repair condition.
- No result, citation, performance claim, or external reproduction is invented.
