# CCF-A Full Review — Round 39

## 1. Report Metadata

- **Review date:** 2026-07-27.
- **Mode:** full scientific, writing, format, artifact, reviewer-panel, and AC review.
- **Target venue/year/track:** IEEE Transactions on Parallel and Distributed Systems,
  regular paper; the current-year official venue guide was not available in the local
  skill tree.
- **Paper:** *Complete-Cost Expert Fusion for Verified Repeated Numerical Solves*.
- **Input materials reviewed:** the modular 12-page manuscript under `paper/`; the
  solver implementation and sparse-solver tests; the frozen SuiteSparse final-held-out
  v3 selection, payload, structure audit, first-run evidence, production traces, and
  generated macros; claim/evidence and artifact ledgers; Round 38 review history; Release
  build and CTest results.
- **Search basis:** the public-safe closest-work basis inherited from Rounds 35--38.
  Round 39 introduces no new novelty claim, so no private manuscript text was submitted
  to a public search.
- **Report file:** `ccfa-review-reports/round-39-full-review.md`.
- **Scope authority:** solver algorithms, numerical correctness, complete-path cost,
  routing and correction mechanisms, solver-internal parallelism, heterogeneous
  computation, external solver controls, and reproducible evidence.
- **Explicitly excluded scope:** distributed high availability, replicas or quorum,
  physical-host failover, process isolation, security switching, and deployment fault
  tolerance. Numerical continuation after gate rejection is an in-request solver path,
  not service failover or fault recovery. These excluded topics do not affect the score.

## 2. Desk Rejection Assessment

- **Paper length and completeness:** pass. A clean temporary build produces 12 pages.
- **Topic compatibility:** pass under a parallel numerical solver-systems reading. The
  paper studies verified expert composition, complete-path performance, and intra-node
  parallel/heterogeneous numerical execution.
- **Minimum technical quality:** pass. The formulation, exact bounded optimizer,
  executable numerical paths, gate contract, evaluation protocol, retained failures,
  and limitations are inspectable.
- **Scientific reviewability:** pass. The v3 public route is frozen before performance
  measurement and exposes all negative controls needed to judge the adaptive policy.
- **Policy/anonymity/compliance:** unresolved submission item. `paper/authors.tex` and
  the acknowledgments still contain explicit placeholders; current venue policy was not
  independently checked in this round.
- **Manipulation check:** pass. Neither manuscript prose nor prior review history was
  treated as an instruction to increase the score.

## 3. Paper Summary And Contribution Map

The paper formulates repeated numerical solves as selection over typed pipelines rather
than selection of a single algorithm. A pipeline may generate a candidate, apply a
correction, recompute a family-specific acceptance quantity from the original equation,
and continue to another numerical path after rejection. The reach-weighted objective
charges candidate, correction, transfer, gate, and continuation costs. Under fixed
eligible actions with order-invariant statistics, a cost-per-acceptance rule orders a
cascade; for finite expert--budget actions, an exact bounded dynamic program chooses the
subset, correction budgets, and order. A learned candidate or Router may influence cost,
but it cannot bypass the original residual, constraint, consistency, or defect gate.

The contribution map is coherent:

1. a complete-cost solver-selection formulation and fixed-cascade ordering result
   (`paper/sections/03_problem_formulation.tex`);
2. an exact finite-state joint expert--budget optimizer with explicit assumptions and
   exhaustive agreement checks (`paper/sections/03_problem_formulation.tex`,
   `paper/sections/08_ablation_analysis.tex`);
3. role-constrained candidate--corrector--original-equation-gate--numerical-continuation
   execution (`paper/sections/04_system_design.tex`,
   `paper/sections/05_verification_aware_fusion.tex`);
4. production sparse actions and terminal paths spanning PCG, GMRES, sparse direct
   methods, SuperLU `dgssv`, and LSQR, including structural-rank rejection before direct
   factorization (`src/superlu_sparse.cpp`, `src/linear.cpp`, `src/solve_service.cpp`);
5. complete-path evidence across public SuiteSparse matrices, PDEBench-derived systems,
   learned operators, HINTS, gate scaling, batch amortization, and retained negative
   results (`paper/sections/06_experimental_methodology.tex`,
   `paper/sections/07_evaluation.tex`).

## 4. Round 39 Advance

Round 39 replaces the prior development-informed public split with a genuinely new
final-held-out v3 set. The selection was frozen before payload acquisition and before
any solver action, route, regret, interaction, or timing measurement. It excludes all
pre-v3 collection groups and contains two SPD, two symmetric non-SPD, and two
nonsymmetric matrices from six unseen collection groups. The structure audit records
zero development matrices and `performance_measurements=0`
(`benchmark/data-lock/SUITESPARSE_FINAL_HELDOUT_V3.md`,
`build/release/suitesparse-final-heldout-v3-structure-audit/evidence.txt`).

The first-run v3 contract uses 6/4/6 train/calibration/held-out matrices, 48/32/48
requests, 19 candidate actions plus a terminal predictor, and 3,672 raw observations.
All 48 production requests succeed, 23 use terminal numerical continuation, and gate,
plan-order, and DP/exhaustive mismatches are all zero. This materially strengthens the
external-validity and correctness evidence over Round 38.

The policy result is, however, more negative:

- conditioned regret is `1.895881` versus `1.900423` for the static profile, an
  improvement of only about `0.239%`;
- fixed SuperLU reaches `1.530423`, so the conditioned route is about `23.9%` worse;
- a training-selected numeric-family fixed policy reaches `1.457103`, so the
  conditioned route is about `30.1%` worse;
- size-only regret is `1.563748`, while RHS-only and tolerance-only both reach
  `1.457103`, equal to the family-fixed control;
- median/p95/maximum cost relative error is
  `0.941202`/`36.416773`/`1481.162531`;
- Brier/ECE/maximum-action calibration error is
  `0.308879`/`0.322093`/`0.624217`;
- measured interaction/order deltas reach `0.169242`/`0.027393`.

The stronger split therefore increases confidence in the negative conclusion: the
original-equation gate and terminal solver preserve numerical correctness, but the
current request-conditioned predictor and order-invariant cost model do not justify an
adaptive-routing superiority claim.

## 5. Likely Stance And Calibration

- **Expected outcome:** **8/10, accept**.
- **Confidence:** **5/5** for the scientific artifact and evidence; current venue-policy
  compliance remains separately unresolved.
- **Relative interpretation:** above average and technically credible, but not a
  strong-accept paper under the present public policy result.
- **Main accept signal:** a coherent solver method with mandatory original-equation
  acceptance, exact finite-action optimization within its model, production execution,
  strong negative-result retention, and unusually auditable evidence.
- **Main reject signal:** request-conditioned routing barely improves the static profile
  and loses decisively to both fixed SuperLU and a transparent training-selected
  family-fixed policy, with extreme cost-prediction tails and non-negligible action
  interactions.
- **Why the score does not rise:** group-disjoint freezing and family balance improve
  evidence quality, not method effectiveness. A more rigorous negative result supports
  the bounded solver-composition claim but cannot be scored as an adaptive-routing win.
- **Why the score does not fall:** the paper now states the negative result in the
  abstract, evaluation, limitations, and conclusion; numerical correctness is intact;
  no gate mismatch, hidden failure, or asymmetric success accounting was found.

## 6. Quantitative Scorecard

| Dimension | Score (1-5) | Confidence (1-5) | Evidence basis | Deduction / score-change condition |
| --- | ---: | ---: | --- | --- |
| Novelty | 4 | 4 | Reach-weighted candidate--corrector--gate--continuation cost, role-constrained pipelines, and exact bounded expert--budget selection | Components have portfolio and hybrid-solver precedents; raise only with a sharper algorithmic advance over strong solver-family selection or a stronger closest-method comparison |
| Soundness | 4 | 5 | Original-equation gates, exact DP/exhaustive agreement, production plan-order checks, structural-rank rejection, and LSQR continuation tests | The DP assumes order-invariant action statistics despite `0.169` measured interaction; raise after interaction-aware optimization or decision-rank stability evidence |
| Evidence | 4 | 5 | Frozen group-disjoint v3, 48/48 production success, public traces, family-fixed and feature controls, PDE/operator/HINTS studies, retained failures | The adaptive policy loses to both strong controls and has extreme tail error; raise after a tail-aware method competes on a new frozen public set |
| Significance | 4 | 4 | Complete verified cost and gate-controlled expert fusion address a real repeated-solve problem across multiple equation families | One-host timing, limited public hybrid baselines, and no adaptive public win constrain broad impact |
| Clarity | 4 | 5 | The abstract and conclusion now place the fixed-control losses beside the conditioned result; numerical continuation terminology is explicit | The dense multi-suite narrative can still obscure the single decisive policy result; lower if the negative control is moved away from headline claims |
| Reproducibility | 5 | 5 | Pre-experiment v3 freeze, byte hashes, structure audit, first-run directory, generated macros, evidence checker, artifact manifest logic, Release build, and 29/29 CTests | Final PDF/manifest metadata must remain synchronized; a mismatch would reduce this dimension |
| Ethics / Limitations | 5 | 5 | One-host, HINTS, callback, order-interaction, public-route, and workload boundaries are explicit; negative results are retained | No scientific deduction; preserve the solver-only scope and do not introduce unsupported deployment claims |

**Overall:** **8/10**  | **Scholarly Confidence:** **5/5**

**Recommendation:** accept

**Verdict:** The manuscript supports a credible verified solver-composition contribution,
not a robust adaptive-routing superiority claim. A 9/10 stance requires a method-level
advance that controls prediction tails or interactions and competes with strong fixed
solver-family policies on a new frozen public portfolio. A 7/10 or lower stance would be
warranted if the paper hid the family-fixed loss, if complete costs were asymmetric, or
if any gate or production-order mismatch emerged.

## 7. Top Strengths

1. **Correctness authority is explicit.** Every returned result must pass a
   family-specific original-equation gate; candidate and Router predictions cannot
   certify a result (`paper/abstract.tex:10`, `paper/sections/05_verification_aware_fusion.tex`).
2. **Complete-path accounting is scientifically relevant.** Candidate, correction,
   transfer, gate, rejection, and terminal continuation costs are charged rather than
   reporting candidate latency alone (`paper/sections/03_problem_formulation.tex`).
3. **The finite optimizer is auditable.** DP/exhaustive and production plan-order
   mismatches are zero under the declared bounded model.
4. **The new held-out contract is substantially stronger.** Six unseen collection groups
   and balanced numeric classes close the main Round 38 split-design weakness.
5. **Sparse failure handling is numerical rather than rhetorical.** Structural-rank
   deficiency rejects singular direct paths before SuperLU; LSQR then succeeds under
   strict original-residual checks for the retained deficiency-3 and deficiency-2 cases
   (`tests/sparse_suite_unit.cpp:72`, `tests/sparse_suite_unit.cpp:113`).
6. **Negative results are not hidden.** Fixed SuperLU, family-fixed routing, prediction
   tails, action interactions, failed device paths, and transfer regressions remain in
   the manuscript and evidence ledger.
7. **Artifact discipline is unusually strong.** The first-run directory is preserved,
   v1/v2 are explicitly development data, and frozen hashes are executable checks rather
   than prose-only promises.

## 8. Major And Minor Concerns

### R39-C1 — Adaptive Routing Does Not Beat Strong Controls

- **Severity:** major.
- **Affected criteria:** evidence, significance, method effectiveness.
- **Evidence:** `conditioned_heldout_regret=1.895881`,
  `fixed_action_heldout_regret=1.530423`, and
  `family_fixed_action_heldout_regret=1.457103` in the v3 first-run evidence.
- **Deduction:** the central adaptive predictor is not the best deployable policy in the
  strongest public experiment. The contribution must remain solver composition and
  verified complete-cost selection, not learned routing superiority.
- **Repair condition:** introduce a tail-aware, support-aware, or family-hierarchical
  decision method and evaluate it on a newly frozen public set against fixed SuperLU and
  the training-selected family-fixed policy.
- **Expected movement:** potentially `+1` overall only if the new method produces a
  decision-level improvement without weakening gate correctness or cost accounting.

### R39-C2 — Prediction Tails Dominate Policy Quality

- **Severity:** major.
- **Affected criteria:** soundness, evidence.
- **Evidence:** p95 cost error `36.417`, maximum cost error `1481.163`, Brier `0.309`,
  ECE `0.322`, and maximum-action calibration error `0.624`.
- **Deduction:** median fit quality is not sufficient for cascade planning because rare
  large cost errors can change reach-weighted plan cost and order.
- **Repair condition:** report support coverage and uncertainty by action/family, add a
  robust loss or uncertainty bound, and compare selected-plan regret before and after
  tail-aware abstention.
- **Expected movement:** `+0.5` to `+1` overall if the tail control materially improves
  strong-control regret on unseen groups.

### R39-C3 — Action Interaction Is Too Large To Treat As Incidental

- **Severity:** major.
- **Affected criteria:** soundness.
- **Evidence:** maximum interaction delta `0.169242` and maximum order delta `0.027393`.
- **Deduction:** exact DP optimality is valid only under its stated order-invariant model;
  the measured interaction is large enough that a reviewer will ask whether it changes
  selected-plan ranking, not merely measured cost.
- **Repair condition:** either model pairwise/state-dependent action cost and pass
  changes, or replay all competitive plan permutations and prove the chosen plan remains
  decision-optimal under measured interactions.
- **Expected movement:** dimension-level improvement unless it also changes public
  regret; lower the overall score if interactions reverse key plan rankings.

### R39-C4 — External Validity Remains Narrow

- **Severity:** moderate.
- **Affected criteria:** significance, evidence.
- **Evidence:** all authoritative timing is one Apple M4; both v3 SPD matrices occupy the
  large band; HINTS remains one 1D Poisson configuration; no stronger external hybrid
  Router is executed.
- **Deduction:** the paper establishes a qualified local solver system, not cross-platform
  or broadly family-independent superiority.
- **Repair condition:** add another CPU architecture or accelerator and one competitive
  external hybrid learned-solver/preconditioner baseline under the same complete-cost
  and original-equation contract.
- **Expected movement:** mostly significance/evidence; not sufficient alone for 9/10.

### R39-C5 — The Main Decision Result Competes With Too Many Secondary Results

- **Severity:** minor.
- **Affected criteria:** clarity, reviewer-facing risk.
- **Evidence:** the abstract and evaluation combine SuiteSparse policy regret, gate-only
  scaling, Navier--Stokes complete-path scaling, PDEBench-derived comparisons, operators,
  and HINTS.
- **Deduction:** a reviewer may remember the positive workload speedups but miss that the
  strongest adaptive public route loses to both fixed controls.
- **Repair condition:** keep the family-fixed loss adjacent to every headline routing
  statement and label the other results as evidence for component/complete-path behavior,
  not adaptive policy dominance.
- **Expected movement:** clarity only; lower if the negative result is de-emphasized.

### R39-C6 — Submission Metadata Is Incomplete

- **Severity:** minor, non-scientific.
- **Affected criteria:** format/readiness.
- **Evidence:** `paper/authors.tex` and `paper/sections/11_acknowledgments.tex` contain
  explicit placeholders.
- **Deduction:** the artifact is reviewable but not submission-ready.
- **Repair condition:** fill or blind author, affiliation, correspondence, funding, and
  acknowledgment metadata according to the current venue policy.
- **Expected movement:** no scientific score change; removes desk-risk uncertainty.

## 9. Claim–Evidence Audit

| Claim | Evidence | Status | Reviewer judgment |
| --- | --- | --- | --- |
| Every returned result is controlled by the original equation | Family-specific gates, production traces, 48/48 v3 successes, zero gate mismatch | Supported | Strongest soundness axis; retain exact tolerance and callback boundaries |
| Fixed-cascade ordering is optimal under order-invariant statistics | Derivation plus 24-permutation exhaustive check | Supported within scope | Do not generalize to interacting or adaptive statistics |
| Finite expert--budget DP is exact under its model | Zero DP/exhaustive mismatch | Supported within scope | Model exactness is not environment-level global optimality |
| Request conditioning improves a static profile | `1.895881` versus `1.900423` | Technically supported but practically weak | State the approximately `0.239%` margin; do not present as robust dominance |
| Request conditioning beats strong fixed policies | Fixed SuperLU `1.530423`; family-fixed `1.457103` | Contradicted | The manuscript correctly retains this as a negative result |
| v3 is unseen at first evaluation | Pre-action freeze, new groups, first-run directory, v1/v2 declared development | Supported | Strong evidence-design improvement over Round 38 |
| Numerical correctness is preserved on structurally singular cases | Structural-rank gate, LSQR terminal path, strict residual regression | Supported | Important solver-core result; not a claim about arbitrary inconsistent systems |
| Parallel and heterogeneous execution improve complete-path performance universally | One-host mixed positive and negative studies | Not claimed | Current qualified wording is appropriate |

## 10. Experiment, Baseline, And Reproducibility Audit

- **Split integrity:** strong. The final-held-out set is frozen before action timing,
  excludes all pre-v3 groups, and is balanced across three numeric classes.
- **Control strength:** improved. Static, fixed SuperLU, family-fixed, size-only,
  RHS-only, and tolerance-only policies are all visible. Family-fixed is the decisive
  control and must remain primary.
- **Numerical baselines:** broad and executable. PCG, GMRES-ILU/ILUT, sparse direct,
  Accelerate, SuperLU, LSQR, structured direct, Newton, and model solvers are represented.
- **External learned/hybrid baselines:** still limited. HINTS is executed, but the Greedy
  PDE Router and a stronger sparse hybrid learned-solver/preconditioner control remain
  absent.
- **Metrics:** decision-relevant. Complete runtime, regret, cost error, Brier/ECE, gate
  mismatch, continuation count, DP agreement, plan order, and interaction/order deltas
  expose both performance and correctness.
- **Statistical rigor:** paired timing and bootstrap contracts are strong for the main
  timing suites. Three repetitions per compatible v3 action expose gross interaction
  and prediction failures but do not fully characterize tail uncertainty.
- **Failure retention:** strong. The artifact keeps no-common-success cases, device and
  transfer regressions, public policy losses, singular direct rejections, and terminal
  continuation.
- **Reproduction integrity:** strong. The v3 first-run directory was not overwritten;
  CMake reproduction output is redirected to a separate v3 reproduction directory.
- **Data lock:** 57 SuiteSparse systems, 57 matrix files, and six RHS files pass local
  byte checks and online official-page verification.

## 11. Writing And Presentation Review

### Writing Scorecard

| Dimension | Weight | Score (1-5) | Confidence (1-5) | Evidence basis | Concrete repair |
| --- | ---: | ---: | ---: | --- | --- |
| Storyline and motivation | 12 | 4 | 5 | Introduction states the complete-cost and original-equation authority problem before the method | Keep the solver question central; do not add deployment narratives |
| Contribution display | 12 | 4 | 5 | Contributions separate formulation, execution, and evidence | Make the sparse structural-rank/LSQR robustness contribution more visible if space permits |
| Paragraph logic | 10 | 4 | 5 | Sections generally have one role and stable transitions | Split only the densest evaluation paragraphs if final page budget allows |
| Claim-evidence alignment | 14 | 5 | 5 | Strong controls and negative results are adjacent to routing claims | Preserve this alignment after final edits |
| Method readability | 10 | 4 | 5 | Roles, gate authority, and DP assumptions are explicit | Add one compact sentence distinguishing model exactness from interaction-aware optimality wherever DP exactness is summarized |
| Experiment narration | 10 | 4 | 5 | The public route is interpreted through controls and tail metrics | State that lower regret is better at the first public-route occurrence |
| Related-work positioning | 8 | 4 | 4 | Portfolio, hybrid solver, and learned-component distinctions are present | A direct comparison table to the closest hybrid Router would reduce reviewer effort |
| Terminology and notation consistency | 8 | 5 | 5 | `gate` and `numerical continuation` are now stable and solver-specific | Keep `fallback` only in historical code or metric field names |
| LaTeX and format discipline | 8 | 4 | 5 | Temporary build is 12 pages with no undefined references or overfull boxes | Resolve author/acknowledgment placeholders before submission |
| Reviewer-facing risk | 8 | 4 | 5 | Abstract and conclusion disclose the fixed-control losses | Do not let positive PDE/operator results imply adaptive routing dominance |

**Weighted writing score:** **4.22/5**

**Writing risk band:** **low**, with the main risk being evidence density rather than
logical inconsistency.

### Format Audit

- Synchronized PDF: 12 pages, 304,777 bytes,
  SHA-256 `33c96bdb279629fe6d111a8b7b8275de8a7f7c28f10c61aaf145b27ff82711cf`.
- Undefined citations/references: none detected.
- Overfull boxes: none detected.
- Underfull boxes: present but non-fatal; no content loss was observed from the log.
- Author/affiliation/acknowledgment fields: incomplete.
- Current-year venue-specific policy check: unresolved.

## 12. Multi-Reviewer Panel

### Reviewer R1 — Numerical Algorithms

- **Score tendency:** 8/10.
- **Confidence:** 5/5.
- **Main positive signal:** gate-controlled return, structural-rank rejection, and LSQR
  continuation provide real numerical correctness mechanisms.
- **Main negative signal:** the adaptive public route loses to transparent fixed solver
  controls, and model exactness is conditional on action invariance.
- **Score-change condition:** raise after interaction-aware or tail-aware routing wins on
  a new frozen public set; lower if any original-residual mismatch appears.

### Reviewer R2 — Solver Selection And Statistical Learning

- **Score tendency:** 7/10.
- **Confidence:** 5/5.
- **Main positive signal:** frozen group-disjoint evaluation and multiple feature/control
  policies make the negative result interpretable.
- **Main negative signal:** extreme cost tails and the equality of RHS-only,
  tolerance-only, and family-fixed regret suggest the current high-dimensional predictor
  is not extracting stable request-level value.
- **Score-change condition:** raise with uncertainty-aware abstention and decisive regret
  gains over family-fixed; lower if control selection used held-out information.

### Reviewer R3 — Parallel Numerical Systems

- **Score tendency:** 8/10.
- **Confidence:** 5/5.
- **Main positive signal:** gate-only, complete-path, batch, transfer, and device costs are
  separated, preventing kernel-only speedup claims.
- **Main negative signal:** all authoritative timing is one Apple M4, and adaptive sparse
  routing does not outperform the strong fixed direct path.
- **Score-change condition:** raise with another architecture and a competitive hybrid
  baseline under identical complete-cost accounting.

### Reviewer R4 — Reproducibility And Artifact

- **Score tendency:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** pre-experiment hashes, first-run preservation, executable
  claim checks, generated values, Release build, and 29/29 CTests form a strong audit
  chain.
- **Main negative signal:** the core bundle excludes large public payloads and external
  HINTS dependencies; complete third-party reproduction remains unverified.
- **Score-change condition:** maintain 9 if all final hashes agree; lower if the first-run
  directory or frozen manifests are regenerated after seeing results.

### Reviewer R5 — Skeptical Generalist / AC

- **Score tendency:** 8/10.
- **Confidence:** 5/5.
- **Main positive signal:** the paper is honest about where verified composition works
  and where adaptive selection fails.
- **Main negative signal:** the novelty may be read as systems composition unless the
  complete-cost formulation, exact bounded optimization, and numerical invariants remain
  foregrounded.
- **Score-change condition:** raise with a decisive method result; lower if the paper
  reframes the negative route as a positive adaptive-routing claim.

### Panel Synthesis

- **Agreement:** the solver is numerically credible, the v3 evidence is substantially
  stronger, and the adaptive policy result is negative.
- **Disagreement:** artifact and numerical reviewers give more credit to the verified
  composition framework; the statistical-learning reviewer discounts the paper more
  heavily because family-fixed routing wins.
- **Decisive accept axis:** original-equation correctness plus complete-path, production,
  and negative-result evidence.
- **Decisive reject axis:** would be hidden leakage, asymmetric cost accounting, a gate
  mismatch, or overclaiming adaptive superiority; none is currently observed.
- **Unresolved evidence:** cross-hardware timing, stronger external hybrid baselines,
  and decision-rank stability under measured interactions.
- **Final calibrated stance:** **8/10, accept, confidence 5/5**.

## 13. Concern-To-Action Table

| ID | Severity | Concern | Evidence basis | Fix class | Concrete action | Score impact condition |
| --- | --- | --- | --- | --- | --- | --- |
| R39-C1 | Major | Conditioned policy loses to fixed SuperLU and family-fixed | v3 regret fields | Solver method | Add tail/support/family-aware policy and new frozen evaluation | Raise only after strong-control public improvement |
| R39-C2 | Major | Extreme prediction tails | v3 cost/calibration fields | Modeling and calibration | Add uncertainty/support diagnostics and tail-aware abstention | Raise if plan regret materially improves |
| R39-C3 | Major | `0.169` action interaction challenges invariant-action DP | interaction artifact | Optimizer/soundness | Model interactions or prove ranking stability by replay | Lower if interactions reverse selected plans |
| R39-C4 | Moderate | One host and limited external hybrid controls | methodology and limitations | Experiment | Add another architecture and competitive hybrid baseline | Evidence/significance gain; insufficient alone for 9 |
| R39-C5 | Minor | Dense evidence can hide the decisive negative | abstract/evaluation/conclusion | Writing | Keep family-fixed loss adjacent to routing result | Lower clarity if caveat is separated |
| R39-C6 | Minor | Author and acknowledgment placeholders | author/acknowledgment files | Submission format | Fill or blind metadata under current policy | No scientific score change |

## 14. AC / Meta-Review

The paper must be judged as a numerical solver-method and solver-systems paper, not as a
distributed service, high-availability, isolation, or security paper. Its strongest
contribution is the verified composition of candidate, correction, original-equation
acceptance, and numerical continuation under complete-path cost, together with exact
finite-action optimization under explicit assumptions.

The v3 experiment is an important improvement because it is frozen before measurement,
collection-group-disjoint, balanced across three numeric classes, and executed through
the production path. It also makes the method limitation harder to dismiss as a weak
split artifact: conditioned routing barely beats static and loses to fixed SuperLU and
family-fixed controls. This is a credible negative result, not evidence for increasing
the method score.

The correct AC interpretation is therefore stable from Round 38: **8/10, accept**. The
paper is above average in soundness, auditability, and honesty, but 9/10 would require a
new solver-selection advance that addresses prediction tails or interactions and changes
the decision-level result against strong public controls. Evidence completeness alone
does not justify a higher score.

## 15. Score-Change Conditions

| Change | Condition | Likely affected dimensions | Expected movement |
| --- | --- | --- | --- |
| Raise score | New frozen public evaluation where a tail/interaction-aware policy competes with fixed SuperLU and family-fixed while preserving zero gate mismatch | Soundness, evidence, significance | `+1` overall possible |
| Raise dimension only | Cross-hardware timing and stronger external hybrid baseline without a policy-method gain | Evidence, significance | `+0.5` dimension-level; overall likely unchanged |
| Lower score | Any held-out leakage, overwritten first-run evidence, asymmetric setup/continuation cost, or original-equation gate mismatch | Soundness, evidence, reproducibility | `-1` or fatal depending on scope |
| Lower score | Manuscript hides the family-fixed loss or implies universal adaptive superiority | Clarity, evidence, ethics/limitations | `-1` overall possible |
| No quick change | Replacing the order-invariant optimizer with an interaction-aware model and obtaining a new unseen result | Novelty, soundness | Requires new method and experiment, not wording |

## 16. Questions For Authors

1. Were the family-fixed actions selected strictly from training data, and can the paper
   state this at the first family-fixed occurrence rather than only in artifact fields?
2. Why do RHS-only and tolerance-only controls exactly match family-fixed regret? Is this
   stable policy behavior or a small-split coincidence?
3. Does the measured `0.169242` interaction delta change the ranking of any competitive
   top-$k$ plans, or only their realized costs?
4. Would support clipping, conformal cost bounds, or abstention to family-fixed reduce
   p95/maximum error and regret without changing the gate contract?
5. Are direct-solver setup, iterative setup, correction, gate, and terminal continuation
   charged symmetrically for every public action and control?
6. How should readers interpret the absence of a small-band held-out SPD matrix under the
   deterministic selection rule?

## 17. Checks Run

- `python3 -m py_compile paper/check_evidence.py paper/check_artifact_manifest.py`: pass.
- `python3 paper/check_evidence.py`: pass.
- SuiteSparse data-lock verification with official upstream checks: pass for 57 systems,
  57 matrix files, and six RHS files.
- Release full build: pass.
- Initial CTest exposed the stale expected SuiteSparse discovery count `51`; after
  updating it to the locked count `57`, the targeted regression passed.
- Full Release CTest after the correction: **29/29 pass**.
- Temporary LaTeX build: 12 pages, no undefined references, no overfull boxes.
- v3 first-run evidence directory was read but not regenerated or overwritten.
- Final source/PDF hash synchronization and artifact manifest validation: pass.

## 18. Unresolved Or Unverified

- Current-year TPDS author, anonymity, disclosure, and artifact policy.
- Final author, affiliation, correspondence, funding, and acknowledgment metadata.
- Independent payload-complete reproduction on another host.
- Native cross-architecture performance.
- Stronger external hybrid learned-solver or learned-preconditioner comparisons.
- Interaction-aware decision ranking on a new unseen public set.

## 19. Recommended Next Owner

- **Primary:** solver methods and experimental design, focused on tail-aware,
  family-aware, and interaction-aware routing against strong fixed controls.
- **Secondary:** artifact audit for future evidence or manuscript changes.
- **Writing:** only local clarification and compression; no deployment, high-availability,
  isolation, or security work should enter the scientific backlog.

## 20. Output Self-Check

- Scope remains solver-only and excludes unrelated infrastructure requirements.
- Every score is tied to inspectable manuscript or artifact evidence.
- The stronger v3 split is not misrepresented as a positive policy result.
- No experiment, baseline, citation, or venue policy was invented.
- No acceptance probability is claimed.
- Negative results and score-limiting conditions remain explicit.
