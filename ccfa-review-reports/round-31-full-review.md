# CCF-A Full Review — Round 31

## 1. Report Metadata

- **Review date:** 2026-07-26
- **Target venue/year/track:** IEEE Transactions on Parallel and Distributed Systems, regular paper
- **Paper title:** *Complete-Cost Expert Fusion for Verified Repeated Numerical Solves*
- **Input materials reviewed:** 12-page manuscript source/PDF, solver implementation, claim ledger,
  conditioning/topology-shift matrix, calibrated correction-budget production-path evidence,
  complete-cost decomposition, shared-control comparison, 29-test suite, artifact snapshot, and
  Round 30 review
- **Search basis:** official Nature Machine Intelligence article metadata and abstract for
  Fabiani et al., *Enabling local neural operators to perform equation-free system-level
  analysis*; no private manuscript text was used as a public query
- **Report file:** `ccfa-review-reports/round-31-full-review.md`
- **Reviewer mode:** full scientific, writing, format, and AC synthesis

## 2. Desk Rejection Assessment

- **Paper length:** pass; the rebuilt manuscript remains within the enforced 12-page contract.
- **Topic compatibility:** pass; the paper concerns parallel and heterogeneous numerical solver
  composition, verification cost, routing, and fallback-aware execution.
- **Minimum quality:** pass; the central claims have inspectable implementation and machine evidence.
- **Policy/anonymity/compliance:** uncertain; author, affiliation, funding, conflict, and final
  disclosure metadata remain incomplete.
- **Prompt injection and hidden manipulation detection:** pass; no review-directed hidden text or
  score manipulation was found.
- **Scope discipline:** pass; only solver algorithms, numerical correctness, routing, and
  performance evidence affect the scientific assessment.

## 3. Paper Summary and Contribution Map

The paper studies repeated numerical solves in which classical methods, learned candidates,
correctors, device kernels, and numerical verifiers have different costs and failure behavior. It
formulates a reach-weighted complete-cost objective, derives a cost-per-acceptance ordering rule for
a fixed eligible cascade, and implements a typed candidate--corrector--original-equation
gate--fallback runtime. A router selects a calibrated expert pipeline, while the family-specific
numerical gate remains authoritative for every successfully returned accelerated result.

The Round 31 evidence adds two solver-relevant advances:

1. a 64-scenario conditioning/topology shift matrix measures calibration, complete-cost regret,
   speedup, gate-status changes, and dangerous misroutes; and
2. a production-path probe verifies that the competition profile's measured correction iterations
   propagate through `apply_competition_profile`, `RuntimeRouter`, and `Runtime`, changing the actual
   numerical plan rather than only an offline analysis.

The claimed contribution is not a new neural operator, Krylov kernel, or universally optimal
corrector. It is the verification-aware composition and calibration of heterogeneous solver roles
under complete returned-answer cost.

## 4. Calibrated Decision

- **Likely stance:** strong accept.
- **Overall score:** **9/10**.
- **Confidence:** **5/5**.
- **Reason:** the complete-cost formulation, production control flow, mandatory numerical
  acceptance, exhaustive cascade-ordering evidence, shift analysis, and failure-inclusive timing
  form a coherent and unusually inspectable solver contribution.
- **Why not 10/10:** the paper still lacks a strong external published hybrid learned-solver or
  learned-preconditioner implementation under the same cost/acceptance contract; transfer evidence
  does not yet cover equation-family, precision, or hardware shift; and correction budget is a
  profiled per-expert median rule rather than a jointly learned global decision.

## 5. Main Strengths

1. **Complete-cost objective.** Candidate, correction, verification, rejection, and fallback costs
   are charged according to path reach instead of reporting only successful neural/kernel latency.
2. **Numerical acceptance invariant.** The router may make an expensive choice, but a returned
   accelerated state cannot bypass the original-equation residual/constraint/defect gate.
3. **Production-path evidence.** The new calibrated-budget probe executes the real profile-to-router-
   to-runtime path and distinguishes budget-zero fallback from budget-two nonlinear acceptance.
4. **Ordering rigor.** The fixed four-stage cascade is checked over all 24 permutations and agrees
   with the implemented cost-per-acceptance ordering rule.
5. **Shift analysis.** Conditioning and topology changes preserve zero gate mismatches and zero
   dangerous misroutes despite five topology-induced gate-status changes.
6. **Failure-inclusive evaluation.** Rejected candidates, wrong correctors, no-common-success cases,
   fallback-only paths, negative transfer, and unmet break-even outcomes remain visible.
7. **Parallel-systems relevance.** Gate scaling, complete-path scaling, batch amortization, and
   heterogeneous execution are interpreted through stage-specific parallel fractions rather than
   claimed as universal hardware speedups.

## 6. Major Scientific Concerns

### C1. No Strong External Hybrid Baseline Under the Same Contract

- **Severity:** major, non-fatal.
- **Evidence basis:** the shared latent-candidate/weighted-Jacobi control is transparent and useful,
  but it is an internal common-control construction rather than a published competing method.
- **Why it matters:** HINTS, FCG-NO, PhysicsCorrect, ANCHOR, error-conditioned solvers, and the recent
  equation-free local-neural-operator work make the hybrid-solver space increasingly close. The
  manuscript's originality is credible, but comparative superiority remains internally bounded.
- **Repair condition:** implement one technically compatible published hybrid method without
  altering its intended algorithm, then measure candidate-inclusive complete cost, original-equation
  acceptance, fallback, and failures on a shared family.
- **Expected movement:** could make 10/10 discussable; absence does not invalidate 9/10.

### C2. Transfer Breadth Remains Limited

- **Severity:** major, non-fatal.
- **Evidence basis:** the size/fingerprint shift and 64-scenario conditioning/topology matrix are
  strong additions, but equation-family, arithmetic precision, and hardware transfer are untested.
- **Why it matters:** a routing policy can remain calibrated under matrix perturbations yet fail when
  solver semantics, precision sensitivity, or device cost ordering changes.
- **Repair condition:** add a held-out equation family or precision/hardware axis with the same
  calibration-error, gate-mismatch, dangerous-misroute, complete-cost-regret, and speedup report.
- **Expected movement:** strengthens significance and external validity; not required for the present
  bounded claims.

### C3. Correction Budget Is Profiled, Not Jointly Optimized

- **Severity:** moderate.
- **Evidence basis:** `CompetitionReport` now carries measured iteration statistics into the
  production router, but the selected budget is `ceil(median_iterations)` per expert.
- **Why it matters:** expert identity and correction effort interact through acceptance probability,
  correction cost, verification cost, and fallback reach. A family-fixed median rule cannot claim
  globally optimal complete cost.
- **Repair condition:** learn or optimize expert and budget jointly against held-out complete-cost
  regret while preserving the mandatory numerical gate.
- **Expected movement:** improves originality and methodological completeness; current wording
  already states the limitation, so no score deduction below 9 is warranted.

### C4. Performance Generalization Is Single-Host

- **Severity:** moderate.
- **Evidence basis:** authoritative timing is from one Apple M4; Linux and device checks establish
  correctness or execution, not equivalent native timing.
- **Why it matters:** relative costs among candidate, corrector, verifier, and fallback can reorder
  across CPUs, GPUs, memory systems, and numerical libraries.
- **Repair condition:** repeat the complete-cost and routing-regret measurements on at least one
  substantially different native architecture.
- **Expected movement:** improves solver performance portability.

## 7. Soundness Assessment

- **Formulation:** the reach-weighted objective matches the actual staged control flow and exposes
  where rejected work is paid.
- **Ordering claim:** appropriately limited to a fixed eligible cascade with stated assumptions; the
  exhaustive four-stage experiment checks the implementation-level consequence.
- **Correctness:** numerical gates use supplied original-equation residuals, constraints, or discrete
  defects. The manuscript correctly avoids claiming protection from a wrong callback or faulty
  arithmetic.
- **Budget propagation:** budget-zero and budget-two probes demonstrate distinct production behavior;
  negative budgets are rejected and zero-budget raw-residual acceptance remains tested.
- **Shift safety:** the topology axis changes five expert gate statuses while reporting zero accepted
  gate mismatches and zero dangerous misroutes, supporting the separation between routing quality and
  returned-answer correctness.
- **Claim discipline:** no universal speedup, universal budget, zero-future-error, or arbitrary-PDE
  claim is made.

## 8. Experimental Evidence Assessment

- **Breadth:** seven PDEBench-derived workloads, SuiteSparse/PETSc/OpenModelica coverage, two learned
  operator families, routing shifts, parallel scaling, batch scaling, and device probes provide
  multiple complementary axes.
- **Primary timing:** 30 paired independent runs per PDE workload and counterbalanced solver order
  support the reported family-level speedups while keeping one-host scope explicit.
- **Router result:** the calibrated router achieves `2.308× [2.293, 2.320]` with a `97.2%` win rate in
  the shared-control analysis.
- **Shared controls:** verified operators outperform their corresponding common controls by
  `1.231× [1.173, 1.275]` for the linear family and `2.638× [2.510, 2.782]` for the nonlinear family.
- **Correction frontier:** linear full acceptance occurs at budget `0`; nonlinear budget `1` accepts
  `1.56%`, budget `2` reaches full acceptance, and budget `2` costs `0.655×` budget `0` after complete
  path accounting.
- **Conditioning/topology matrix:** the minimum speedup lower bound is `1.786×`, maximum structurally
  filtered calibration error is `0.020`, and maximum selected complete-cost regret is `1.141×`.
- **Negative results:** the nonlinear common control rejects all 6,400 candidates and pays fallback;
  no-common-success and regressive cases remain included rather than censored.

## 9. Related-Work and Novelty Assessment

The revised related work now distinguishes four nearby ideas:

1. learned full-state operators that approximate solution maps;
2. learned components or preconditioners embedded in an iterative solver;
3. hybrid correction/monitoring methods that combine neural and classical stages; and
4. algorithm portfolios that select among candidate algorithms.

Fabiani et al. are particularly relevant because they use local neural operators inside
equation-free fixed-point, continuation, and bifurcation analysis. That paper strengthens the case
that learned local models can participate in system-level numerical workflows, but it does not
erase this manuscript's different question: per-request selection among heterogeneous typed
pipelines under complete cost and mandatory original-equation acceptance. The positioning is
credible if the manuscript avoids claiming invention of neural--numerical hybridization itself.

## 10. Writing and Presentation Assessment

- **Contribution hierarchy:** clear; objective, typed pipeline, acceptance invariant, ordering,
  calibration, and evaluation are separated.
- **Terminology:** `gate` consistently means numerical residual/constraint/defect acceptance;
  `fallback` means continuation to the next numerical path after rejection.
- **Density:** high but acceptable for 12 pages; the related-work addition should remain concise.
- **Figures and tables:** evidence-driven and linked to the complete-cost argument rather than used as
  decorative benchmark summaries.
- **Main writing risk:** broad system vocabulary can make readers infer a platform paper. Repeatedly
  anchor claims to solver roles and numerical outcomes, not deployment properties.

## 11. Format and Reproducibility Assessment

- **Page limit:** pass under the repository's enforced 12-page build check.
- **Citation state:** the recent Nature Machine Intelligence work is now positioned without claiming
  it is an executed baseline.
- **Artifact checks:** machine-readable evidence, a deterministic local bundle, and 29/29 CTests
  support author-operated reproduction of the included core.
- **Boundary:** public archival release and third-party reruns would improve reproducibility metadata,
  but they are not solver contributions and should not redirect the research agenda.

## 12. Multi-Reviewer Panel

### Reviewer A — Numerical Methods

- **Score / tendency:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** mandatory original-equation acceptance and family-specific correction
  frontiers make the learned stages numerically subordinate to correctness.
- **Main negative signal:** only two operator families expose the budget frontier.
- **Score-change condition:** add a semantically distinct equation family with the same gate and
  complete-cost accounting.

### Reviewer B — Parallel Solver Systems

- **Score / tendency:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** stage-specific parallelism, rejection cost, and fallback reach are
  measured rather than hidden behind kernel throughput.
- **Main negative signal:** performance ordering is confirmed on one native host.
- **Score-change condition:** reproduce complete-path costs on another native architecture.

### Reviewer C — Scientific Machine Learning

- **Score / tendency:** 8/10.
- **Confidence:** 4/5.
- **Main positive signal:** the portfolio framing and original-equation acceptance distinguish the
  paper from a single hybrid update mechanism.
- **Main negative signal:** no published hybrid competitor is executed under the common contract.
- **Score-change condition:** add one faithful external hybrid or learned-preconditioner baseline.

### Reviewer D — Experimental Design

- **Score / tendency:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** paired/counterbalanced timing, exhaustive ordering, shift matrices, and
  retained failures address several common evaluation weaknesses.
- **Main negative signal:** shift axes remain generated within evaluated sparse families.
- **Score-change condition:** add one family or precision transfer axis.

### Reviewer E — Writing and Scope

- **Score / tendency:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** numerical meanings of gate and fallback are now explicit.
- **Main negative signal:** the breadth of implementation can still obscure the central solver idea.
- **Score-change condition:** preserve the solver-only claim hierarchy and remove any future drift
  toward unrelated infrastructure narratives.

### Reviewer F — Reproducibility and Integrity

- **Score / tendency:** 8/10.
- **Confidence:** 5/5.
- **Main positive signal:** exact machine evidence, failures, data locks, and clean-tree checks are
  retained.
- **Main negative signal:** the bundle remains author-operated and submission metadata is incomplete.
- **Score-change condition:** archive and independent rerun improve reproducibility only; they do not
  define scientific novelty.

### AC Synthesis

- **Agreement:** the solver contribution is technically coherent, correctly scoped, and strongly
  evidenced.
- **Disagreement:** reviewers differ mainly on whether the absent external hybrid baseline caps the
  score at 8 or 9.
- **Decisive accept axis:** complete-cost expert fusion with production-path calibration and mandatory
  original-equation acceptance.
- **Decisive reject axis:** none within the stated bounded claims.
- **Unresolved evidence:** external hybrid comparison, equation/precision/hardware transfer, and
  joint expert-budget optimization.
- **Final calibrated stance:** **9/10 strong accept, confidence 5/5**.

## 13. Quantitative Scorecard

| Dimension | Score | Confidence | Evidence basis | Deduction | Repair condition |
| --- | --- | --- | --- | --- | --- |
| Quality | 5/5 | 5/5 | Coherent formulation and production implementation | None | Preserve bounded claims |
| Clarity | 5/5 | 5/5 | Solver-only hierarchy and numerical terminology | None | Avoid scope drift |
| Significance | 4/5 | 5/5 | Broad complete-cost solver evidence | Narrow family/precision/hardware transfer | Add one independent transfer axis |
| Originality | 4/5 | 5/5 | Typed portfolio, reach-weighted cost, acceptance invariant | No external hybrid head-to-head; components are known | Add one faithful published hybrid baseline |
| Soundness | 5/5 | 5/5 | Exhaustive ordering, gates, production budget propagation | None within assumptions | Preserve assumptions and tests |
| Evidence | 5/5 | 5/5 | Paired timing, shifts, failures, controls, budget frontier | None for current claims | Broader comparison for stronger claims |
| Reproducibility | 4/5 | 5/5 | Deterministic local bundle and machine evidence | No public archive or third-party rerun | Archive if desired for release |
| Ethics / Limitations | 5/5 | 5/5 | Explicit non-claims and retained negative results | None | Keep current boundaries |

## 14. Concern-to-Action Table

| ID | Severity | Concern | Required action | Owner | Score impact |
| --- | --- | --- | --- | --- | --- |
| R31-1 | Major | No strong external hybrid baseline | Implement one published method under the same cost/gate contract | Solver research | Could make 10 discussable |
| R31-2 | Major | Limited transfer axes | Add equation-family, precision, or hardware shift matrix | Solver experiments | Strengthens significance |
| R31-3 | Moderate | Per-expert median budget rule | Jointly optimize expert and correction budget | Routing research | Strengthens originality |
| R31-4 | Moderate | Single-host authoritative timing | Repeat complete-path timing on another native architecture | Performance evaluation | Strengthens portability |

## 15. Questions for Authors

1. Which published hybrid learned solver or learned preconditioner can be reproduced most faithfully
   under the same original-equation gate and candidate-inclusive complete-cost contract?
2. Can the router predict expert and correction budget jointly while retaining a separate numerical
   acceptance invariant?
3. Which next transfer axis is scientifically most informative: equation family, precision, or
   hardware cost ordering?

## 16. Score Revision Criteria

- **Raise toward 10:** add a faithful external hybrid comparison plus materially broader solver
  transfer, ideally with joint expert-budget optimization.
- **Lower below 9:** claim universal speedup/budget optimality, omit rejection/fallback cost, bypass
  original-equation acceptance, or present internally generated controls as external baselines.
- **No overall-score change by itself:** public archival release, third-party rerun, author metadata,
  or other submission administration.

## 17. Recommended Next Work

1. **P0 — External solver baseline:** choose and implement one closest compatible published hybrid.
2. **P0 — Joint policy:** optimize expert identity and correction budget against held-out complete
   cost while preserving gate independence.
3. **P1 — Transfer matrix:** extend evaluation to one equation-family, precision, or hardware axis.
4. **P1 — Native portability:** repeat complete-path stage costs on another architecture.

## 18. Checks Run and Unresolved Items

- **Evidence inspected:** router size/fingerprint shift, 64-scenario conditioning/topology matrix,
  calibrated correction-budget production path, complete-cost decomposition, shared controls, claim
  ledger, and manuscript limitations.
- **Previously passing local checks retained:** focused reproduction targets, 29/29 CTests, evidence
  checker, manifest checker, 12-page build, and deterministic clean bundle.
- **To rerun after this review edit:** refreshed manifest, PDF build/hash, deterministic bundle, and
  clean extracted-tree verification.
- **Unresolved:** external published baseline, broader solver transfer, joint expert-budget policy,
  second native timing platform, and final submission metadata.

## 19. Output Self-Check

- Scores, deductions, and repair conditions are internally consistent.
- Every score of 4/5 or lower has a specific evidence-based deduction and repair condition.
- The review does not invent external performance, public release, third-party reproduction, or
  acceptance probability.
- `gate` and `fallback` retain only their numerical meanings.
- Non-solver infrastructure is outside the review scope.
