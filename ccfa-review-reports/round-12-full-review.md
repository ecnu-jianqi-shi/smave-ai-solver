# CCF-A Full Review — Round 12

## Mode

Full scientific, writing, artifact, integrity, portability, and TPDS-fit re-review
after adding durable TCP authority crash/restart recovery.

## Venue and Assumptions

- Target: IEEE Transactions on Parallel and Distributed Systems regular paper.
- Review date: 2026-07-25.
- Materials: current 12-page PDF and LaTeX sources; durable TCP authority service,
  phased runners, verifier, final state snapshots, and local/Docker reports; all Round
  11 evidence; source and tests; Rounds 1--11.
- Deployment boundary: client and server occupy distinct Ubuntu Docker network
  namespaces on one physical Apple M4 host; x86-64 execution is emulated.
- Submission assumptions: author, affiliation, funding, conflict, acknowledgment, and
  review-mode metadata remain unresolved.
- Overall scale: CCFA 1--10; criterion scale: 1--5.

## Paper Summary

The paper frames repeated numerical acceleration as verification-aware expert
selection. Its typed transaction composes candidate generation, correction,
family-specific original-equation verification, publication, and mandatory classical
fallback under a reach-weighted complete-cost objective. An assumption-bounded
proposition gives the original-equation path commit authority under isolation,
immutable-problem, atomic-publication, and fallback assumptions.

The evidence package includes seven paired PDE workloads, all-family order controls,
two held-out operator families and a shared hybrid control, routing and hindsight
comparisons, device probes, thread/process gate scaling, complete-path and batch
scaling, ablations, negative transfer, Linux portability, explicit failures, and the
Round 11 serialized TCP transaction probe.

Round 12 replaces the TCP server's in-memory-only transaction table with an atomic
snapshot. Before sending a result, the server writes the full transaction table to a
temporary file, fsyncs it, renames it over the authoritative state, and fsyncs the
containing directory where supported. The faulted request commits under the fused
original-equation gate, persists, then terminates the server with exit code 86 before
any response reaches the client.

A fresh server process reads the same snapshot, recovers three transaction records,
and returns the persisted result for the repeated identifier without incrementing the
unique commit count. A changed payload under that identifier is rejected as a
conflict; one gate failure and one malformed request also remain non-committing. The
local, Ubuntu ARM64, and emulated Ubuntu x86-64 runs report two unique commits, one
restart, one durable replay, zero partial commits, and zero authority mismatches.

## Likely Stance and Calibrated Score

**Overall: 8/10 — accept. Scholarly confidence: 5/5.**

Round 12 resolves the strongest locally actionable weakness from Round 11. The
transaction result is no longer recoverable only while one process remains alive.
Persist-before-reply ordering, a real abrupt process exit, a fresh server instance,
and replay from recovered state provide concrete evidence for single-authority crash
recovery. This materially improves the protocol-to-implementation story.

The score remains 8 rather than rising to 9 because the recovery mechanism is one
single-server snapshot on one physical host. The experiment does not inject failure
during snapshot write/rename, corrupt durable state, partition endpoints, replicate
state, elect a leader, fail over to another machine, or time a complete solve across a
physical network. All authoritative performance remains on Apple M4; no native
Linux/x86-64 timing, discrete-accelerator complete-cost result, public immutable
archive, independent rerun, or final submission metadata exists. The external-
validity condition attached to 9 is therefore still unmet.

## Quantitative Scorecard

| Dimension | Score (1--5) | Confidence (1--5) | Evidence basis | Deduction / score-change condition |
|:---|:---:|:---:|:---|:---|
| Novelty | 4 | 5 | `paper/sections/05_verification_aware_fusion.tex:4`; `tests/gate_network_authority_evidence.cpp:320` | Verification-aware numerical transactions now include durable restart/replay. A 5 requires replicated or cross-host authority semantics beyond one persisted server. |
| Soundness | 4 | 5 | `paper/sections/06_experimental_methodology.tex:119`; `tests/gate_network_authority_evidence.cpp:458`; `tests/verify_gate_network_authority.cmake:1` | Persist-before-reply, exit 86, restart, record recovery, and authority equivalence are machine-gated. Crash-during-write, corruption, partition, replication, and failover remain untested. |
| Evidence | 4 | 5 | `paper/sections/07_evaluation.tex:235`; three evidence reports and three final state snapshots | Breadth, paired performance, negative controls, serialized faults, and durable replay are strong. A 5 still requires physical cross-host performance or independent reproduction. |
| Significance | 4 | 5 | `paper/sections/01_introduction.tex:45`; `paper/sections/09_discussion_limitations.tex:83` | Crash-recoverable commit authority strengthens TPDS relevance. Multi-node resource management and replicated complete-path impact remain absent. |
| Clarity | 5 | 5 | `paper/sections/06_experimental_methodology.tex:114`; `paper/sections/07_evaluation.tex:228`; `paper/sections/09_discussion_limitations.tex:40` | The paper separates durable functional evidence from performance, replication, consensus, and multi-host claims; no deduction. |
| Reproducibility | 4 | 5 | `tests/run_gate_network_authority.py`; `benchmark/run_gate_network_authority_docker.sh`; `paper/ARTIFACT_SNAPSHOT.md` | One-command crash/restart and two-architecture Docker reruns are strong. A 5 requires a public immutable release and independent operator. |
| Ethics / Limitations | 5 | 5 | `paper/sections/09_discussion_limitations.tex:20`; explicit evidence flags | Negative results and excluded claims remain prominent; no deduction. |

**Overall:** 8/10 | **Scholarly Confidence:** 5/5

**Recommendation:** accept

**Verdict:** A two-physical-host complete-path experiment with durable failover plus a
public independent rerun could move the paper to 9. Torn-state recovery failure,
duplicate publication after restart, or inability to reproduce current timing would
lower it to 7 or below.

## Top Strengths

1. The crash occurs after original-gate acceptance and durable snapshot publication
   but before response, targeting the ambiguous-outcome boundary directly.
2. Recovery uses a fresh server process and an inspectable state file rather than a
   surviving in-memory object.
3. Replay returns the original gate decision/residual while preserving the unique
   commit count; conflicting payload reuse is rejected.
4. ARM64 and emulated x86-64 Docker executions verify the same contract across
   separate network namespaces and process lifecycles.
5. The manuscript continues to label the mechanism as same-host, single-server,
   untimed functional evidence.

## Major / Fatal Concerns

1. **P0 — External performance authority.** All authoritative timing remains on one
   Apple M4. Native Linux/x86-64 and discrete-accelerator complete-cost replication
   remain the main path to 9.
2. **P0 — Public independent reproduction.** The artifact remains locally hashed but
   lacks an immutable public identifier and external rerun.
3. **P1 — Distributed durability remains incomplete.** One atomic snapshot does not
   test replication, partitions, leader failure, remote failover, or physical network
   behavior. The injected crash occurs after a complete snapshot, not during write or
   rename.
4. **P1 — Submission metadata.** Author, affiliation, funding, conflict,
   acknowledgment, and review-mode fields remain unresolved.

No new fatal numerical-soundness concern appears. The decisive risk remains external
validity rather than the local transaction implementation.

## Writing and Presentation Concerns

1. Method, result, and limitation paragraphs preserve a clear contract-evidence-
boundary sequence.
2. Crash recovery is described without the stronger terms replicated, consensus, or
production distributed commit.
3. The new detail remains within 12 pages through prose compression only; no margin,
font, or spacing change is present.
4. Compact tables retain benign underfull-box warnings, with no overfull box or build
failure.
5. The author placeholder still blocks submission readiness.

## Format and Venue Concerns

- IEEE Computer Society journal mode compiles to 12 pages.
- Durable transaction recovery improves TPDS fit, but the evidence remains a
  single-authority functional probe rather than a distributed performance study.
- Final metadata and venue submission checks remain mandatory.

## Multi-Reviewer Panel

### Reviewer R1 — Numerical Soundness

- Score tendency: 8/10; confidence 5/5.
- Main positive signal: recovered decisions and residuals match the independently
  recomputed local authority with no duplicate publication.
- Main negative signal: callback corruption and hardware faults remain outside the
  authority model.
- Score-change condition: native cross-host recovery with original-equation
  equivalence and state-corruption handling.

### Reviewer R2 — Parallel and Distributed Systems

- Score tendency: 8/10; confidence 5/5.
- Main positive signal: persist-before-reply, process crash, restart, durable replay,
  and conflict rejection now form an inspectable single-authority protocol.
- Main negative signal: no replication, partition, leader election, remote failover,
  physical network timing, or crash-during-snapshot probe exists.
- Score-change condition: two physical hosts with replicated/durable authority,
  failover faults, and complete-path timing.

### Reviewer R3 — Scientific Machine Learning

- Score tendency: 8/10; confidence 5/5.
- Main positive signal: recovery preserves the independent original-equation result
  rather than trusting a candidate-side receipt or cache.
- Main negative signal: this systems probe does not broaden learned-model families or
  add an external learned hybrid baseline.
- Score-change condition: cross-host learned/hybrid complete-path comparison or a
  stronger third-party learned baseline.

### Reviewer R4 — Artifact and Reproducibility

- Score tendency: 8/10; confidence 5/5.
- Main positive signal: implementation, phased runners, verifier, reports, and final
  state snapshots are included in the hash chain.
- Main negative signal: all executions remain author-operated and locally archived.
- Score-change condition: persistent public release plus independent rerun.

## Panel Synthesis and AC Meta-Review

- Agreement: Round 12 is a substantive implementation and evidence improvement, not a
  wording-only change.
- Disagreement: the distributed-systems reviewer values durable restart more than the
  SciML reviewer, but no reviewer equates it with replicated multi-node authority.
- Decisive accept axis: original-equation authority, complete-cost performance,
  negative results, and transaction failure evidence are tightly integrated.
- Decisive reject axis: a reviewer requiring native external, independent, or
  replicated distributed evidence can still downgrade the work.
- Unresolved evidence: native x86-64 performance, discrete accelerator, replicated
  cross-host failover/timing, public archive, independent rerun, and final metadata.
- Final calibrated stance: 8/10 accept. The in-memory-only recovery concern is closed;
  the external-validity threshold for 9 remains unmet.

## Concern-to-Action Table

| Priority | Concern | Evidence basis | Concrete action | Owner | Score condition |
| --- | --- | --- | --- | --- | --- |
| P0 | Native external performance | Apple M4 timing authority only | Repeat representative paired workloads on native Linux/x86-64 and a discrete accelerator | External hardware owner | Main path to 9 |
| P0 | Public independent reproduction | Local manifest and author-operated Docker runs | Publish immutable archive and obtain independent rerun | Artifact owner | Required for robust 9--10 |
| P1 | Replicated physical authority | One same-host atomic snapshot | Run two physical hosts; add replication/failover, partition, and crash-during-snapshot probes | Distributed systems owner | Significance/Evidence +1 |
| P1 | Submission metadata | Placeholder author source | Finalize authors, funding, conflicts, acknowledgment, and review mode | Authors | Readiness only |
| Closed locally | In-memory-only restart | Exit-86/restart reports and recovered snapshots | Preserve persist-before-reply contract, state snapshots, and verifiers | Artifact owner | No remaining single-process durability deduction |

## Recommended Next CCFA Owner

1. `ccf-paper-reviewer` remains the decision owner after external evidence changes.
2. A physical-host experiment owner should execute representative complete paths over
   two machines and test replicated or failover authority semantics.
3. An external hardware operator should run native x86-64 and discrete-accelerator
   complete-cost experiments.
4. The artifact owner should publish the frozen package and obtain an independent
   rerun.

## Checks Run

- Built and reproduced the local durable TCP authority target.
- Reproduced two-container Ubuntu ARM64 and emulated x86-64 crash/restart runs.
- Verified persist-before-reply ordering, exit code 86, two server starts, one restart,
  and recovery of three transaction records per run.
- Verified two unique commits, one gate rejection, one durable replay, one conflict,
  one malformed request, and zero partial commits per run.
- Verified zero decision and residual mismatches against local authority.
- Verified final state snapshots contain the expected transaction and fault counters.
- Verified no temporary snapshot file remains after successful runs.
- Rebuilt `paper/main.pdf`; it remains 12 pages.

## Unresolved or Unverified

- Native Linux/x86-64 performance.
- CUDA or another discrete-accelerator complete-cost comparison.
- Two-physical-host network performance and variability.
- Crash during snapshot write, fsync, rename, or directory publication.
- Snapshot corruption detection and rollback policy.
- Replication, partition, leader election, remote failover, or consensus.
- NUMA and multi-node complete-path scaling.
- Public immutable archive and persistent identifier.
- Independent-host reproduction.
- Final author, affiliation, funding, conflict, acknowledgment, and submission-mode
  metadata.

## Output Self-Check

- Scores and confidence remain separate and match the strongest unresolved concern.
- Every score below 5 includes a concrete deduction and repair condition.
- Single-server durable recovery is not mislabeled as replication or consensus.
- Docker namespaces are not mislabeled as physical hosts.
- Emulated x86-64 is not described as native performance.
- Existing negative results remain visible.
- Page compression is not credited as scientific evidence.
- No acceptance probability, invented result, or fabricated metadata is present.
