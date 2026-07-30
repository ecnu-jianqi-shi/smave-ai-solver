# CCF-A Full Review — Round 34

## 1. Report Metadata

- **Review date:** 2026-07-26.
- **Target venue/year/track:** IEEE Transactions on Parallel and Distributed Systems;
  regular paper; repository-defined 12-page IEEE Computer Society journal contract.
- **Paper title:** *Complete-Cost Expert Fusion for Verified Repeated Numerical Solves*.
- **Input materials reviewed:** manuscript sources and rendered PDF; solver source and
  tests; paired operator, shared-control, HINTS, routing, gate, scaling, shift, and
  claim-ledger evidence; Round 33 review and Round 34 revisions.
- **Search basis:** repository references and pinned HINTS paper/code metadata. The IEEE
  author page was fetched from the official site, but its client-rendered response did
  not expose independently parseable page-limit text; format assessment therefore uses
  the repository's explicit 12-page contract.
- **Report file:** `ccfa-review-reports/round-34-full-review.md`.
- **Reviewer mode:** full scientific, writing, format, multi-reviewer, and AC synthesis.
- **Scientific scope:** solver algorithms, numerical acceptance, routing calibration,
  correction budgets, complete-path cost, parallelism, heterogeneous execution, and
  reproducible solver evidence.

## 2. Desk Rejection Assessment

- **Paper length — pass:** final PDF has 12 pages.
- **Topic compatibility — pass:** the paper concerns parallel and heterogeneous
  numerical solver execution, algorithm selection, learned candidates, and verified
  complete-path performance.
- **Minimum quality — pass:** the method, equations, implementation, negative results,
  and machine-generated evidence are inspectable.
- **Policy/anonymity/compliance — uncertain:** author metadata remains a visible
  placeholder and must be replaced or blinded according to the actual submission mode.
- **Prompt injection and hidden manipulation — pass:** no reviewer-directed hidden text,
  score instruction, or adversarial manuscript content was found.
- **Ethics and reviewability — pass:** limitations and negative results are explicit;
  no fabricated external reproduction or universal performance claim is made.

## 3. Paper Summary And Contribution Map

The paper formulates repeated numerical acceleration as a typed cascade of candidate,
corrector, original-equation gate, and numerical fallback stages. It optimizes
reach-weighted complete verified runtime rather than raw candidate latency. For a fixed
eligible cascade with order-invariant statistics, it derives a cost-per-acceptance
ordering rule. For finite calibrated expert--budget actions, it implements an exact
bounded dynamic program that selects subset, one budget per expert, and order while
charging rejection continuation and terminal fallback. Every successful result must
pass a family-specific original-equation residual, constraint, defect, or consistency
test.

- **Claimed problem:** heterogeneous classical and learned paths have workload-dependent
  benefit, while raw learned latency omits correction, verification, rejection, and
  fallback cost.
- **Claimed gap:** algorithm selection and learned-solver studies often separate path
  choice from the numerical condition that decides whether a result is acceptable.
- **Method map:** typed equation IR; capability filtering; complete-cost routing;
  candidate--corrector--gate roles; exact bounded expert--budget optimization; paired
  complete-runtime measurement; intra-node and heterogeneous placement.
- **Evidence package:** seven PDEBench-derived workloads; SuiteSparse/PETSc/OpenModelica
  breadth; fixed-cascade exhaustive oracle; size/conditioning/topology shift; held-out
  joint-policy study; linear and nonlinear operator families; shared control; HINTS
  schedule control; gate/complete-path/batch scaling; negative transfer and failed
  cases; deterministic evidence checks.
- **Stated limitations:** one authoritative timing architecture, controlled joint-policy
  families, calibrated rather than jointly predicted action statistics, incomplete
  closest-method reproduction, and bounded equation semantics.

## 4. Search And Related-Work Basis

- **Queries used:** no private manuscript text was sent to a public search service. The
  round used the paper's bibliography, the pinned HINTS DOI, and official public-code
  revision recorded by the artifact.
- **Sources searched:** local bibliography and evidence; pinned HINTS public repository
  revision `0c8b712f81ed08bdf27c3a215f8edb99910f5e2f`; official IEEE TPDS author page.
- **Closest works found:** HINTS, DeepONet, Fourier Neural Operator, learned
  preconditioners, and classical algorithm-selection/portfolio work already positioned
  in the manuscript.
- **Unverified related-work risks:** the review does not claim a new exhaustive
  2026-wide literature search; a closer recent complete-path hybrid may exist.
- **Source-quality screening:** the published HINTS claim is bounded to the paper and
  pinned public schedule source; the manuscript does not claim execution of the public
  code or reproduction of its DeepONet/training/weights.

## 5. Expected Review Outcome

- **Expected outcome:** strong accept, **9/10**.
- **Main accept signal:** a coherent solver contribution joins complete-cost selection,
  exact bounded expert--budget optimization, mandatory original-equation acceptance,
  and unusually explicit complete-path evidence with retained failures.
- **Main reject signal:** the closest published hybrid comparison remains a schedule-
  level control rather than a faithful full-method implementation; policy evidence also
  remains controlled.
- **Confidence:** **5/5**, because the manuscript, code, generated evidence, raw report
  contracts, tests, and final PDF were inspectable.

## 6. Strengths And Weaknesses

### Strengths

1. **The research question is precise.** The paper asks how to minimize complete
   verified solver cost while letting the original equation determine publication.
2. **The control-flow invariant is meaningful.** Router or candidate error can waste
   time but cannot bypass the numerical gate in the tested paths.
3. **The optimization is implemented.** The bounded dynamic program jointly selects
   expert, correction budget, subset, and order under a one-budget-per-expert state
   constraint.
4. **Continuation cost is explicit.** Rejection probability multiplies the value of
   later numerical paths or terminal fallback.
5. **Round 34 repairs a real statistical defect.** Operator schema v3 makes the paired
   median and bootstrap interval authoritative; marginal medians remain diagnostic.
   The final linear result is `1.053× [1.031, 1.091]`, even when separate marginal
   summaries could otherwise imply a different conclusion.
6. **Correction is evaluated as part of the solver.** The nonlinear raw candidate is
   inaccurate, but Newton correction produces 6,400 accepted same-accuracy solves at
   `1.536× [1.475, 1.589]`; raw candidate quality no longer incorrectly vetoes the
   corrected pipeline's break-even.
7. **The closest-control boundary is honest.** HINTS's public `25:1` schedule,
   `ω=0.8`, 400-update cap, and zero-anchored correction are tested under common cost
   and gate semantics; the negative result is retained without relabeling it a full
   HINTS reproduction.
8. **Negative evidence is visible.** Shared controls regress, several device paths do
   not break even, one operator transfer case lacks stable benefit, and no-common-
   success cases remain in the accounting.

### Major Weakness W1: Full Published Hybrid Baseline Remains Missing

- **Evidence basis:** the HINTS schedule control uses the common latent operator and
  stopping contract; it does not reproduce the published DeepONet, training protocol,
  or weights.
- **Reviewer deduction:** the paper can isolate schedule cost and show its own mechanism
  is not explained by that alternation pattern, but it cannot claim superiority to the
  full published method.
- **Required fix:** execute a faithful compatible published hybrid or learned-
  preconditioner implementation under the same complete-cost and original-equation
  contract.

### Moderate Weakness W2: Joint-Policy Evidence Is Controlled

- **Evidence basis:** two 12-variable nonlinear SCC families, two experts, three actions,
  and deterministic training/held-out profiles establish exactness and profile freezing.
- **Reviewer deduction:** the experiment proves the mechanism but not calibration on a
  large public solver portfolio with more complex action interactions.
- **Required fix:** repeat the frozen-profile/held-out exhaustive-oracle protocol on a
  public sparse, operator, ODE/DAE, or PDE-derived workload with a larger action set.

### Moderate Weakness W3: Action Statistics Are Inputs, Not Joint Predictions

- **Evidence basis:** the optimizer consumes measured/calibrated cost and pass
  probability profiles.
- **Reviewer deduction:** optimization is exact conditional on those profiles, but the
  paper does not establish end-to-end prediction quality from request features.
- **Required fix:** predict or cross-validate cost and acceptance jointly and report
  held-out complete-cost regret against the same exhaustive oracle.

### Moderate Weakness W4: Timing Breadth Remains Limited

- **Evidence basis:** authoritative performance comes from one Apple M4; operator timing
  uses 100 within-process paired repetitions. Across the Round 34 debugging runs, point
  estimates moved while the final paired intervals retained the tested conclusions.
- **Reviewer deduction:** the paired estimator is now correct, but process-level and
  architecture-level variance remain outside the main inference unit.
- **Required fix:** add counterbalanced process-level operator runs and selected native
  timing on another architecture.

## 7. Potentially Missing Related Work

| Work | Status | Why Relevant | Overlap | Needed Comparison |
| --- | --- | --- | --- | --- |
| HINTS | searched and locally pinned | Closest explicit neural-operator/relaxation schedule | Learned corrections interleaved with classical relaxation | Full DeepONet/training/weight reproduction or a precise incompatibility argument |
| Learned preconditioners | cited, not exhaustively re-searched | Close alternative for repeated sparse solves | Learned component improves a classical iterative path | One strong public implementation under common complete-cost accounting |
| Solver portfolios / algorithm selection | cited | Closest routing formulation | Chooses among heterogeneous algorithms | Clarify where acceptance probability and numerical gate change the objective |

No additional paper is asserted as definitely missing without a broader public search.

## 8. Claim-Evidence Audit

| Claim | Where Stated | Evidence Provided | Strength | Reviewer Deduction | Required Fix |
| --- | --- | --- | --- | --- | --- |
| Complete-cost routing can beat a fixed expert | Abstract, Introduction, RQ3 | `2.282× [2.253, 2.309]` paired router result | Strong but scoped | Supported on the evaluated sparse family | Add broader held-out families for universality |
| Every successful accelerated return passes an original-equation gate | Problem formulation, fusion, tests | Proposition, runtime traces, zero gate mismatches | Strong implementation invariant | Supported for implemented callbacks and hardware | Keep callback/hardware limitation explicit |
| Exact bounded joint expert--budget policy matches exhaustive optimization | Method, RQ3 | Controlled production-path probe and two-family held-out oracle | Strong conditional claim | Exact under finite order-invariant profiles and state bound | Add realistic larger action set |
| Verified operators outperform classical paths | Abstract, RQ4 | Linear `1.053× [1.031, 1.091]`; nonlinear `1.536× [1.475, 1.589]` | Strong on two families | Paired statistical claim is now internally consistent | Add process-level and cross-architecture repetitions |
| Verified operators beat shared correction controls | RQ4 | `1.255× [1.194, 1.336]` and `2.661× [2.532, 2.818]` | Strong internal control | Gain is not merely any candidate/correction wrapper | Add a full strong external baseline |
| HINTS schedule is slower under the common candidate/contract | RQ4, ledger | `0.104× [0.101, 0.106]`; operator/HINTS `10.038× [9.684, 10.610]` | Strong schedule-level negative control | Supports a schedule-cost statement only | Do not upgrade to full HINTS superiority |

## 9. Experiment / Benchmark / Reproducibility Audit

- **Baselines:** classical paths, fixed experts, hindsight oracles, shared learned-
  candidate/Jacobi controls, HINTS schedule control, device/reference paths, and
  external solver suites are present. The missing piece is a faithful strongest
  published hybrid.
- **Ablations:** candidate, correction, gate, complete path, correction budget, shared
  control, and router complexity are separated. The nonlinear corrector failure is
  especially informative.
- **Datasets/benchmarks:** breadth is high but uneven; public PDE/sparse/modeling suites
  coexist with controlled operator and policy families.
- **Metrics:** complete runtime, paired speedup, bootstrap intervals, acceptance,
  fallback, failure, residual/QoI error, regret, calibration, and break-even are reported.
- **Statistical rigor:** the primary estimator is the median of paired ratios; Round 34
  removes a prior ratio-of-marginal-medians inconsistency from operator claims.
- **Robustness/failure cases:** size, conditioning, topology, order, device transfer,
  no-common-success, and negative operator/control outcomes are retained.
- **Implementation details:** schedule revision, correction budgets, tolerances, request
  counts, timing inclusions, and evidence schemas are machine checked.
- **Artifacts:** evidence values generate manuscript macros; 29/29 CTests and paper
  evidence checks pass. Public archival release and independent rerun remain open.
- **Limitations:** appropriately bounded; the paper does not claim universal speedup,
  zero future erroneous accepts, or complete external-method reproduction.

## 10. Multi-Reviewer Panel

### Reviewer 1

- **Reviewer:** Numerical methods and solver correctness.
- **Likely score:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** original-equation acceptance and correction semantics are
  explicit and tested across linear/nonlinear/ODE/DAE-related paths.
- **Main negative signal:** exactness is conditional on declared callbacks, tolerances,
  and bounded action assumptions.
- **Evidence basis:** problem formulation, gate proposition, runtime traces, validation,
  and failure probes.
- **Score-change condition:** broader difficult families without weakening tolerances.

### Reviewer 2

- **Reviewer:** Experimental methodology and statistics.
- **Likely score:** 8/10.
- **Confidence:** 5/5.
- **Main positive signal:** paired estimator, bootstrap interval, order control, complete-
  cost timing, and failure accounting are unusually explicit.
- **Main negative signal:** operator inference units are within-process repetitions on one
  architecture.
- **Evidence basis:** schema v3 reports, statistics traces, methodology, and repeated
  Round 34 reproduction.
- **Score-change condition:** process-level counterbalancing and another native platform.

### Reviewer 3

- **Reviewer:** Scientific machine learning and neural operators.
- **Likely score:** 8/10.
- **Confidence:** 4/5.
- **Main positive signal:** learned candidates are treated as components of verified
  numerical pipelines rather than automatically accepted solvers.
- **Main negative signal:** the closest HINTS comparison is schedule-level, not a full
  DeepONet/training reproduction.
- **Evidence basis:** RQ4, HINTS evidence contract, operator/shared-control reports.
- **Score-change condition:** faithful published hybrid baseline under common cost.

### Reviewer 4

- **Reviewer:** Algorithm selection and optimization.
- **Likely score:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** reach-weighted continuation and exact bounded expert--budget
  selection go beyond winner classification.
- **Main negative signal:** cost and pass probabilities are calibrated inputs and state
  interactions are bounded away.
- **Evidence basis:** recurrence, exhaustive oracle, held-out profile-freezing study.
- **Score-change condition:** joint prediction with held-out regret on a larger portfolio.

### Reviewer 5

- **Reviewer:** Parallel and heterogeneous systems for numerical computing.
- **Likely score:** 8/10.
- **Confidence:** 5/5.
- **Main positive signal:** the paper separates gate-only, complete-path, batch, transfer,
  and placement effects instead of reporting kernel latency alone.
- **Main negative signal:** performance portability and native accelerator breadth remain
  limited.
- **Evidence basis:** RQ5, gate family scaling, complete-path/batch results, negative
  placement outcomes.
- **Score-change condition:** selected native cross-architecture complete-path results.

### Reviewer 6

- **Reviewer:** Reproducibility and artifact audit.
- **Likely score:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** generated values, claim ledger, evidence schemas, tests, and
  deterministic artifact tooling provide strong inspectability.
- **Main negative signal:** no public archival identifier or independent operator rerun.
- **Evidence basis:** check scripts, artifact snapshot/manifest workflow, 29/29 CTests.
- **Score-change condition:** public archive plus independent clean rerun.

### Reviewer 7

- **Reviewer:** Writing, clarity, and reviewer-facing risk.
- **Likely score:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** contribution, evidence, negative results, and limitations are
  visible and use stable terminology.
- **Main negative signal:** the manuscript is dense, and interface breadth can distract
  from the solver algorithm if read quickly.
- **Evidence basis:** abstract, contribution list, method/evaluation flow, 12-page PDF.
- **Score-change condition:** no major scientific change; minor compression of peripheral
  runtime-interface detail could sharpen emphasis.

### Reviewer 8

- **Reviewer:** Skeptical novice advocate.
- **Likely score:** 8/10.
- **Confidence:** 4/5.
- **Main positive signal:** the architecture figure and candidate--corrector--gate wording
  make the central idea understandable without accepting learned-model output on trust.
- **Main negative signal:** the number of workloads and evidence contracts can obscure
  which result is decisive.
- **Evidence basis:** Introduction, RQ structure, operator table, limitations.
- **Score-change condition:** emphasize the three decisive results: exact joint policy,
  original-equation acceptance, and paired complete-cost benefit.

### Panel Synthesis

- **Agreement:** the solver mechanism is sound, implemented, and supported by unusually
  inspectable complete-path evidence.
- **Disagreement:** reviewers differ on whether the breadth of internal controls offsets
  the absence of a faithful full published hybrid baseline.
- **Decisive positive axis:** complete-cost solver selection with mandatory numerical
  acceptance and exact bounded expert--budget routing.
- **Decisive negative axis:** external closest-method and broader held-out policy evidence.
- **Unresolved evidence:** full HINTS/strong-hybrid reproduction, process-level operator
  timing, another native architecture, and jointly predicted action profiles.
- **AC stance:** **9/10, strong accept**; insufficient evidence for 10/10.

## 11. Concerns Table

| ID | Severity | Concern | Evidence Basis | Affected Criterion | Fix Class | Required Action | Owner Skill | Score-Change Condition |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| C1 | major | No faithful full strong published hybrid baseline | HINTS control reproduces schedule only | Evidence, significance | experiment | Reproduce one compatible published method under common complete cost | solver implementation + ccf-paper-reviewer | Could move evidence/significance from 4 to 5 |
| C2 | moderate | Joint-policy families and action set are controlled | Two families, two experts, three actions | External validity | experiment | Apply frozen-profile/oracle protocol to a public larger portfolio | solver experiments | Could raise overall confidence toward 10 |
| C3 | moderate | Cost and pass statistics are not jointly predicted | Calibrated profile inputs | Originality, evidence | method/soundness | Add feature-based joint prediction and held-out regret | solver modeling | Could raise originality/evidence |
| C4 | moderate | Operator timing lacks process/platform breadth | One Apple M4, within-process pairs | Evidence, portability | experiment | Add process-level paired runs and another architecture | solver benchmarking | Could strengthen performance generalization |
| C5 | minor | Author block remains placeholder | `paper/authors.tex` | Format/compliance | writing | Replace or blind before submission | author | Removes desk/compliance risk |

## 12. AC / Meta-Review

The reviewers agree that the paper's strongest contribution is not an isolated learned
kernel but a solver-level formulation in which complete cost and original-equation
acceptance jointly determine the returned path. The exact bounded optimizer, continuation
semantics, corrected nonlinear operator result, and transparent negative evidence form a
coherent package. Round 34 materially improves soundness by correcting the operator
statistical estimator and by aligning break-even with the accepted corrected pipeline.

The main dispute is the strength of external comparison. The HINTS schedule control is
useful and fair for the schedule-level question, but its common latent operator removes
the architecture/training choices that define the published method. Therefore it cannot
close the strongest-baseline concern. Controlled policy families and one-platform timing
are secondary limitations rather than fatal flaws.

- **Reviewer consensus:** accept/strong accept.
- **Reviewer disagreement:** 8 versus 9 depending on the weight assigned to the missing
  full external hybrid baseline.
- **Decisive acceptance axis:** sound solver mechanism plus exact/paired/complete-path
  evidence.
- **Decisive rejection axis:** none fatal; closest-baseline and generalization gaps block
  an award-level score.
- **AC stance:** strong accept, **9/10**.
- **Discussion risk:** reviewers may incorrectly read the HINTS schedule result as a full
  method comparison unless the existing boundary remains prominent.

## 13. Quantitative Scores

| Criterion | Score (1–5) | Confidence | Evidence Basis | Deduction / Repair Condition |
| --- | ---: | ---: | --- | --- |
| Quality | 5 | 5 | Coherent theory, implementation, and evidence chain | No material deduction |
| Clarity | 5 | 5 | Stable solver terminology, explicit RQs, readable figures/tables | Dense interface detail is minor |
| Significance | 4 | 5 | Complete-cost verified solver fusion is broadly relevant | Full strong external baseline could raise to 5 |
| Originality | 4 | 5 | Joint complete-cost/gate/budget formulation is distinctive | Joint feature-based prediction could raise to 5 |
| Soundness | 5 | 5 | Exact bounded recurrence, oracle checks, numerical gate, corrected statistics | No material deduction within stated assumptions |
| Evidence | 4 | 5 | Broad suites, paired intervals, controls, shifts, failures | Full published baseline and process/platform breadth needed for 5 |
| Reproducibility | 5 | 5 | Machine-generated values, tests, manifests, deterministic tooling | Public independent rerun remains release maturity |
| Ethics / Limitations | 5 | 5 | Negative results and claim boundaries are explicit | No material deduction |

### Writing Scorecard

| Dimension | Weight | Score (1–5) | Confidence | Evidence Basis | Concrete Repair |
| --- | ---: | ---: | ---: | --- | --- |
| Storyline and motivation | 12 | 5 | 5 | Problem-to-gap-to-mechanism progression is explicit | None required |
| Contribution display | 12 | 5 | 5 | Three solver contributions are visible in Introduction | Keep HINTS boundary visible |
| Paragraph logic | 10 | 5 | 5 | Sections and RQs have clear jobs | Minor peripheral compression only |
| Claim-evidence alignment | 14 | 5 | 5 | Generated values and ledger match claims | Preserve paired estimator wording |
| Method readability | 10 | 5 | 5 | Objective, recurrence, gate, and roles are ordered well | Optional pseudocode could aid reuse |
| Experiment narration | 10 | 5 | 5 | Text states what positive and negative results prove | Emphasize decisive results in RQ4 |
| Related-work positioning | 8 | 4 | 4 | Closest families are compared on technical axes | Add full closest-method experiment |
| Terminology and notation consistency | 8 | 5 | 5 | Gate/fallback/complete cost are stable | None required |
| LaTeX and format discipline | 8 | 5 | 5 | 12 pages, no undefined refs or overfull boxes | Replace/blind author placeholder |
| Reviewer-facing risk | 8 | 4 | 5 | Limitations are honest | Avoid any wording that implies full HINTS reproduction |

- **Weighted writing score:** **4.84/5**.
- **Writing risk band:** low.
- **Overall:** **9/10, strong accept**.
- **Confidence:** **5/5**.
- **Score-change conditions:** a faithful full strong published baseline plus broader
  held-out/process/platform evidence is required for a credible 10/10 discussion.

## 14. Questions For Authors

1. Which published hybrid learned solver or learned preconditioner can be reproduced
   most faithfully under the paper's complete-cost and original-equation contract?
2. Can the operator timing protocol be promoted from within-process repetitions to
   counterbalanced process-level pairs without changing the solver implementation?
3. Which public workload offers a finite but materially larger expert--budget action
   set for the held-out exhaustive-oracle protocol?
4. Can action cost and pass probability be predicted jointly while preserving the
   exact bounded optimizer as the downstream decision rule?

## 15. Score Revision Criteria

### Raising the score would require

- faithful full-method timing for a strong published closest hybrid;
- realistic held-out joint expert--budget regret on a larger public portfolio;
- process-level and cross-architecture complete-path timing;
- joint cost/acceptance prediction with calibration and regret.

### Lowering the score would be triggered by

- any regression that moves an operator paired 95% lower bound to or below one while
  retaining the acceleration claim;
- a gate mismatch, erroneous accepted result, or omitted fallback/failure cost;
- wording that upgrades the HINTS schedule control to a full published-method
  reproduction;
- stale generated values or a paper/ledger/evidence inconsistency.

### Concerns unlikely to change before submission

- one-platform authoritative timing;
- absence of independent third-party reproduction;
- bounded equation semantics and controlled joint-policy families.

## 16. Action Plan And CCFA Handoffs

| Priority | Action | Owner Skill | Input Needed | Expected Output | Handoff Required |
| --- | --- | --- | --- | --- | --- |
| P0 | Implement faithful strong published hybrid baseline | solver implementation | Public code, compatible workload, cost contract | Complete-path paired comparison | No |
| P0 | Add process-level paired operator timing | solver benchmarking | Current operator commands and fixed scenarios | Run-level bootstrap evidence | No |
| P1 | Scale held-out joint policy to public larger portfolio | solver experiments | Public workload, finite actions/budgets | Frozen-profile/oracle regret matrix | No |
| P1 | Predict cost and acceptance jointly | solver modeling | Request features and training splits | Calibrated held-out predictions and regret | No |
| P2 | Finalize submission metadata | author/writing | Author list and review mode | Compliant author block | No |

- **Checks run:** targeted HINTS/shared/operator reproduction; operator v3 unit and
  compatibility tests; 29/29 CTests; Python compilation; paper evidence checker; LaTeX
  build; page-count/log checks; rendered 12-page contact-sheet inspection.
- **Checks skipped:** no full public-method HINTS DeepONet training run; no independent
  machine or second native architecture; no exhaustive new 2026 literature search.
- **Unresolved risks:** strongest published baseline, policy breadth, joint prediction,
  process/platform timing, and submission author metadata.

## Output Self-Check

- Section order follows the standard full-review contract.
- Scores, confidence, reviewer tendencies, and AC stance are internally consistent.
- Every criterion at 4 rather than 5 has a concrete evidence basis and repair condition.
- No acceptance probability, fabricated result, external reproduction, or universal
  solver claim is asserted.
- The review evaluates only decision-relevant solver claims and documented limitations.
