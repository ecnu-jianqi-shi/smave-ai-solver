# CCF-A Full Review — Round 2

## Mode

Full scientific, writing, LaTeX, and TPDS-fit re-review after Round 1 revision.

## Venue and Assumptions

- Target: IEEE Transactions on Parallel and Distributed Systems (TPDS).
- Review basis: revised 11-page manuscript, new intra-node scaling evidence, source implementation, claim ledger, and all Round 1 artifacts.
- Scoring scale: 1–10 overall and 1–5 per criterion under the CCFA calibration guide.

## Paper Summary

The revised paper presents a verification-aware solver runtime whose selected unit is a candidate--corrector--gate--fallback transaction. It now defines cascade reach events and a complete expected-cost objective, proves a protocol-level commit-authority invariant under explicit assumptions, differentiates itself from HINTS, PhysicsCorrect, and ANCHOR, separates contract coverage from acceleration evidence, and adds a 30-repetition worker-scaling study for the original-equation gate and complete verified path.

## Likely Stance and Calibrated Score

**Overall: 6/10 — borderline positive. Scholarly confidence: 5/5.**

The revision removes the main soundness and positioning weaknesses from Round 1. The paper now has a visible formal core and a concrete parallel verification result. The decisive remaining concern is evidence scope: all authoritative performance measurements still come from one Apple M4 machine, and the only controlled scaling result is one intra-node Navier--Stokes subsystem. This prevents a clear TPDS accept despite the stronger manuscript.

## Scorecard

| Dimension | Score (1–5) | Confidence (1–5) | Evidence basis | Deduction / score-change condition |
|:---|:---:|:---:|:---|:---|
| Novelty | 4 | 5 | Abstract; Sections 2.2, 2.5, 3.2–3.3 | The reach-weighted transaction objective and commit invariant are now differentiated from hybrid correction and portfolio selection. Raise to 5 with a broader scheduling result or nontrivial distributed composition theorem. |
| Soundness | 4 | 5 | Proposition 1; Sections 3.2–3.4, 5.1, 6.5 | Assumptions and proof boundaries are explicit, and zero-event statistics use scenario-level trials. Raise to 5 with mechanized/formal verification or systematic fault-injection coverage for each assumption. |
| Evidence | 4 | 5 | Tables 2–4; Figures 3–5; claim ledger; parallel-scaling report | Failure-inclusive evidence, paired intervals, baselines, and worker scaling are visible. Raise to 5 with independent cross-platform reproduction, broader scaling, and paired PDEBench distributions. |
| Significance | 4 | 4 | Introduction; RQ5; Discussion | Verification cost is shown to be a real parallel bottleneck, but the impact remains bounded to qualified workloads. Raise to 5 with external adoption or broad multi-platform impact. |
| Clarity | 4 | 5 | Revised abstract, contributions, formal model, result qualifications | Formal, empirical, and limitation claims are separated. Raise to 5 after a final table-density and float-order pass. |
| Reproducibility | 4 | 5 | `paper/check.sh`, scaling runner/analyzer, machine reports, claim ledger | The new experiment is repeatable and statistically scripted. Raise to 5 with an immutable public archive, environment lock, and independent rerun. |
| Ethics / Limitations | 5 | 5 | Sections 6.5 and 9; descriptive PDE labeling | The manuscript avoids independence inflation, universal speedup, zero-risk, and cross-platform overclaiming. |

## Top Strengths

1. The central transaction is now formal rather than metaphorical.
2. Complete cost explicitly charges rejected cascade stages and fallback reach probability.
3. Recent closest hybrid-correction work is acknowledged and differentiated without dismissive framing.
4. PDEBench speedups are correctly labeled descriptive when paired samples are unavailable.
5. The operator safety bound uses 64 independent scenarios rather than 6,400 repeated timings.
6. The 30-repetition scaling study shows both the useful gate speedup and the much smaller complete-path gain.

## Major Concerns

### C1 — Major: performance portability remains untested

- Evidence: all measurements use one Apple M4 host; no Linux/x86-64, CUDA, discrete GPU, NUMA, or multi-node result is present.
- Deduction: TPDS readers cannot tell whether the routing and verification bottlenecks are architecture-specific.
- Repair condition: reproduce the same contract on at least one Linux/x86-64 host and one discrete accelerator, including transfer, residency, and failure outcomes.

### C2 — Moderate: the parallel study covers one subsystem and one explicit worker layer

- Evidence: RQ5 controls classical-PCG and gate workers for 40 Navier--Stokes Helmholtz solves while fixing Accelerate internal threads to one.
- Deduction: the result demonstrates gate parallelism, not general solver-runtime scaling.
- Repair condition: add batch-size scaling, resource contention, and at least one second equation family or gate type.

### C3 — Moderate: formal assumptions are tested indirectly

- Evidence: Proposition 1 maps assumptions to implementation components, but no assumption-indexed fault-injection matrix is shown.
- Deduction: the proof is protocol-level and the implementation correspondence remains partly narrative.
- Repair condition: add tests for candidate-state mutation attempts, gate failure, cancellation before commit, fallback failure, and concurrent revocation, then map each test to (A1)–(A4).

### C4 — Moderate: PDEBench uncertainty remains incomplete

- Evidence: per-workload totals and solve counts are visible, but the aggregate reports do not retain paired per-solve samples.
- Deduction: the `111.13×` result remains descriptive and cannot support a stability claim.
- Repair condition: rerun with repeated paired traces and report fixed-seed intervals for all seven workloads.

## Writing and Presentation Concerns

1. Table 4 is dense in one-column format and breaks several labels across lines.
2. The delayed PDE comparison table appears after the RQ5 heading, weakening local figure/table narration.
3. The abstract is now disciplined, but “Hybrid DAE” breadth remains secondary to the strongest sparse/PDE evidence and should stay de-emphasized.

## Format and Venue Concerns

- Template and compilation: pass.
- Length: 11 pages, within the current working limit.
- References: current closest works added and BibTeX resolves.
- Figures: readable; scaling intervals are visible.
- Author and funding placeholders: unresolved submission metadata, not a scientific-score issue.

## Multi-Reviewer Panel

### Reviewer R1 — Numerical methods

- Score tendency: 7/10; confidence 5/5.
- Positive: complete-cost selection and correction/gate separation are technically coherent.
- Negative: the protocol theorem does not establish correctness of callback implementations or floating-point execution.
- Score-change condition: assumption-indexed fault injection and stronger gate sensitivity analysis.

### Reviewer R2 — Parallel systems

- Score tendency: 6/10; confidence 5/5.
- Positive: the gate has a measured $2.068\times$ ten-worker speedup with a paired interval.
- Negative: the complete path reaches only $1.207\times$ and remains single-host.
- Score-change condition: multi-family scaling, contention, and cross-platform reproduction.

### Reviewer R3 — Scientific machine learning

- Score tendency: 7/10; confidence 5/5.
- Positive: the paper now distinguishes mandatory commit gating from residual-triggered correction.
- Negative: learned components are heterogeneous and not compared under one common training/baseline protocol.
- Score-change condition: a unified learned-candidate benchmark with strongest recent hybrid baselines.

### Reviewer R4 — Artifact and reproducibility

- Score tendency: 8/10; confidence 5/5.
- Positive: one-command scaling runner, deterministic analyzer, claim ledger, and negative evidence are strong.
- Negative: no immutable external archive or independent rerun.
- Score-change condition: public release and second-machine reproduction.

## Panel Synthesis and AC Meta-Review

- Agreement: Round 1 soundness and novelty-positioning concerns are substantially resolved.
- Disagreement: artifact quality supports acceptance, while performance portability remains below a clear TPDS bar.
- Decisive accept axis: cross-platform scaling and assumption-indexed implementation validation.
- Decisive reject axis: treating one Apple M4 subsystem as general parallel-runtime evidence.
- Final calibrated stance: **borderline positive, 6/10**.

## Concern-to-Action Table

| Priority | Concern | Required action | Score-impact condition | Status |
| --- | --- | --- | --- | --- |
| P0 | Cross-platform portability | Linux/x86-64 and discrete-accelerator rerun | Required for overall 8–10 | Requires external hardware |
| P0 | Narrow parallel evidence | Add contention, batch scaling, and second family | Can raise evidence and venue fit | Open |
| P1 | Assumption correspondence | Add (A1)–(A4) fault-injection matrix | Can raise soundness | Open |
| P1 | PDE uncertainty | Retain paired traces and bootstrap all workloads | Can raise evidence | Open |
| P2 | Table/float density | Compress Table 4 and improve placement | Can raise clarity | Open |
| P2 | Public artifact | Archive release and environment lock | Can raise reproducibility | Requires release action |

## Checks Run

- Full manuscript reread after revision.
- Source-to-proposition correspondence scan.
- Public closest-work verification.
- Parallel-scaling raw report and analyzer inspection.
- LaTeX build, reference, overflow, page-count, and visual contact-sheet checks.

## Unresolved or Unverified

- No independent architecture or multi-node evidence.
- No contention or second-family worker-scaling result yet.
- No paired PDEBench timing distributions.
- No immutable public artifact identifier.

## Output Self-Check

- Criterion scores, overall stance, and confidence are separate.
- Every score below 5 has a concrete repair condition.
- The review does not promise acceptance or fabricate missing hardware results.
