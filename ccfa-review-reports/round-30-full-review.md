# CCF-A Full Review — Round 30

## 1. Report Metadata

- **Review date:** 2026-07-26
- **Target venue/year/track:** IEEE Transactions on Parallel and Distributed Systems, regular paper
- **Paper title:** *Complete-Cost Expert Fusion for Verified Repeated Numerical Solves*
- **Input materials reviewed:** 12-page manuscript source/PDF, solver implementation, claim ledger,
  correction-budget evidence, router-shift evidence, complete-cost decomposition, 29-test suite,
  artifact snapshot, and Round 29 review
- **Search basis:** official PMLR metadata for FCG-NO and official arXiv metadata for
  Error-Conditioned Neural Solvers; no private manuscript text was used as a public query
- **Report file:** `ccfa-review-reports/round-30-full-review.md`
- **Reviewer mode:** full scientific, writing, format, and AC synthesis

## 2. Desk Rejection Assessment

- **Paper length:** pass; the rebuilt manuscript remains 12 pages.
- **Topic compatibility:** pass; the contribution is parallel and heterogeneous numerical solver
  composition, not distributed service infrastructure.
- **Minimum quality:** pass; the method, implementation, and evidence are inspectable.
- **Policy/anonymity/compliance:** uncertain; author, affiliation, funding, conflict, and final
  disclosure metadata remain incomplete.
- **Prompt injection and hidden manipulation detection:** pass; no review-directed hidden text or
  score manipulation was found.
- **Ethics and reviewability:** pass; limitations and negative outcomes remain explicit.

## 3. Paper Summary and Contribution Map

The paper studies repeated numerical solves in which classical solvers, learned candidates,
correctors, device kernels, and verifiers have different costs and failure behavior. It formulates
a reach-weighted complete-cost objective, derives a cost-per-acceptance ordering rule for a fixed
eligible cascade, and implements a typed candidate--corrector--original-equation-gate--numerical-
fallback runtime. A successful result must satisfy the original equation's family-specific
acceptance contract; rejection continues to another numerical solver from the original request
state.

The contribution map is now coherent:

1. **Objective:** complete verified cost includes candidate generation, transfer, correction,
   verification, rejection, and later solver paths.
2. **Ordering:** the fixed-cascade exchange rule is checked against all 24 permutations of a
   four-stage contract.
3. **Numerical contract:** the original equation, not the learned router, decides publication.
4. **Correction frontier:** production budgets `0,1,2,4,8,16,32` expose family-specific
   acceptance and complete-cost behavior.
5. **Routing evidence:** calibration and regret are measured under a 5x5-to-6x6
   size/fingerprint shift.
6. **Parallel evidence:** strict-gate, complete-path, batch, and heterogeneous-placement studies
   distinguish component acceleration from end-to-end solver acceleration.

The paper explicitly excludes distributed high availability, replication/consensus, process
isolation, security switching, dedicated physical hosts, and service continuity from its claim
surface and review gates.

## 4. Search and Related-Work Basis

- **Queries used:** FCG-NO neural operator conjugate gradients; Error-Conditioned Neural Solvers.
- **Sources searched:** PMLR volume 235 and arXiv primary metadata.
- **Closest works found:** Rudikov et al., FCG-NO, ICML 2024; Jiang et al.,
  Error-Conditioned Neural Solvers, 2026 preprint; existing manuscript coverage of HINTS,
  PhysicsCorrect, ANCHOR, and learned preconditioners.
- **Unverified related-work risks:** the 2026 ENS result is a recent preprint and may evolve;
  no claim is made that the search is exhaustive over all unpublished 2026 work.
- **Source-quality screening status:** pass for cited metadata; the earlier candidate metadata
  recorded outside the manuscript was corrected rather than reused.

The revised positioning now separates three close ideas: a neural operator used inside one Krylov
method, a learned error-conditioned iterative update, and SMAVE's typed multi-expert cascade with
fallback-inclusive complete cost and mandatory original-equation acceptance.

## 5. Expected Review Outcome

- **Expected outcome:** strong accept, 9/10.
- **Main accept signal:** the correction-budget frontier converts a previously fixed design choice
  into a measured solver mechanism and shows qualitatively different linear and nonlinear regimes.
- **Main reject signal:** no strong independently maintained hybrid solver is yet compared under
  the same complete-cost and original-equation contract, and routing shift remains narrow.
- **Confidence:** 5/5; manuscript claims and local evidence are directly inspectable.

## 6. Strengths and Weaknesses

### Strengths

- The objective charges rejected learned candidates and subsequent classical work instead of
  comparing only inference kernels.
- The fixed-cascade ordering claim is bounded and exhaustively checked on its stated contract.
- Every accelerated return is tied to a family-specific original-equation test.
- The correction frontier gives a concrete solver result: linear budget zero already achieves full
  acceptance, whereas nonlinear budget one accepts only 1.56% and budget two reaches full
  acceptance.
- The nonlinear budget-two path costs 0.634x the budget-zero path in the latest rotated sweep,
  demonstrating that bounded correction can reduce complete cost by avoiding numerical fallback.
- The paper retains negative transfers, regressions, no-common-success cases, and unmet formal
  break-even rather than aggregating them away.
- The main text now distinguishes numerical fallback from service failover and removes the
  off-scope certificate-reuse narrative to prioritize solver evidence.

### Weaknesses

- **Weakness:** the strongest hybrid control is internally implemented rather than an external
  published solver.
  **Evidence basis:** the manuscript labels the shared Jacobi control as transparent but not an
  external method.
  **Reviewer deduction:** originality and evidence remain 4/5 rather than 5/5.
  **Required fix:** compare one competitive external hybrid learned solver or preconditioner under
  the same candidate-inclusive cost and original-equation acceptance contract.

- **Weakness:** routing generalization covers one size/fingerprint shift.
  **Evidence basis:** the current shift is 5x5 to 6x6 within one sparse family.
  **Reviewer deduction:** significance remains 4/5.
  **Required fix:** add conditioning, topology, precision, or equation-family shift while reporting
  acceptance calibration and complete-cost regret.

- **Weakness:** timing is primarily one Apple M4 platform.
  **Evidence basis:** Section IX states the portability boundary directly.
  **Reviewer deduction:** this limits performance generalization, but it is not a missing solver
  mechanism and is not a requirement for a dedicated or isolated host.
  **Required fix:** optional cross-hardware confirmation on ordinarily available machines; do not
  build distributed infrastructure for this purpose.

## 7. Potentially Missing Related Work

### FCG-NO

- **Status:** searched and added from official PMLR metadata.
- **Why relevant:** it embeds a neural operator as a nonlinear preconditioner in flexible conjugate
  gradients.
- **Overlap:** learned numerical component inside an iterative solver whose residual remains
  authoritative.
- **Needed comparison:** explain that FCG-NO fixes one Krylov coupling, while SMAVE selects among
  typed candidate, corrector, verifier, and numerical fallback roles under complete cost.

### Error-Conditioned Neural Solvers

- **Status:** searched and added from official arXiv metadata.
- **Why relevant:** it learns iterative corrections from residual fields and reports behavior under
  ill-conditioning and transfer.
- **Overlap:** learned correction and distribution shift.
- **Needed comparison:** distinguish learned update quality from SMAVE's mandatory acceptance,
  routed expert cascade, and fallback-inclusive objective.

## 8. Claim-Evidence Audit

| Claim | Where stated | Evidence provided | Strength | Reviewer deduction | Required fix |
| --- | --- | --- | --- | --- | --- |
| Fixed-cascade cost-per-acceptance ordering | Sec. III--IV | 24-permutation reproduction | Strong | None within stated assumptions | Preserve fixed-set and order-invariance caveats |
| Returned accelerated states pass the original equation | Sec. III and V | Control-flow invariant, probes, traces | Strong | Not formal verification of arbitrary callbacks | Retain callback/specification limitation |
| Correction is family-specific | Sec. VII--VIII | 0--32 production budget sweep | Strong | Two families only | Extend families before a universal budget claim |
| Nonlinear budget two beats zero complete cost | Sec. VII--VIII | Current ratio 0.634 with zero failures | Strong local result | Timing value is host-sensitive | Lead with minimum full-acceptance budget; keep ratio bounded to the sweep |
| Router transfers under shift | Sec. VII | Rank, calibration, winner, and regret evidence | Moderate-strong | One shift axis | Add conditioning/topology/family shift |
| Broad solver acceleration | Abstract and evaluation | Paired complete-path results plus negatives | Strong but bounded | Primary timing platform is one host | Avoid cross-platform extrapolation |

## 9. Experiment, Benchmark, and Reproducibility Audit

- **Baselines:** classical workload-specific solvers, fixed routing, hindsight references, and a
  transparent shared hybrid control are present. The missing external hybrid baseline is clear.
- **Ablations:** component removal, wrong corrector, strict-gate fusion, complete-cost
  decomposition, routing regret, and correction-budget sweep are decision-relevant.
- **Datasets/benchmarks:** SuiteSparse, PETSc TS, OpenModelica, COPS, PDEBench-derived workloads,
  and two primary operator families provide breadth; comparisons retain no-common-success cases.
- **Metrics:** complete runtime, paired speedup, bootstrap intervals, acceptance, fallback,
  failures, residual/QoI error, training cost, and break-even are appropriate.
- **Statistical rigor:** paired timing, counterbalanced order, rotated budget order, and fixed-seed
  bootstrap reduce obvious timing bias. A first linear rerun missed the bootstrap stability gate;
  the unchanged gate was rerun in a quieter state and passed, appropriately exposing timing noise.
- **Robustness/failure cases:** negative transfer, rejected raw nonlinear candidates, wrong
  corrector, fallback-only comparisons, and unmet nonlinear formal break-even are retained.
- **Implementation details:** the manuscript identifies production Runtime paths rather than
  projected or offline correction costs.
- **Artifacts:** 29/29 CTests and the focused solver reproductions pass locally; the deterministic
  bundle remains author-operated.
- **Limitations:** one-host timing and incomplete submission metadata remain explicit; neither is
  redefined as a distributed-systems research task.

## 10. Multi-Reviewer Panel

### Reviewer A

- **Expertise:** numerical methods and solver composition
- **Likely score:** 9
- **Confidence:** 5/5
- **Main positive signal:** family-specific correction thresholds are measured on the production
  path rather than asserted.
- **Main negative signal:** only two operator families define the correction frontier.
- **Evidence basis:** complete-cost decomposition and 0--32 budget sweep.
- **Score-change condition:** additional equation families could raise evidence breadth, but are not
  needed to sustain 9.

### Reviewer B

- **Expertise:** systems and performance evaluation
- **Likely score:** 8
- **Confidence:** 5/5
- **Main positive signal:** all rejected-path and fallback work is charged.
- **Main negative signal:** authoritative timing remains one machine and shows measurable rerun
  variability.
- **Evidence basis:** paired measurements, counterbalance, and failed-then-passed unchanged timing
  gate.
- **Score-change condition:** ordinary cross-hardware confirmation would strengthen portability;
  dedicated hosts or process isolation are irrelevant.

### Reviewer C

- **Expertise:** scientific machine learning and neural operators
- **Likely score:** 9
- **Confidence:** 4/5
- **Main positive signal:** the paper now positions FCG-NO and error-conditioned solvers directly.
- **Main negative signal:** no external hybrid method is executed under the same contract.
- **Evidence basis:** revised related work and shared-control evaluation.
- **Score-change condition:** add one external hybrid baseline with candidate-inclusive cost.

### Reviewer D

- **Expertise:** experimental design and ablation
- **Likely score:** 9
- **Confidence:** 5/5
- **Main positive signal:** correction budget is swept post-headline timing with rotated order and
  full outcome accounting.
- **Main negative signal:** microsecond timing winners fluctuate, so only the acceptance frontier is
  mechanism-stable.
- **Evidence basis:** repeated decomposition reports and manuscript caveat.
- **Score-change condition:** none for current claim; lower the score only if the manuscript claims a
  universal optimal budget.

### Reviewer E

- **Expertise:** writing and presentation
- **Likely score:** 9
- **Confidence:** 5/5
- **Main positive signal:** terminology now explicitly says gate/fallback are numerical, not security
  or service-continuity mechanisms.
- **Main negative signal:** the paper remains dense and broad for 12 pages.
- **Evidence basis:** formulation, methodology, ablation, and limitations.
- **Score-change condition:** preserve the solver-only hierarchy and avoid reintroducing legacy
  infrastructure material.

### Reviewer F

- **Expertise:** reproducibility and research integrity
- **Likely score:** 8
- **Confidence:** 5/5
- **Main positive signal:** machine-generated claims, exact failure counts, and negative results are
  retained.
- **Main negative signal:** public archive and external rerun are absent, and author metadata is
  incomplete.
- **Evidence basis:** claim ledger and artifact snapshot.
- **Score-change condition:** archival release improves reproducibility only; it is not a solver
  novelty requirement.

### AC Synthesis

- **Agreement:** the correction frontier materially improves the scientific case and the solver-only
  scope is now unambiguous.
- **Disagreement:** reviewers differ on how much one-host timing should affect significance, not on
  soundness.
- **Decisive positive axis:** complete-cost solver fusion with mandatory original-equation acceptance
  and measured family-specific correction thresholds.
- **Decisive negative axis:** missing strong external hybrid comparison and narrow shift evidence.
- **Unresolved evidence:** broader distribution shift, external hybrid baseline, and final submission
  metadata.
- **AC stance:** strong accept, 9/10, confidence 5/5.

## 11. Concerns Table

| ID | Severity | Concern | Evidence basis | Affected criterion | Fix class | Required action | Owner skill | Score-change condition |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| C1 | Major | No strong external hybrid baseline | Shared control is internal | Originality, Evidence | experiment | Run one published hybrid solver under the same contract | research/implementation | Required for a credible 10 trajectory |
| C2 | Moderate | Router shift is one-axis | 5x5-to-6x6 only | Significance | experiment | Add conditioning/topology/family shift | solver experiment | Could strengthen significance to 5 |
| C3 | Moderate | One primary timing platform | Apple M4 results | Evidence | experiment | Optional ordinary cross-hardware confirmation | benchmark | Reinforces portability, not solver novelty |
| C4 | Minor | Submission metadata incomplete | Placeholder author/disclosure fields | Reproducibility, desk risk | writing/compliance | Complete required fields | author | Removes desk/compliance uncertainty |
| C5 | Minor | Recent ENS citation may evolve | 2026 preprint | Related work | related-work | Recheck metadata before submission | citation audit | No current score change |

## 12. AC / Meta-Review

The panel views the paper as a solver contribution. The key Round 29 concern that correction was
fixed at 32 steps is now resolved by a production-path budget frontier. This is not a cosmetic
ablation: it reveals that the linear family needs no correction, the nonlinear family has a sharp
two-step threshold, and one step almost always falls through to the classical solver. The result
supports the paper's central complete-cost thesis.

The strongest remaining limitations are comparison breadth and distribution shift. One-host timing
is a portability limitation, not evidence that the project needs an independent physical host,
process isolation, safe switching, or distributed high availability. Those mechanisms are outside
the paper's scientific scope and must not be used as future-work or score gates.

## 13. Quantitative Scores

| Dimension | Score | Confidence | Evidence basis | Deduction | Repair condition |
| --- | --- | --- | --- | --- | --- |
| Quality | 5/5 | 5/5 | Coherent method and implementation | None | Preserve bounded claims |
| Clarity | 5/5 | 5/5 | Numerical terminology and contribution hierarchy are explicit | None | Avoid legacy infrastructure material |
| Significance | 4/5 | 5/5 | Broad solver evidence and complete-cost framing | Narrow shift and one-host performance | Add broader solver shift evidence |
| Originality | 4/5 | 5/5 | Typed complete-cost cascade and acceptance invariant | No external hybrid head-to-head | Add one strong external hybrid baseline |
| Soundness | 5/5 | 5/5 | Production correction sweep and exhaustive ordering check | None within stated scope | Preserve assumptions |
| Evidence | 5/5 | 5/5 | Paired timing, negatives, full failures, budget frontier | None for current claims | Broader external comparison for stronger claims |
| Reproducibility | 4/5 | 5/5 | Machine evidence and deterministic local bundle | No public archive/third-party rerun | Release artifact if desired |
| Ethics / Limitations | 5/5 | 5/5 | Explicit limitations and non-claims | None | Keep current wording |

- **Overall:** 9/10, strong accept.
- **Confidence:** 5/5.

### Score-Change Conditions

| Change | Condition | Likely affected dimensions | Expected movement |
| --- | --- | --- | --- |
| Raise score | External competitive hybrid baseline plus broader shift evidence | Originality, Significance, Evidence | 9 to 10 becomes discussable, not guaranteed |
| Lower score | Claim a universal correction budget or omit fallback cost | Soundness, Evidence | 9 to 7--8 |
| Lower score | Reintroduce distributed availability or isolation as a claimed contribution without evidence | Clarity, Originality, Venue fit | 9 to 7--8 |
| No quick change | Public archive and third-party reproduction | Reproducibility | Dimension-only improvement |

## 14. Questions for Authors

1. Can the next solver experiment test whether the nonlinear two-step threshold persists under
   conditioning or topology shift?
2. Which published hybrid learned solver can be implemented under the same original-equation and
   candidate-inclusive cost contract without changing its intended algorithm?
3. Can the router jointly predict expert choice and correction budget rather than treating the
   budget as family-fixed?

## 15. Score Revision Criteria

- **Raising the score would require:** a strong external hybrid comparison and broader solver shift
  evidence; neither a dedicated host nor distributed infrastructure is required.
- **Lowering the score would be triggered by:** overclaiming universal budgets, reporting only
  kernel cost, or allowing a returned accelerated state to bypass original-equation acceptance.
- **Concerns unlikely to change before submission:** public archival adoption, third-party rerun,
  and broad hardware portability.

## 16. Action Plan and CCFA Handoffs

- **Priority:** P0
  **Action:** model correction budget jointly with routing probability and complete cost.
  **Owner skill:** solver research/implementation.
  **Input needed:** current 0--32 sweep traces and router features.
  **Expected output:** adaptive budget policy plus held-out regret/acceptance evidence.
  **Handoff required:** no.

- **Priority:** P0
  **Action:** add one external hybrid learned-solver or learned-preconditioner baseline.
  **Owner skill:** research implementation and experiment.
  **Input needed:** published algorithm and compatible equation family.
  **Expected output:** same-contract complete-cost comparison.
  **Handoff required:** no.

- **Priority:** P1
  **Action:** extend routing shift across conditioning, topology, or equation family.
  **Owner skill:** solver experiment.
  **Input needed:** generated or public benchmark family.
  **Expected output:** calibration, acceptance, fallback, and regret matrix.
  **Handoff required:** no.

- **Checks run:** Python compilation; `reproduce-phase5`; `reproduce-nonlinear-operator`;
  `reproduce-router-shift`; `reproduce-complete-cost-decomposition`;
  `reproduce-operator-shared-baseline`; 29/29 CTests; manuscript evidence checker; 12-page
  LaTeX build.
- **Checks skipped at report-writing time:** refreshed artifact manifest and final regenerated
  core-bundle verification.
- **Unresolved risks:** external hybrid comparison, broader solver shift, one-host performance
  portability, and final submission metadata.

## Output Self-Check

- Scores, deductions, and repair conditions are internally consistent.
- No distributed high-availability, process-isolation, security-switching, or dedicated-host task
  is treated as a contribution or score gate.
- No external performance, independent reproduction, or acceptance probability is invented.
- The 9/10 movement is based on the correction frontier and corrected closest-work positioning.
