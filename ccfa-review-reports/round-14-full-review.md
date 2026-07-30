# CCF-A Full Review — Round 14

## Mode

Full scientific, writing, artifact, integrity, portability, and TPDS-fit re-review
after adding highest-valid-generation selection and dual-corruption fail-closed startup.

## Venue and Assumptions

- Target: IEEE Transactions on Parallel and Distributed Systems regular paper.
- Review date: 2026-07-25.
- Materials: current 12-page PDF and LaTeX sources; TCP authority implementation,
  phased local/Docker runners, CMake and Python verifiers, valid/corrupt snapshots,
  durable recovery-failure records, all prior evidence, and Rounds 1--13.
- Deployment boundary: client and server occupy distinct Ubuntu Docker network
  namespaces on one physical Apple M4 host; x86-64 execution is emulated.
- Integrity boundary: SHA-256 is unkeyed. It detects the injected corruption but does
  not authenticate state or detect a coherent rewrite/rollback of both files.
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
scaling, ablations, negative transfer, Linux portability, explicit failures, and
serialized TCP authority transactions.

Round 14 closes two local recovery hazards left by Round 13. First, the runner creates
a valid but stale current snapshot while retaining a newer valid previous snapshot.
Startup parses both, selects the higher generation, records stale-current rejection,
republishes recovered state, and returns the persisted transaction without a duplicate
unique commit. Second, the runner copies the final state into an isolated directory,
corrupts both generation checksums, and starts the same server. State loading occurs
before socket creation; with no valid generation the server writes a durable failure
record and exits 88 rather than listening or initializing an empty transaction table.

Local, Ubuntu ARM64, and emulated Ubuntu x86-64 runs each report five healthy starts,
four healthy restarts, one blocked startup attempt, four recovered transaction records,
three unique commits, three durable replays, one gate rejection, one conflict, one
malformed request, one orphan cleanup, one corrupted-current fallback, one
valid-stale-current rejection, two previous-generation recoveries, zero partial
commits, and zero authority mismatches. Each failed-start artifact retains both invalid
snapshots and a pre-listen exit-88 record.

## Likely Stance and Calibrated Score

**Overall: 8/10 — accept. Scholarly confidence: 5/5.**

Round 14 is a substantive soundness and artifact improvement. It removes a silent
rollback hazard when the two local files disagree by generation and proves that loss
of both valid copies fails closed before service exposure. The new manuscript wording
also corrects a security-interpretation risk by distinguishing checksumming from
authentication.

The score remains 8 rather than rising to 9. Fail-closed behavior is safety, not
availability: both-copy corruption still leaves no recoverable authority. Pairwise
comparison cannot detect coherent rollback of both files without an authenticated
external monotonic anchor. The mechanism remains one server on one physical host,
without replication, partitions, election, remote failover, or consensus. All
authoritative performance remains on Apple M4; no native Linux/x86-64 timing,
discrete-accelerator complete-cost result, public immutable archive, independent
rerun, or final submission metadata exists. The threshold for 9 remains external.

## Quantitative Scorecard

| Dimension | Score (1--5) | Confidence (1--5) | Evidence basis | Deduction / score-change condition |
|:---|:---:|:---:|:---|:---|
| Novelty | 4 | 5 | `paper/sections/05_verification_aware_fusion.tex:4`; `tests/gate_network_authority_evidence.cpp:551` | Verification-aware transactions now include generation arbitration and pre-listen fail-closed startup. A 5 requires authenticated replicated or physical cross-host authority. |
| Soundness | 4 | 5 | `paper/sections/06_experimental_methodology.tex:119`; `tests/gate_network_authority_evidence.cpp:390`; `tests/verify_gate_network_authority.cmake:1` | Higher-generation selection and dual-invalid exit 88 are machine-gated. Coherent rollback, replicated recovery, partition, and failover remain untested. |
| Evidence | 4 | 5 | `paper/sections/07_evaluation.tex:235`; three main reports, six valid snapshots, six corrupt snapshots, and three failure records | Local failure evidence is unusually inspectable, but a 5 requires physical cross-host performance or independent reproduction. |
| Significance | 4 | 5 | `paper/sections/01_introduction.tex:45`; `paper/sections/09_discussion_limitations.tex:32` | Fail-closed authority strengthens TPDS relevance. Multi-node resource management and recovery availability remain absent. |
| Clarity | 5 | 5 | `paper/sections/06_experimental_methodology.tex:119`; `paper/sections/07_evaluation.tex:235`; `paper/sections/09_discussion_limitations.tex:34` | Safety, availability, authentication, performance, and replication claims are now explicitly separated; no deduction. |
| Reproducibility | 4 | 5 | `tests/run_gate_network_authority.py`; `benchmark/run_gate_network_authority_docker.sh`; `paper/check_evidence.py` | Local and two-architecture runs verify valid and invalid generations plus exit 88. A 5 requires an immutable public release and independent operator. |
| Ethics / Limitations | 5 | 5 | `paper/sections/09_discussion_limitations.tex:17`; explicit unkeyed-checksum and excluded-claim wording | Negative results and residual risks remain prominent; no deduction. |

**Overall:** 8/10 | **Scholarly Confidence:** 5/5

**Recommendation:** accept

**Verdict:** Authenticated replicated authority across physical hosts, complete-path
network timing, and a public independent rerun could move the paper to 9. Any silent
blank-state startup, acceptance of a lower valid generation, duplicate unique commit,
or checksum bypass would lower it to 7 or below.

## Top Strengths

1. Startup compares both valid local generations rather than trusting the filename
   designated as current.
2. Dual-invalid state blocks before socket creation and preserves an inspectable
   durable failure record instead of silently resetting authority.
3. The failed-start test uses copied final state, so destructive fault injection does
   not erase the valid evidence snapshots.
4. Local, native ARM64 container, and emulated x86-64 container runs enforce identical
   counters, generation relations, invalid checksums, and exit behavior.
5. The manuscript explicitly states that unkeyed SHA-256 is integrity checking, not
   authentication or coherent rollback protection.

## Major / Fatal Concerns

1. **P0 — External performance authority.** All authoritative timing remains on one
   Apple M4. Native Linux/x86-64 and discrete-accelerator complete-cost replication
   remain the main path to 9.
2. **P0 — Public independent reproduction.** The artifact remains locally hashed and
   author-operated, without an immutable public identifier or external rerun.
3. **P1 — Authenticated distributed authority.** Pairwise local generations cannot
   detect coherent two-file rollback and cannot recover if both are lost. Replication,
   external monotonic authority, partition behavior, election, and remote failover are
   outside evidence.
4. **P1 — Submission readiness.** Author, affiliation, funding, conflict,
   acknowledgment, and submission-mode metadata remain placeholders or unresolved.

No new fatal scientific inconsistency was found. The strongest remaining concerns are
external validity, distributed availability/authentication, and submission completion.

## Writing and Presentation Concerns

- The recovery paragraph is dense but decision-relevant and distinguishes a healthy
  server start from a blocked startup attempt.
- The phrase “SHA-256-checksummed” is safer than treating an unkeyed digest as a seal
  against an adversary.
- The limitations paragraph now states both residual risks: no availability after
  dual loss and no coherent rollback detection.
- Prose-only integration retains the 12-page limit without layout manipulation.

## Format and Venue Concerns

- The IEEE-style PDF builds to 12 pages with no undefined citation/reference or
  overfull-box failure.
- TPDS systems fit improves through explicit startup arbitration and failure-state
  evidence, but physical distributed execution remains absent.
- Final author and compliance metadata still require submission-time completion.

## Multi-Reviewer Panel

### Reviewer R1 — Numerical Methods and Scientific ML

- Score tendency: 8/10; confidence 5/5.
- Main positive signal: original-equation authority, complete-cost evaluation, paired
  PDE evidence, shared hybrid controls, and negative transfer remain intact.
- Main negative signal: learned/operator evidence remains workload-qualified and
  externally limited.
- Score-change condition: independent native-hardware complete-cost reproduction.

### Reviewer R2 — Parallel and Distributed Systems

- Score tendency: 8/10; confidence 5/5.
- Main positive signal: generation arbitration and pre-listen fail-closed startup are
  concrete operational semantics, not prose-only claims.
- Main negative signal: local snapshots provide neither authenticated monotonicity nor
  replicated availability.
- Score-change condition: authenticated two-host replication with failover, partition,
  and complete-path timing experiments.

### Reviewer R3 — Experimental Methodology

- Score tendency: 8/10; confidence 5/5.
- Main positive signal: the same valid/stale/dual-invalid state sequence is reproduced
  on three execution configurations with exact artifact checks.
- Main negative signal: Docker x86-64 is emulated and all performance authority is one
  Apple M4.
- Score-change condition: native cross-platform paired reruns and external operator.

### Reviewer R4 — Artifact and Reproducibility

- Score tendency: 8/10; confidence 5/5.
- Main positive signal: valid snapshots, intentionally invalid snapshots, failure
  records, runners, and verifiers enter the artifact hash chain.
- Main negative signal: no public immutable archive or independent rerun exists.
- Score-change condition: persistent release plus independent verification.

## Panel Synthesis and AC Meta-Review

- Agreement: Round 14 closes the local stale-file and dual-invalid silent-start hazards
  for the tested single-server implementation.
- Disagreement: the systems reviewer values fail-closed startup more than the SciML
  reviewer, but no reviewer treats safety-only local files as distributed recovery.
- Decisive accept axis: original-equation authority, complete-cost results, negative
  evidence, and machine-gated fault handling form a coherent package.
- Decisive reject axis: a reviewer requiring native external, independent,
  authenticated, or replicated multi-host evidence can still downgrade the paper.
- Unresolved evidence: native x86-64 performance, discrete accelerator, authenticated
  monotonic authority, physical replication/failover/timing, public archive,
  independent rerun, and final metadata.
- Final calibrated stance: 8/10 accept. The locally actionable silent-start hazards are
  closed; the external/distributed threshold for 9 remains unmet.

## Concern-to-Action Table

| Priority | Concern | Evidence basis | Concrete action | Owner | Score condition |
| --- | --- | --- | --- | --- | --- |
| P0 | Native external performance | Apple M4 timing authority only | Repeat representative paired workloads on native Linux/x86-64 and a discrete accelerator | External hardware owner | Main path to 9 |
| P0 | Public independent reproduction | Local manifest and author-operated runs | Publish immutable archive and obtain an independent rerun | Artifact owner | Required for robust 9--10 |
| P1 | Authenticated replicated authority | One server, two unkeyed local files | Add authenticated external monotonic state and two-host replication; test failover and partitions | Distributed systems owner | Significance/Evidence +1 |
| P1 | Submission metadata | Placeholder author source | Finalize authors, funding, conflicts, acknowledgment, and review mode | Authors | Readiness only |
| Closed locally | Stale/dual-invalid local state | Higher-generation selection and exit-88 records | Preserve fault phases, invalid snapshots, and verifiers | Artifact owner | No remaining tested-local deduction |

## Recommended Next CCFA Owner

1. `ccf-paper-reviewer` remains the decision owner after external evidence changes.
2. A distributed-systems owner should provide authenticated monotonic state,
   replication, failover, and partition experiments on physical hosts.
3. An external hardware operator should run native x86-64 and discrete-accelerator
   complete-cost experiments.
4. The artifact owner should publish the frozen package and obtain an independent
   rerun.

## Checks Run

- Reproduced the local highest-generation and dual-corruption authority target.
- Reproduced two-container Ubuntu ARM64 and emulated x86-64 runs.
- Verified exits 86, 87, and 88; five healthy starts; four healthy restarts; and one
  blocked pre-listen startup attempt per run.
- Verified one stale-current rejection, one corrupted-current fallback, two
  previous-generation recoveries, four recovered records, and three idempotent replays.
- Verified three unique commits, one gate rejection, one conflict, one malformed
  request, zero partial commits, and zero authority mismatches per run.
- Verified six final valid snapshot checksums, six preserved invalid checksums, three
  durable failure records, and absence of temporary snapshots.
- Built the full Release tree and passed 29/29 CTests.
- Rebuilt `paper/main.pdf`; it remains 12 pages without layout hacks.

## Unresolved or Unverified

- Native Linux/x86-64 performance.
- CUDA or another discrete-accelerator complete-cost comparison.
- Authenticated external monotonic generation authority.
- Coherent malicious rollback of both local files.
- Recovery availability after loss of both local generations.
- Replication, partition, leader election, remote failover, or consensus.
- Two-physical-host network performance and variability.
- NUMA and multi-node complete-path scaling.
- Public immutable archive and persistent identifier.
- Independent-host reproduction.
- Final author, affiliation, funding, conflict, acknowledgment, and submission-mode
  metadata.

## Output Self-Check

- Scores and confidence remain separate and match the strongest unresolved concern.
- Every score below 5 includes a concrete deduction and repair condition.
- Fail-closed local safety is not mislabeled as replicated recovery availability.
- Unkeyed checksums are not mislabeled as authentication.
- Docker namespaces are not mislabeled as physical hosts.
- Emulated x86-64 is not described as native performance.
- Existing negative results and complete-cost caveats remain visible.
- No acceptance probability, invented result, or fabricated metadata is present.
