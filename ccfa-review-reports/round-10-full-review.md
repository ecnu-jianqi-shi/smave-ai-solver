# CCF-A Full Review — Round 10

## Mode

Full scientific, writing, artifact, integrity, portability, and TPDS-fit re-review
after adding same-host POSIX process-isolated original-equation gate scaling.

## Venue and Assumptions

- Target: IEEE Transactions on Parallel and Distributed Systems regular paper.
- Review date: 2026-07-24.
- Materials: current 12-page PDF and LaTeX sources; process-isolated gate harness,
  verifier, generated macros, and raw evidence; all Round 9 evidence; macOS and clean
  Ubuntu ARM64/x86-64 portability outputs; source and tests; Rounds 1--9.
- Submission assumptions: author, affiliation, funding, conflict, and review-mode
  metadata remain unresolved. The placeholder acknowledgment file is no longer
  rendered in the manuscript.
- Overall scale: CCFA 1--10; criterion scale: 1--5.

## Paper Summary

The paper treats repeated numerical acceleration as verification-aware expert
selection. Its central transaction composes candidate generation, correction,
family-specific original-equation verification, commit, and mandatory classical
fallback under a reach-weighted complete-cost objective. A bounded proposition
establishes caller-visible commit authority under explicit isolation, immutable-
problem, atomic-publication, and fallback assumptions.

The evidence package includes seven paired PDE workloads, all-family solver-order
controls, two verified operator families and a common hybrid control, routing and
hindsight comparisons, device probes, gate and complete-path thread scaling, batch
amortization, ablations, negative transfer, portability checks, and explicit failure
accounting.

Round 10 adds a process-isolation probe for the fused original-equation gate. The
linear and nonlinear families each evaluate 2,080 requests at 1, 2, 4, and 8 fresh
POSIX child processes over 30 paired repetitions. The full experiment contains 930
child processes and 499,200 measured gate evaluations. Read-only models and requests
are inherited copy-on-write after fork; fixed-size summaries cross pipes. Process
launch, result IPC, and wait are timed. Network transport and request serialization
are explicitly excluded.

At eight processes, the heavier linear gate reaches 2.687x [2.554, 2.779], whereas
the cheap nonlinear gate regresses to 0.508x [0.469, 0.553]. Every recorded decision
and residual matches the sequential authority. The same harness completes on native
macOS/ARM64, Ubuntu ARM64, and emulated Ubuntu x86-64, although only the Apple M4
timing is treated as performance evidence.

## Likely Stance and Calibrated Score

**Overall: 8/10 — accept. Scholarly confidence: 5/5.**

Round 10 materially strengthens the TPDS systems story. The paper no longer jumps
directly from threads to an untested distributed aspiration: it now measures a real
process-isolation boundary, includes fresh process lifecycle and summary IPC, checks
authority equivalence, and retains a decisive negative result for a cheap gate. The
contrast between 2.687x linear scaling and 0.508x nonlinear regression supports a
clear systems conclusion: isolation boundaries must be selected by complete cost,
not assumed beneficial from worker count alone.

The score remains 8 rather than rising to 9 because this experiment is deliberately
same-host and fork-based. Inputs are inherited copy-on-write; there is no network,
request serialization, remote failure, distributed commit, cross-node consistency,
or independent machine. All authoritative performance remains on one Apple M4.
There is still no native Linux/x86-64 performance, discrete-accelerator complete-
cost result, public immutable archive, or independent rerun. The new evidence narrows
the distributed-systems concern but does not close the external-validity boundary.

## Quantitative Scorecard

| Dimension | Score (1--5) | Confidence (1--5) | Evidence basis | Deduction / score-change condition |
|:---|:---:|:---:|:---|:---|
| Novelty | 4 | 5 | `paper/sections/05_verification_aware_fusion.tex:21`; `tests/gate_process_scaling_evidence.cpp:158` | Verification-aware transactions plus measured process-boundary selection are differentiated. A 5 requires a distributed authority/commit mechanism beyond same-host fork isolation. |
| Soundness | 4 | 5 | `paper/sections/06_experimental_methodology.tex:114`; `build/release/gate-process-scaling/evidence.txt`; `tests/verify_gate_process_scaling.cmake` | The contract names every included and excluded cost, and strict equivalence is machine-gated. Network faults, remote callback error, and distributed commit remain outside the evidence. |
| Evidence | 4 | 5 | `paper/sections/07_evaluation.tex:220`; 499,200 measured process-gate evaluations; 930 child processes | The new experiment includes paired intervals, a positive and a negative family, and full process lifecycle cost. A 5 still requires native cross-machine, networked, or independent-host evidence. |
| Significance | 4 | 5 | `paper/sections/07_evaluation.tex:228`; `paper/sections/09_discussion_limitations.tex:40` | The family-dependent isolation crossover is a useful TPDS result. Its reach remains limited by one host, fork-inherited inputs, and gate-only scope. |
| Clarity | 5 | 5 | `paper/sections/06_experimental_methodology.tex:114`; `paper/CLAIM_EVIDENCE.md:22`; generated process macros | The paper explicitly distinguishes threads, fresh processes, copy-on-write input inheritance, pipe summaries, and absent networking. No material deduction. |
| Reproducibility | 4 | 5 | `benchmark/run_linux_portability_checks.sh`; `paper/check_evidence.py`; `paper/check_artifact_manifest.py` | The harness builds and executes with strict equivalence on three environments, but the artifact is still local and lacks a persistent public archive and independent rerun. |
| Ethics / Limitations | 5 | 5 | `paper/sections/09_discussion_limitations.tex:40`; process evidence contract | The manuscript does not relabel same-host fork measurements as multi-node or network performance and retains the nonlinear regression. No material deduction. |

**Overall:** 8/10 | **Scholarly Confidence:** 5/5

**Recommendation:** accept.

**Verdict:** a networked two-host authority experiment or representative native
Linux/x86-64 and discrete-accelerator complete-cost reproduction could support 9 if
the conclusions persist. Public archival release and independent rerun remain the
strongest reproducibility requirements. Additional same-host wording or microbenchmarks
should not move the score.

## Score-Change Conditions

| Change | Condition | Likely affected dimensions | Expected movement |
| --- | --- | --- | --- |
| Raise score | Execute a networked two-host or multi-node authority experiment with request/result serialization, remote failure handling, and commit semantics included | Novelty, Evidence, Significance | +0.5 to +1 overall if authority and conclusions persist |
| Raise score | Repeat representative complete-cost workloads on native Linux/x86-64 and a discrete accelerator | Evidence, Significance, Reproducibility | +1 overall is defensible if conclusions persist |
| Raise score | Publish an immutable source/data/report archive and obtain an independent rerun | Reproducibility, Evidence | +0.5 to +1 overall depending on scope |
| Lower score | Networked/native runs reveal decision mismatch, residual drift, or representative benefit reversal hidden by the local setup | Soundness, Evidence | -1 to -3 overall depending on severity |
| No quick change | Add another same-host process count or rewrite the distributed motivation without new transport/failure evidence | None decisive | No overall movement |

## Top Strengths

1. **The process boundary is measured rather than implied.** Fresh process creation,
   wait, and pipe-summary IPC are part of the timing boundary.
2. **The contract is explicit about exclusions.** Inputs are fork-inherited copy-on-
   write; network transport and request serialization are zero by construction.
3. **The experiment retains family-dependent failure.** Eight processes help the
   linear gate but decisively harm the nonlinear gate; both intervals exclude one.
4. **Authority is preserved.** All 499,200 measured process-gate evaluations retain
   zero decision and residual mismatches against the sequential authority.
5. **The harness is functionally portable.** macOS/ARM64, Ubuntu ARM64, and emulated
   Ubuntu x86-64 all build and execute the process-isolation verifier successfully.
6. **The result composes with prior evidence.** It complements thread scaling,
   complete-path scaling, batch amortization, and the shared learned/hybrid control
   without replacing or overstating any of them.
7. **The paper stays within 12 pages.** The result is integrated into existing gate
   methodology and evaluation rather than adding a redundant figure or layout hack.

## Major and Fatal Concerns

No fatal local correctness or integrity concern is visible. The remaining concerns
are external validity, true distributed semantics, and submission readiness.

### P0 — Native and Independent Performance

- Evidence basis: all authoritative timing remains on one Apple M4 host.
- Concern: clean Linux containers prove build, API, test, and process-harness
  correctness, not native server performance or independent reproduction.
- Required action: repeat representative paired workloads on native Linux/x86-64
  and one discrete accelerator, preferably on independently operated machines.
- Score condition: primary path from 8 to 9.

### P0 — Public Archival Reproduction

- Evidence basis: local hashes, raw reports, analyzers, and manifests are strong, but
  there is no committed source history, release tag, persistent identifier, or
  independent rerun.
- Required action: publish an immutable archive with source revision, dependencies,
  licenses, dataset acquisition, raw reports, and manifest; obtain an external rerun.
- Score condition: required for a robust 9 and likely for 10.

### P1 — Networked and Distributed Authority

- Evidence basis: the new process experiment uses same-host POSIX fork, inherited
  read-only inputs, and pipe summaries.
- Concern: it does not cover network serialization, latency variation, remote worker
  failure, coordinator failover, duplicate delivery, distributed commit, or
  cross-node artifact consistency.
- Required action: build a two-host request/verification protocol with explicit
  failure injection and end-to-end timing.
- Score condition: can raise Novelty and Significance; combined with native external
  performance it could move the overall score.

### P1 — Submission Metadata

- Evidence basis: `paper/authors.tex` remains a placeholder; funding, conflicts, and
  acknowledgments are not finalized.
- Required action: finalize identities, affiliations, correspondence, funding,
  acknowledgments, conflicts, and review mode.
- Score condition: submission readiness only.

### Narrowed — Process-Isolation Breadth

- Prior concern: the paper discussed distributed execution while measuring only
  thread-level authority scaling.
- New evidence: same-host process isolation across two equation families, 30 paired
  repetitions, 930 child processes, and cross-platform functional execution.
- Residual boundary: no network, request serialization, remote failure, or
  multi-node commit.
- Score effect: strengthens TPDS fit and the Significance confidence, but does not
  remove the P1 distributed-authority concern.

## Writing and Presentation Concerns

1. The process experiment is integrated where readers already compare gate scaling,
   avoiding a disconnected systems subsection.
2. The words “same-host,” “copy-on-write,” and “no network/request serialization”
   prevent the central overclaim risk.
3. The generated macros prevent process timing values from drifting after reruns.
4. The acknowledgment placeholder is no longer rendered, preserving the 12-page
   boundary. Final acknowledgment text must still be added when author metadata is
   available.
5. The LaTeX log retains underfull boxes in compact tables and compound terms, but no
   overfull box or compilation failure is visible.

## Format and Venue Concerns

- The IEEE Computer Society journal draft compiles to 12 pages.
- The new evidence is presented without reducing margins, fonts, or spacing.
- The process study improves TPDS relevance but remains gate-only and same-host.
- Author metadata still prevents a submission-ready package.
- A real networked/native experiment would contribute more to venue fit than another
  local worker curve.

## Multi-Reviewer Panel

### Reviewer R1 — Numerical Soundness

- Score tendency: 8/10; confidence 5/5.
- Positive signal: every process result is checked against the same sequential
  original-equation authority with zero decision/residual mismatch.
- Negative signal: remote callback and hardware correctness are not tested.
- Score-change condition: reproduce authority equivalence under networked or native
  external execution.

### Reviewer R2 — Parallel and Distributed Systems

- Score tendency: 8/10; confidence 5/5.
- Positive signal: the process-isolation crossover is a concrete systems result with
  process lifecycle costs and a decisive negative family.
- Negative signal: fork-inherited data and pipe summaries are not a distributed
  protocol; remote failures and commit are absent.
- Score-change condition: two-host transport, failure injection, and commit evidence.

### Reviewer R3 — Scientific Machine Learning

- Score tendency: 8/10; confidence 5/5.
- Positive signal: process overhead is evaluated alongside the prior shared hybrid
  baseline, demonstrating that learned acceleration depends on verification and
  placement cost rather than inference alone.
- Negative signal: the process study is gate-only and does not broaden learned-model
  family coverage.
- Score-change condition: networked complete-path learned/hybrid comparisons or a
  published third-party learned baseline.

### Reviewer R4 — Artifact and Reproducibility

- Score tendency: 8/10; confidence 5/5.
- Positive signal: the new source, analyzer, verifier, generated macros, raw evidence,
  and Linux functional runs are all included in the artifact chain.
- Negative signal: no public persistent archive or independent operator.
- Score-change condition: frozen public release and external rerun.

## Panel Synthesis and AC Meta-Review

- Agreement: the new experiment is useful, honest, and directly relevant to TPDS.
- Disagreement: the systems reviewer gives it more weight than the SciML reviewer,
  but no reviewer treats it as multi-node evidence.
- Decisive accept axis: failure-inclusive complete-cost evidence now spans threads,
  fresh processes, operator correction/fallback, routing, and heterogeneous paths.
- Decisive reject axis: a reviewer requiring native multi-node or independently
  reproduced performance can still downgrade the paper.
- Unresolved evidence: network transport, remote failure, distributed commit, native
  x86-64 performance, discrete accelerator, public archive, independent rerun, and
  final metadata.
- Final calibrated stance: 8/10 accept. Round 10 strengthens the systems contribution
  but correctly stops short of claiming the external evidence needed for 9.

## Concern-to-Action Table

| Priority | Concern | Evidence basis | Concrete action | Owner | Score condition |
| --- | --- | --- | --- | --- | --- |
| P0 | Native external performance | Apple M4 timing authority only | Repeat representative paired workloads on native Linux/x86-64 and a discrete accelerator | External hardware owner | Main path to 9 |
| P0 | Public independent reproduction | Local manifest only | Publish immutable archive and obtain independent rerun | Artifact owner | Required for robust 9--10 |
| P1 | Networked authority semantics | Same-host fork/pipe summaries | Add two-host transport, remote failure injection, and commit protocol | Distributed systems owner | Novelty/Significance +1 |
| P1 | Submission metadata | Placeholder author file | Finalize authors, funding, conflicts, and review mode | Authors | Readiness only |
| Narrowed | Process-isolation evidence | 930 child processes and 499,200 measured evaluations | Preserve harness, raw evidence, macros, and platform checks | Artifact owner | No remaining same-host process deduction |

## Recommended Next CCFA Owner

1. `ccf-paper-reviewer` remains the decision owner after new external evidence.
2. A distributed-systems experiment owner should implement a two-host authority
   protocol if suitable machines are available.
3. An external hardware operator should run native x86-64 and discrete-accelerator
   complete-cost experiments.
4. The artifact owner should publish the frozen source/report package with a
   persistent identifier.

## Checks Run

- Reproduced `reproduce-gate-process-scaling` under the final explicit contract.
- Verified 930 child processes and 499,200 measured gate evaluations.
- Verified eight-process linear scaling of 2.687x [2.554, 2.779].
- Verified eight-process nonlinear regression of 0.508x [0.469, 0.553].
- Verified zero decision and residual mismatches for both families.
- Reconstructed generated process macros with `paper/check_evidence.py`.
- Rebuilt the Release tree; macOS/ARM64 CTest passed 29/29.
- Clean Ubuntu 24.04 ARM64 passed 29/29 and executed the process harness.
- Clean Ubuntu 24.04 emulated x86-64 passed 29/29 and executed the process harness.
- Compiled `paper/main.pdf`; it remains 12 pages.

## Unresolved or Unverified

- Native Linux/x86-64 performance.
- CUDA or another discrete-accelerator complete-cost comparison.
- Network request/result serialization and latency.
- Remote worker failure, coordinator failure, duplicate delivery, and failover.
- Distributed commit and cross-node artifact consistency.
- NUMA and multi-node scaling.
- Public immutable archive and persistent identifier.
- Independent-host reproduction.
- Final author, affiliation, funding, conflict, acknowledgment, and submission-mode
  metadata.

## Output Self-Check

- Scores remain separated from confidence and align with the strongest unresolved
  external-validity concern.
- Every score below 5 has a concrete deduction and repair condition.
- Same-host process isolation is not mislabeled as network or multi-node evidence.
- Container execution is described as correctness/functional portability, not
  performance authority.
- The nonlinear process regression remains visible.
- The report does not credit wording or page compression as scientific evidence.
- No placeholder, invented result, or acceptance probability is present.
