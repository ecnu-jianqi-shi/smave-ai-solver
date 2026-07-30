# CCF-A Full Review — Round 9

## Mode

Full scientific, writing, artifact, integrity, portability, and TPDS-fit re-review
after adding a shared learned-candidate/correction/gate/fallback control across the
two primary held-out operator families.

## Venue and Assumptions

- Target: IEEE Transactions on Parallel and Distributed Systems regular paper.
- Review date: 2026-07-24.
- Materials: current 12-page PDF and LaTeX sources; pinned repeated-PDE and
  order-sensitivity report trees; shared-control raw reports and aggregate evidence;
  routing, gate, complete-path, and batch-scaling summaries; source, tests, artifact
  manifest tooling, and Rounds 1--8.
- Submission assumptions: the manuscript remains at the 12-page boundary. Author,
  affiliation, funding, and acknowledgment metadata are still placeholders and must
  be resolved before submission.
- Overall scale: CCFA 1--10; criterion scale: 1--5.

## Paper Summary

The paper frames repeated numerical acceleration as verification-aware expert
selection. Its central object is a candidate--correction--gate--fallback transaction
whose router minimizes reach-weighted complete verified cost. A bounded proposition
establishes caller-visible commit authority under explicit isolation, immutable-
problem, atomic-publication, and fallback assumptions. The implementation combines
classical solvers, learned candidates, device kernels, typed equation families,
parallel original-equation gates, an SDK, and failure-aware routing.

The evaluation retains the prior seven-family PDE timing and order-control studies,
two held-out operator families, two-family gate studies, Navier--Stokes complete-path
and batch scaling, routing comparisons, device probes, ablations, portability checks,
and negative results. Round 9 adds one transparent control shared by the linear and
nonlinear operator families: the same fitted FP64 latent candidate, 16 training and
64 held-out scenarios, at most 32 weighted diagonal-residual Jacobi steps, an
original-equation gate, an independent Runtime commit gate for accepted corrections,
and mandatory original-solver fallback for rejection. Candidate construction,
correction, both gates, trace persistence, and fallback are timed over 100
repetitions of 64 requests per family.

The shared control retains all 12,800 outcomes. The linear case accepts 6,400 of
6,400 requests but is slower than classical at 0.945x; the nonlinear case accepts
none, executes 6,400 fallbacks, and reaches 0.568x. Both have zero failures and gate
mismatches. The verified operators are 1.198x [1.152, 1.237] and 2.489x [2.353,
2.631] faster than the corresponding controls. The paper correctly calls this a
common algorithmic control rather than an external published implementation.

## Likely Stance and Calibrated Score

**Overall: 8/10 — accept. Scholarly confidence: 5/5.**

Round 9 closes the prior P1 concern that the primary linear and nonlinear learned
operator results lacked a common complete-cost comparison protocol. The new control
uses the same split, precision, candidate, correction budget, gate semantics,
fallback rule, trace accounting, repetition count, and paired analysis across both
families. It includes a positive acceptance case and a full-fallback negative case,
so it does not obtain significance by filtering unsuccessful outcomes. The
independent Runtime commit gate also prevents the harness from self-certifying an
accepted corrected candidate.

The score remains 8 rather than rising to 9 because the dominant limitations are
external and unchanged. All authoritative performance still comes from one Apple M4
host. The Linux/ARM64 and emulated Linux/x86-64 runs establish correctness and API
portability only. There is no native Linux/x86-64 performance study, no complete-cost
result on a discrete accelerator, no NUMA or multi-node authority-scaling result, no
immutable public archive, and no independent rerun. The shared control covers two
operator families rather than all seven PDE workloads and is not a published
third-party learned baseline. Those facts limit external validity but do not negate
the paper's bounded contribution.

## Quantitative Scorecard

| Dimension | Score (1--5) | Confidence (1--5) | Evidence basis | Deduction / score-change condition |
|:---|:---:|:---:|:---|:---|
| Novelty | 4 | 5 | `paper/sections/01_introduction.tex:24`; `paper/sections/05_verification_aware_fusion.tex:21` | The complete-cost transaction, role constraints, and commit-authority proposition remain differentiated. A 5 requires broader composition or distributed-authority novelty beyond the bounded single-process protocol. |
| Soundness | 4 | 5 | `paper/sections/06_experimental_methodology.tex:84`; `include/smave/runtime.hpp:186`; `src/runtime.cpp:1151`; `tests/test_smave.cpp:3514` | The shared control now has an independently gated Runtime commit path, strict fallback, and explicit failure retention. Arbitrary callback error, faulty hardware, and distributed commit remain outside the guarantee. |
| Evidence | 4 | 5 | `paper/sections/07_evaluation.tex:132`; `build/release/operator-shared-baseline/evidence.txt`; 12,800 shared-control outcomes | The common two-family protocol closes the previous local comparator gap, and both paired lower bounds exceed one. A 5 still requires native cross-platform performance, broader external-hardware evidence, and independent reproduction. |
| Significance | 4 | 4 | `paper/abstract.tex:16`; `paper/sections/09_discussion_limitations.tex:78` | The work is relevant to parallel verification and heterogeneous numerical runtimes, but NUMA, multi-node, discrete-accelerator, and external deployment impact remain unmeasured. |
| Clarity | 5 | 5 | `paper/abstract.tex:26`; `paper/sections/07_evaluation.tex:146`; `paper/sections/08_ablation_analysis.tex:75`; generated operator/router/gate macros | The manuscript distinguishes verified operators, shared controls, classical baselines, external implementations, and negative transfer. Generated values now prevent router, gate, and operator timing drift. |
| Reproducibility | 4 | 5 | `paper/ARTIFACT_SNAPSHOT.md`; `paper/check_artifact_manifest.py`; `benchmark/run_linux_portability_checks.sh` | Local reconstruction is unusually strong and now includes the shared-control harness and raw reports. A 5 requires committed history, a persistent public identifier, release packaging, and an independent rerun. |
| Ethics / Limitations | 5 | 5 | `paper/sections/09_discussion_limitations.tex:22`; `paper/CLAIM_EVIDENCE.md`; shared-control negative result | Universal speedup, transfer, device, theorem, portability, and baseline limits are explicit. The full nonlinear fallback and unstable external-comparator outcomes are retained rather than hidden. |

**Overall:** 8/10 | **Scholarly Confidence:** 5/5

**Recommendation:** accept.

**Verdict:** native Linux/x86-64 paired workload measurements plus an immutable
public artifact and independent rerun are the clearest conditions for 9. A
complete-cost discrete-accelerator or distributed authority-scaling result would
further strengthen the TPDS contribution. Wording changes alone should not move the
score.

## Score-Change Conditions

| Change | Condition | Likely affected dimensions | Expected movement |
| --- | --- | --- | --- |
| Raise score | Repeat representative paired complete-cost workloads on native Linux/x86-64 and one discrete accelerator, retaining failures and confidence intervals | Evidence, Significance, Reproducibility | +1 overall is defensible if conclusions persist |
| Raise score | Publish an immutable source/data/report archive and obtain an independent rerun | Reproducibility, Evidence | +0.5 to +1 overall depending on rerun scope |
| Raise dimension only | Add a published third-party learned/hybrid implementation under the same complete-cost contract or extend the shared control across the seven PDE families | Evidence | Evidence may reach 5; overall movement is not automatic |
| Lower score | Native or independent runs reverse the representative complete-cost conclusions or reveal gate/fallback mismatches | Soundness, Evidence, Reproducibility | -1 to -3 overall depending on severity |
| No quick change | Add more prose, local timing repetitions, or another same-host microbenchmark without new external validity | None decisive | No overall movement |

## Top Strengths

1. **Shared comparison protocol is now real rather than rhetorical.** The two
   primary operator families use the same candidate, data split, correction budget,
   gate semantics, fallback, timing boundary, repetition count, and paired analysis.

2. **The control preserves inconvenient outcomes.** The linear control accepts all
   requests but regresses; the nonlinear control rejects every candidate and pays
   full fallback cost. Zero failures and gate mismatches make those negative results
   interpretable rather than silently censored.

3. **Accepted corrections are independently committed.** The public
   `Runtime::commit_corrected_candidate` path validates block, context, candidate
   shape, and finiteness; evaluates the original-equation gate independently; writes
   the standard trace; and invokes the original solver for invalid or rejected
   candidates.

4. **Primary paired intervals remain decisive.** The verified linear and nonlinear
   operators beat the common controls with lower bounds 1.152 and 2.353,
   respectively, over 6,400 paired requests per family.

5. **The evidence package is unusually failure-aware.** Seven-family PDE timing,
   seven-family solver-order controls, routing regret, device rejection, failed
   topology transfer, gate scaling, complete-path scaling, and fallback costs are
   all reported under bounded claims.

6. **Evidence synchronization improved.** Router, fused-gate, operator, and shared-
   control values are generated from direct authorities and reconstructed by
   `paper/check_evidence.py`, reducing manuscript drift after reproduction runs.

7. **Portability claims remain honest.** Clean Ubuntu ARM64 and emulated Ubuntu
   x86-64 builds pass the complete test suite, while the paper and snapshot explicitly
   deny that these runs establish native performance or independent reproduction.

## Major and Fatal Concerns

No fatal technical or policy concern is visible in the reviewed local materials.
The remaining concerns are decision-relevant external-validity and submission-
readiness limits.

### P0 — Native Performance Portability

- Evidence basis: all reported speedups are measured on one Apple M4 host;
  `paper/sections/09_discussion_limitations.tex` states the boundary.
- Concern: TPDS readers cannot yet determine whether the complete-cost hierarchy
  survives native x86-64 server memory systems, NUMA effects, different sparse
  libraries, or a discrete accelerator.
- Required action: rerun representative PDE, operator, gate, and complete-path
  workloads on native Linux/x86-64 and at least one discrete accelerator. Preserve
  the same paired, failure-inclusive contract.
- Score condition: the main evidence path from 8 to 9.

### P0 — Public and Independent Reproduction

- Evidence basis: the local manifest hashes source, harness, summaries, and 651
  pinned PDE/order reports, but the repository has no committed history, release tag,
  persistent identifier, or external rerun.
- Concern: local reproducibility is strong, yet archival and independent
  reproducibility remain unverified.
- Required action: publish an immutable archive with source revision, licenses,
  dataset acquisition, report trees, environment information, and manifest; obtain
  an independent rerun.
- Score condition: required for a robust 9 and likely necessary for 10.

### P1 — Submission Metadata and Boundary

- Evidence basis: `paper/authors.tex` and
  `paper/sections/11_acknowledgments.tex` remain placeholders.
- Concern: the scientific draft is reviewable, but the submission package is not
  ready.
- Required action: finalize author identities, affiliations, correspondence,
  funding, acknowledgments, conflicts, and review mode; scrub or retain artifact
  identity consistently with that mode.
- Score condition: readiness rather than scientific-score movement.

### Closed — Shared Learned/Hybrid Comparison Protocol

- Prior concern: the two primary learned operator families lacked one common
  complete-cost baseline.
- Closure evidence: `build/release/operator-shared-baseline/evidence.txt`, both raw
  `operator-shared-hybrid-baseline.txt` reports, the public corrected-candidate commit
  API, and focused Runtime tests.
- Residual boundary: the control is transparent and useful, but it is not an external
  published implementation and does not span the seven PDE families.
- Score effect: closes the Round 8 P1 deduction; does not override the external P0
  limitations.

## Writing and Presentation Concerns

1. The shared-control paragraphs in methodology, evaluation, and ablation are
   concise and appropriately scoped. They explicitly deny that the control is a
   published external method.
2. The seven-bar fusion figure makes the below-one control regressions visible rather
   than truncating the axis at one.
3. Generated macros now cover the router, fused gates, operator paths, and shared
   controls. This is preferable to manually copied timing values.
4. The PDF remains 12 pages. The log contains underfull boxes, especially in compact
   tables and long compound terms, but no overfull-box or compilation failure was
   observed. This is polish debt, not a readability blocker.
5. The conclusion still correctly calls for stronger external baselines. It should
   not be changed to imply that the new shared control is an external published
   baseline.

## Format and Venue Concerns

- The IEEE Computer Society journal format compiles successfully to 12 pages.
- Figures and tables remain legible in the generated PDF, and the updated fusion
  plot exposes regressions below one.
- Author and acknowledgment placeholders prevent final submission readiness.
- The artifact is locally inspectable but not yet packaged as a public TPDS artifact.
- The systems contribution remains strongest as a verification-aware parallel
  runtime paper; native/distributed evidence would improve TPDS fit more than adding
  another same-host algorithmic result.

## Multi-Reviewer Panel

### Reviewer R1 — Numerical Soundness

- Score tendency: 8/10; confidence 5/5.
- Positive signal: the shared control preserves original-equation authority,
  independent commit gating, same-accuracy behavior, and mandatory fallback.
- Negative signal: callback correctness and hardware correctness remain assumptions,
  and the primary timings remain single-host.
- Evidence basis: methodology shared-control contract, Runtime commit path, focused
  unit tests, and zero failure/gate-mismatch evidence.
- Score-change condition: independent native reruns with unchanged gate decisions.

### Reviewer R2 — Parallel and Distributed Systems

- Score tendency: 7/10 to 8/10; confidence 5/5.
- Positive signal: the paper separates parallel gate scaling from complete-path
  scaling and treats verification as an optimizable systems component.
- Negative signal: no NUMA, multi-node, distributed commit, server-class x86-64, or
  discrete-accelerator complete-cost authority exists.
- Evidence basis: gate and Navier--Stokes scaling sections, artifact snapshot, and
  stated limitations.
- Score-change condition: native heterogeneous or distributed scaling under the same
  failure-inclusive contract.

### Reviewer R3 — Scientific Machine Learning

- Score tendency: 8/10; confidence 5/5.
- Positive signal: the shared two-family protocol now distinguishes the verified
  method from a generic learned-candidate/Jacobi wrapper and includes both positive
  acceptance and complete fallback outcomes.
- Negative signal: the comparator is transparent but local, not a published
  third-party implementation, and it does not cover all seven PDE families.
- Evidence basis: 12,800 shared-control outcomes and direct paired intervals.
- Score-change condition: a published external learned/hybrid implementation or
  broader family coverage under the same timing and gate contract.

### Reviewer R4 — Artifact and Reproducibility

- Score tendency: 8/10; confidence 5/5.
- Positive signal: direct-source macro generation, independent evidence
  reconstruction, manifest coverage, raw reports, and three passing platform suites.
- Negative signal: no source history, release tag, persistent archive, or independent
  host rerun.
- Evidence basis: artifact snapshot, manifest checker, portability evidence, and
  paper checks.
- Score-change condition: frozen public archive plus an external rerun.

## Panel Synthesis and AC Meta-Review

- Agreement: all reviewers find the shared-control addition substantive, correctly
  scoped, and scientifically honest. The prior local comparator concern is closed.
- Disagreement: the numerical and SciML reviewers lean clear accept; the systems and
  artifact reviewers remain more conservative because the decisive remaining gaps
  are native/distributed performance and independent reproduction.
- Decisive accept axis: verification-aware complete-cost composition backed by
  failure-inclusive paired evidence, explicit negative outcomes, and independent
  commit authority.
- Decisive reject axis: a reviewer requiring native server/discrete-accelerator
  performance or public independent reproduction could still downgrade the paper.
- Unresolved evidence: native x86-64 performance, discrete accelerator, NUMA,
  multi-node scaling, public archive, independent rerun, and final metadata.
- Final calibrated stance: 8/10 accept. Round 9 closes a real P1 concern but does not
  cross the external-evidence boundary required for 9.

## Concern-to-Action Table

| Priority | Concern | Evidence basis | Concrete action | Owner | Score condition |
| --- | --- | --- | --- | --- | --- |
| P0 | Native performance portability | Single Apple M4 timing host | Repeat representative paired workloads on native Linux/x86-64 and a discrete accelerator | Authors / external hardware owner | Main path to 9 |
| P0 | Public independent reproduction | Local manifest and report hashes only | Publish an immutable archive and obtain an independent rerun | Authors / artifact owner | Required for robust 9--10 |
| P1 | Distributed systems breadth | Intra-node gates and batches only | Add NUMA or multi-node authority/commit scaling with complete-cost accounting | Systems experiment owner | Significance +1; possible overall movement |
| P1 | Submission metadata | Placeholder author and acknowledgment files | Finalize identities, funding, acknowledgments, and review mode | Authors | Readiness only |
| Closed | Shared learned/hybrid control | Two common 6,400-request protocols and independent commit gate | Preserve the harness, raw reports, generated macros, and manifest pin | Artifact owner | Prior P1 deduction removed |

## Recommended Next CCFA Owner

1. `ccf-paper-reviewer` should remain the decision owner after any native or
   independent evidence arrives.
2. Experimental ownership should move to an external/native hardware operator for
   Linux/x86-64 and discrete-accelerator measurements.
3. Artifact ownership should publish the frozen source, report trees, manifests,
   dataset instructions, and licenses under a persistent identifier.
4. If only local work is available, distributed authority scaling is higher value
   than further wording changes or another same-host operator microbenchmark.

## Checks Run

- Reproduced the shared operator-control target from its direct authorities.
- Verified 12,800 shared-control attempts, zero failures, zero gate mismatches, and
  the required `independent_runtime_commit_gate_for_accepts=1` contract field in both
  raw reports.
- Verified paired lower bounds of 1.152 and 2.353 for the verified linear and
  nonlinear operators against the common controls.
- Ran the focused `smave_unit` corrected-candidate commit and fallback tests.
- Rebuilt the complete Release tree; macOS/ARM64 CTest passed 29/29.
- Clean Ubuntu 24.04 ARM64 container passed 29/29.
- Clean Ubuntu 24.04 emulated x86-64 container passed 29/29.
- Reconstructed generated operator, router, and fused-gate values with
  `paper/check_evidence.py`.
- Compiled `paper/main.pdf`; it remains 12 pages.

## Unresolved or Unverified

- Native Linux/x86-64 performance.
- CUDA or another discrete-accelerator complete-cost comparison.
- NUMA, multi-node, and distributed commit/authority scaling.
- Public immutable archive and persistent identifier.
- Independent-host reproduction.
- Published third-party learned/hybrid baseline under the same complete-cost
  contract.
- Shared-control coverage across the seven PDE families.
- Final author, affiliation, funding, acknowledgment, conflict, and submission-mode
  metadata.

## Output Self-Check

- Scores are separated from confidence and match the strongest unresolved concern.
- Every score below 5 has a concrete deduction and repair condition.
- The shared-baseline concern is marked closed rather than repeated mechanically.
- No container result is described as native performance or independent
  reproduction.
- The shared control is not mislabeled as an external published implementation.
- Negative acceptance, fallback, device, transfer, and comparator outcomes remain
  visible.
- No wording-only change is credited as external evidence or used to inflate the
  overall score.
- The report contains no placeholder, invented result, or acceptance probability.
