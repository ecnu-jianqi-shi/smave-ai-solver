# CCF-A Full Review — Round 6

## Mode

Full scientific, writing, artifact, integrity, and TPDS-fit re-review after adding
counterbalanced solver-order sensitivity and regenerating all repeated PDE evidence.

## Venue and Assumptions

- Target: IEEE TPDS regular journal paper.
- Review date: 2026-07-23.
- Evidence: current 12-page PDF, manuscript sources, 217 pinned repeated-PDE reports,
  124 pinned order-sensitivity reports, two-family gate evidence, analyzers, generated
  claim macros, local artifact manifest, and Rounds 1--5.
- Overall scale: CCFA 1--10; criterion scale: 1--5.

## Paper Summary

The paper presents verification-aware expert selection for repeated numerical solves.
It combines a reach-weighted complete-cost objective, role-constrained
candidate--corrector--gate--fallback transactions, a bounded commit-authority
proposition, and a typed heterogeneous runtime. The evaluation now includes seven
30-run paired PDE comparisons, two-family gate scaling, complete-path and batch
scaling, matched solver-order probes at the minimum and maximum speedup endpoints,
routing and operator studies, ablations, failure cases, and assumption-indexed probes.

## Likely Stance and Calibrated Score

**Overall: 8/10 — accept. Scholarly confidence: 5/5.**

The new experiment closes the strongest remaining local timing-design concern.
Diffusion--Sorption and CFD1D were each rerun in 30 matched classical-first and
SMAVE-first process pairs with alternating pair order. The SMAVE-first/classical-first
speedup ratios are `1.062 [1.024, 1.141]` and `1.039 [1.007, 1.090]`, showing a
measurable `3.9%--6.2%` order effect without reversing either speedup. All seven PDE
results were regenerated under the corrected benchmark code and now span
`1.56x--136.74x`, with every bootstrap lower bound above one. The score remains 8,
rather than increasing to 9, because performance portability and independent public
reproduction are still absent.

## Scorecard

| Dimension | Score (1--5) | Confidence (1--5) | Evidence basis | Deduction / score-change condition |
|:---|:---:|:---:|:---|:---|
| Novelty | 4 | 5 | Sections 3--5; contribution list | The transaction/objective combination is differentiated. A 5 requires a broader scheduling/composition theorem or stronger new algorithmic result. |
| Soundness | 4 | 5 | Proposition 1; probes; order study; 29/29 tests | Timing order is now measured at two endpoints, but callback, floating-point, and hardware faults remain outside the formal guarantee. |
| Evidence | 4 | 5 | Seven repeated PDE families; 124 order reports; two gate families; negative results | Robust local evidence is a clear strength. A 5 requires independent platform/runtime reproduction and stronger external baselines. |
| Significance | 4 | 4 | Parallel verification, complete-cost accounting, and broad runtime integration | Distributed impact and external workload adoption remain unverified. |
| Clarity | 5 | 5 | Generated evidence macros; tables; captions; explicit limitations | No material deduction in the current snapshot. |
| Reproducibility | 4 | 5 | 341 pinned raw reports; analyzers; manifest; paper and test gates | A 5 requires a committed public archive, dependency/data instructions, and independent rerun. |
| Ethics / Limitations | 5 | 5 | Platform, order, transfer, device, failure, and theorem boundaries | No material deduction. |

**Overall:** 8/10 | **Scholarly Confidence:** 5/5

**Recommendation:** accept.

**Verdict:** native non-Apple reproduction plus an immutable public artifact and
independent rerun can raise the paper to 9; failure to preserve complete-cost and
negative-result accounting would lower it to 7.

## Writing Risk Scorecard

| Dimension | Weight | Score (1--5) | Confidence (1--5) | Evidence basis | Concrete repair |
| --- | ---: | ---: | ---: | --- | --- |
| Storyline and motivation | 12 | 5 | 5 | The paper progresses from heterogeneous solver risk to verifiable transaction and evidence | Preserve the bounded thesis. |
| Contribution display | 12 | 5 | 5 | Four contributions separate formulation, invariant, implementation, and evidence | Keep result macros generated. |
| Paragraph logic | 10 | 4 | 5 | Dense evaluation remains traceable at the 12-page boundary | Recheck float order after metadata. |
| Claim-evidence alignment | 14 | 5 | 5 | Gate, PDE, and order claims are authority-generated or checker-verified | Extend generation to any future headline result. |
| Method readability | 10 | 5 | 5 | Main paired protocol and separate order protocol are now distinguished | No material action. |
| Experiment narration | 10 | 5 | 5 | Main intervals, order effect, complete-path limit, and negative results are interpreted | Preserve endpoint-only scope. |
| Related-work positioning | 8 | 4 | 4 | Closest portfolio, learned solver, correction, and assurance work are separated | Add one shared learned-baseline protocol. |
| Terminology and notation consistency | 8 | 5 | 5 | Transaction roles and performance units are stable | No action. |
| LaTeX and format discipline | 8 | 5 | 5 | `paper/check.sh` passes; 12 pages; no undefined reference or overfull box | Re-run after author metadata. |
| Reviewer-facing risk | 8 | 4 | 5 | Remaining risk is external evidence, not internal contradiction | Publish and independently reproduce the snapshot. |

**Weighted writing score: 4.74/5. Writing risk: low for the reviewed snapshot.**

## Top Strengths

1. The timing protocol now measures, rather than merely disclaims, solver-order
   sensitivity at the lowest and highest observed PDE speedup points.
2. The regenerated PDE evidence remains positive across all seven families while
   exposing wider and more realistic intervals.
3. All gate, PDE, and order values are generated from authoritative reports and
   protected by paper checks.
4. The manifest pins 341 raw reports, harness sources, evidence summaries, and paper
   sources.
5. The paper continues to separate gate-only, complete-path, batch, routing, device,
   transfer, and failure evidence.
6. The full test suite and manuscript gate pass after the benchmark changes.

## Major / Fatal Concerns

### C1 — Major: single-platform performance evidence

- Evidence: all authoritative timing remains Apple M4/macOS/Accelerate.
- Decision risk: host-specific FFT/BLAS, memory, threading, and runtime effects may
  determine the measured scaling and crossover behavior.
- Repair: native Linux/x86-64 plus one discrete-accelerator reproduction under the
  same complete-cost, paired, order-aware, and failure-inclusive contract.
- Score condition: required for 9--10.

### C2 — Major: no immutable public archive or independent rerun

- Evidence: the local hash manifest is strong, but the repository has no committed
  snapshot, release tag, persistent identifier, or external reproduction.
- Decision risk: reviewers cannot independently recover and rerun the exact artifact.
- Repair: commit and archive the exact source, manifests, raw reports, dataset
  acquisition/license instructions, and environment; obtain an independent rerun.
- Score condition: required for reproducibility 5/5 and overall 9--10.

### C3 — Moderate: order sensitivity is measured only at two endpoints

- Evidence: matched order probes cover Diffusion--Sorption and CFD1D.
- Decision risk: the remaining five workloads may have different cache, warm-start,
  or first-call order effects.
- Repair: extend the counterbalanced protocol to all seven families if space and
  runtime permit, or state why the endpoint design is sufficient.

### C4 — Moderate: shared learned-baseline protocol remains absent

- Evidence: learned/operator comparisons remain family-specific.
- Decision risk: the scientific-ML reviewer may score comparative depth below the
  systems contribution.
- Repair: evaluate the strongest applicable learned/hybrid baselines on one common
  split, hardware path, accuracy target, and complete-cost contract.

### C5 — Submission readiness: author metadata remains incomplete

- Evidence: author, affiliation, funding, and acknowledgment placeholders remain.
- Repair: insert final or anonymous-submission metadata and rerun paper/manifest
  checks.

## Writing and Presentation Concerns

1. The new order paragraph is decision-relevant and appropriately bounded to two
   endpoints.
2. The regenerated CFD interval is visibly wider than the earlier snapshot; the
   paper correctly reports it instead of retaining a more favorable historical value.
3. The manuscript is exactly 12 pages, leaving no room for uncontrolled metadata or
   new tables.

## Format and Venue Concerns

- IEEE journal template: pass.
- TPDS topical fit: pass; parallel verification and heterogeneous runtime scheduling
  are central.
- Length: exactly 12 pages.
- Citations, references, labels, overfull boxes, and hidden manipulation text: pass.
- Metadata/anonymity mode: unresolved.
- Desk rejection risk: low for topic and minimum quality; medium for submission
  readiness until metadata and public artifact actions are complete.

## Multi-Reviewer Panel

### Reviewer R1 — Numerical soundness

- Score tendency: 8/10; confidence 5/5.
- Positive: bounded theorem, explicit probes, cross-solver agreement, and endpoint
  order sensitivity form a coherent correctness/evaluation boundary.
- Negative: arbitrary callback and hardware faults remain outside the guarantee.
- Score-change condition: broader implementation verification.

### Reviewer R2 — Parallel systems

- Score tendency: 8/10; confidence 5/5.
- Positive: gate, complete-path, batch, and order experiments expose mechanism,
  saturation, and measurement sensitivity.
- Negative: no second host, NUMA, or multi-node evidence.
- Score-change condition: independent platform and distributed/NUMA scaling.

### Reviewer R3 — Scientific machine learning

- Score tendency: 7/10; confidence 5/5.
- Positive: learned candidates remain subordinate to independent authority and failed
  transfer is visible.
- Negative: no common learned-baseline protocol.
- Score-change condition: matched learned/hybrid comparison.

### Reviewer R4 — Artifact and reproducibility

- Score tendency: 8/10; confidence 5/5.
- Positive: 341 raw reports, deterministic analyzers, generated claims, and manifest
  checks materially reduce artifact risk.
- Negative: no persistent public archive or independent rerun.
- Score-change condition: immutable release and external reproduction.

## Panel Synthesis and AC Meta-Review

- Agreement: the paper is internally coherent, locally reproducible, and has a
  credible TPDS-facing parallel verification contribution.
- Disagreement: the ML reviewer remains at 7 because comparative learned evidence is
  narrower than systems evidence.
- Decisive accept axis: formal transaction, two-family gate scaling, seven-family
  paired evidence, measured order sensitivity, and failure-inclusive complete cost.
- Decisive reject axis: unsupported portability or universal speedup claims; the paper
  does not make them.
- Unresolved evidence: independent platform, public archive/rerun, all-family order
  sensitivity, shared learned baselines, and final metadata.
- Final calibrated stance: **accept, 8/10.**

## Concern-to-Action Table

| Priority | Concern | Required action | Expected effect | Status |
| --- | --- | --- | --- | --- |
| P0 | Platform portability | Native Linux/x86-64 and discrete-accelerator reproduction | Required for 9--10 | External hardware |
| P0 | Public artifact | Commit, persistent archive, dataset instructions, independent rerun | Required for 9--10 | Open |
| P1 | Order breadth | Extend counterbalancing from 2 to 7 PDE families | Strengthens evidence robustness | Partly closed |
| P1 | Learned baselines | Shared training/evaluation/complete-cost protocol | Raises ML reviewer score | Open |
| P2 | Metadata | Fill submission metadata and rerun checks | Removes readiness risk | Pending author input |

## Recommended Next CCFA Owner

- Evaluation maintainer with Linux/x86-64, accelerator, NUMA, or cluster access.
- Artifact maintainer for committed archival publication and external rerun.
- Scientific-ML maintainer for shared learned/hybrid baseline comparison.
- Author for final submission metadata.

## Checks Run

- Recomputed the seven-family repeated statistics from 217 pinned reports: exact
  match to stored evidence and plot data.
- Recomputed order sensitivity from 124 pinned reports: exact match to stored evidence.
- `paper/check_evidence.py`: pass.
- `paper/check_artifact_manifest.py`: pass.
- `paper/check.sh`: pass; 12 pages; no undefined citation/reference or overfull box.
- Full CTest suite: 29/29 passed.
- Targeted rebuild and four order-mode correctness probes for the modified benchmarks.
- Current-claim stale-number and hidden-manipulation audit.

## Unresolved or Unverified

- Native non-Apple, discrete-accelerator, NUMA, and multi-node performance.
- Public persistent archive and independent rerun.
- Solver-order sensitivity for the remaining five PDE families.
- Shared-protocol learned/hybrid baseline.
- Final author, affiliation, funding, acknowledgment, and anonymity configuration.

## Output Self-Check

- The review does not raise the score merely because another experiment was added.
- The remaining 8-to-9 boundary is tied to external evidence, not local prose.
- Every score of 3 or below has a concrete repair condition.
- No acceptance probability or guaranteed reviewer response is asserted.
