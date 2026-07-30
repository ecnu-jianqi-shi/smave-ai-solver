# CCF-A Full Review — Round 32

## 1. Report Metadata

- **Review date:** 2026-07-26
- **Target venue/year/track:** IEEE Transactions on Parallel and Distributed Systems, regular paper
- **Paper title:** *Complete-Cost Expert Fusion for Verified Repeated Numerical Solves*
- **Materials reviewed:** 12-page manuscript/PDF, routing implementation, exact joint
  expert--budget dynamic program, unit tests, production Runtime probe, exhaustive-oracle evidence,
  claim ledger, artifact snapshot, and Round 31 report
- **Reviewer mode:** full scientific, writing, format, and AC synthesis

## 2. Desk Assessment

- **Length:** pass; the manuscript remains 12 pages.
- **Venue fit:** pass; the paper studies parallel/heterogeneous numerical solver selection,
  correction, verification, and complete-path cost.
- **Reviewability:** pass; the new algorithm, recurrence, bounded state space, implementation, and
  machine evidence are inspectable.
- **Scope:** pass; only solver algorithms, numerical correctness, routing, and performance affect
  the scientific judgment.
- **Submission metadata:** incomplete author/disclosure fields remain an administrative issue.

## 3. Paper Summary

The paper formulates repeated numerical acceleration as a typed portfolio of candidate,
corrector, original-equation gate, and fallback stages. It charges every reached stage and terminal
fallback through a complete-cost objective. For a fixed eligible cascade, a pairwise exchange
argument gives the cost-per-acceptance order. Every successfully returned accelerated state must
pass the supplied equation's residual, constraint, or defect gate.

Round 32 adds a substantive algorithmic extension. Each calibrated action is an expert--correction-
budget pair $(e,b)$ with attempt cost and acceptance probability. After sorting actions by the
fixed-cascade index, an exact dynamic program selects the subset, one budget per expert, and top-$k$
order while recursively charging rejection continuation and terminal fallback. The production
Router enforces a finite state limit and retains the numerical gate outside the optimizer.

## 4. Calibrated Decision

- **Overall score:** **9/10, strong accept**.
- **Confidence:** **5/5**.
- **Score movement from Round 31:** scientific strength increases within the 9 band; the former
  fixed per-expert median-budget implementation concern is closed.
- **Why not 10/10:** the joint optimizer is validated on a controlled three-action/two-expert profile,
  not a held-out multi-family action-calibration campaign; action statistics are calibrated inputs
  rather than jointly learned; and no strong published hybrid learned-solver implementation is
  compared under the same complete-cost/gate contract.

## 5. Decisive Strengths

1. **Joint decision is now real.** Expert identity, budget, subset, and order are selected by one
   recurrence rather than by winner-first routing plus a fixed budget heuristic.
2. **Continuation cost is explicit.** The take branch charges $K_i+(1-p_i)V_{i+1}$, so a cheap but
   frequently rejected action can be useful only when its downstream cost remains favorable.
3. **Mutual exclusion is enforced.** The state remembers used experts, preventing the cascade from
   paying for multiple independently configured budgets of the same expert.
4. **Exactness is independently checked.** The selected cost is `4.5`, the exhaustive oracle is
   `4.5`, and the reported oracle gap is zero over every valid subset, budget assignment, and order.
5. **The plan changes actual Runtime behavior.** Production execution rejects
   `expert-B@budget1`, continues to `expert-A@budget2`, performs two second-stage corrections, and
   accepts only through the original-equation gate.
6. **Best-single comparison is transparent.** The calibrated joint objective costs `0.900×` the
   best single action; this is not presented as wall-clock workload speedup.
7. **Legacy compatibility remains.** Existing single-budget CompetitionReport profiles still use
   their previous path, while multi-budget profiles activate the exact optimizer.

## 6. Major Concerns

### C1. Joint-Policy Evidence Is Controlled Rather Than Distributional

- **Severity:** major, non-fatal.
- **Evidence:** the new probe contains three actions, two experts, one scalar nonlinear equation,
  fixed calibrated costs/probabilities, and one production request.
- **Deduction:** the implementation and recurrence are sound, but the evidence does not establish
  held-out regret, calibration stability, or budget changes across equation families.
- **Repair condition:** derive action profiles from training scenarios, freeze them, and report
  held-out joint-policy regret, acceptance, fallback, oracle agreement, and selected-budget changes
  across at least two equation families or one family under conditioning/topology shift.
- **Expected movement:** materially strengthens evidence and makes 10/10 more defensible.

### C2. No Strong External Published Hybrid Baseline

- **Severity:** major, non-fatal.
- **Evidence:** the shared learned-candidate/Jacobi control is reproducible but internally designed.
- **Deduction:** the paper establishes internal mechanism superiority, not head-to-head superiority
  over a faithful HINTS-, FCG-NO-, learned-preconditioner-, or related hybrid implementation.
- **Repair condition:** implement one compatible published method without changing its intended
  algorithm and evaluate candidate-inclusive cost, gate acceptance, fallback, failures, and accuracy.
- **Expected movement:** closes the strongest remaining comparative gap.

### C3. Action Statistics Are Inputs, Not Jointly Learned Predictions

- **Severity:** moderate.
- **Evidence:** the exact DP consumes finite calibrated action costs and acceptance probabilities.
- **Deduction:** optimization is joint, but estimation is not. The paper correctly avoids claiming
  end-to-end learning or optimality under state-dependent action interactions.
- **Repair condition:** learn or cross-validate $K_{e,b}(\phi)$ and $p_{e,b}(\phi)$ jointly, then
  compare held-out complete-cost regret with the per-expert median and fixed-budget policies.
- **Expected movement:** improves novelty and transfer evidence; not required for current bounded claim.

### C4. Transfer and Native Performance Breadth

- **Severity:** moderate.
- **Evidence:** routing covers size/fingerprint, conditioning, and topology shifts, but not equation-
  family, precision, or native hardware cost reordering; authoritative timing remains one Apple M4.
- **Deduction:** the numerical mechanism is credible, while performance generalization remains bounded.
- **Repair condition:** add one equation-family/precision transfer and one different native timing
  platform using the same complete-cost report.
- **Expected movement:** strengthens significance and portability.

## 7. Soundness Review

- **Recurrence correctness:** for globally index-sorted actions, skip/take recursion enumerates every
  admissible subsequence while the used-expert state enforces one budget per expert.
- **Terminal condition:** an exhausted action list or top-$k$ capacity returns calibrated terminal
  fallback cost.
- **Bounded computation:** profiles with invalid counts, negative budgets, duplicate expert-budget
  actions, more than 63 experts, or excessive DP states are rejected.
- **Risk treatment:** calibrated attempt cost receives the existing risk/OOD penalty before
  optimization; risk does not bypass complete-cost comparison.
- **Numerical safety:** routing rejection continues to another numerical action; acceptance still
  requires the independently evaluated original equation.
- **Limits stated:** action statistics must be order-invariant for the recurrence, and the method does
  not model expert-induced state interactions.

## 8. Evidence Review

- **Unit test:** exact joint selection, continuation benefit, same-expert mutual exclusion, duplicate
  action rejection, state-limit rejection, and inconsistent-count rejection pass.
- **Independent oracle:** all valid subsets, budgets, and orders are enumerated separately from the DP.
- **Production path:** `RuntimeRouter -> Runtime -> Newton correction -> original-equation gate` is
  executed; this is not an offline projection.
- **Controlled result:** selected plan is `B@1 -> A@2`; expected cost is `4.5`, best single action is
  `5.0`, and ratio is `0.900`.
- **Failure semantics:** the first action is retained as a visible rejection, the second is accepted,
  and terminal fallback is not used.
- **Existing evidence retained:** exhaustive fixed-cascade ordering, budget frontier, cross-shift
  routing, paired timing, shared controls, negative transfer, and complete-path scaling remain intact.

## 9. Writing and Presentation

- **Method visibility:** the joint recurrence appears in the formulation and is referenced from the
  Router design.
- **Evidence visibility:** methodology describes the independent oracle; ablation reports the selected
  sequence and explicitly distinguishes calibrated objective improvement from wall-clock speedup.
- **Scope discipline:** SDK, data-lock, and portability detail was compressed to preserve the
  algorithmic narrative and 12-page limit.
- **Terminology:** `gate` remains a numerical acceptance test; `fallback` remains numerical
  continuation after rejection.
- **Remaining clarity risk:** the phrase “exact optimizer” must always retain “finite calibrated action
  set” and “order-invariant statistics” nearby.

## 10. Multi-Reviewer Panel

### Reviewer A — Numerical Algorithms

- **Score:** 9/10.
- **Confidence:** 5/5.
- **Positive:** the recurrence closes a genuine method gap and is exactly oracle-checked.
- **Concern:** one controlled equation does not show calibration behavior across numerical families.
- **Raise condition:** held-out multi-family action-profile evaluation.

### Reviewer B — Parallel Solver Systems

- **Score:** 9/10.
- **Confidence:** 5/5.
- **Positive:** continuation and terminal cost are part of the production decision rather than a
  post-hoc timing calculation.
- **Concern:** cost ordering is not yet demonstrated across native architectures.
- **Raise condition:** second-platform complete-path timing and route changes.

### Reviewer C — Scientific Machine Learning

- **Score:** 8/10.
- **Confidence:** 4/5.
- **Positive:** expert and correction effort are no longer artificially separated.
- **Concern:** costs/probabilities are calibrated inputs and the external hybrid comparison is absent.
- **Raise condition:** learned held-out action model plus one faithful published baseline.

### Reviewer D — Experimental Design

- **Score:** 9/10.
- **Confidence:** 5/5.
- **Positive:** independent exhaustive enumeration and visible rejection chain make the mechanism
  difficult to fake.
- **Concern:** three actions are sufficient for correctness, not for statistical generalization.
- **Raise condition:** repeated held-out regret and calibration matrices.

### Reviewer E — Writing and Scope

- **Score:** 9/10.
- **Confidence:** 5/5.
- **Positive:** the new core result displaced non-core implementation detail instead of causing scope
  expansion.
- **Concern:** the paper remains dense.
- **Raise condition:** preserve the current contribution hierarchy.

### Reviewer F — Reproducibility and Integrity

- **Score:** 9/10.
- **Confidence:** 5/5.
- **Positive:** recurrence, tests, oracle, Runtime trace, claim ledger, and generated values align.
- **Concern:** final clean-bundle verification is pending after Round 32 edits.
- **Raise condition:** rerun the deterministic bundle and extracted-tree checks.

### AC Synthesis

- **Agreement:** Round 32 adds real solver innovation and closes the fixed-budget implementation gap.
- **Disagreement:** reviewers differ on whether a controlled exactness probe is enough to raise the
  overall score or only strengthen confidence within 9.
- **Decisive accept axis:** complete-cost verified fusion now includes exact finite-action joint
  expert--budget selection on the production path.
- **Decisive limit:** no external hybrid head-to-head and no held-out distributional joint-policy
  evaluation.
- **Final stance:** **9/10 strong accept, confidence 5/5**.

## 11. Quantitative Scorecard

| Dimension | Score | Confidence | Evidence basis | Deduction | Repair condition |
| --- | --- | --- | --- | --- | --- |
| Quality | 5/5 | 5/5 | Coherent objective, recurrence, implementation | None | Preserve bounds |
| Clarity | 5/5 | 5/5 | Method and numerical semantics explicit | None | Keep exactness qualifiers |
| Significance | 4/5 | 5/5 | Broad solver evidence and new joint policy | Controlled joint-policy scope | Held-out family evaluation |
| Originality | 5/5 | 4/5 | Typed verified portfolio plus expert-budget DP | No exhaustive novelty proof against all portfolio variants | Stronger comparison/positioning |
| Soundness | 5/5 | 5/5 | DP/oracle equality and production Runtime path | None within assumptions | Preserve validation |
| Evidence | 4/5 | 5/5 | Strong internal machine evidence | Joint result has one controlled profile | Multi-family held-out matrix |
| Reproducibility | 4/5 | 5/5 | Deterministic local artifact design | Round 32 clean bundle pending | Complete final rerun |
| Limitations | 5/5 | 5/5 | Explicit finite-action and transfer boundaries | None | Keep wording |

## 12. Concern-to-Action Table

| ID | Severity | Concern | Action | Score effect |
| --- | --- | --- | --- | --- |
| R32-1 | Major | Controlled joint-policy evidence | Train/freeze action profiles and evaluate held-out regret across families/shifts | Strongest route toward 10 |
| R32-2 | Major | No published hybrid implementation | Add one faithful external baseline under the same gate/cost contract | Strongest route toward 10 |
| R32-3 | Moderate | Statistics not jointly learned | Predict $K_{e,b}$ and $p_{e,b}$ jointly and cross-validate | Raises originality/evidence |
| R32-4 | Moderate | One-host performance | Repeat complete-path timing on another native architecture | Raises portability |

## 13. Recommended Next Work

1. **P0:** build a held-out joint-action calibration matrix using real production traces rather than
   fixed probe probabilities.
2. **P0:** implement one closest compatible published hybrid solver/preconditioner baseline.
3. **P1:** evaluate joint-policy regret under conditioning/topology and one equation-family shift.
4. **P1:** rerun the full artifact manifest, 29 tests, PDF, deterministic bundle, and clean extraction.

## 14. Score Revision Criteria

- **Raise to 10 becomes defensible when:** the joint policy shows low held-out regret across materially
  different solver families or shifts and one strong external hybrid baseline is evaluated fairly.
- **Remain at 9:** exact controlled optimizer evidence without broader action-model generalization.
- **Lower below 9:** overclaim global optimality beyond finite order-invariant action sets, report the
  `0.900×` calibrated objective as workload speedup, or allow any accepted result to bypass the gate.

## 15. Checks and Self-Audit

- **Passed before report:** joint target, C++ unit suite, evidence checker, manifest generation, and
  12-page LaTeX build without overfull boxes.
- **Pending after report creation:** final manifest refresh, 29/29 CTests, deterministic bundle, and
  clean extracted-tree verification.
- **No invented evidence:** no external performance, learned transfer, public archive, independent
  reproduction, or acceptance probability is claimed.
- **Scope preserved:** every recommended scientific action concerns solver algorithms, calibration,
  numerical correctness, or performance.
