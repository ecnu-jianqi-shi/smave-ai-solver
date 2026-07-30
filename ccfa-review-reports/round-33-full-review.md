# CCF-A Full Review — Round 33

## 1. Report Metadata

- **Mode:** full scientific, writing, format, and AC synthesis.
- **Review date:** 2026-07-26.
- **Venue and assumptions:** IEEE Transactions on Parallel and Distributed Systems,
  regular paper; 12-page IEEE Computer Society journal format.
- **Paper:** *Complete-Cost Expert Fusion for Verified Repeated Numerical Solves*.
- **Materials reviewed:** manuscript sources, exact expert--budget dynamic program,
  production Router/Runtime path, unit tests, controlled exhaustive oracle,
  two-family training/held-out joint-policy evidence, claim ledger, and artifact checks.
- **Scientific scope:** solver algorithms, numerical acceptance, routing calibration,
  correction budgets, complete-path cost, parallelism, and heterogeneous execution.
  Distributed high availability, process isolation, security switching, and dedicated
  hosts are outside the contribution surface and are not score conditions.

## 2. Paper Summary

The paper formulates repeated numerical acceleration as a typed cascade of candidate,
corrector, original-equation gate, and numerical fallback stages. It optimizes the
reach-weighted complete cost rather than raw candidate latency. For a fixed eligible
cascade with order-invariant action statistics, it derives the cost-per-acceptance
ordering rule. For calibrated expert--budget actions, it implements an exact bounded
dynamic program that jointly selects subset, one budget per expert, and order while
charging rejection continuation and terminal fallback.

Round 33 adds the missing held-out evidence for the joint policy. Two controlled
12-variable coupled nonlinear SCC families, quadratic and cubic, each provide 32
training and 32 disjoint held-out scenarios. Costs, pass probabilities, and terminal
fallback costs come from production Runtime traces. Profiles are frozen on training
data, applied to held-out Runtime requests, repriced using held-out observations, and
compared with a held-out exhaustive oracle.

## 3. Calibrated Decision

- **Overall score:** **9/10, strong accept**.
- **Confidence:** **5/5**.
- **Score movement from Round 32:** evidence strength rises substantially within the
  9 band; the single-profile joint-policy breadth concern is closed.
- **Why not 10/10:** the new families and experts remain controlled constructions;
  action costs and pass probabilities are measured inputs rather than jointly predicted;
  and no strong published hybrid learned-solver or learned-preconditioner implementation
  is compared under the same complete-cost and original-equation contract.

## 4. Decisive Strengths

1. **The scientific question is now correctly scoped.** The paper is about solver
   selection, correction, numerical acceptance, and complete cost. The gate is an
   original-equation residual/constraint/defect test; fallback is continuation to the
   next numerical path.
2. **The joint decision is implemented, not simulated.** The production Router chooses
   expert, correction budget, subset, and order under one-budget-per-expert and explicit
   state-bound constraints.
3. **Continuation semantics are charged.** The recurrence includes action cost plus the
   rejection-weighted value of later actions or terminal fallback.
4. **Exactness has an independent oracle.** The controlled three-action probe matches
   exhaustive enumeration and exercises rejection, continuation, correction, and final
   original-equation acceptance in Runtime.
5. **Round 33 adds real profile freezing.** Training and held-out scenario grids are
   disjoint; held-out observations are unavailable to the source policy.
6. **The selected budget changes with equation-family difficulty.** The quadratic family
   selects budget `2`, while the cubic family selects budget `4`.
7. **Held-out complete-cost regret is explicit.** The maximum selected-plan regret is
   `1.000×` against the held-out exhaustive oracle for the measured action profiles.
8. **Numerical acceptance remains intact.** All 64 held-out requests succeed with zero
   gate mismatches; 32 first-stage rejections continue to 32 second-stage acceptances.
9. **Claims remain bounded.** The paper does not relabel calibrated objective values as
   workload speedups or call the controlled profiles a learned universal policy.

## 5. Major Concerns

### C1. Strong External Hybrid Baseline Is Still Missing

- **Severity:** major for a 10/10 claim, not fatal for acceptance.
- **Evidence basis:** the shared learned-candidate/Jacobi control is transparent and
  complete-cost fair, but it is not a faithful implementation of a published closest
  hybrid solver or learned preconditioner.
- **Deduction:** the paper establishes a strong internal method story but cannot show that
  its joint fusion dominates the closest published alternative under one contract.
- **Repair condition:** implement one compatible published hybrid method, preserve its
  algorithmic choices, and charge candidate, correction, gate, rejection, and fallback.

### C2. Joint-Policy Families Remain Controlled

- **Severity:** moderate.
- **Evidence basis:** the Round 33 matrix uses two constructed 12-variable SCC families,
  two experts, three actions, deterministic acceptance structure, and three repetitions.
- **Deduction:** it closes the single-profile correctness/evidence gap but does not show
  calibration under a complex public solver workload or a larger expert portfolio.
- **Repair condition:** evaluate the same frozen-profile/oracle protocol on at least one
  existing public sparse, operator, ODE/DAE, or PDE-derived workload with materially
  heterogeneous experts and budgets.

### C3. Action Statistics Are Calibrated Inputs

- **Severity:** moderate.
- **Evidence basis:** the optimizer consumes measured median action costs and pass
  probabilities; it does not jointly predict them from request features.
- **Deduction:** optimization is exact conditional on the profile, but end-to-end policy
  learning and robustness to cost reordering remain open.
- **Repair condition:** predict or cross-validate action cost and acceptance jointly and
  report held-out regret against the same exhaustive action oracle.

### C4. Performance Generalization Is Bounded

- **Severity:** secondary.
- **Evidence basis:** authoritative timing remains one Apple M4 system.
- **Deduction:** numerical correctness claims remain valid, but timing and placement
  conclusions cannot be generalized across architectures.
- **Repair condition:** repeat selected complete-path experiments on another native
  architecture; this is solver-performance evidence, not infrastructure research.

## 6. Soundness Review

- **Recurrence:** skip/take recursion enumerates admissible sorted subsequences; the
  used-expert mask enforces one budget per expert.
- **Terminal cost:** exhausted capacity or action list returns calibrated numerical
  fallback cost.
- **Profile source:** attempt cost is measured from Runtime total block time minus
  terminal fallback time; pass probability comes from attempt records.
- **Data separation:** training roots use a `0.025` grid beginning at `1.20`; held-out
  roots use a half-step-shifted `0.025` grid beginning at `1.3625`, so no scenario is
  shared.
- **Oracle comparison:** the held-out source plan is repriced with held-out action
  profiles and compared against exact held-out optimization.
- **Acceptance invariant:** routing changes cost and path selection, while the supplied
  original equation determines whether a result is returned.
- **Verifier integrity:** zero-valued calibration error is accepted correctly after
  fixing the CMake verifier's former truthiness bug; the numerical threshold remains
  unchanged.

## 7. Writing and Presentation Review

- The evaluation now names the held-out joint-policy result without presenting it as a
  general speedup.
- Methodology distinguishes the exactness probe from the training/held-out profile study.
- Evaluation reports budgets, regret, calibration, success, continuation, and gate
  consistency together.
- Limitations explicitly retain controlled-family, fixed-action, order-invariance, and
  state-interaction boundaries.
- Ambiguous phrases such as “independent gate” were replaced with
  “original-equation gate” or “separately evaluated gate,” preventing an erroneous
  interpretation as process or host isolation.
- Remaining risk: the manuscript is dense and must preserve the hierarchy of one core
  objective, one acceptance invariant, one joint policy, and bounded evidence.

## 8. Format and Venue Review

- **Template:** IEEEtran journal/compsoc.
- **Current length target:** 12 pages.
- **Venue fit:** parallel and heterogeneous numerical solver selection, complete-path
  performance, and scientific machine learning are compatible with TPDS.
- **Administrative boundary:** author, affiliation, funding, conflict, and disclosure
  fields remain submission metadata rather than scientific-score criteria.

## 9. Quantitative Scorecard

| Dimension | Score | Confidence | Evidence basis | Deduction | Repair condition |
| --- | --- | --- | --- | --- | --- |
| Quality | 5/5 | 5/5 | Coherent objective, recurrence, implementation, and bounded claims | None | Preserve scope |
| Clarity | 5/5 | 5/5 | Numerical meanings of gate and fallback are explicit | Dense presentation | Keep contribution hierarchy |
| Significance | 4/5 | 5/5 | Broad complete-cost solver evidence plus held-out joint policy | Controlled families | Public complex joint-policy workload |
| Originality | 5/5 | 4/5 | Verified typed fusion plus exact expert--budget policy | Closest external method not implemented | Fair published baseline |
| Soundness | 5/5 | 5/5 | Exact oracle, production traces, disjoint held-out scenarios | None within assumptions | Preserve tests |
| Evidence | 5/5 | 5/5 | 64 held-out requests, two families, regret/calibration/gate accounting | Breadth remains controlled | Larger realistic portfolio |
| Reproducibility | 4/5 | 5/5 | Machine-generated evidence and deterministic-bundle workflow | Public/third-party rerun absent | Public archival release |
| Limitations | 5/5 | 5/5 | Controlled scope and negative results retained | None | Keep qualifiers |

## 10. Multi-Reviewer Panel

### Reviewer A — Algorithms and Theory

- **Score tendency:** 9/10; **confidence:** 5/5.
- **Main positive signal:** exact joint subset/budget/order recurrence with bounded state.
- **Main negative signal:** exactness is conditional on order-invariant calibrated actions.
- **Score-change condition:** model state-dependent action interactions without losing an
  inspectable objective.

### Reviewer B — Numerical Methods

- **Score tendency:** 9/10; **confidence:** 5/5.
- **Main positive signal:** every returned result remains governed by the original
  residual/constraint/defect contract.
- **Main negative signal:** controlled quadratic/cubic SCCs do not represent all
  nonlinear stiffness and Jacobian pathologies.
- **Score-change condition:** add a realistic nonlinear or DAE joint-policy workload.

### Reviewer C — Parallel and Heterogeneous Systems

- **Score tendency:** 9/10; **confidence:** 5/5.
- **Main positive signal:** complete-path accounting prevents gate-only or kernel-only
  speedups from being advertised as solver speedups.
- **Main negative signal:** timing portability remains one-system bounded.
- **Score-change condition:** native cross-architecture complete-path comparison.

### Reviewer D — Scientific Machine Learning

- **Score tendency:** 8--9/10; **confidence:** 5/5.
- **Main positive signal:** learned candidates cannot decide acceptance, and correction
  budgets are selected by measured complete cost.
- **Main negative signal:** the action model is calibrated rather than learned jointly.
- **Score-change condition:** held-out feature-conditioned prediction of cost and pass
  probability with regret reporting.

### Reviewer E — Writing and Scope

- **Score tendency:** 9/10; **confidence:** 5/5.
- **Main positive signal:** the manuscript now excludes infrastructure interpretations
  and keeps gate/fallback as numerical terms.
- **Main negative signal:** evidence density may obscure the central algorithm.
- **Score-change condition:** preserve solver-only contribution ordering through final edit.

### Reviewer F — Reproducibility and Integrity

- **Score tendency:** 9/10; **confidence:** 5/5.
- **Main positive signal:** source, verifier, generated macros, claim ledger, and Runtime
  evidence are tied together.
- **Main negative signal:** no public immutable release or third-party rerun is claimed.
- **Score-change condition:** archive the final artifact and obtain an independent rerun;
  this improves reproducibility, not solver novelty.

### AC Synthesis

- **Agreement:** Round 33 closes the prior single-profile joint-policy evidence gap.
- **Disagreement:** reviewers differ on whether two controlled families justify an
  evidence score of 5/5; all agree they do not justify a 10/10 overall score.
- **Decisive accept axis:** exact production-path joint optimization plus held-out
  complete-cost/gate evidence.
- **Decisive limit:** no strong external published hybrid comparison and no realistic
  complex-workload joint-policy matrix.
- **Final calibrated stance:** **9/10 strong accept, confidence 5/5**.

## 11. Concern-to-Action Table

| ID | Severity | Concern | Solver-focused action | Score effect |
| --- | --- | --- | --- | --- |
| R33-1 | Major | No strong published hybrid baseline | Implement one closest compatible method under the same complete-cost/gate contract | Strongest route beyond 9 |
| R33-2 | Moderate | Controlled joint-policy families | Run frozen-profile/oracle evaluation on a public complex workload | Raises significance/evidence |
| R33-3 | Moderate | Statistics not jointly predicted | Learn/cross-validate cost and pass probability and report regret | Raises originality |
| R33-4 | Secondary | One-system timing | Repeat selected complete paths on another architecture | Raises performance portability |

## 12. Recommended Next Owner

- **Primary:** solver implementation/evaluation owner for a faithful published hybrid
  baseline and realistic joint-policy workload.
- **Secondary:** paper writer for density reduction after evidence is stable.
- **Not recommended:** distributed-systems, process-isolation, security-switching, or
  dedicated-host work; these do not repair any scientific concern in this review.

## 13. Score Revision Conditions

- **A 10/10 discussion becomes defensible only if:** a strong external hybrid baseline is
  evaluated fairly and the joint policy retains low regret on a materially more realistic
  solver workload or larger action portfolio.
- **Remain at 9/10 if:** the current exact optimizer and controlled held-out matrix remain
  the strongest joint-policy evidence.
- **Lower below 9/10 if:** the paper overclaims universal optimality, treats calibrated
  objective values as wall-clock speedups, hides fallback cost, or permits any successful
  return to bypass the original-equation gate.

## 14. Checks Run

- `reproduce-joint-route-budget-shift`: passed after the held-out grid was made disjoint.
- `paper/check_evidence.py`: passed with exact budget, regret, calibration, success, and
  gate-mismatch checks.
- `ctest --test-dir build/release --output-on-failure`: 29/29 passed.
- `paper/check.sh`: passed with a 12-page PDF and no unresolved reference or overfull-box
  failure.
- Deterministic bundle generation and clean extracted-tree verification passed, including
  the joint-route-budget-shift target, 29/29 CTests, evidence/manifest checks, and PDF
  rebuild; these release-closure checks do not alter the solver-focused score.

## 15. Unresolved or Unverified

- No strong external published hybrid implementation has been run.
- No realistic public workload yet exercises joint expert--budget selection with the
  same held-out oracle protocol.
- No joint feature model predicts action cost and acceptance probability.
- No cross-architecture timing result is claimed.
- No public archive or third-party reproduction is claimed.

## 16. Output Self-Check

- No result, citation, external comparison, archive, or performance number was invented.
- Every score deduction has an evidence basis and repair condition.
- Gate and fallback are used only as numerical solver terms.
- No non-solver infrastructure item is treated as a contribution, future-work mandate,
  or score gate.
