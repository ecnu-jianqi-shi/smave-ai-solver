# CCF-A Full Review — Round 11

## Mode

Full scientific, writing, artifact, integrity, portability, and TPDS-fit re-review
after adding a functional serialized TCP authority transaction probe.

## Venue and Assumptions

- Target: IEEE Transactions on Parallel and Distributed Systems regular paper.
- Review date: 2026-07-25.
- Materials: current 12-page PDF and LaTeX sources; TCP authority client/server,
  runner, verifier, and local/Docker evidence; all Round 10 evidence; source and tests;
  Rounds 1--10.
- Deployment boundary: the Docker client and server use distinct Ubuntu network
  namespaces on one physical Apple M4 host. The x86-64 run is emulated.
- Submission assumptions: author, affiliation, funding, conflict, acknowledgment, and
  review-mode metadata remain unresolved.
- Overall scale: CCFA 1--10; criterion scale: 1--5.

## Paper Summary

The paper treats repeated numerical acceleration as verification-aware expert
selection. Candidate generation, correction, family-specific original-equation
verification, commit, and mandatory classical fallback form a role-constrained
transaction optimized by a reach-weighted complete-cost objective. An assumption-
bounded proposition gives the original-equation path commit authority under explicit
isolation, immutable-problem, atomic-publication, and fallback assumptions.

The established package includes seven paired PDE workloads, all-family order
controls, two held-out operator families and a shared hybrid control, routing and
hindsight comparisons, device probes, thread/process gate scaling, complete-path and
batch scaling, ablations, negative transfer, Linux portability, and explicit failure
accounting.

Round 11 adds a canonical text protocol over TCP/IPv4. The client serializes complete
linear or nonlinear candidates to a gate service that evaluates the same FP64 fused
original-expression authority. Each run tests two unique accepts, one original-gate
rejection, one reply dropped after commit and recovered by same-transaction replay,
one conflicting reuse of that transaction identifier, and one malformed request.
Commit counts remain unchanged after all rejected/conflicting/malformed operations;
replay is idempotent; and decisions and residuals match local authority.

The protocol passes over loopback on macOS and across two Docker-bridge containers on
Ubuntu ARM64 and emulated Ubuntu x86-64. The reports explicitly state
`same_physical_host=1`, `multi_host=0`, `performance_evidence=0`,
`consensus_protocol=0`, and `production_distributed_commit=0`.

## Likely Stance and Calibrated Score

**Overall: 8/10 — accept. Scholarly confidence: 5/5.**

Round 11 closes a concrete portion of the prior P1 network-semantics concern. The
paper now exercises real socket transport, request/result serialization, distinct
network namespaces, duplicate delivery after a lost response, conflict rejection,
and original-gate equivalence. This is stronger than the prior fork/pipe-only probe
and makes the transaction claim more inspectable for TPDS readers.

The score remains 8 rather than rising to 9. Both endpoints remain on one physical
host; no network timing, remote machine, worker/coordinator crash during execution,
failover, replicated state, consensus, or production commit protocol is evaluated.
All authoritative performance remains on Apple M4. Native Linux/x86-64 performance,
discrete-accelerator complete-cost evidence, public immutable archival, independent
rerun, and final submission metadata are still absent. The new evidence narrows a
systems concern but does not satisfy the external-validity condition previously tied
to a higher score.

## Quantitative Scorecard

| Dimension | Score (1--5) | Confidence (1--5) | Evidence basis | Deduction / score-change condition |
|:---|:---:|:---:|:---|:---|
| Novelty | 4 | 5 | `paper/sections/05_verification_aware_fusion.tex:4`; `tests/gate_network_authority_evidence.cpp:290` | Verification-aware numerical transactions plus serialized idempotent authority are differentiated. A 5 requires a substantive distributed authority mechanism with replicated or cross-host commit semantics. |
| Soundness | 4 | 5 | `paper/sections/06_experimental_methodology.tex:119`; `tests/verify_gate_network_authority.cmake:1`; three network evidence reports | The probe machine-checks exactly-once unique commits and authority equivalence under named faults. It does not model coordinator crash, state loss, partitions, Byzantine behavior, or consensus. |
| Evidence | 4 | 5 | `paper/sections/07_evaluation.tex:235`; ARM64 and x86-64 Docker reports; full Round 10 package | Breadth, paired performance, negative results, failure probes, and cross-namespace functional execution are strong. A 5 requires native cross-host performance or independent reproduction. |
| Significance | 4 | 5 | `paper/sections/01_introduction.tex:45`; `paper/sections/09_discussion_limitations.tex:83` | Safe heterogeneous numerical acceleration is TPDS-relevant, and the transaction probe sharpens the systems story. Multi-node resource management and distributed complete-path impact remain unmeasured. |
| Clarity | 5 | 5 | `paper/sections/06_experimental_methodology.tex:114`; `paper/sections/07_evaluation.tex:228`; `paper/sections/09_discussion_limitations.tex:40` | Included costs, functional claims, and excluded distributed claims are consistently separated; no score deduction. |
| Reproducibility | 4 | 5 | `benchmark/run_gate_network_authority_docker.sh`; `paper/ARTIFACT_SNAPSHOT.md`; `paper/check_evidence.py` | Local one-command and two-container reproduction are strong. A 5 requires a public immutable release and independent operator rerun. |
| Ethics / Limitations | 5 | 5 | `paper/sections/09_discussion_limitations.tex:20`; evidence flags disabling performance/multi-host/consensus claims | Negative results and scope boundaries remain explicit; no score deduction. |

**Overall:** 8/10 | **Scholarly Confidence:** 5/5

**Recommendation:** accept

**Verdict:** Native cross-host complete-cost evidence plus a public independent rerun
could move the paper to 9. Failure of the TCP protocol under repeated or crash faults,
or inability to reproduce current timing claims, would lower it to 7 or below.

## Top Strengths

1. The serialized TCP path reuses the actual fused original-expression gate rather
   than a checksum or proxy authority.
2. Commit-count assertions distinguish rejected operations from successful unique
   commits and make idempotent replay observable.
3. ARM64 and emulated x86-64 container executions expose protocol portability while
   retaining `performance_evidence=0`.
4. The manuscript integrates the result without implying multi-host speed, consensus,
   or production distributed commit.
5. The overall evidence package retains positive, negative, failure, and order-
   sensitivity results under machine-readable contracts.

## Major / Fatal Concerns

1. **P0 — External performance authority.** Every authoritative timing result remains
   on one Apple M4 host. Native Linux/x86-64 and discrete-accelerator complete-cost
   replication remain necessary for a robust 9.
2. **P0 — Public independent reproduction.** The manifest is local and no immutable
   public identifier or independent operator rerun exists.
3. **P1 — Distributed execution remains incomplete.** The new transport is same-host
   and untimed. It does not test physical network variability, partitions, server
   crash/restart, replicated commit state, failover, or multi-node complete paths.
4. **P1 — Submission metadata.** Author, affiliation, funding, conflict,
   acknowledgment, and review-mode fields remain unresolved.

No new fatal numerical-soundness concern appears in Round 11. The decisive residual
risk remains external validity and submission readiness.

## Writing and Presentation Concerns

1. The new method/result/limitation statements form a coherent contract-evidence-
   boundary chain.
2. The conclusion was compressed to remove redundant numerical recitation and restore
   the 12-page boundary without margin, font, or spacing changes.
3. The PDF has no overfull-box or compilation failure; compact tables retain benign
   underfull-box warnings.
4. The author placeholder remains intentionally visible in source and blocks a final
   submission package.

## Format and Venue Concerns

- IEEE Computer Society journal mode compiles to 12 pages.
- The new evidence improves TPDS fit through network namespace, retry, and commit
  semantics, but it is not a distributed performance evaluation.
- The paper remains a research-platform paper rather than a production distributed
  solver claim.
- Final metadata and venue-specific submission checks remain mandatory.

## Multi-Reviewer Panel

### Reviewer R1 — Numerical Soundness

- Score tendency: 8/10; confidence 5/5.
- Main positive signal: network responses match the local FP64 fused authority with
  zero decision and residual mismatch.
- Main negative signal: remote callback faults and faulty hardware remain outside the
  model.
- Score-change condition: reproduce authority equivalence under native cross-host
  execution and injected server restart.

### Reviewer R2 — Parallel and Distributed Systems

- Score tendency: 8/10; confidence 5/5.
- Main positive signal: the protocol now includes real TCP serialization, distinct
  namespaces, lost-reply replay, transaction identifiers, and conflict rejection.
- Main negative signal: one physical host and in-memory server state do not establish
  distributed durability, failover, consensus, or scaling.
- Score-change condition: two physical hosts with timed complete paths, crash/restart,
  durable transaction records, and a clearly scoped commit protocol.

### Reviewer R3 — Scientific Machine Learning

- Score tendency: 8/10; confidence 5/5.
- Main positive signal: remote authority remains independent of candidate internals,
  preserving the paper's verification-aware learning boundary.
- Main negative signal: the functional transport probe does not add learned-model
  families or an external published hybrid baseline.
- Score-change condition: cross-host learned/hybrid complete-path comparison or a
  stronger third-party learned baseline.

### Reviewer R4 — Artifact and Reproducibility

- Score tendency: 8/10; confidence 5/5.
- Main positive signal: source, runner, verifier, three reports, manuscript ledger,
  and hash inputs are synchronized and machine-checkable.
- Main negative signal: no public persistent archive or independent operator.
- Score-change condition: frozen public release with persistent identifier and an
  external rerun report.

## Panel Synthesis and AC Meta-Review

- Agreement: Round 11 is a real systems-evidence improvement and is accurately scoped.
- Disagreement: the distributed-systems reviewer values the retry/transaction probe
  more than the SciML reviewer, but neither treats it as multi-host evaluation.
- Decisive accept axis: complete-cost performance, original-equation authority,
  negative results, and failure-inclusive transactions are unusually well integrated.
- Decisive reject axis: a reviewer requiring native external or multi-node performance
  can still downgrade the work.
- Unresolved evidence: native x86-64 performance, discrete accelerator, two-physical-
  host timing/failure evidence, public archive, independent rerun, and final metadata.
- Final calibrated stance: 8/10 accept. The prior network serialization/retry concern
  is narrowed; the external-validity threshold for 9 remains unmet.

## Concern-to-Action Table

| Priority | Concern | Evidence basis | Concrete action | Owner | Score condition |
| --- | --- | --- | --- | --- | --- |
| P0 | Native external performance | Apple M4 timing authority only | Repeat representative paired workloads on native Linux/x86-64 and a discrete accelerator | External hardware owner | Main path to 9 |
| P0 | Public independent reproduction | Local manifest and local Docker runs | Publish immutable archive and obtain independent rerun | Artifact owner | Required for robust 9--10 |
| P1 | Physical distributed authority | Same-host Docker bridge, in-memory transaction table | Run two physical hosts; add server crash/restart, durable replay records, and timed complete paths | Distributed systems owner | Significance/Evidence +1 |
| P1 | Submission metadata | Placeholder author source | Finalize authors, funding, conflicts, acknowledgment, and review mode | Authors | Readiness only |
| Narrowed | TCP serialization and lost reply | ARM64/x86-64 Docker reports | Preserve protocol source, verifier, evidence flags, and negative scope | Artifact owner | No remaining serialization/replay deduction |

## Recommended Next CCFA Owner

1. `ccf-paper-reviewer` remains the decision owner after external evidence changes.
2. A physical-host experiment owner should execute the protocol and representative
   complete paths across two machines with crash/restart evidence.
3. An external hardware operator should run native x86-64 and discrete-accelerator
   complete-cost experiments.
4. The artifact owner should publish the frozen package and obtain an independent
   rerun.

## Checks Run

- Built the finalized Release tree and passed 29/29 CTests on macOS ARM64.
- Reproduced the loopback TCP authority target.
- Reproduced two-container Ubuntu ARM64 and emulated x86-64 Docker-bridge probes.
- Verified two unique commits, one gate rejection, one lost reply, one idempotent
  replay, one conflict rejection, one malformed request, and zero partial commits per
  Docker run.
- Verified zero decision and residual mismatches against local authority.
- Verified every report disables multi-host, performance, consensus, and production-
  commit claims.
- Rebuilt `paper/main.pdf`; it remains 12 pages.

## Unresolved or Unverified

- Native Linux/x86-64 performance.
- CUDA or another discrete-accelerator complete-cost comparison.
- Two-physical-host network performance and variability.
- Coordinator/server crash, restart, partition, failover, and durable replay state.
- Replicated state, consensus, or production distributed commit.
- NUMA and multi-node complete-path scaling.
- Public immutable archive and persistent identifier.
- Independent-host reproduction.
- Final author, affiliation, funding, conflict, acknowledgment, and submission-mode
  metadata.

## Output Self-Check

- Scores and confidence are separated and calibrated to the strongest unresolved
  concern.
- Every score below 5 has a concrete deduction and repair condition.
- Docker namespaces are not mislabeled as physical hosts.
- Emulated x86-64 is not described as native performance.
- Functional TCP evidence is not described as network scaling, consensus, or
  production distributed commit.
- Existing negative results remain visible.
- Page recovery is not credited as scientific evidence.
- No acceptance probability, invented result, or fabricated metadata is present.
