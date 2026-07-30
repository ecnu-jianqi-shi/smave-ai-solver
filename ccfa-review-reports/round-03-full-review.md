# CCF-A Full Review — Round 3

## Mode

Full scientific, writing, artifact, and TPDS-fit re-review after adding assumption-indexed probes and fixed-worker batch scaling.

## Venue and Assumptions

- Target: IEEE TPDS regular journal paper.
- Evidence: current 11-page PDF, all manuscript sources, worker and batch raw reports, analyzers, fault-probe reports, claim ledger, and Round 1–2 reviews.
- Overall scale: CCFA 1–10; criterion scale: 1–5.

## Paper Summary

The paper presents a typed verification-aware expert runtime for repeated numerical solves. The router selects a role-constrained transaction and minimizes reach-weighted complete cost. The protocol proves a commit-authority invariant under four explicit assumptions; machine-readable probes now map each assumption to fresh-buffer, tamper/revocation, atomic-cancellation, and original-gated fallback behavior. The revised evaluation includes 30-repetition worker scaling and 30-repetition fixed-worker batch scaling on an incompressible Navier--Stokes subsystem, in addition to cross-suite, routing, operator, device, and negative-transfer evidence.

## Likely Stance and Calibrated Score

**Overall: 7/10 — weak accept. Scholarly confidence: 5/5.**

The manuscript now has a coherent formal contribution, explicit implementation correspondence, reproducible parallel and batch experiments, and unusually strong negative-result discipline. It crosses the weak-accept boundary because a TPDS-facing mechanism—parallel original-equation verification—is now measured at both worker and batch dimensions with paired intervals. It is not an 8–10 paper because all authoritative performance evidence remains single-host and the scaling mechanism is demonstrated on one equation subsystem.

## Scorecard

| Dimension | Score (1–5) | Confidence (1–5) | Evidence basis | Deduction / score-change condition |
|:---|:---:|:---:|:---|:---|
| Novelty | 4 | 5 | Sections 2.2, 2.5, 3.2–3.3, 5.1 | The transaction objective and commit invariant are clearly differentiated. A 5 requires a broader scheduling/composition result beyond one runtime transaction. |
| Soundness | 4 | 5 | Proposition 1; Table 1; Sections 6.5 and 9.4 | Assumptions, proof, fault probes, and statistical independence boundaries align. A 5 requires formal implementation verification or substantially broader adversarial/fault coverage. |
| Evidence | 4 | 5 | Tables 2–5; Figures 3–5; scaling reports | Worker and batch curves, intervals, baselines, ablations, and failures are visible. A 5 requires cross-platform reproduction, a second scaling family, and paired PDEBench traces. |
| Significance | 4 | 4 | Introduction; RQ5; Discussion | Verification is shown to be an optimizable parallel stage, but broad impact remains unproven. External workloads/adoption would raise this score. |
| Clarity | 5 | 5 | Revised abstract, contribution list, descriptive-result labels, proof/evidence separation | The paper now consistently distinguishes theorem, observation, and limitation. Maintain the score by preserving float readability under final metadata. |
| Reproducibility | 4 | 5 | Runner scripts, deterministic analyzers, claim ledger, `paper/check.sh` | Local reproduction is strong. A 5 requires immutable archival release, environment lock, and independent rerun. |
| Ethics / Limitations | 5 | 5 | Sections 6.5 and 9; negative evidence | No universal acceleration, zero-risk, or multi-node claim is made from local evidence. |

## Top Strengths

1. The central novelty is stated as a transaction and objective, not a list of solvers.
2. The theorem's assumptions are connected to concrete machine probes.
3. The worker study separates a `2.068×` gate gain from a `1.207×` full-path gain.
4. The batch study shows `2.432×` throughput scaling from 12 to 160 solves and reports saturation.
5. PDE, operator, routing, and device results use appropriately different statistical labels.
6. Negative device, nonlinear-routing, and topology-transfer outcomes remain first-class evidence.

## Major Concerns

### C1 — Major: no independent performance portability evidence

- Current evidence: one Apple M4 host and one software stack.
- Decision risk: a reviewer may attribute the observed bottleneck and scaling to Accelerate/macOS implementation choices.
- Full repair: Linux/x86-64 plus one discrete accelerator under the same complete-cost and failure contract.
- Score implication: prevents evidence, reproducibility, and overall scores from reaching 5/5 and 8–10.

### C2 — Moderate: scaling remains one-family

- Current evidence: worker and batch scaling both use the incompressible Navier--Stokes Helmholtz subsystem.
- Decision risk: gate parallelism may depend on this regular stencil and request shape.
- Repair: repeat controlled scaling on a nonlinear gate or irregular sparse family.

### C3 — Moderate: PDE headline uncertainty remains descriptive

- Current evidence: seven total-runtime comparisons but no paired per-solve traces.
- Decision risk: the largest `111.13×` point has no stability interval.
- Repair: preserve repeated paired traces and bootstrap each workload.

### C4 — Moderate: no public immutable artifact

- Current evidence: deterministic local reports and scripts.
- Decision risk: external reviewers cannot independently reproduce the current worktree state.
- Repair: archive source, configuration, report hashes, and data manifest with a persistent identifier.

## Writing and Presentation Concerns

1. The new assumption table is dense but readable; moving report paths to the ledger was the correct choice.
2. Floating tables in Section 7 can appear after their first local subsection; final production should recheck order after author metadata is inserted.
3. The title remains accurate but does not foreground parallel verification; a later title comparison may improve TPDS fit without changing scope.

## Format and Venue Concerns

- IEEE journal template: pass.
- Current length: 11 pages, leaving limited room under the working 12-page review target.
- Citations/references/overfull boxes: pass under `paper/check.sh`.
- Figure readability: pass in rendered PDF.
- Author/funding placeholders: unresolved submission metadata.

## Multi-Reviewer Panel

### Reviewer R1 — Numerical soundness

- Score tendency: 8/10; confidence 5/5.
- Positive: bounded theorem, exact gate contracts, correction ablation, and fault probes form a coherent safety story.
- Negative: callback correctness and floating-point faults remain outside the proof.
- Score-change condition: formalized callback/gate specification or implementation verification.

### Reviewer R2 — Parallel systems

- Score tendency: 7/10; confidence 5/5.
- Positive: worker and batch scaling expose both useful parallelism and Amdahl saturation.
- Negative: one host and one regular PDE subsystem limit generality.
- Score-change condition: second-family and cross-platform scaling.

### Reviewer R3 — Scientific machine learning

- Score tendency: 7/10; confidence 5/5.
- Positive: correction, gating, and fallback are separated from model inference and closest hybrid work is handled.
- Negative: learned experts are not compared under one unified training protocol.
- Score-change condition: strongest hybrid learned baselines on a shared workload.

### Reviewer R4 — Artifact and reproducibility

- Score tendency: 8/10; confidence 5/5.
- Positive: scripts generate raw reports, deterministic statistics, figures, and claim-ledger entries.
- Negative: no archival release or independent machine.
- Score-change condition: immutable public artifact and external rerun.

## Panel Synthesis and AC Meta-Review

- Agreement: the paper is now scientifically coherent and reviewer-auditable.
- Disagreement: numerical/artifact reviewers lean accept more strongly than the systems reviewer.
- Decisive accept axis: formal transaction plus measured verification parallelism and honest complete-path accounting.
- Decisive reject axis: single-machine, one-family performance evidence.
- Final calibrated stance: **weak accept, 7/10**.

## Concern-to-Action Table

| Priority | Concern | Required action | Expected effect | Status |
| --- | --- | --- | --- | --- |
| P0 | Cross-platform portability | Native Linux/x86-64 and discrete-accelerator reproduction | Needed for 8–10 | Requires external hardware |
| P0 | One-family scaling | Add nonlinear or irregular sparse worker/batch scaling | Can strengthen evidence to clear accept | Open |
| P1 | PDE uncertainty | Add repeated paired traces for all seven workloads | Can raise evidence | Open |
| P1 | Public artifact | Create immutable archive and environment lock | Can raise reproducibility | Requires release action |
| P2 | Final float order | Re-render after author/funding insertion | Preserves clarity | Pending metadata |

## Checks Run

- Full manuscript and rendered-PDF review.
- `paper/check.sh` and page-count audit.
- Worker and batch raw-report consistency audit.
- Fixed-seed analyzer review.
- Five targeted cancellation/fallback CTest probes.
- Claim-ledger scope review.

## Unresolved or Unverified

- Native non-Apple performance.
- Multi-node or NUMA behavior.
- Second-family scaling.
- Paired PDEBench intervals.
- Public artifact identifier and author metadata.

## Output Self-Check

- The score increase is tied to new theorem/probe/scaling evidence, not prose alone.
- No acceptance probability or guaranteed reviewer response is asserted.
- Missing external evidence remains a hard ceiling on the score.
