# CCF-A Full Review — Round 5

## Mode

Full scientific, writing, artifact, integrity, and TPDS-fit re-review after
synchronizing gate evidence, adding generated manuscript macros, and adding a
local environment/artifact manifest.

## Venue and Assumptions

- Target: IEEE TPDS regular journal paper.
- Review date: 2026-07-23.
- Evidence: current 12-page PDF, manuscript sources, current gate authority,
  210 repeated PDE reports plus seven warm-ups, deterministic analyzers, claim
  ledger, local snapshot/manifest, and Rounds 1--4.
- Overall scale: CCFA 1--10; criterion scale: 1--5.

## Paper Summary

The paper formulates repeated numerical acceleration as verification-aware expert
selection. A reach-weighted complete-cost router composes classical and learned
candidates, correction, original-equation gates, device kernels, and mandatory
fallback. A protocol proposition gives commit authority under explicit isolation,
immutable-problem, atomic-publication, and fallback assumptions. The evaluation
now includes seven repeated paired PDE comparisons, two-family fused-gate scaling,
complete-path worker scaling, fixed-worker batch scaling, routing and operator
studies, failure-inclusive device results, ablations, and assumption-indexed probes.

## Likely Stance and Calibrated Score

**Overall: 8/10 — clear weak accept. Scholarly confidence: 5/5.**

The previous review's evidence drift is repaired. All transcribed gate values now
come from generated macros checked against the authority report, the claim ledger
and README match the same report, and a hash manifest pins the reviewed source,
harness, evidence summaries, and 217 PDE reports. The second gate family and the
paired PDE distributions materially strengthen the parallel-systems case. The
paper is not a 9--10 submission because all performance measurements remain on one
Apple M4 host, there is no independent public rerun, and the learned-baseline and
solver-order studies remain incomplete.

## Quantitative Scorecard

| Dimension | Score (1--5) | Confidence (1--5) | Evidence basis | Deduction / score-change condition |
|:---|:---:|:---:|:---|:---|
| Novelty | 4 | 5 | Formulation, reach-weighted objective, transaction, and commit invariant | A 5 requires a broader scheduling/composition result or a stronger algorithmic theorem. |
| Soundness | 4 | 5 | Proposition 1; assumption-indexed probes; zero gate mismatches; 29/29 CTests | Callback, floating-point, and hardware faults remain outside the theorem. Implementation verification or broader adversarial coverage is required for 5. |
| Evidence | 4 | 5 | Seven paired PDE families; two gate families; worker and batch scaling; synchronized ledger | Local evidence is now internally consistent, but one host and no independent platform keep the score below 5. |
| Significance | 4 | 4 | Parallel verification mechanism, complete-cost accounting, and failure-inclusive evidence | External workload impact and distributed benefit remain unverified. |
| Clarity | 5 | 5 | Generated values, bounded claims, captions, limitations, and repaired table layout | Maintain the generated evidence path after metadata insertion. |
| Reproducibility | 4 | 5 | 217-report manifest; deterministic analyzers; snapshot; test and paper gates | A 5 requires a committed/public immutable archive and an independent rerun. |
| Ethics / Limitations | 5 | 5 | Explicit platform, order, failure, transfer, and applicability limitations | No material deduction. |

## Writing Risk Scorecard

| Dimension | Weight | Score (1--5) | Confidence (1--5) | Evidence basis | Concrete repair |
| --- | ---: | ---: | ---: | --- | --- |
| Storyline and motivation | 12 | 5 | Problem--gap--transaction--evidence progression | Preserve bounded system framing. |
| Contribution display | 12 | 5 | Contributions separate formulation, invariant, runtime, and evidence | Keep the four-part contribution structure. |
| Paragraph logic | 10 | 4 | Dense but traceable evaluation and limitation paragraphs | Recheck float order after metadata insertion. |
| Claim-evidence alignment | 14 | 5 | Generated gate macros and report-backed claim checker | Extend the same mechanism to additional headline metrics if the snapshot changes. |
| Method readability | 10 | 4 | Roles, assumptions, cost, and transaction order are explicit | A shared learned-baseline protocol would reduce comparison ambiguity. |
| Experiment narration | 10 | 5 | Paired units, complete cost, intervals, saturation, and failures are named | Preserve order-bias qualification. |
| Related-work positioning | 8 | 4 | Portfolio, learned solver, correction, and runtime-assurance axes are separated | Add the shared-protocol baseline if available. |
| Terminology and notation consistency | 8 | 5 | Gate, fallback, authority, candidate, and correction remain stable | No action. |
| LaTeX and format discipline | 8 | 5 | `paper/check.sh` passes; 12 pages; no undefined references or overfull boxes | Re-run after final author metadata. |
| Reviewer-facing risk | 8 | 4 | Remaining risk is external evidence rather than internal contradiction | Add public release and platform rerun. |

**Weighted writing score: 4.64/5. Writing risk: low for the current local snapshot;
submission-readiness risk remains moderate until metadata and public artifact actions
are complete.**

## Claim-Evidence Integrity Matrix

| Claim class | Current status | Authority |
| --- | --- | --- |
| Commit authority | Supported under explicit assumptions; not arbitrary callback/hardware verification | Proposition 1, assumption-indexed probes, CTest probes |
| PDE speedups | Supported as 30 run-level paired medians with fixed-seed bootstrap intervals | `pdebench-repeated-timing/evidence.txt`, 217 raw reports, analyzer |
| Gate parallelism | Supported for linear and nonlinear families with zero decision/residual mismatches | `gate-parallel-scaling/evidence.txt`, generated macros, claim checker |
| Complete-path scaling | Supported but Amdahl-limited on the measured Navier--Stokes subsystem | `parallel-scaling/evidence.txt`, Figure 4, Section 7 |
| Universal acceleration | Not claimed; explicitly contradicted by negative device/routing/transfer cases | Sections 8--10 |
| Cross-platform portability | Not supported; explicitly listed as unresolved | Snapshot and limitations |

## Top Strengths

1. The central contribution is a verifiable runtime transaction and objective, not a
   loose collection of solver implementations.
2. Two-family gate scaling directly addresses the prior one-family concern.
3. Seven PDE families now have paired run-level uncertainty rather than descriptive
   aggregate-only claims.
4. Generated TeX values, a claim checker, and a hash manifest remove the prior
   report-to-manuscript drift.
5. The paper separates gate-only speedup from complete-path speedup and retains
   negative transfer, device, routing, and failure outcomes.
6. The reviewed worktree passes the full 29-test CTest suite and the 12-page paper
   build gate.

## Major / Fatal Concerns

### C1 — Major: no independent performance portability evidence

- Evidence: all authoritative measurements use one Apple M4/macOS/Accelerate stack.
- Decision risk: a reviewer cannot distinguish portable algorithmic benefit from
  host-specific memory, FFT, BLAS, and runtime behavior.
- Repair: reproduce the complete-cost and failure-inclusive protocol on native
  Linux/x86-64 and one discrete accelerator; report setup, transfer, gate, fallback,
  and accuracy separately.
- Score condition: required for 9--10; not a blocker to the current 8/10 stance.

### C2 — Major: no public immutable release or independent rerun

- Evidence: the local snapshot has report/source hashes but no commit, tag, persistent
  archive identifier, or external rerun.
- Decision risk: the manifest is auditable on this machine but not independently
  recoverable by reviewers.
- Repair: commit the exact source, publish the manifest and raw reports with dataset
  acquisition/license instructions, and obtain an independent rerun.
- Score condition: required for reproducibility 5/5 and overall 9--10.

### C3 — Moderate: fixed solver order within PDE processes

- Evidence: the outer repeated runner alternates workload order, while each benchmark
  process retains a fixed internal classical/SMAVE order.
- Decision risk: intervals capture run variation but not systematic within-process
  cache, thermal, or first-call order effects.
- Repair: add explicit solver-order counterbalancing or a matched order-sensitivity
  experiment for representative and headline workloads.

### C4 — Moderate: shared learned-baseline protocol remains incomplete

- Evidence: learned/operator studies use family-specific contracts rather than one
  common training split, accuracy target, hardware path, and complete-cost table.
- Decision risk: the scientific-ML reviewer may discount the breadth of the learned
  comparison even though the runtime claims remain supported.
- Repair: compare the strongest applicable learned/hybrid baselines under one shared
  protocol and report failed transfer as a first-class result.

### C5 — Submission readiness: author and acknowledgment metadata remain placeholders

- Evidence: `paper/authors.tex` and acknowledgments still contain explicit placeholders.
- Decision risk: the scientific score is unaffected, but the current PDF is not a
  camera-ready or complete author-submission artifact.
- Repair: fill author, affiliation, funding, and acknowledgment fields and rerun the
  paper/manifest gates.

## Writing and Presentation Concerns

1. The manuscript is exactly 12 pages, so author metadata or new external evidence
   may cause float movement or page overflow.
2. The generated macro/checker pattern should be extended to any new headline metric
   rather than returning to hand-copied values.
3. The fixed solver-order limitation is correctly stated in the abstract and
   conclusion; preserve it when presenting the paired intervals.

## Format and Venue Concerns

- IEEE journal template: pass.
- TPDS topical fit: pass; the focus is parallel verification and heterogeneous
  runtime scheduling.
- Length: exactly 12 formatted pages under the checked IEEE Computer Society guidance.
- References, labels, undefined references, and overfull boxes: pass under
  `paper/check.sh`.
- Hidden reviewer/LLM manipulation text: not found.
- Author/anonymity/funding metadata: unresolved.
- Desk rejection risk: low for topic and minimum quality; medium for submission
  readiness until metadata and archival release are addressed.

## Multi-Reviewer Panel

### Reviewer R1 — Numerical soundness

- Score tendency: 8/10; confidence 5/5.
- Positive: explicit assumptions, probes, cross-solver agreement, and zero gate
  mismatches create a coherent safety boundary.
- Negative: arbitrary callback, floating-point, and hardware faults remain outside it.
- Score-change condition: implementation-level verification or broader fault model.

### Reviewer R2 — Parallel systems

- Score tendency: 8/10; confidence 5/5.
- Positive: linear/nonlinear gate scaling plus complete-path and batch scaling show
  both mechanism-level parallelism and end-to-end saturation.
- Negative: one host and no NUMA, multi-node, or independent runtime stack.
- Score-change condition: cross-platform and distributed/NUMA evidence.

### Reviewer R3 — Scientific machine learning

- Score tendency: 7/10; confidence 5/5.
- Positive: learned candidates are separated from verification authority, and failed
  transfer/device outcomes are not hidden.
- Negative: no single shared learned-baseline protocol.
- Score-change condition: matched learned/hybrid baseline study.

### Reviewer R4 — Artifact and reproducibility

- Score tendency: 8/10; confidence 5/5.
- Positive: authority-backed macros, claim checks, raw reports, deterministic analyzers,
  and a local hash manifest substantially reduce artifact drift.
- Negative: no public persistent identifier or independent rerun.
- Score-change condition: immutable public archive and external rerun.

## Panel Synthesis and AC Meta-Review

- Agreement: the local manuscript is internally consistent and scientifically
  coherent; the new evidence materially improves the TPDS case.
- Disagreement: reviewers differ mainly on how strongly to penalize the absence of
  external platform and archive evidence, not on the local claim support.
- Decisive accept axis: formal transaction plus two-family parallel verification,
  paired PDE uncertainty, and complete-cost accounting.
- Decisive reject axis: overclaiming portability or universal acceleration; the paper
  currently avoids both.
- Unresolved evidence: native non-Apple performance, immutable public release,
  independent rerun, solver-order sensitivity, and shared learned baselines.
- Final calibrated stance: **clear weak accept, 8/10.**

## Concern-to-Action Table

| Priority | Concern | Required action | Expected effect | Status |
| --- | --- | --- | --- | --- |
| P0 | Platform portability | Native Linux/x86-64 and discrete-accelerator reproduction | Required for 9--10 | External hardware |
| P0 | Public artifact | Commit, archive, dataset instructions, and independent rerun | Raises reproducibility to 5/5 | Open |
| P1 | Solver order | Counterbalance or measure order sensitivity | Strengthens paired timing validity | Open |
| P1 | Learned baselines | Shared training/evaluation/complete-cost protocol | Strengthens ML reviewer score | Open |
| P2 | Metadata | Fill authors, affiliations, funding, acknowledgments | Removes submission-readiness risk | Pending author input |

## Recommended Next CCFA Owner

- Artifact maintainer: publish the manifest-backed source and raw-report snapshot.
- Evaluation maintainer: run native Linux/x86-64, accelerator, and order-sensitivity
  studies.
- Scientific-ML maintainer: add the shared learned/hybrid baseline protocol.
- Author: fill submission metadata and rerun the final PDF/manifest checks.

## Checks Run

- Full source, rendered-PDF, claim, numeric, terminology, figure/table, and citation
  integrity audit.
- `paper/check.sh`: pass; 12 pages; no undefined citation/reference or overfull box.
- `python3 paper/check_evidence.py`: pass.
- `python3 paper/check_artifact_manifest.py`: pass.
- `reproduce-gate-parallel-scaling`: pass; two-family strict equivalence.
- Recomputed all seven PDE statistics from 210 raw reports: byte-identical to stored
  evidence and plot data.
- Full CTest suite: 29/29 passed.
- Placeholder, duplicate-label, hidden-manipulation, and bibliography-key checks.
- Docker/Linux portability attempt: unavailable because the local Docker daemon is
  not running; no portability result is claimed.

## Unresolved or Unverified

- Native Linux/x86-64, discrete-accelerator, NUMA, and multi-node performance.
- Public persistent archive identifier and independent rerun.
- Within-process solver-order sensitivity.
- Shared-protocol learned/hybrid baseline.
- Final author, affiliation, funding, acknowledgment, and anonymity configuration.

## Output Self-Check

- The 8/10 increase follows new evidence and a verified authority-sync mechanism,
  not prose-only edits.
- Every score of 3 or below has a concrete deduction and repair condition.
- No acceptance probability or guaranteed reviewer response is asserted.
- The current 8/10 ceiling and the external evidence needed for 9--10 are explicit.
