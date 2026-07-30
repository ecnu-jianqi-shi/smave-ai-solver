# CCF-A Full Review — Round 13

## Mode

Full scientific, writing, artifact, integrity, portability, and TPDS-fit re-review
after adding sealed generational TCP authority recovery.

## Venue and Assumptions

- Target: IEEE Transactions on Parallel and Distributed Systems regular paper.
- Review date: 2026-07-25.
- Materials: current 12-page PDF and LaTeX sources; sealed TCP authority service,
  phased runners, strict verifier, current/previous snapshots, and local/Docker
  reports; all Round 12 evidence; source and tests; Rounds 1--12.
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
serialized TCP transaction probe.

Round 13 replaces the single unsealed snapshot with SHA-256-sealed, monotonically
generated current and previous snapshots. The first injected exit remains code 86
after durable commit but before reply. A second request writes and fsyncs a temporary
next generation, exits with code 87 before publication, and is absent from the current
authority. Restart removes the orphan temporary and commits the retry once. The runner
then corrupts the current checksum while the service is stopped. A fresh server
rejects that generation, loads the verified previous generation, republishes recovered
state, and returns the persisted transaction on replay without a second unique commit.

Local, Ubuntu ARM64, and emulated Ubuntu x86-64 runs each report four server starts,
three restarts, four recovered transaction records, three unique commits, two durable
replays, one gate rejection, one conflict, one malformed request, one orphan cleanup,
one checksum rejection, one previous-generation recovery, zero partial commits, and
zero authority mismatches. Current and previous snapshots are independently checksum-
verified and no temporary remains.

## Likely Stance and Calibrated Score

**Overall: 8/10 — accept. Scholarly confidence: 5/5.**

Round 13 closes the strongest remaining local snapshot failure windows from Round 12.
The evidence now distinguishes a committed-but-unacknowledged request from an
unpublished temporary generation, tests cleanup and retry, and exercises verified
fallback after deliberate current-state corruption. These are substantive
implementation and machine-gated evidence improvements.

The score remains 8 rather than rising to 9 because the mechanism still has one
server authority and two files on one physical host. It does not replicate state,
lose both generations, partition endpoints, elect a leader, fail over remotely, or
time a complete solve across a physical network. All authoritative performance remains
on Apple M4; no native Linux/x86-64 timing, discrete-accelerator complete-cost result,
public immutable archive, independent rerun, or final submission metadata exists.
The external-validity condition attached to 9 remains unmet.

## Quantitative Scorecard

| Dimension | Score (1--5) | Confidence (1--5) | Evidence basis | Deduction / score-change condition |
|:---|:---:|:---:|:---|:---|
| Novelty | 4 | 5 | `paper/sections/05_verification_aware_fusion.tex:4`; `tests/gate_network_authority_evidence.cpp:290` | Verification-aware transactions now include sealed-generation crash/corruption recovery. A 5 requires replicated or physical cross-host authority semantics. |
| Soundness | 4 | 5 | `paper/sections/06_experimental_methodology.tex:119`; `tests/gate_network_authority_evidence.cpp:380`; `tests/verify_gate_network_authority.cmake:1` | Exit 86/87, orphan cleanup, checksum rejection, verified fallback, and replay equivalence are machine-gated. Loss of both generations, replication, partition, and failover remain untested. |
| Evidence | 4 | 5 | `paper/sections/07_evaluation.tex:235`; three reports and six sealed snapshots | Breadth, paired performance, negative controls, two fault windows, and corruption recovery are strong. A 5 requires physical cross-host performance or independent reproduction. |
| Significance | 4 | 5 | `paper/sections/01_introduction.tex:45`; `paper/sections/09_discussion_limitations.tex:35` | Recoverable authority improves TPDS relevance. Replicated resource management and multi-node complete-path impact remain absent. |
| Clarity | 5 | 5 | `paper/sections/06_experimental_methodology.tex:119`; `paper/sections/07_evaluation.tex:235`; `paper/sections/09_discussion_limitations.tex:35` | The paper separates functional recovery from performance, replication, consensus, and multi-host claims; no deduction. |
| Reproducibility | 4 | 5 | `tests/run_gate_network_authority.py`; `benchmark/run_gate_network_authority_docker.sh`; `paper/check_evidence.py` | One-command local and two-architecture reruns now verify both generations cryptographically. A 5 requires an immutable public release and independent operator. |
| Ethics / Limitations | 5 | 5 | `paper/sections/09_discussion_limitations.tex:17`; explicit evidence flags | Negative results and excluded claims remain prominent; no deduction. |

**Overall:** 8/10 | **Scholarly Confidence:** 5/5

**Recommendation:** accept

**Verdict:** A physical cross-host complete-path experiment with replicated authority
and failover plus a public independent rerun could move the paper to 9. Failure to
recover the tested prior generation, duplicate commit, checksum bypass, or inability
to reproduce current timing would lower it to 7 or below.

## Top Strengths

1. The two injected exits target distinct ambiguity boundaries: after durable commit
   before reply, and after temporary fsync before publication.
2. Recovery rejects a deliberately corrupted current generation and verifies the
   previous generation before loading it.
3. Orphan cleanup and retry preserve the distinction between unpublished in-memory
   work and unique durable commits.
4. Local, native ARM64 container, and emulated x86-64 container runs enforce identical
   counters, sealed snapshots, and original-gate equivalence.
5. The manuscript continues to label the mechanism as untimed, same-host,
   single-server functional evidence.

## Major / Fatal Concerns

1. **P0 — External performance authority.** All authoritative timing remains on one
   Apple M4. Native Linux/x86-64 and discrete-accelerator complete-cost replication
   remain the main path to 9.
2. **P0 — Public independent reproduction.** The artifact remains locally hashed and
   author-operated, without an immutable public identifier or external rerun.
3. **P1 — Distributed authority.** Two local generations improve single-server
   recovery but are not replication. Loss of both, partitions, election, remote
   failover, and consensus remain outside evidence.
4. **P1 — Submission readiness.** Author, affiliation, funding, conflict,
   acknowledgment, and submission-mode metadata remain placeholders or unresolved.

No new fatal scientific inconsistency was found. The decisive concerns are external
validity and submission completion, not the tested local recovery contract.

## Writing and Presentation Concerns

- The three recovery paragraphs are dense but precise and remain separated from
  performance claims.
- Prose compression restored 12 pages without margin, font, or spacing manipulation;
  the compressed limitations retain every material negative boundary.
- Terminology is consistent: current generation, previous generation, temporary
  publication, unique commit, replay, and same-host authority are not conflated.

## Format and Venue Concerns

- The IEEE-style PDF builds to 12 pages with no undefined citation/reference or
  overfull-box failure.
- TPDS systems fit is stronger through operational failure evidence, but one physical
  host and no replicated authority remain the main venue-risk axis.
- Final author and compliance metadata still require submission-time completion.

## Multi-Reviewer Panel

### Reviewer R1 — Numerical Methods and Scientific ML

- Score tendency: 8/10; confidence 5/5.
- Main positive signal: original-equation authority, complete-cost evaluation, paired
  PDE evidence, shared hybrid controls, and negative transfer remain intact.
- Main negative signal: learned/operator evidence is still workload-qualified and
  externally limited.
- Score-change condition: independent native-hardware complete-cost reproduction.

### Reviewer R2 — Parallel and Distributed Systems

- Score tendency: 8/10; confidence 5/5.
- Main positive signal: separate crash windows, checksummed generations, corruption
  rejection, verified rollback, and idempotent replay are concrete systems evidence.
- Main negative signal: this remains one authority on one physical host, without
  replication, partitions, election, or remote failover.
- Score-change condition: replicated two-host authority with failure and partition
  experiments plus complete-path timing.

### Reviewer R3 — Experimental Methodology

- Score tendency: 8/10; confidence 5/5.
- Main positive signal: exact fault exits and final counters are reproduced on three
  execution configurations and checked against sealed state.
- Main negative signal: Docker x86-64 is emulated and all performance authority is
  still one Apple M4.
- Score-change condition: native cross-platform paired reruns and external operator.

### Reviewer R4 — Artifact and Reproducibility

- Score tendency: 8/10; confidence 5/5.
- Main positive signal: source, runners, verifier, three reports, and six sealed
  snapshots enter the artifact hash chain.
- Main negative signal: no public immutable archive or independent rerun exists.
- Score-change condition: persistent release plus independent verification.

## Panel Synthesis and AC Meta-Review

- Agreement: Round 13 is a substantive implementation/evidence improvement and closes
  torn-temporary and corrupted-current concerns for the tested single-server design.
- Disagreement: the distributed-systems reviewer assigns more value to generational
  recovery than the SciML reviewer, but neither treats it as replication.
- Decisive accept axis: original-equation authority, complete-cost results, negative
  evidence, and fault recovery form a coherent inspectable package.
- Decisive reject axis: a reviewer requiring native external, independent, or
  replicated multi-host evidence can still downgrade the paper.
- Unresolved evidence: native x86-64 performance, discrete accelerator, physical
  cross-host replication/failover/timing, public archive, independent rerun, and final
  metadata.
- Final calibrated stance: 8/10 accept. Local torn/corrupt snapshot recovery is
  closed; the external-validity threshold for 9 remains unmet.

## Concern-to-Action Table

| Priority | Concern | Evidence basis | Concrete action | Owner | Score condition |
| --- | --- | --- | --- | --- | --- |
| P0 | Native external performance | Apple M4 timing authority only | Repeat representative paired workloads on native Linux/x86-64 and a discrete accelerator | External hardware owner | Main path to 9 |
| P0 | Public independent reproduction | Local manifest and author-operated runs | Publish immutable archive and obtain an independent rerun | Artifact owner | Required for robust 9--10 |
| P1 | Replicated physical authority | One server and two local generations | Run two physical hosts with replication, failover, partition, and dual-generation-loss probes | Distributed systems owner | Significance/Evidence +1 |
| P1 | Submission metadata | Placeholder author source | Finalize authors, funding, conflicts, acknowledgment, and review mode | Authors | Readiness only |
| Closed locally | Torn/corrupt current snapshot | Exit-87 cleanup and checksum-fallback reports | Preserve sealed generations, fault phases, and verifiers | Artifact owner | No remaining tested-local deduction |

## Recommended Next CCFA Owner

1. `ccf-paper-reviewer` remains the decision owner after external evidence changes.
2. A physical-host experiment owner should implement replicated/failover authority and
   representative complete-path runs over two machines.
3. An external hardware operator should run native x86-64 and discrete-accelerator
   complete-cost experiments.
4. The artifact owner should publish the frozen package and obtain an independent
   rerun.

## Checks Run

- Built and reproduced the local sealed-generation TCP authority target.
- Reproduced two-container Ubuntu ARM64 and emulated x86-64 recovery runs.
- Verified exit codes 86 and 87, four starts, three restarts, orphan cleanup, deliberate
  checksum corruption, previous-generation fallback, and four recovered records.
- Verified three unique commits, one gate rejection, two idempotent replays, one
  conflict, one malformed request, and zero partial commits per run.
- Verified zero decision and residual mismatches against original-equation authority.
- Verified current and previous SHA-256 seals, generation relation, final counters,
  and absence of temporary snapshots.
- Built the full Release tree and passed 29/29 CTests.
- Rebuilt `paper/main.pdf`; it remains 12 pages without layout hacks.

## Unresolved or Unverified

- Native Linux/x86-64 performance.
- CUDA or another discrete-accelerator complete-cost comparison.
- Two-physical-host network performance and variability.
- Loss or corruption of both local generations.
- Replication, partition, leader election, remote failover, or consensus.
- NUMA and multi-node complete-path scaling.
- Public immutable archive and persistent identifier.
- Independent-host reproduction.
- Final author, affiliation, funding, conflict, acknowledgment, and submission-mode
  metadata.

## Output Self-Check

- Scores and confidence remain separate and match the strongest unresolved concern.
- Every score below 5 includes a concrete deduction and repair condition.
- Generational single-server recovery is not mislabeled as replication or consensus.
- Docker namespaces are not mislabeled as physical hosts.
- Emulated x86-64 is not described as native performance.
- Existing negative results and complete-cost caveats remain visible.
- Prose compression is not credited as scientific evidence.
- No acceptance probability, invented result, or fabricated metadata is present.
