# CCF-A Full Review — Round 37

## 1. Report Metadata

- **Review date:** 2026-07-26.
- **Mode:** full scientific, writing, format, artifact, and AC synthesis review.
- **Target:** IEEE Transactions on Parallel and Distributed Systems regular paper.
- **Paper:** *Complete-Cost Expert Fusion for Verified Repeated Numerical Solves*.
- **Scope authority:** numerical solver mechanisms only; deployment and operations
  architecture is excluded from contributions and scoring.
- **Materials:** current 12-page manuscript; production sparse-linear Router and solve
  service; sparse ILUT/ILU(0), PCG, GMRES, direct-solver implementations; request-
  conditioned model and 5,760 frozen action observations; 29-test Release build;
  equation-assessment v2; evidence and artifact manifests.
- **Search basis:** Round 35--36 primary-source search remains current on the same review
  date. No new novelty claim or newly executed external method was added in Round 37.

## 2. Paper Summary

The paper formulates heterogeneous repeated numerical solving as typed expert-cascade
selection under reach-weighted complete verified cost. A plan contains candidate,
optional correction, original-equation numerical acceptance, and continuation to another
solver path. Static or request-conditioned action cost/pass estimates feed an exact
bounded dynamic program. Returned results are accepted only after the original residual,
constraint, consistency condition, or discretization defect is recomputed.

Round 37 strengthens the production implementation behind this formulation. The public
linear solve service now executes the Router-selected `(expert, work budget)` action for
PCG-IC(0), PCG-Jacobi, GMRES-ILUT, GMRES-ILU(0), and compatible direct solvers. CSR ILUT
uses bounded sparse row fill rather than a dense factor. Every attempt records planned
and executed iterations, wall time, status, and original-matrix residual. Plan identity
includes work budget. A terminal numerical cascade remains executable even if Top-k or
the learned model omits every robust action, and CLI/C API iteration contracts agree.

## 3. Decision And Scorecard

- **Likely stance:** **9/10, strong accept**.
- **Confidence:** **5/5**.
- **Score movement from Round 36:** none. The implementation gap narrowed, but the main
  external-validity experiment required for 10/10 has not yet been executed.

| Dimension | Score | Evidence basis | Deduction / boundary |
|---|---:|---|---|
| Novelty | 5/5 | Complete-cost typed cascades, request-conditioned expert--budget prediction, exact bounded routing, original-equation acceptance | No deduction within the bounded formulation |
| Soundness | 5/5 | Exact-DP/exhaustive agreement, production gate checks, budget validation, terminal numerical fallback, 29/29 CTests | Exactness still assumes order-invariant action statistics and no stateful expert interaction |
| Evidence | 5/5 | PDEBench-derived workloads, SuiteSparse breadth, HINTS execution, 5,760 action observations, negative results, frozen artifacts | Request-conditioned policy evidence remains synthetic rather than a realistic public solver portfolio |
| Significance | 4/5 | Production Router now controls real Krylov/direct actions and budgets | Decision-level benefit across materially different public solvers is not yet measured |
| Reproducibility | 5/5 | Evidence checks, manifest checks, 12-page rebuild, equation-assessment v2, 29/29 CTests | Full public portfolio data remain external to the core archive |
| Clarity | 5/5 | Solver-only scope, numerical definitions of gate/fallback, bounded claims and retained negative results | No decision-level deduction |

## 4. Round 37 Advances

1. `verified_linear_solve` executes the planned Krylov budget rather than only routing
   to AMG/direct backends.
2. PCG-IC(0), PCG-Jacobi, GMRES-ILUT, and GMRES-ILU(0) are production actions with
   original-matrix gate checks after every candidate.
3. ILUT accepts CSR input and retains bounded fill per lower/upper row; the previous
   sparse-input construction failure is closed.
4. Attempt traces expose backend, requested work, executed iterations, wall time,
   status, and residual, enabling real action-level model fitting.
5. Iterative model budgets must be positive and within the caller's maximum; direct
   actions must use budget zero.
6. Work budget enters `plan_id`, and generic equation-assessment serialization is now
   `SMAVE_EQUATION_ASSESSMENT 2` with `WORK_ITERATIONS`.
7. Top-k exhaustion invokes `terminal-numerical-linear-cascade-v1`; cancellation stops
   continuation, and optional caller fallback remains last.
8. Forced PCG, GMRES-ILU(0), sparse ILUT, low-budget rejection/full-budget continuation,
   invalid feature/budget, cancellation, and CLI/C API consistency tests pass.

These are substantive solver mechanisms. They are not evidence that the learned policy
beats controls on a realistic public portfolio.

## 5. Major Concerns

### C1 — Realistic Public Portfolio Is Still Missing

- **Severity:** major; decisive barrier to 10/10.
- **Evidence:** the request-conditioned study still uses three controlled 12-variable
  nonlinear families and synthetic experts, despite the production service now exposing
  materially different SuiteSparse-compatible actions.
- **Deduction:** the paper proves the optimizer and model plumbing, but not that request
  features predict useful expert--budget decisions across real matrix structures,
  preconditioner setup costs, Krylov convergence, direct-solver crossover, and failures.
- **Repair condition:** execute a matrix-ID-disjoint SuiteSparse train/calibration/held-
  out experiment with real production attempts, exhaustive action measurements, static
  and fixed controls, exact-DP/exhaustive comparison, production attempt traces, and the
  unchanged original-matrix gate.
- **Expected movement:** necessary component for movement from 9/10 to 10/10.

### C2 — Cost-Prediction Tails Remain Weak

- **Severity:** moderate.
- **Evidence:** median relative error `0.225`, p95 `1.352`, maximum `2.515`; conditioned
  regret remains `1.178×` versus `1.451×` controls on the controlled portfolio.
- **Deduction:** current policy quality survives the observed errors, but the result does
  not establish robustness under harder action cost distributions.
- **Repair condition:** report tail-aware route regret on the public portfolio and compare
  the current mean log-cost model with a conservative uncertainty or quantile penalty.
- **Expected movement:** supports 10/10 when paired with C1; not sufficient alone.

### C3 — Action Interaction Boundary Remains Untested

- **Severity:** moderate.
- **Evidence:** the exact policy treats action cost and pass probability as order-
  invariant and restricts one budget per expert.
- **Deduction:** shared setup, warm caches, correlated failures, or candidate reuse could
  change the optimal order in a real portfolio.
- **Repair condition:** measure repeated-action interaction deltas on the public portfolio;
  either show they are negligible relative to policy gains or introduce a bounded state
  feature/cost correction.
- **Expected movement:** closes the strongest soundness caveat if public gains are small.

### C4 — Closest Learned Hybrid Breadth Is Narrow

- **Severity:** minor-to-moderate.
- **Evidence:** native HINTS is faithful but limited to a 29-unknown 1D Poisson setting;
  Greedy PDE Router is positioned but not executed.
- **Deduction:** this limits breadth, not the validity of the current solver contribution.
- **Repair condition:** optional after C1--C3; execute a harder faithful hybrid artifact
  only if public code and a compatible workload are available.

## 6. Writing And Presentation Review

- The manuscript now consistently defines `gate` as original-problem numerical
  acceptance and `fallback` as continuation to another solver path.
- Scope text explicitly excludes infrastructure availability/isolation work from the
  contribution and score surface.
- The current paper should not yet describe the new production linear portfolio as an
  evaluated contribution; it lacks frozen public-policy results.
- When C1 is completed, distinguish mechanism evidence from policy-benefit evidence and
  report matrix-ID-disjoint splits prominently to preclude leakage concerns.
- The 12-page build has no undefined references or overfull boxes. Existing underfull
  warnings are cosmetic and not decision-relevant.

## 7. Multi-Reviewer Panel

### Reviewer R1 — Numerical Algorithms

- **Score tendency:** 9/10.
- **Confidence:** 5/5.
- **Positive:** real PCG/GMRES/direct budgets now execute under the same original-matrix
  acceptance rule, and CSR ILUT is a genuine sparse implementation.
- **Negative:** no held-out public action-crossover result yet.
- **Score-change condition:** matrix-disjoint SuiteSparse policy evaluation with oracle
  regret and retained failure cases.

### Reviewer R2 — Parallel Solver Systems

- **Score tendency:** 9/10.
- **Confidence:** 5/5.
- **Positive:** complete-path timing, attempt traces, solver-internal parallel evidence,
  and heterogeneous placement are inspectable.
- **Negative:** public portfolio setup/solve/gate crossovers and interaction effects are
  not measured.
- **Score-change condition:** demonstrate decision-level benefit under real sparse solver
  diversity; distributed availability is explicitly irrelevant.

### Reviewer R3 — Scientific Machine Learning

- **Score tendency:** 8--9/10.
- **Confidence:** 4/5.
- **Positive:** learned routing cannot override the numerical gate; disjoint calibration
  and held-out regret are reported.
- **Negative:** action-cost prediction tails are large and the current action study is
  synthetic.
- **Score-change condition:** public action observations with tail-aware uncertainty and
  feature ablations.

### Reviewer R4 — Reproducibility And Artifacts

- **Score tendency:** 9/10.
- **Confidence:** 5/5.
- **Positive:** 29/29 CTests, evidence/manifest checks, deterministic model/observation
  files, and a 12-page rebuild pass.
- **Negative:** a future full SuiteSparse experiment may not fit the core archive.
- **Score-change condition:** freeze evidence/model/split/traces and include a small
  mechanism fixture plus exact public data locks.

### AC Synthesis

- **Agreement:** the paper is a strong solver paper; Round 37 closes real production
  execution gaps without changing the central numerical contract.
- **Disagreement:** R3 is more cautious because the request-conditioned evidence remains
  synthetic and cost tails are weak.
- **Decisive accept axis:** complete-cost solver composition with mandatory original-
  problem acceptance and unusually strong artifact coverage.
- **Decisive barrier to 10/10:** absence of realistic public decision-level portfolio
  evidence, not any infrastructure deployment mechanism.
- **Final stance:** **9/10, strong accept, confidence 5/5**.

## 8. Concern-To-Action Table

| ID | Priority | Required action | Evidence required | Score role |
|---|---:|---|---|---|
| C1 | P0 | Run SuiteSparse request-conditioned real solver portfolio | Matrix-disjoint split, exhaustive attempts, held-out regret, production traces, gate checks | Necessary for 10 |
| C2 | P1 | Add tail-aware cost uncertainty/control | p95/max errors and worst-case route regret against current model | Supports 10 |
| C3 | P1 | Measure action interactions/shared setup | Pair/order delta table or bounded state correction | Supports soundness at 10 |
| C4 | P2 | Add harder faithful learned hybrid | Executed public artifact on compatible workload | Optional breadth |

## 9. Checks Run

- `python3 paper/check_evidence.py` — pass.
- `python3 paper/check_artifact_manifest.py` — pass.
- Release CTest suite — 29/29 pass.
- `reproduce-equation-assessment` — pass with generic schema v2.
- `reproduce-request-conditioned-joint-route` — pass.
- Paper build — 12 pages, no undefined references, no overfull boxes.
- Current PDF SHA-256:
  `71e80d5900eff5a70a519d8426d3e80b731a0414eb1056a0f459860314ef0b4d`.

## 10. Unresolved Or Unverified

- No public SuiteSparse request-conditioned portfolio result exists yet.
- No tail-aware or interaction-aware public route comparison exists yet.
- No new external learned Router was executed in this round.
- No claim is made about universal cross-platform performance or independent reruns.

## 11. Recommended Next Owner

**Solver experiment implementation.** Build and execute a public SuiteSparse
request-conditioned multi-expert, multi-budget study using the production linear service.
Keep the next iteration limited to solver mechanisms, numerical evidence, and
decision-level analysis.

## 12. Output Self-Check

- Scores are separated from confidence and grounded in inspectable evidence.
- No result, citation, performance gain, or external reproduction is invented.
- Every 10/10 condition is a solver-method or solver-evidence condition.
- The review preserves negative results and the current 9/10 stance.
