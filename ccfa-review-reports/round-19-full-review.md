# CCF-A Full Review — Round 19

## 1. Report Metadata

- **Review date:** 2026-07-25.
- **Target venue/year/track:** IEEE Transactions on Parallel and Distributed Systems,
  regular paper, 2026 submission cycle.
- **Paper title:** *Verification-Aware Expert Fusion with Parallel Original-Equation
  Gates for Repeated Numerical Solves*.
- **Input materials reviewed:** current 12-page PDF and LaTeX sources; implementation
  and 29-test Release suite; local, Ubuntu ARM64, and emulated x86-64 authority
  evidence; authenticated hash-chained v4 state snapshots; current/previous monotonic
  witnesses; five fail-closed fixture directories; independent Python verifier;
  artifact manifest and bundle tooling; data locks; and Rounds 1--18.
- **Search basis:** no new public novelty search. Round 19 changes a local durability
  and rollback-detection property, not the paper's novelty or related-work position.
- **Report file:** `ccfa-review-reports/round-19-full-review.md`.
- **Reviewer mode:** full scientific, writing, format, artifact, integrity, and
  reproducibility review.

## 2. Desk Rejection Assessment

- **Paper length — pass.** The manuscript rebuilds at exactly 12 pages.
- **Topic compatibility — pass.** Parallel verification, heterogeneous numerical
  execution, authority boundaries, and recovery mechanisms fit TPDS.
- **Minimum quality — pass.** The paper combines a formal commit-authority argument,
  implemented mechanisms, baselines, repeated experiments, ablations, failure cases,
  negative results, and explicit limitations.
- **Policy/anonymity/compliance — uncertain administratively.** Author, affiliation,
  funding, conflict, acknowledgment, review-mode, and final artifact metadata remain
  incomplete.
- **Prompt injection/hidden manipulation — pass locally.** No reviewer-directed hidden
  instruction was found in the inspected manuscript or source paths.
- **Ethics and reviewability — pass with stated limits.** Dataset licensing, finite-
  sample safety limits, emulation limits, fixture-key limits, and negative results are
  visible.

**Desk rejection risk:** low scientifically; medium administratively until metadata and
final TPDS policy checks are completed.

## 3. Paper Summary and Contribution Map

The paper presents verification-aware expert fusion for repeated numerical solves.
Candidate generation, optional correction, family-specific original-equation gates,
publication, and fallback form a typed transaction optimized by verified complete cost.
Under explicit isolation, immutability, atomic-publication, and fallback assumptions,
only a prior committed state or a gate-accepted state may become caller-visible.

The contribution map is coherent:

1. A reach-weighted complete-cost objective for heterogeneous candidate cascades.
2. A role-constrained candidate--corrector--gate--fallback transaction and bounded
   commit-authority proposition.
3. A typed C/C++ numerical-service implementation spanning multiple equation families.
4. A broad evaluation that preserves unavailable comparisons, regressions, and other
   negative results rather than presenting only successful workloads.
5. An inspectable authority and reproduction artifact with explicit same-host,
   emulation, key-custody, archival, and independence boundaries.

### Round 19 Evidence Change

Round 18 rejected valid-HMAC divergent bodies at the same highest generation, but a
coherent rollback of every authenticated state copy remained undetectable.

Round 19 upgrades the state envelope to `SMAVE_GATE_TXN_STATE 4`. Each authenticated
body records `previous_body_sha256`, creating a checked one-generation hash chain.
A separately keyed witness envelope records the generation and authenticated state-
body SHA-256 under its own SHA-256 and HMAC-SHA-256. Current and previous witnesses are
durably published beside, but logically separate from, the primary/mirror stores.

Startup now requires a valid witness after authenticated state selection. It rejects:

1. a wrong state-authentication key;
2. four corrupt authenticated-state generations;
3. a valid-HMAC same-generation body fork;
4. every authenticated state copy below a preserved witness generation; and
5. a witness that does not authenticate under the configured witness key.

All five failure fixtures record a pre-listen failure and exit 88 without blank-state
reinitialization. Local, Ubuntu ARM64, and emulated x86-64 campaigns all report
`snapshots=4 failures=5`. The independent Python verifier recomputes both HMAC domains,
checks primary/mirror current and previous state equality, validates both parent links,
binds current/previous witnesses to their state generations and body digests, and
validates all five negative fixtures.

The new evidence proves only preserved-witness rollback detection. The witness shares
the same process, filesystem, host, operator, and fixture-key environment as the state
stores. Joint rollback of state and witness remains undetected. The evidence correctly
records `external_monotonic_anchor=0`,
`joint_state_and_witness_rollback_detection=0`, and
`witness_independent_failure_domain=0`.

## 4. Search and Related-Work Basis

- **Queries used:** none in Round 19.
- **Sources searched:** none newly; the manuscript references and prior-round public
  positioning were retained.
- **Closest works retained in the paper:** classical algorithm selection, neural
  operators, convergence-gated learned PDE solvers, hybrid numerical correction,
  selective prediction, and simplex-style safety architectures.
- **Unverified related-work risk:** a new 2026 literature sweep was not performed in
  this round; this does not affect the assessment of the local witness mechanism.
- **Source-quality screening:** unchanged from prior rounds; no new citation or novelty
  claim was introduced by Round 19.

## 5. Expected Review Outcome

- **Expected outcome:** **8/10 — accept**.
- **Main accept signal:** a coherent verification-aware systems contribution supported
  by formal assumptions, broad inspectable experiments, explicit negative results, and
  increasingly strong fail-closed local authority evidence.
- **Main reject signal:** all authoritative performance remains single-host, while the
  distributed-authority evidence lacks physical replicas, independent failure domains,
  partitions, quorum, remote failover, and external monotonic freshness.
- **Confidence:** **5/5**, because the source, fault fixtures, machine records,
  independent verifier, manuscript, and tests are directly inspectable.

Round 19 closes the narrower case in which all state copies roll back while the witness
is preserved. It does not change the decision-level ceiling because the witness itself
is same-host and jointly rollbackable. Raising the score for this local integrity
refinement would conflate stronger artifact engineering with external distributed-
systems evidence.

## 6. Strengths and Weaknesses

### Strengths

1. The witness is independently keyed and binds a generation to the authenticated
   state-body digest rather than trusting a generation number alone.
2. The state parent digest makes the authenticated one-step history explicit and is
   independently checked in Python.
3. The rollback fixture preserves a newer valid witness while replacing all four state
   copies with an older authenticated generation; rejection therefore exercises the
   intended monotonic condition rather than ordinary corruption.
4. Wrong state-key, wrong witness-key, corruption, fork, and rollback failures are
   separated, durable, pre-listen, and non-reinitializing.
5. The same evidence schema and boundary fields match across local, ARM64-container,
   and emulated x86-64 execution.
6. The manuscript explicitly distinguishes preserved-witness rollback detection from
   joint state-and-witness rollback protection, physical replication, and consensus.
7. Existing strengths remain intact: complete-cost measurement, paired statistics,
   original-equation authority, broad interface coverage, negative results, data locks,
   and deterministic author-operated reproduction.

### Major Weaknesses

1. **No independent rollback authority.** The witness is a same-host file and therefore
   can be reverted with the state. This prevents a production freshness claim.
   **Required fix:** use a physically independent monotonic service, append-only
   transparency log, trusted counter, or replicated quorum, then inject coordinated and
   uncoordinated rollback faults.
2. **No physical replication or failover protocol.** Primary, mirror, and witness remain
   one server's files. **Required fix:** deploy cross-host replicas with independent
   storage and keys, then test partitions, leader loss, split brain, quorum behavior,
   recovery, and remote failover.
3. **No native external performance evidence.** Docker portability remains correctness-
   only on the Apple M4 host. **Required fix:** repeat complete-cost campaigns on native
   x86-64/Linux and representative discrete accelerators or clusters.
4. **No independent public reproduction.** The deterministic bundle is author-operated
   and not deposited under a persistent identifier. **Required fix:** publish an
   immutable archive and obtain an independent clean rerun, including the large-data
   acquisition path.
5. **No production key lifecycle.** Test keys do not establish provisioning, isolation,
   rotation, revocation, compromise recovery, KMS, or HSM use. **Required fix:** specify
   and fault-test a production key lifecycle.

### Moderate Weaknesses

1. The implemented one-generation witness-lag recovery path is not directly fault-
   tested. It is correctly excluded from the manuscript evidence, but should be tested
   before being relied on operationally.
2. The authority paragraph remains dense because it compresses mechanism, five fault
   classes, and scope boundaries into the fixed page budget.
3. Final submission metadata and policy/anonymity checks remain incomplete.

## 7. Potentially Missing Related Work

No new missing work is identified from the Round 19 mechanism change. The following
positioning risks remain unchanged:

- **External monotonic counters and transparency logs — unverified this round.** These
  are relevant to production rollback freshness, but the paper does not claim such a
  mechanism as a contribution.
- **Replicated state-machine and consensus recovery — unverified this round.** These are
  relevant only if the paper broadens the same-host authority probe into a distributed-
  commit claim; it currently does not.
- **Production key-management systems — unverified this round.** Comparison becomes
  necessary only if key custody or operational security becomes a paper claim.

No score deduction is added because the manuscript preserves the narrower scope.

## 8. Claim-Evidence Audit

| Claim | Where stated | Evidence provided | Strength | Reviewer deduction | Required fix |
| --- | --- | --- | --- | --- | --- |
| Original-equation gates remain commit authority | Abstract, method, proposition, evaluation | Formal assumptions plus family-specific probes and fallback tests | Strong within assumptions | Does not verify arbitrary faulty callbacks or hardware | Maintain bounded theorem language |
| Highest authenticated generation is selected | Methodology, evaluation, authority evidence | Four valid primary/mirror current/previous snapshots | Strong local functional evidence | One process/filesystem/host | Physical replicas for distributed claim |
| Same-generation authenticated forks fail closed | Evaluation and limitations | Valid-HMAC divergent highest-generation copies; exit 88 before listen | Strong local fork evidence | Not a consensus or split-brain protocol | Cross-host partition/quorum campaign |
| Preserved-witness all-state rollback fails closed | Methodology, evaluation, limitations | All four authenticated state copies below a newer valid witness; exit 88 | Strong for preserved same-host witness | Joint state-and-witness rollback remains possible | Independent external monotonic anchor |
| Wrong state and witness keys fail closed | Methodology and artifact ledger | Separate wrong-key fixtures and failure records | Strong fixture-key evidence | No production custody or lifecycle | KMS/HSM-backed lifecycle tests |
| Primary-pair loss recovers from mirror | Evaluation and artifact evidence | Both primary generations removed; replay recovers from mirror | Strong same-host availability evidence | Mirror is not an independent replica | Physical storage/host separation |
| Complete-path speedups hold | Evaluation figures/tables and pinned reports | Paired runs, bootstrap intervals, failures retained | Strong for measured host/workloads | No native external performance | External complete-cost campaigns |
| Artifact is reproducible | Artifact documentation, manifest, bundle tools | Deterministic bundle mechanism, 29/29 tests, data locks | Strong author-operated evidence | No persistent public archive or independent operator | Public deposit and independent rerun |

No current claim requires treating Docker namespaces as physical hosts, a file witness
as an external monotonic anchor, HMAC as consensus, emulation as native performance, or
same-host mirrors as independent replicas.

## 9. Experiment, Benchmark, and Reproducibility Audit

- **Baselines:** workload-specific classical, learned, routing, shared-control, and
  fallback baselines remain explicit; no baseline was removed in Round 19.
- **Ablations:** candidate, correction, gate, fallback, fusion, certificate reuse,
  thread/process scaling, and order sensitivity remain inspectable.
- **Datasets and benchmarks:** PDEBench-derived, SuiteSparse, PETSc TS, Modelica, and
  operator families retain explicit coverage and failure accounting.
- **Metrics:** complete verified runtime, gate-only throughput, decisions, residuals,
  commits, replays, failures, and confidence intervals remain separated.
- **Statistical rigor:** authoritative positive performance claims retain paired repeats
  and fixed-seed bootstrap intervals; finite safety trials are not inflated by timing
  repetitions.
- **Robustness and failure cases:** Round 19 expands the startup campaign from three to
  five authenticated failure fixtures and adds an independently checked rollback case.
- **Implementation details:** state/witness formats, keys, parent digest, exit codes,
  persistence order, and verifier contracts are inspectable.
- **Artifacts:** local, ARM64, and emulated x86-64 records agree; 29/29 Release CTests,
  paper evidence checks, and the 12-page manuscript build pass.
- **Remaining artifact task at review time:** regenerate and clean-tree verify the
  deterministic bundle so the new v4/witness records and Round 19 report are frozen.

## 10. Multi-Reviewer Panel

### Reviewer 1 — Method and Soundness

- **Likely score / confidence:** 8/10, 5/5.
- **Main positive signal:** the monotonic condition is tied to an independently keyed
  authenticated state digest and enforced before listen.
- **Main negative signal:** the witness shares the state failure domain.
- **Evidence basis:** v4 snapshots, witness envelopes, rollback fixture, C++ recovery
  path, and independent Python checks.
- **Score-change condition:** independent monotonic authority with coordinated rollback
  faults.

### Reviewer 2 — Distributed Systems

- **Likely score / confidence:** 8/10, 5/5.
- **Main positive signal:** local recovery semantics are explicit and fail closed.
- **Main negative signal:** no physical replicas, quorum, partition semantics, election,
  or remote failover.
- **Evidence basis:** all evidence records retain `same_physical_host=1`, `multi_host=0`,
  and `consensus_protocol=0`.
- **Score-change condition:** physical cross-host replication and partition/failover
  campaigns.

### Reviewer 3 — Evidence and Experiments

- **Likely score / confidence:** 8/10, 5/5.
- **Main positive signal:** the rollback fixture is authenticated, platform-repeated,
  and independently parsed rather than inferred from labels.
- **Main negative signal:** authoritative performance is still one-host.
- **Evidence basis:** three matching authority records, paired benchmark reports, and
  29/29 CTests.
- **Score-change condition:** native external complete-cost repetitions.

### Reviewer 4 — Novelty and Positioning

- **Likely score / confidence:** 8/10, 4/5.
- **Main positive signal:** verification-aware complete-cost expert fusion remains a
  coherent systems contribution.
- **Main negative signal:** Round 19 is artifact soundness engineering, not a new
  algorithmic or distributed-systems mechanism.
- **Evidence basis:** contribution framing, related work, and unchanged central claims.
- **Score-change condition:** a differentiated externally validated authority or
  scheduling mechanism; wording alone does not qualify.

### Reviewer 5 — Writing and Clarity

- **Likely score / confidence:** 8/10, 5/5.
- **Main positive signal:** the manuscript explicitly states what the preserved witness
  detects and what joint rollback defeats.
- **Main negative signal:** the authority result remains compressed and information-
  dense.
- **Evidence basis:** methodology, evaluation, limitations, claim ledger, and exact
  12-page rebuild.
- **Score-change condition:** improve readability only if space can be recovered without
  deleting evidence or limitations; prose alone does not raise the scientific score.

### Reviewer 6 — Ethics, Artifact, and Reproducibility

- **Likely score / confidence:** 8/10, 5/5.
- **Main positive signal:** licensing, fixture-key status, same-host boundaries, negative
  results, and finite-sample limits remain explicit.
- **Main negative signal:** no public immutable deposit or independent operator.
- **Evidence basis:** data locks, artifact snapshot, manifest, bundle tooling, and review
  history.
- **Score-change condition:** persistent public archive and independent clean rerun.

### Reviewer 7 — AC / Meta-Review

- **Likely score / confidence:** 8/10, 5/5.
- **Main positive signal:** Round 19 closes the exact Round 18 rollback gap it can close
  locally, without claim inflation.
- **Main negative signal:** none of the prior decision-level external blockers changes.
- **Evidence basis:** matching platform fields and manuscript boundaries.
- **Score-change condition:** satisfy at least one major external evidence axis and avoid
  regressions on the current local contract.

### Panel Synthesis

- **Agreement:** all reviewers regard preserved-witness rollback rejection as a real
  local soundness improvement.
- **Disagreement:** none on the overall score; reviewers differ only on whether external
  performance or physical replicated authority should be prioritized first.
- **Decisive positive axis:** coherent system design, explicit assumptions, broad local
  evidence, preserved negative results, and inspectable failure artifacts.
- **Decisive negative axis:** no independent physical or operational evidence for the
  distributed-authority and performance dimensions most likely to distinguish 8 from 9.
- **Unresolved evidence:** joint rollback protection, physical replication, quorum and
  failover, native external performance, public preservation, independent rerun,
  production key lifecycle, and final metadata.
- **AC stance:** accept at 8/10, confidence 5/5.

## 11. Concerns Table

| ID | Severity | Concern | Evidence basis | Affected criterion | Fix class | Required action | Owner skill | Score-change condition |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| R19-1 | Closed locally | All authenticated state copies could roll back while a preserved witness remained newer | Authenticated rollback fixture passes on three execution paths | Soundness | Method/soundness | Preserve v4 chain, witness binding, and five-fixture verifier | Implementation + integrity audit | Prevents regression; no overall inflation |
| R19-2 | Major | Joint state-and-witness rollback remains undetected | `joint_state_and_witness_rollback_detection=0`; same host/filesystem | Soundness, significance | Experiment | Add independent monotonic authority and coordinated rollback faults | Systems implementation | Necessary path toward 9 |
| R19-3 | Major | No physical replicas, quorum, partitions, or remote failover | `multi_host=0`, `consensus_protocol=0`, shared failure domain | Significance, evidence | Experiment | Deploy cross-host replicas and inject storage/network failures | Distributed-systems experiment | Necessary path toward 9 |
| R19-4 | Major | Native external performance remains absent | All authoritative timing is Apple M4; Docker is correctness-only | Evidence, external validity | Experiment | Repeat paired complete-cost campaigns externally | Benchmark owner | Performance path toward 9 |
| R19-5 | Major | No public immutable independent reproduction | Author-operated local archive and mutable upstream data hosts | Reproducibility | Reproducibility | Deposit frozen artifact/data metadata and obtain independent rerun | Artifact owner | Reproducibility path toward 9 |
| R19-6 | Moderate | Production key lifecycle is untested | State/witness keys are fixtures | Soundness, ethics | Method/soundness | Specify provisioning, separation, rotation, revocation, and compromise recovery | Security owner | Security maturity; may support 9 with external authority |
| R19-7 | Moderate | One-step witness-lag recovery is implemented but untested | Recovery code accepts one chained generation ahead; no dedicated fixture | Soundness | Experiment | Add exact crash-window fault injection before operational reliance | Implementation owner | Closes local coverage; no automatic overall raise |
| R19-8 | Moderate | Final administrative and policy metadata is incomplete | Placeholder author/funding/acknowledgment fields | Readiness | Writing | Finalize metadata and run submission-mode policy checks | Author | Desk/readiness only |
| R19-9 | Minor | Round 19 archive has not yet been regenerated at review time | Source/evidence changed after Round 18 archive | Reproducibility | Reproducibility | Rebuild twice, compare bytes, and clean-tree verify | Artifact owner | Restores current artifact consistency |

## 12. AC / Meta-Review

Reviewer consensus is stable. The paper's strongest axis is disciplined claim-evidence
alignment: original-equation authority is formalized under explicit assumptions,
performance is reported under complete cost, negative results are retained, and local
recovery claims are backed by inspectable fault fixtures. Round 19 meaningfully improves
that axis by detecting rollback of all state copies when the independently keyed witness
is preserved.

The decisive limitation is also stable. The new witness is not independent in the
systems sense: it remains same-host, file-backed, jointly rollbackable, and operated
under test keys. It therefore cannot support physical replication, consensus, quorum,
remote failover, or production freshness claims. Likewise, the performance and
reproduction packages remain externality-limited.

**AC stance:** accept at **8/10**, confidence **5/5**. The score should not rise solely
because a local artifact becomes more internally sound. A higher score requires
decision-level external evidence.

## 13. Quantitative Scores

| Dimension | Score | Confidence | Evidence basis | Deduction and repair condition |
| --- | ---: | ---: | --- | --- |
| Quality | 4/5 | 5/5 | Coherent formal, implementation, experiment, and artifact package | External evidence remains incomplete; repair with native and distributed validation |
| Clarity | 4.54/5 | 5/5 | Claims and limits are explicit within exactly 12 pages | Dense authority paragraph; improve only without dropping evidence or limits |
| Significance | 4/5 | 4/5 | Broad numerical-service and verification relevance | Production distributed operation and customer-scale evidence remain absent |
| Originality | 4/5 | 4/5 | Verification-aware complete-cost expert fusion is well integrated | Round 19 itself is an integrity refinement, not a new central mechanism |
| Soundness | 4/5 | 5/5 | Formal assumptions, authenticated state chain, witness, five fail-closed fixtures | Joint rollback and one-step lag fault coverage remain open |
| Evidence | 4/5 | 5/5 | Repeated paired timing, negative results, three-path authority campaign | Native external performance and physical distributed faults remain absent |
| Reproducibility | 4/5 | 5/5 | Tests, data locks, manifest, verifier, deterministic bundle mechanism | Public immutable archive and independent rerun remain absent |
| Ethics / Limitations | 5/5 | 5/5 | Scope, key, emulation, licensing, negative-result, and rollback boundaries are explicit | Maintain these statements in final metadata and artifact claims |

- **Overall:** **8/10 — accept**.
- **Confidence:** **5/5**.
- **Score-change condition:** evidence from native external performance, physical
  replicated authority with partitions/failover and external monotonic freshness, or a
  public independent rerun could support movement toward 9. Local wording or same-host
  fault additions alone do not.

No criterion is 3 or below; no hidden fatal scientific defect is being averaged away.

## 14. Questions for Authors

1. What concrete independent authority would hold the monotonic witness in a production
   deployment, and what rollback/fork semantics would remain during disconnection?
2. Can the one-generation witness-lag recovery path be fault-injected at each publication
   boundary and independently verified before artifact release?
3. Which native external hardware and workloads will be used to test whether complete-
   path gains and negative results generalize beyond the Apple M4 host?
4. What public archive and independent operator will reproduce the source, locked-data
   acquisition, benchmark, and authority campaigns?
5. What key provisioning, rotation, revocation, and compromise-recovery model is intended
   for state and witness authentication?

## 15. Score Revision Criteria

### Raising the Score Would Require

- Native external paired complete-cost evidence on representative x86-64/Linux and/or
  discrete-accelerator systems.
- Physical cross-host state and witness authority with independent storage/key domains,
  partition, quorum, split-brain, failover, and coordinated rollback faults.
- Externally anchored monotonic freshness that detects joint state/witness rollback.
- Public immutable preservation with a persistent identifier and independent full-data
  rerun.

One of these axes may support movement toward 9 if the current local soundness and
claim discipline are preserved. A 10 requires several axes to converge without a new
central weakness.

### Lowering the Score Would Be Triggered By

- Calling the same-host witness an external monotonic anchor.
- Claiming physical replication, consensus, quorum, or multi-host failover from files or
  Docker namespaces.
- Treating emulated x86-64 correctness as native performance.
- Omitting negative results, failure counts, finite-sample limits, or fixture-key status.
- Failing the independent verifier, 12-page build, CTests, manifest, or bundle gates.

### Concerns Unlikely to Change Before Submission

- Native external performance and independent-host reproduction require resources beyond
  local code changes.
- Physical replicated authority and production key lifecycle require deployment work,
  not prose revision.

## 16. Action Plan and CCFA Handoffs

1. **Priority: immediate.** Regenerate the deterministic core archive twice, compare the
   archive/sidecar/contract byte-for-byte, and clean-tree verify it. **Owner:** artifact
   implementation. **Handoff required:** no.
2. **Priority: immediate.** Update latest-review pointers and rerun claim, stale-term,
   manifest, page-count, and allowlist audits. **Owner:** integrity audit.
   **Handoff required:** no.
3. **Priority: near term.** Add a dedicated witness-lag publication-crash fixture without
   expanding claims until it passes all platforms. **Owner:** implementation/testing.
   **Handoff required:** yes for a future iteration.
4. **Priority: decisive external.** Run native external complete-cost performance.
   **Owner:** benchmark experiment. **Handoff required:** yes.
5. **Priority: decisive external.** Implement physical replicated authority with an
   independent monotonic anchor and partition/failover campaign. **Owner:** distributed-
   systems implementation. **Handoff required:** yes.
6. **Priority: submission.** Finalize metadata, policy, anonymity, and public archive
   records. **Owner:** authors/artifact owner. **Handoff required:** yes.

### Checks Run

- Passed local `reproduce-gate-network-authority` with C++ HMAC self-test, six healthy
  starts, and five exit-88 startup failures.
- Passed Ubuntu ARM64 and emulated x86-64 two-container authority campaigns.
- Independently verified all three authority directories with
  `tests/verify_gate_network_authority.py`.
- Confirmed 22 witness/failure boundary fields match across all three evidence records.
- Passed 29/29 Release CTests.
- Passed `paper/check_evidence.py`, `paper/check_artifact_manifest.py`, and
  `paper/check.sh`.
- Rebuilt the manuscript at exactly 12 pages without a layout hack.
- Audited current sources for v3 headers, three-failure counts, stale Round 14 pointers,
  duplicate evidence fields, and obsolete rollback terminology.

### Checks Skipped at Review Time

- Round 19 deterministic archive regeneration and clean-tree verification.
- Native external performance and physical cross-host authority.
- Public independent reproduction and production key lifecycle.
- New public novelty search, because Round 19 does not change novelty positioning.

### Unresolved Risks

- Joint state-and-witness rollback.
- Physical cross-host replicas, independent storage, partitions, quorum, election, and
  remote failover.
- Native Linux/x86-64, NUMA, multi-node, multi-GPU, and discrete-accelerator complete
  performance.
- Production key custody, rotation, revocation, compromise recovery, KMS, and HSM.
- Public immutable artifact/data preservation and independent rerun.
- Final author, affiliation, funding, conflict, acknowledgment, review-mode, and
  artifact-anonymity metadata.

## 17. Output Self-Check

- Criterion scores, overall score, and confidence are separated.
- Every decision-level statement is tied to inspectable current evidence.
- No acceptance probability or fabricated external result is stated.
- Preserved-witness rollback detection is not called joint rollback protection.
- The file witness is not called an external monotonic anchor.
- The mirror is not called physical replication, quorum, or consensus.
- Docker namespaces are not called physical hosts.
- Emulated x86-64 is not called native performance.
- The one-step witness-lag path is not claimed as tested evidence.
- The score remains 8 because the decisive external evidence axes remain absent.
