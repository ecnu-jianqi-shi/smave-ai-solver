# CCF-A Full Review — Round 20

## 1. Report Metadata

- **Review date:** 2026-07-25.
- **Target venue/year/track:** IEEE Transactions on Parallel and Distributed Systems,
  regular paper, 2026 submission cycle.
- **Paper title:** *Verification-Aware Expert Fusion with Parallel Original-Equation
  Gates for Repeated Numerical Solves*.
- **Input materials reviewed:** current 12-page PDF and LaTeX sources; implementation,
  29-test Release suite, local/Ubuntu ARM64/emulated x86-64 authority records; v5
  authenticated state snapshots; current/previous and prepared witnesses; witness-lag
  crash/recovery records; five fail-closed fixture directories; independent Python
  verifier; manifest, data locks, reproduction tooling; and Rounds 1--19.
- **Search basis:** no new public novelty search. Round 20 closes a local publication
  crash-window test gap and does not introduce a new central algorithm or related-work
  claim.
- **Report file:** `ccfa-review-reports/round-20-full-review.md`.
- **Reviewer mode:** full scientific, writing, format, artifact, integrity, and
  reproducibility review.

## 2. Desk Rejection Assessment

- **Paper length — pass.** The manuscript rebuilds at exactly 12 pages.
- **Topic compatibility — pass.** Verification-aware numerical execution, parallel
  selection, and explicitly bounded authority/recovery fit TPDS.
- **Minimum quality — pass.** The submission combines a formal proposition,
  implementation, complete-cost experiments, negative results, fault fixtures, and
  inspectable artifact checks.
- **Policy/anonymity/compliance — uncertain administratively.** Author, affiliation,
  funding, conflict, acknowledgment, and final artifact metadata remain incomplete.
- **Prompt injection/hidden manipulation — pass locally.** No reviewer-directed hidden
  instruction was found in the inspected manuscript or source paths.
- **Ethics and reviewability — pass with limits.** Same-host, emulation, fixture-key,
  finite-sample, licensing, and external-validity boundaries are stated.

## 3. Paper Summary and Contribution Map

The paper presents verification-aware expert fusion for repeated numerical solves.
Candidate generation, optional correction, family-specific original-equation gates,
publication, and fallback form a typed transaction optimized by verified complete cost.
Under explicit isolation, immutability, atomic-publication, and fallback assumptions,
only a prior committed state or a gate-accepted state becomes caller-visible.

The contribution map remains coherent:

1. A reach-weighted complete-cost objective for heterogeneous candidate cascades.
2. A role-constrained candidate--corrector--gate--fallback transaction and bounded
   commit-authority proposition.
3. A typed C/C++ numerical-service implementation spanning equation families.
4. A broad evaluation retaining regressions, unavailable comparisons, and negative
   outcomes rather than reporting only successful workloads.
5. An inspectable local authority/reproduction artifact with explicit distributed,
   performance, key-custody, and archival boundaries.

### Round 20 Evidence Change

Round 19 identified that the implemented one-generation state-ahead-of-witness path
had no exact publication crash fixture. Round 20 adds `SMAVE_GATE_TXN_STATE 5`, a
crash after durable primary and mirror publication but before witness rename (exit 89),
and startup recovery that accepts exactly one parent-linked generation ahead of the
preserved witness, republishes the witness, and listens only afterward.

The local, Ubuntu ARM64, and emulated x86-64 campaigns all independently pass the
same contract: four preserved lag-crash state snapshots, current/previous witnesses,
one prepared witness, generation delta one, matching parent digest, durable recovery
record, and `listen_socket_created=0` during catch-up. The Python verifier recomputes
all v5 SHA-256/HMAC envelopes, state and witness chains, prepared-witness binding,
and five pre-listen failure fixtures. This closes R19-7 locally. It does not establish
recovery for more than one generation, unchained advances, joint state-and-witness
rollback protection, an external monotonic anchor, physical replication, or
production custody.

## 4. Search and Related-Work Basis

- **Queries used:** none in Round 20.
- **Sources searched:** no new public sources; prior related-work basis is retained.
- **Closest works found:** prior algorithm-selection, learned numerical solver,
  hybrid-correction, selective-prediction, and safety-architecture positioning remains
  unchanged.
- **Unverified related-work risks:** external monotonic counters/transparency logs,
  replicated state-machine recovery, and production key-management comparisons remain
  relevant only if the paper broadens its claims.
- **Source-quality screening:** no new citation or novelty claim was added this round.

## 5. Expected Review Outcome

- **Expected outcome:** **8/10 — accept**.
- **Main accept signal:** coherent verification-aware complete-cost fusion supported by
  formal assumptions, broad local evidence, preserved negative results, and a genuine
  three-platform crash-window artifact.
- **Main reject signal:** authoritative performance remains one-host, while authority
  evidence lacks physical replicas, independent failure domains, partitions, quorum,
  remote failover, external freshness, and independent reproduction.
- **Confidence:** **5/5**.

The new exit-89 fixture is a real local soundness and reproducibility improvement, and
it closes the precise R19-7 concern. It does not change the decision-level ceiling:
same-host recovery evidence cannot be credited as physical distributed authority or
external rollback freshness.

## 6. Strengths and Weaknesses

### Strengths

1. The crash is injected at the actual state-before-witness publication boundary,
   rather than inferred from a post hoc record.
2. The preserved bytes show a valid one-generation state advance, a parent digest equal
   to the prior witness digest, and a prepared witness matching the lagged state.
3. Recovery republishes the witness before listen, and the durable recovery record is
   checked against preserved bytes by an independent verifier.
4. Local, ARM64, and emulated amd64 reports agree on all Round 20 boundary fields:
   v5 format, eight starts, exit 89, one-generation delta, one recovery, four snapshots,
   and five fail-closed fixtures.
5. The manuscript explicitly limits the result to one generation and preserves the
   same-host, fixture-key, no-external-anchor, no-physical-replication boundary.
6. Existing strengths remain: complete-cost measurement, paired statistics, original
   equation authority, broad interfaces, negative results, data locks, and deterministic
   author-operated reproduction.

### Major Weaknesses

1. **No independent rollback authority.** The witness shares the process, filesystem,
   host, operator, and fixture-key environment with state. Joint rollback remains
   undetected. **Required fix:** deploy an independent monotonic service, trusted
   counter, append-only log, or quorum and inject coordinated rollback faults.
2. **No physical replication or failover protocol.** Primary, mirror, and witness are
   still same-host files. **Required fix:** add cross-host storage/key domains and test
   partitions, split brain, quorum, leader loss, recovery, and remote failover.
3. **No native external performance evidence.** Correctness containers do not establish
   native Linux/x86-64 or accelerator performance. **Required fix:** repeat complete-cost
   campaigns on external native systems.
4. **No public independent reproduction.** The bundle is author-operated and omits the
   large benchmark payloads. **Required fix:** immutable public preservation and an
   independent full-data rerun.
5. **No production key lifecycle.** Fixture keys do not establish provisioning,
   separation, rotation, revocation, or compromise recovery. **Required fix:** specify
   and fault-test an operational lifecycle.

### Moderate Weaknesses

1. The authority paragraph remains information-dense within the fixed page budget.
2. Final administrative and submission-policy metadata remains incomplete.
3. Larger-than-one-generation and unchained state-ahead-of-witness advances are not
   separately fault-injected; they are correctly outside the stated evidence claim.

## 7. Potentially Missing Related Work

- **External monotonic counters and transparency logs:** status **unverified**; relevant
  to a future external-freshness claim; no comparison is required for the present
  same-host claim.
- **Replicated state machines and consensus recovery:** status **unverified**; relevant
  to a future physical-replication or quorum claim; current evidence explicitly makes
  no such claim.
- **Production key-management systems:** status **unverified**; relevant if operational
  key custody becomes part of the contribution; fixture-key evidence is not presented
  as production security.

## 8. Claim-Evidence Audit

| Claim | Where stated | Evidence provided | Strength | Reviewer deduction | Required fix |
| --- | --- | --- | --- | --- | --- |
| Original-equation gates retain commit authority | Abstract, method, proposition, evaluation | Formal assumptions, family-specific gates, fallback and mismatch probes | Strong within assumptions | Does not verify arbitrary faulty callbacks or hardware | Preserve bounded theorem language |
| Highest authenticated generation is selected | Methodology, evaluation, authority ledger | Authenticated primary/mirror current and previous snapshots | Strong local functional evidence | One process/filesystem/host | Physical replicas for distributed claim |
| One-generation witness-lag recovery catches up before listen | Methodology, evaluation, claim ledger | Exit-89 crash, four preserved snapshots, prepared witness, recovery record, three platform verifiers | Strong for the tested one-step path | No larger/unchained path or external authority | Keep the one-generation qualification; add broader faults only with evidence |
| Preserved witness rejects all-state rollback | Evaluation and limitations | Five pre-listen failure fixtures including all states below witness | Strong preserved-witness evidence | Joint state+witness rollback remains possible | Independent monotonic anchor |
| Wrong keys, corruption, and authenticated forks fail closed | Methodology and artifact ledger | Five copied-state failure classes, exit 88, no reinitialization | Strong fixture evidence | No production custody or distributed quorum | Operational key/replication campaign |
| Complete-path speedups hold | Evaluation figures/tables and pinned reports | Paired runs, bootstrap intervals, failures retained | Strong for measured host/workloads | No native external performance | External complete-cost campaigns |
| Artifact is reproducible | Artifact documentation, manifest, bundle tools | Deterministic bundle, data locks, 29/29 tests, clean checks | Strong author-operated evidence | No public persistent archive or independent rerun | Public immutable deposit and independent operator |

## 9. Experiment, Benchmark, and Reproducibility Audit

- **Baselines:** classical, learned, routing, shared-control, and fallback baselines
  remain explicit; Round 20 removes none.
- **Ablations:** candidate, correction, gate, fallback, fusion, certificate reuse,
  worker/process scaling, and order sensitivity remain inspectable.
- **Benchmarks:** PDEBench-derived, SuiteSparse, PETSc TS, Modelica, FMI/SSP, and
  operator families retain their stated coverage and failure accounting.
- **Metrics and statistics:** complete verified runtime is separated from gate-only
  throughput; paired repetitions and bootstrap intervals remain attached to timing
  claims, while finite safety/fault trials are not inflated into statistical certainty.
- **Robustness:** the authority campaign now covers exits 86/87/89, four cleanups,
  one-generation catch-up, mirror recovery, four replays, and five fail-closed starts.
- **Implementation detail:** v5 state format, parent digest, separate witness HMAC,
  exit codes, publication order, preserved bytes, and verifier contract are inspectable.
- **Reproducibility:** local, ARM64, and emulated amd64 verifiers pass; manifest,
  evidence checker, paper gate, and 29/29 CTests pass. Public preservation, independent
  full-data rerun, native external performance, and physical distributed faults remain
  unavailable.

## 10. Multi-Reviewer Panel

### Reviewer 1 — Method and Soundness

- **Likely score / confidence:** 8/10, 5/5.
- **Main positive signal:** the exact publication crash window is now tested and the
  one-step parent-linked catch-up is verified before listen.
- **Main negative signal:** the witness shares the state failure domain and joint rollback
  remains possible.
- **Evidence basis:** v5 snapshots, prepared witness, exit-89 record, C++ path, Python
  verifier, and five failure fixtures.
- **Score-change condition:** an independent monotonic authority with coordinated
  rollback faults could support movement toward 9.

### Reviewer 2 — Distributed Systems

- **Likely score / confidence:** 8/10, 5/5.
- **Main positive signal:** local persistence and fail-closed recovery semantics are
  precise and honestly scoped.
- **Main negative signal:** no physical replicas, quorum, partition semantics, election,
  or remote failover.
- **Evidence basis:** all platform records retain `same_physical_host=1`, `multi_host=0`,
  and `consensus_protocol=0`.
- **Score-change condition:** physical cross-host replication and fault campaigns.

### Reviewer 3 — Evidence and Experiments

- **Likely score / confidence:** 8/10, 5/5.
- **Main positive signal:** the new crash fixture is byte-preserved, independently
  parsed, and repeated across three execution paths.
- **Main negative signal:** authoritative timing and authority remain same-host.
- **Evidence basis:** matching boundary fields, independent verifier output, pinned
  reports, and complete test suite.
- **Score-change condition:** native external complete-cost repetitions and independent
  full-data reproduction.

### Reviewer 4 — Novelty and Positioning

- **Likely score / confidence:** 8/10, 4/5.
- **Main positive signal:** verification-aware complete-cost expert fusion remains a
  coherent systems contribution.
- **Main negative signal:** Round 20 strengthens artifact soundness rather than adding a
  new algorithmic or distributed mechanism.
- **Evidence basis:** unchanged contribution map and related-work positioning.
- **Score-change condition:** a differentiated externally validated mechanism; wording
  alone cannot raise originality.

### Reviewer 5 — Writing and Clarity

- **Likely score / confidence:** 8/10, 5/5.
- **Main positive signal:** one-generation scope, exit codes, and no-external-anchor
  boundaries are visible in methodology, evaluation, and limitations.
- **Main negative signal:** the authority paragraph remains dense.
- **Evidence basis:** exact 12-page PDF, claim ledger, generated evidence, and limitations.
- **Score-change condition:** a small space-neutral reorganization could reduce reading
  load, but prose alone does not raise the scientific score.

### Reviewer 6 — Ethics, Artifact, and Reproducibility

- **Likely score / confidence:** 8/10, 5/5.
- **Main positive signal:** fixture-key status, emulation limits, negative results, and
  the new preserved crash bytes are explicit.
- **Main negative signal:** no public immutable archive or independent operator.
- **Evidence basis:** manifest, locks, bundle tooling, three platform records, and
  review history.
- **Score-change condition:** persistent public archive and independent clean rerun.

### Reviewer 7 — Numerical-Solver Domain Application

- **Likely score / confidence:** 8/10, 5/5.
- **Main positive signal:** the paper reports both acceleration and break-even failures
  across heterogeneous equation families instead of implying universal superiority.
- **Main negative signal:** customer-scale deployment and native external hardware remain
  untested.
- **Evidence basis:** PDEBench-derived, SuiteSparse, operator, DAE, device, and negative-
  result sections.
- **Score-change condition:** representative external application traces and deployment
  costs could strengthen significance.

### Reviewer 8 — Evidence and Ablation

- **Likely score / confidence:** 8/10, 5/5.
- **Main positive signal:** candidate, correction, gate, fallback, shared control,
  worker/process scaling, and order effects are separately inspectable.
- **Main negative signal:** the strongest positive timing results are still confined to
  one physical host.
- **Evidence basis:** ablation section, paired reports, bootstrap intervals, and retained
  failures.
- **Score-change condition:** repeat the same complete-cost and ablation contracts on
  native external systems.

### Reviewer 9 — Novice-Advocate Reader

- **Likely score / confidence:** 8/10, 4/5.
- **Main positive signal:** the central question, transaction roles, and non-universal
  conclusion are explicit.
- **Main negative signal:** the compressed authority paragraph demands familiarity with
  persistence, HMAC, witnesses, and fail-closed startup semantics.
- **Evidence basis:** introduction, methodology authority paragraph, evaluation, and
  limitations.
- **Score-change condition:** a space-neutral mechanism/fault/boundary split would improve
  accessibility but not the scientific score.

### Reviewer 10 — AC / Meta-Review

- **Likely score / confidence:** 8/10, 5/5.
- **Main positive signal:** Round 20 closes the exact local concern without inflating
  the claim beyond one authenticated generation.
- **Main negative signal:** all decision-level external blockers remain.
- **Evidence basis:** matching Round 20 fields, manuscript boundaries, and passing gates.
- **Score-change condition:** satisfy at least one major external evidence axis without
  regression.

### Panel Synthesis

- **Agreement:** the one-generation crash-window and pre-listen catch-up are real local
  soundness improvements; the stated boundaries are appropriate.
- **Disagreement:** no overall-score disagreement; prioritization differs between native
  performance, physical authority, and public reproduction.
- **Decisive positive axis:** coherent design, explicit assumptions, broad local evidence,
  negative results, and inspectable fault artifacts.
- **Decisive negative axis:** no independent physical or operational evidence for the
  distributed-authority and external-performance axes.
- **Unresolved evidence:** joint rollback, physical replication/failover, native
  performance, public preservation, independent rerun, production key lifecycle, and
  final metadata.
- **AC stance:** accept at **8/10**, confidence **5/5**.

## 11. Concerns Table

| ID | Severity | Concern | Evidence basis | Affected criterion | Fix class | Required action | Owner skill | Score-change condition |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| R20-1 | Minor | Closed locally: one-generation witness-lag recovery lacked a crash fixture | Exit-89 fixture, four snapshots, prepared witness, recovery record, three passes | Soundness | method/soundness | Preserve v5 chain and verifier contract | Implementation + integrity audit | Prevents regression; no overall inflation |
| R20-2 | Major | Joint state-and-witness rollback remains undetected | Same process/filesystem/host; `joint_state_and_witness_rollback_detection=0` | Soundness, significance | experiment | Add external monotonic authority and coordinated rollback faults | Distributed-systems implementation | Necessary path toward 9 |
| R20-3 | Major | No physical replicas, quorum, partitions, or remote failover | `multi_host=0`, `consensus_protocol=0`, shared failure domain | Significance, evidence | experiment | Deploy independent cross-host replicas and fault campaign | Distributed-systems experiment | Necessary path toward 9 |
| R20-4 | Major | Native external performance remains absent | Timing is Apple M4; containers are correctness-only | Evidence, external validity | experiment | Repeat complete-cost campaigns on native external systems | Benchmark owner | Performance path toward 9 |
| R20-5 | Major | No public immutable independent reproduction | Author-operated bundle; large payloads omitted | Reproducibility | reproducibility | Deposit frozen artifact/data metadata and obtain independent full-data rerun | Artifact owner | Reproducibility path toward 9 |
| R20-6 | Moderate | Production key lifecycle is untested | State/witness keys are fixture keys | Soundness, ethics | method/soundness | Specify provisioning, separation, rotation, revocation, compromise recovery | Security owner | Security maturity; may support 9 with external authority |
| R20-7 | Moderate | Final administrative and policy metadata is incomplete | Placeholder author/funding/acknowledgment fields | Readiness | writing | Finalize metadata and submission-mode checks | Author | Desk/readiness only |
| R20-8 | Minor | Authority presentation is dense | Fixed 12-page methodology/evaluation paragraphs | Clarity | compression | Reorganize only if no evidence or limitation is removed | Paper writer | Readability only |

## 12. AC / Meta-Review

The panel agrees that Round 20 closes a specific implementation-evidence mismatch:
the state-before-witness crash is now injected at the real publication boundary, the
preserved bytes establish exactly one authenticated parent-linked advance, and recovery
republishes the witness before listening. This is stronger than a wording change and
deserves explicit credit in soundness and reproducibility.

The panel also agrees that this does not establish independent distributed authority.
The state, mirror, witness, keys, filesystem, and host remain coupled; no quorum,
partition, remote failover, external freshness, native external performance, public
immutable archive, or independent full-data rerun exists. The central TPDS decision
therefore remains unchanged.

**AC stance:** accept at **8/10**, confidence **5/5**. A local crash fixture should not
be converted into a 9 by score inflation.

## 13. Quantitative Scores

| Dimension | Score | Confidence | Evidence basis | Deduction and repair condition |
| --- | ---: | ---: | --- | --- |
| Quality | 4/5 | 5/5 | Formal, implementation, experiments, and artifact checks are coherent | External authority and performance remain incomplete; add native/distributed validation |
| Clarity | 4.54/5 | 5/5 | Claims, evidence, and boundaries remain visible in 12 pages | Authority paragraph is dense; reorganize without deleting limits |
| Significance | 4/5 | 4/5 | Broad numerical-service and verification relevance | Production distributed operation and customer-scale evidence remain absent |
| Originality | 4/5 | 4/5 | Verification-aware complete-cost expert fusion is integrated coherently | Round 20 is an integrity refinement, not a new central mechanism |
| Soundness | 4/5 | 5/5 | Formal assumptions, v5 chain, witness, crash recovery, and five fail-closed fixtures | Joint rollback and external authority remain untested; add independent anchor/faults |
| Evidence | 4/5 | 5/5 | Paired statistics, negative results, and three-path authority fault evidence | Native external performance and physical distributed faults remain absent |
| Reproducibility | 4/5 | 5/5 | Tests, manifest, locks, verifier, and deterministic bundle | Public immutable archive and independent full-data rerun remain absent |
| Ethics / Limitations | 5/5 | 5/5 | Same-host, emulation, fixture-key, finite-sample, and rollback limits are explicit | Maintain these boundaries in final metadata |

### Writing Review Scorecard

| Dimension | Weight | Score | Confidence | Evidence basis | Concrete repair |
| --- | ---: | ---: | ---: | --- | --- |
| Storyline and motivation | 12 | 5/5 | 5/5 | Problem, gap, transaction insight, and complete-cost question progress explicitly | Preserve the current sequence |
| Contribution display | 12 | 5/5 | 5/5 | Contributions are visible in the introduction and supported later | Preserve claim boundaries |
| Paragraph logic | 10 | 4/5 | 5/5 | Most paragraphs have one job; authority prose combines mechanism, faults, and limits | Split only if space can be recovered without deleting evidence |
| Claim-evidence alignment | 14 | 5/5 | 5/5 | Strong claims map to formal, benchmark, or fault evidence and are qualified | Keep ledgers synchronized |
| Method readability | 10 | 4/5 | 5/5 | Typed transaction and proposition are clear; persistence details are compressed | Separate publication order from recovery conditions if space permits |
| Experiment narration | 10 | 4/5 | 5/5 | Figures and tables are interpreted, but the authority result is dense | Use a compact mechanism/fault/boundary progression |
| Related-work positioning | 8 | 4/5 | 4/5 | Closest hybrid-correction and algorithm-selection work is positioned | Refresh only if claims broaden |
| Terminology and notation consistency | 8 | 5/5 | 5/5 | Complete cost, gate, witness, same-host, and emulation terms remain stable | Preserve terminology audit |
| LaTeX and format discipline | 8 | 5/5 | 5/5 | Exact 12 pages, no overfull boxes, resolved references | Preserve the current format gate |
| Reviewer-facing risk | 8 | 4/5 | 5/5 | Scope boundaries are explicit; external evidence gaps remain decision-relevant | Do not inflate local evidence into external claims |

**Weighted writing score:** **4.54/5**. **Writing risk:** low.

- **Overall:** **8/10 — accept**.
- **Confidence:** **5/5**.
- **Score-change condition:** native external performance, physical replicated
  authority with independent failure domains and externally anchored freshness, or a
  public independent rerun could support movement toward 9. Local wording or another
  same-host fixture alone does not.

No criterion is 3 or below; no hidden fatal scientific defect is being averaged away.

## 14. Questions for Authors

1. What independent authority will hold freshness in a production deployment, and what
   semantics remain during disconnection?
2. Which native external systems will test complete verified cost beyond the Apple M4?
3. What public archive and independent operator will reproduce the full-data campaign?
4. What provisioning, rotation, revocation, and compromise-recovery model will protect
   state and witness keys?

## 15. Score Revision Criteria

### Raising the Score Would Require

- Native external paired complete-cost evidence on representative Linux/x86-64 and/or
  discrete-accelerator systems.
- Physical cross-host state and witness authority with independent storage/key domains,
  partition, quorum, split-brain, failover, and coordinated rollback faults.
- Externally anchored monotonic freshness that detects joint state/witness rollback.
- Public immutable preservation with a persistent identifier and an independent full-data
  rerun.

One major axis could support movement toward 9 if current claim discipline survives; a
10 requires several axes without a new central weakness.

### Lowering the Score Would Be Triggered By

- Calling the same-host witness an external monotonic anchor.
- Calling Docker namespaces physical hosts or the mirror physical replication.
- Treating emulated x86-64 correctness as native performance.
- Claiming more than one-generation or unchained witness recovery from this fixture.
- Omitting negative results, finite-sample limits, or fixture-key status.
- Failing the independent verifier, 12-page build, CTests, manifest, or bundle gates.

### Concerns Unlikely to Change Before Submission

- Native external performance and independent-host reproduction require resources beyond
  local wording changes.
- Physical replicated authority and production key lifecycle require deployment work.

## 16. Action Plan and CCFA Handoffs

1. **Immediate:** regenerate the deterministic core archive twice, compare archive,
   sidecar, and contract bytes, and clean-tree verify. **Owner:** artifact
   implementation. **Handoff required:** no.
2. **Immediate:** update latest-review pointers and rerun claim, stale-term, manifest,
   page-count, duplicate-field, and allowlist audits. **Owner:** integrity audit.
   **Handoff required:** no.
3. **Submission:** finalize author, affiliation, funding, conflict, acknowledgment,
   policy, and public-archive metadata. **Owner:** authors. **Handoff required:** yes.
4. **Decisive external:** run native performance and physical replicated-authority
   campaigns with external freshness and independent operators. **Owner:** benchmark/
   distributed-systems owners. **Handoff required:** yes.

### Checks Run

- Passed all three `tests/verify_gate_network_authority.py` invocations: each reports
  `snapshots=4 recoveries=1 failures=5`.
- Confirmed Round 20 boundary fields match across local, ARM64, and emulated amd64.
- Passed `paper/check_evidence.py`, `paper/check_artifact_manifest.py`, and
  `paper/check.sh` with the exact 12-page build.
- Passed Release CTest: 29/29 tests.
- Reviewed v5 state/witness code, preserved lag-crash bytes, recovery record, manuscript,
  artifact snapshot, claim ledger, and prior immutable review.

### Checks Skipped

- Native external performance, physical cross-host authority, production key lifecycle,
  public independent full-data rerun, and final submission metadata.
- New public novelty search, because Round 20 adds no central novelty claim.

### Unresolved Risks

- Joint state-and-witness rollback and absent external monotonic freshness.
- Physical replicas, independent storage/key domains, partitions, quorum, election,
  remote failover, native external performance, and public independent reproduction.
- Fixture-key operational security and final administrative metadata.

## 17. Output Self-Check

- Criterion scores, overall stance, and confidence are separated and evidence-grounded.
- R19-7 is marked closed locally; the overall score remains 8/10.
- The one-step path is not generalized to multiple generations or unchained advances.
- Same-host files are not called physical replicas or external anchors.
- Emulated x86-64 is not called native performance; no acceptance probability is stated.
