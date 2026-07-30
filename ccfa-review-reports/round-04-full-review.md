# CCF-A Full Review — Round 4

## Mode

Full scientific, writing, artifact, and TPDS-fit re-review after adding nonlinear
fused-gate scaling and repeated paired PDE timing.

## Venue and Assumptions

- Target: IEEE TPDS regular journal paper.
- Review date: 2026-07-23.
- Evidence: current 12-page PDF, all manuscript sources, 210 repeated PDE reports,
  regenerated linear/nonlinear gate-scaling evidence, analyzers, claim ledger,
  29 CTest results, and Rounds 1--3.
- Venue check: the official IEEE Computer Society journal guidance currently lists
  12 formatted pages as the standard-paper length; the current draft exactly reaches
  that boundary.
- Overall scale: CCFA 1--10; criterion scale: 1--5.

## Paper Summary

The paper presents a typed verification-aware expert runtime for repeated numerical
solves. A router selects role-constrained candidate--corrector--gate--fallback
transactions using reach-weighted complete cost, and a protocol proposition states
the commit-authority invariant under four explicit assumptions. The evidence package
now contains 30-repetition fused-gate scaling for both a linear family and a nonlinear
family, plus 30 run-level paired complete-runtime comparisons for seven PDE families.
It also retains complete-path scaling, fixed-worker batch scaling, routing regret,
operator transfer, device placement, ablations, failure cases, and machine-readable
fault probes.

## Likely Stance and Calibrated Score

**Overall: 7/10 — weak accept, held pending evidence synchronization. Scholarly
confidence: 5/5.**

The new experiments close two major Round-3 gaps: scaling is no longer demonstrated
on only one equation family, and the PDE headline results now have repeated paired
traces and bootstrap intervals. Those additions are strong enough to support an
8/10 paper in principle. The score is held at 7/10 because the authoritative gate
report regenerated during review now records `4.771x [3.980, 5.100]` and
`2.605x [2.386, 2.701]`, while the abstract, introduction, evaluation, conclusion,
README, and claim ledger still state `4.287x [3.885, 4.338]` and
`2.480x [2.366, 2.595]`. A submission whose headline numbers do not match its
declared authority cannot receive a stronger artifact or evidence score.

## Quantitative Scorecard

| Dimension | Score (1--5) | Confidence (1--5) | Evidence basis | Deduction / score-change condition |
|:---|:---:|:---:|:---|:---|
| Novelty | 4 | 5 | Sections 2.2, 2.5, 3.2--3.3, 5.1 | The transaction objective and commit invariant are differentiated from portfolio selection and hybrid correction. A 5 requires a broader scheduling/composition result or a stronger algorithmic theorem. |
| Soundness | 4 | 5 | Proposition 1; assumption-indexed probes; 29/29 CTests | The protocol and probes align, but callback correctness, floating-point faults, and hardware faults remain outside the proof. Formal implementation verification or broader fault injection is required for 5. |
| Evidence | 4 | 5 | Seven repeated PDE studies; linear/nonlinear gate scaling; complete-path and batch studies | Second-family scaling and paired PDE intervals are now present. The live-report/manuscript mismatch and single-host execution prevent 5. Synchronize and pin the evidence, then add an independent platform. |
| Significance | 4 | 4 | Introduction; RQ2--RQ5; Discussion | Verification is demonstrated as an optimizable parallel stage across two gate families, but external workload impact and distributed benefit remain unproven. |
| Clarity | 4 | 5 | Contribution list; result narration; limitations; current PDF | The story is clear, and the prior overfull table was repaired. Conflicting headline numbers create avoidable reviewer confusion; exact synchronization restores 5. |
| Reproducibility | 4 | 5 | Runner scripts; deterministic analyzers; 210 raw reports; claim ledger; `paper/check.sh` | Local reproducibility is strong. A 5 requires a pinned manifest/environment, immutable archive, and independent rerun. |
| Ethics / Limitations | 5 | 5 | Sections 6.5 and 9; failure-inclusive reporting | The paper does not infer universal, zero-risk, multi-node, or cross-platform conclusions from local evidence. |

## Writing Risk Scorecard

| Dimension | Weight | Score (1--5) | Confidence (1--5) | Evidence basis | Concrete repair |
| --- | ---: | ---: | ---: | --- | --- |
| Storyline and motivation | 12 | 5 | 5 | Problem--gap--transaction--evidence progression is explicit | Preserve the bounded system thesis. |
| Contribution display | 12 | 5 | 5 | Four contribution bullets expose formulation, invariant, runtime, and evidence | Keep numbers generated from one pinned evidence snapshot. |
| Paragraph logic | 10 | 4 | 5 | Most paragraphs have one role; the evaluation is dense at the page boundary | Remove redundant numeric repetition if metadata causes reflow. |
| Claim-evidence alignment | 14 | 3 | 5 | Gate-scaling claims differ from the regenerated authority | Update every occurrence and record the run identity/hash. |
| Method readability | 10 | 4 | 5 | Roles, assumptions, cost, and gate contracts are introduced in order | Keep the implementation/proposition boundary explicit. |
| Experiment narration | 10 | 5 | 5 | Tables and figures are introduced before interpretation; negative results are explained | Preserve paired-unit and complete-cost labels. |
| Related-work positioning | 8 | 4 | 4 | Closest portfolio, learned solver, and runtime-assurance axes are separated | A shared learned-baseline comparison would sharpen the positioning. |
| Terminology and notation consistency | 8 | 5 | 5 | Candidate, corrector, gate, fallback, and commit authority remain stable | No action beyond numeric synchronization. |
| LaTeX and format discipline | 8 | 5 | 5 | `paper/check.sh` passes; no undefined references or overfull boxes; 12 pages | Recheck after author and funding insertion. |
| Reviewer-facing risk | 8 | 3 | 5 | Evidence drift and missing external archive can undermine trust despite strong local scripts | Add a manifest, environment lock, and immutable release reference. |

**Weighted writing score: 4.28/5. Writing risk: moderate until the evidence snapshot
is synchronized; low after synchronization if the 12-page layout remains stable.**

## Top Strengths

1. The paper now distinguishes theorem-level commit authority from implementation
   probes and from empirical numerical agreement.
2. Seven PDE families have 30 run-level paired complete-runtime ratios; all reported
   bootstrap lower bounds exceed one and all 210 paired runs favor the measured path.
3. Fused-gate scaling covers both a linear and a nonlinear family with zero decision
   and residual mismatches relative to the serial authority.
4. Gate-only, complete-path, worker, and fixed-worker batch results reveal where
   parallel verification helps and where serial setup/kernel work dominates.
5. Negative device, nonlinear-routing, topology-transfer, and unavailable-comparison
   outcomes remain visible rather than being filtered from the thesis.
6. The manuscript build and all 29 configured tests pass on the reviewed worktree.

## Major / Fatal Concerns

### C1 — Major: headline evidence is not synchronized with its authority

- Evidence: regenerated `build/release/gate-parallel-scaling/evidence.txt` reports
  `4.771x` linear and `2.605x` nonlinear ten-worker paired speedups; seven manuscript
  and ledger locations retain the previous run's numbers.
- Decision risk: reviewers cannot tell which snapshot supports the paper and may
  treat the discrepancy as selective reporting or weak artifact governance.
- Fix class: evidence integrity; owner: manuscript/evidence maintainer.
- Repair: update every transcribed value, store the evidence run identity and hashes,
  and make the manuscript audit fail when claims drift from the report.
- Score condition: synchronization is required before moving from 7/10 to 8/10.

### C2 — Major: performance portability remains unverified

- Evidence: authoritative measurements come from one Apple M4/macOS/Accelerate host.
- Decision risk: scaling and crossover behavior may be specific to one CPU topology,
  operating system, FFT/BLAS stack, and memory system.
- Fix class: external experiment; owner: evaluation maintainer.
- Repair: native Linux/x86-64 reproduction and one discrete-accelerator reproduction
  under the same complete-cost, paired, and failure-inclusive contract.
- Score condition: this remains the principal ceiling on 9--10.

### C3 — Moderate: no immutable public artifact or environment lock

- Evidence: local scripts and reports are deterministic, but the worktree has no
  commit history, release identifier, persistent archive, or locked environment.
- Decision risk: external reviewers cannot recover the exact reviewed snapshot.
- Repair: create a source/evidence manifest, compiler/dependency lock, report hashes,
  tagged commit, and archival release with a persistent identifier.

### C4 — Moderate: PDE within-run order is fixed

- Evidence: each benchmark process times its designated classical and SMAVE paths in
  a fixed internal order; the outer runner alternates workload order only.
- Decision risk: the bootstrap interval quantifies run variation but does not remove
  systematic within-process order, cache, or thermal bias.
- Repair: randomize or counterbalance solver order where implementation permits, or
  add separate-process A/B order experiments and report order sensitivity.

### C5 — Moderate: closest learned experts lack one shared training protocol

- Evidence: learned/operator evidence is distributed across family-specific studies.
- Decision risk: a scientific-ML reviewer may view the system comparison as broader
  than the learned-baseline comparison.
- Repair: compare the strongest applicable learned and hybrid baselines on one shared
  workload, data split, accuracy target, hardware path, and complete-cost contract.

## Writing and Presentation Concerns

1. The repaired PDE table now fits and remains readable, but the draft has no spare
   page capacity for author metadata or additional external-platform evidence.
2. Exact gate values appear in the abstract, introduction, evaluation, conclusion,
   README, and claim ledger; manual duplication caused the current drift.
3. The title now foregrounds parallel original-equation gates and fits TPDS better
   than the Round-3 title.
4. The conclusion correctly states that the fixed within-run order remains a limit;
   this qualification should not be removed when the intervals are highlighted.

## Format and Venue Concerns

- IEEE journal template: pass.
- TPDS topical fit: pass; the central contribution is parallel verification and
  heterogeneous runtime scheduling, not only a numerical-method paper.
- Current length: exactly 12 pages, equal to the standard formatted-paper boundary
  in the checked IEEE Computer Society guidance.
- Citations, references, labels, and overfull boxes: pass under `paper/check.sh`.
- Hidden reviewer/LLM manipulation text: not found.
- Author, affiliation, funding, and acknowledgment placeholders: unresolved.
- Desk rejection risk: low for topic/minimum quality, medium for submission readiness
  until metadata/anonymity mode and the final artifact snapshot are resolved.

## Multi-Reviewer Panel

### Reviewer R1 — Numerical soundness

- Score tendency: 8/10; confidence 5/5.
- Main positive signal: bounded proposition, assumption-indexed probes, zero gate
  mismatches, and cross-solver agreement form a coherent safety argument.
- Main negative signal: incorrect callbacks and floating-point/hardware faults remain
  outside the invariant.
- Score-change condition: formal callback/gate specification or broader adversarial
  implementation verification.

### Reviewer R2 — Parallel systems

- Score tendency: 8/10; confidence 5/5.
- Main positive signal: two-family gate scaling plus complete-path and batch scaling
  expose useful parallelism and Amdahl-limited behavior.
- Main negative signal: one host, no NUMA/multi-node study, and no independent stack.
- Score-change condition: native second-platform and distributed/NUMA scaling.

### Reviewer R3 — Scientific machine learning

- Score tendency: 7/10; confidence 5/5.
- Main positive signal: learned candidates are evaluated behind independent gates,
  and failed transfer/negative device results are retained.
- Main negative signal: strongest learned/hybrid baselines are not compared under one
  unified training and complete-cost protocol.
- Score-change condition: shared-data, shared-hardware learned-baseline study.

### Reviewer R4 — Artifact and reproducibility

- Score tendency: 7/10; confidence 5/5.
- Main positive signal: raw reports, deterministic analyzers, plot data, tests, and a
  claim ledger are inspectable.
- Main negative signal: live regeneration changed headline values without an
  automatic manuscript guard, and no immutable public snapshot exists.
- Score-change condition: synchronized manifest-backed artifact plus independent
  rerun.

## Panel Synthesis and AC Meta-Review

- Agreement: the scientific story is coherent, and the new experiments materially
  improve the TPDS case.
- Disagreement: the systems reviewer is ready to move toward clear accept after local
  synchronization; the artifact reviewer keeps the score at weak accept until the
  authority/claim mismatch is removed.
- Decisive accept axis: formal transaction plus measured two-family parallel
  verification and failure-inclusive complete-cost accounting.
- Decisive reject axis: evidence snapshot drift combined with single-platform results.
- Unresolved evidence: independent platform, immutable archive, shared learned
  baseline, within-run order sensitivity, author/anonymity metadata.
- Final calibrated stance: **weak accept, 7/10; 8/10 after verified synchronization,
  with 9--10 still requiring external evidence.**

## Concern-to-Action Table

| Priority | Concern | Required action | Expected effect | Status |
| --- | --- | --- | --- | --- |
| P0 | Evidence drift | Synchronize all gate values and add an automated authority check | Required for 8/10 | Open |
| P0 | Snapshot identity | Add environment/report manifest and hashes | Raises artifact confidence | Open |
| P0 | Cross-platform portability | Run native Linux/x86-64 and discrete-accelerator experiments | Required for 9--10 | External hardware |
| P1 | Solver-order bias | Counterbalance or separately measure A/B order | Strengthens PDE timing validity | Open |
| P1 | Learned baseline depth | Add one shared-protocol hybrid/learned comparison | Strengthens ML positioning | Open |
| P2 | Final page fit | Re-render after metadata and new evidence | Prevents format regression | Pending metadata |

## Recommended Next CCFA Owner

- Immediate owner: manuscript/evidence maintainer for synchronization and automated
  claim checking.
- Next owner: artifact maintainer for snapshot manifest and environment lock.
- External owner: evaluation maintainer with access to Linux/x86-64, discrete GPU,
  NUMA, or multi-node hardware.

## Checks Run

- Full manuscript-source and rendered-PDF audit.
- Official IEEE Computer Society standard-paper length and TPDS scope check.
- `paper/check.sh`: pass; 12 pages; no undefined citation/reference or overfull box.
- `reproduce-gate-parallel-scaling`: pass; linear/nonlinear strict equivalence.
- Recomputed all seven PDE statistics from 210 raw repeated reports: byte-identical to
  the stored evidence and plot data.
- Full CTest suite: 29/29 passed.
- Claim-ledger, placeholder, duplicate-label, citation, and hidden-manipulation audit.

## Unresolved or Unverified

- Native non-Apple and discrete-accelerator performance.
- NUMA, multi-node, and distributed scheduling behavior.
- Immutable public artifact identifier and committed source snapshot.
- Solver-order sensitivity within each PDE benchmark process.
- Shared-protocol strongest learned/hybrid baseline.
- Final author, affiliation, funding, acknowledgment, and anonymity configuration.

## Output Self-Check

- The score is held because of a measured claim-authority mismatch, not prose taste.
- Every score of 3 has a concrete deduction and repair condition.
- No acceptance probability or guaranteed reviewer response is asserted.
- The conditions for 8, 9, and 10 are separated and tied to inspectable evidence.
