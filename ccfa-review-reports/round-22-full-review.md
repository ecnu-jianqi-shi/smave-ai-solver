# CCF-A Full Review — Round 22

## 1. Report Metadata

- **Review date:** 2026-07-25.
- **Target venue/year/track:** IEEE Transactions on Parallel and Distributed Systems,
  regular paper, 2026 submission cycle.
- **Paper title:** *Verification-Aware Expert Fusion with Parallel Original-Equation
  Gates for Repeated Numerical Solves*.
- **Input materials reviewed:** current 12-page PDF and LaTeX sources; implementation;
  local, Ubuntu ARM64, and emulated x86-64 authority artifacts; independent verifier;
  29-test Release suite; artifact manifest and bundle tooling; rendered pages 7, 10,
  and 11; and immutable Rounds 1--21.
- **Search basis:** official IEEE Computer Society TPDS scope and author-information
  pages were checked for topic and initial-length compatibility. No new novelty search
  was required because Round 22 adds a bounded fixture protocol and explicitly makes
  no production key-management or distributed-authority claim.
- **Report file:** `ccfa-review-reports/round-22-full-review.md`.
- **Reviewer mode:** full scientific, writing, format, artifact, integrity, and
  reproducibility review.

## 2. Desk Rejection Assessment

- **Paper length — pass.** The manuscript rebuilds at exactly 12 pages and 289,476
  bytes without margin, font, spacing, or float manipulation. This matches the checked
  TPDS initial-manuscript length guidance.
- **Topic compatibility — pass.** Verification-aware numerical execution, parallel
  gates, complete-cost selection, and bounded same-host authority evidence fit TPDS.
- **Minimum quality — pass.** Formal, implementation, benchmark, failure, negative-
  result, and artifact evidence remain inspectable.
- **Policy/anonymity/compliance — uncertain administratively.** Author, affiliation,
  funding, conflict, acknowledgment, and final artifact metadata remain incomplete.
- **Prompt injection and hidden manipulation detection — pass locally.** No reviewer-
  directed instruction was found in the inspected manuscript, source, or artifact
  documentation.
- **Ethics and reviewability — pass with limits.** Emulation, same-host storage,
  fixture keys, finite samples, licensing, and external-validity limits remain explicit.

**Desk-rejection risk:** low scientifically; medium administratively until submission
metadata and the final TPDS checklist are completed.

## 3. Paper Summary and Contribution Map

The paper presents verification-aware expert fusion for repeated numerical solves.
Candidate generation, optional correction, original-equation gates, publication, and
fallback form a typed transaction selected by complete verified cost. Under explicit
isolation, immutable-problem, atomic-publication, and fallback assumptions, each
caller-visible state is restricted to a prior commit or a state accepted by the
mandatory family-specific gate.

The contribution map remains:

1. A reach-weighted complete-cost objective for heterogeneous expert cascades.
2. A candidate--corrector--gate--fallback transaction and bounded commit-authority
   proposition.
3. A typed C/C++ runtime spanning algebraic and dynamic equation services.
4. A broad evaluation that retains regressions, unavailable comparisons, and failures.
5. An inspectable authority and reproduction artifact with explicit scope boundaries.

### Round 22 Scientific Change

Round 21 left one locally actionable part of R21-6: fixture-key rotation and
revocation were not exercised. Round 22 adds a deliberately bounded protocol:

1. Startup accepts at most one explicitly configured previous state key and one
   explicitly configured previous witness key.
2. Old-key state and witness bytes authenticate only when those previous keys are
   supplied.
3. Before the listen socket is created, the first durable publication reissues state
   and witness under current keys and writes a durable rotation record.
4. A later current-key-only restart succeeds.
5. Preserved old-key state and witness bytes fail closed under current-key-only trust.

The campaign preserves old and new bytes around the migration boundary and verifies
their key domains independently. It also fixes a platform-sensitive stale-generation
fixture by staging current/previous copies from the authenticated mirror pair. The
Docker harness now compiles once per architecture, while client/server containers,
network namespaces, restart order, and injected faults remain unchanged.

The new evidence is fixture-only. It establishes neither provisioning, production
custody, KMS/HSM integration, compromise recovery, arbitrary crash-point rotation,
external key authority, nor physical replication.

## 4. Search and Related-Work Basis

- **Queries used:** official TPDS author-information/page-limit and journal-scope
  queries on IEEE Computer Society domains.
- **Sources searched:** official IEEE Computer Society TPDS Call for Papers and Author
  Information pages.
- **Closest works found:** unchanged algorithm-selection, learned-solver,
  hybrid-correction, selective-prediction, and runtime-assurance basis already cited.
- **Unverified related-work risks:** production key management, external monotonic
  counters, transparency logs, and replicated-state-machine recovery would require a
  refreshed search only if the paper broadened its current fixture-only claims.
- **Source-quality screening status:** official venue sources used for policy; no
  novelty or citation claim changed.

## 5. Expected Review Outcome

- **Expected outcome:** **8/10 — accept**.
- **Main accept signal:** coherent verification-aware complete-cost fusion, explicit
  assumptions, broad inspectable evidence, retained negative results, and a real
  fail-closed migration/revocation fixture across three execution paths.
- **Main reject signal:** no native external performance, physical replicated
  authority, independent monotonic freshness, public independent reproduction, or
  production key lifecycle.
- **Confidence:** **5/5**.

Round 22 reduces one local operational gap but does not change a decisive external
axis. Same-host fixtures and test keys cannot justify movement toward 9.

## 6. Strengths and Weaknesses

### Major Strengths

1. **The bounded key protocol is real, durable, and independently checked.** The
   server records old/new key identifiers, selected and published generations,
   parent-digest continuity, current-key republication, and pre-listen state.
2. **Revocation is tested by preserved bytes, not only by counters.** Old-key snapshots
   and witness bytes authenticate under old keys, fail under current keys, and cause
   exit 88 before listen when no previous key is configured.
3. **The trust transition is explicit.** Exactly one previous key per HMAC domain is
   permitted; a later restart succeeds with current keys only.
4. **The verifier remains independent of the C++ parser.** Python recomputes SHA-256,
   both HMAC domains, parent links, key identifiers, rotation records, witness-lag
   recovery, and all six fail-closed fixtures.
5. **Cross-platform behavior is consistent.** Local, Ubuntu ARM64, and emulated x86-64
   ledgers match across all 90 fields except the intended deployment and namespace
   indicators.
6. **Claim discipline is strong.** Methodology, evaluation, limitations, artifact
   documentation, and ledgers all call the rotation a same-host fixture and deny
   production lifecycle, KMS/HSM, external authority, performance, and replication.
7. **Negative and bounded results remain visible.** One-generation witness catch-up,
   joint rollback exposure, one-host timing, failed device paths, routing regression,
   and failed operator transfer are retained.

### Major Weaknesses

1. **No independent monotonic authority.** State and witness remain jointly rollbackable
   because they share a process, filesystem, host, and failure domain. **Required fix:**
   external monotonic authority plus coordinated state/witness rollback faults.
2. **No physical replicated authority.** Primary and mirror are same-host files, not
   replicas under quorum, partitions, election, or failover. **Required fix:** cross-host
   replicas and a physical failure campaign.
3. **No native external performance evidence.** Apple M4 remains the only authoritative
   timing host; containers are correctness-only. **Required fix:** repeat complete-cost
   campaigns on native external systems.
4. **No public independent reproduction.** The bundle remains author-operated and
   excludes large benchmark payloads. **Required fix:** immutable public deposit and an
   independent full-data rerun.

### Moderate Weaknesses

1. **Production key lifecycle remains absent.** The fixture exercises migration and
   revocation semantics, but not provisioning, custody, separation, KMS/HSM, compromise
   recovery, or arbitrary interruption points. **Required fix:** deploy and fault-test
   an operational lifecycle under independent authority.
2. **Final administrative metadata remains incomplete.** Author, affiliation, funding,
   conflict, acknowledgment, and release metadata require completion.
3. **Recovery scope remains intentionally narrow.** One authenticated parent-linked
   witness advance is exercised; larger or unchained advances are not claimed.

## 7. Potentially Missing Related Work

### Operational Key Management

- **Status:** unverified for the current round; not required by the bounded claim.
- **Why relevant:** production provisioning, custody, rotation, revocation, and
  compromise recovery differ materially from fixture-key migration.
- **Overlap:** the current work tests only an explicit old-to-current HMAC transition.
- **Needed comparison:** required only if the paper later claims production lifecycle
  or KMS/HSM readiness.

### Replicated State Machines and Quorum Recovery

- **Status:** unverified for the current round; outside the present claim.
- **Why relevant:** physical authority requires independent replicas, ordering,
  partitions, leader loss, quorum, and failover semantics.
- **Overlap:** current primary/mirror files share one process and host.
- **Needed comparison:** required only if same-host recovery is broadened to distributed
  commit or replicated authority.

### External Monotonic Freshness and Transparency

- **Status:** unverified for the current round; explicitly excluded.
- **Why relevant:** an external counter, log, or authority is needed to detect coherent
  rollback of state and witness together.
- **Overlap:** the local witness detects state rollback only while the witness itself is
  preserved.
- **Needed comparison:** required for any joint-rollback or external-freshness claim.

## 8. Claim-Evidence Audit

| Claim | Where stated | Evidence provided | Strength | Reviewer deduction | Required fix |
| --- | --- | --- | --- | --- | --- |
| Complete verified cost should govern expert selection | Abstract lines 7--14; Introduction lines 24--37; formulation and design | Reach-weighted objective, router comparisons, ablations, complete-runtime reports | Strong | Supported as a design and evaluation principle | Preserve complete-cost accounting |
| Mandatory original-equation gates bound caller-visible commits | Abstract lines 12--14; Introduction lines 39--49; Proposition and component probes | Formal assumptions, commit proposition, fresh-buffer, cancellation, tamper/revocation, and fallback probes | Strong within assumptions | Not a proof of arbitrary callbacks or hardware | Preserve assumptions and negative boundary |
| Linear and nonlinear gates scale without decision/residual mismatch | Abstract lines 16--21; Evaluation RQ5 | 30 paired repetitions, sequential authority, linear/nonlinear family reports | Strong on one Apple M4 host | Gate-only, not complete or distributed scaling | Add external timing only for broader claim |
| Seven PDEBench-derived workloads accelerate under the pinned protocol | Abstract lines 22--35; Introduction lines 83--97; Evaluation RQ2 | 30 run-level paired repetitions, bootstrap intervals, counterbalanced order study | Strong for the seven qualified workloads | One host and selected subsystem contracts limit external validity | External native repetition for score movement |
| Two held-out operators beat a shared hybrid control | Abstract lines 26--30; Evaluation RQ4; ablation | Same candidate, correction/gate/fallback control, paired timing, retained transfer failure | Strong for the tested families | Does not establish broad operator transfer | Preserve failed transfer and scope |
| Fixture keys can migrate and later be revoked under one explicit previous key per domain | Methodology page 7; Evaluation page 10; Limitations page 11 | v6 before/after bytes, durable rotation record, current-key-only restart, old-byte rejection, three verifiers | Strong for the fixture protocol | Does not establish production lifecycle, KMS/HSM, or arbitrary crash-safe rotation | Keep claim fixture-only or add operational deployment |
| Same-host witness detects stale state but not joint rollback | Evaluation and Limitations; authority ledgers | Preserved-witness rollback fixture, wrong-witness-key failure, explicit zero fields for external anchor and joint rollback | Strong and honest | External freshness remains absent | Add independent monotonic authority for broader claim |
| The system provides qualified rather than universal acceleration | Abstract lines 30--35; Introduction lines 100--103; Conclusion lines 10--19 | Positive results, device failures, routing regression, operator-transfer failure, one $>100\times$ workload | Strong | Bounded conclusion matches evidence | Preserve negative results |

## 9. Experiment / Benchmark / Reproducibility Audit

- **Baselines — strong.** Classical solvers, fixed and calibrated routing, hindsight
  references, shared hybrid controls, sequential gates, and fallback paths are visible.
- **Ablations — strong.** Candidate, correction, gate, fallback, router, strict-gate
  fusion, certificate reuse, worker/process scaling, and failure paths are separated.
- **Datasets and benchmarks — broad but qualified.** SuiteSparse, PETSc TS,
  OpenModelica, COPS, PDEBench-derived, operator, and device studies retain exclusions.
- **Metrics — appropriate.** Complete verified runtime, paired speedup, throughput,
  residual/decision agreement, acceptance/fallback, and failure counts match claims.
- **Statistical rigor — strong for the stated runs.** Thirty paired repetitions,
  fixed-seed bootstrap intervals, paired win rates, and counterbalanced orders are
  reported; finite-sample limits remain explicit.
- **Robustness and failure cases — unusually strong locally.** Publication exits
  86/87/89, orphan cleanup, witness lag, corruption, stale generations, mirror loss,
  forks, rollback, wrong keys, wrong witness keys, revoked keys, replay, conflict, and
  malformed input are exercised.
- **Implementation details — strong.** v6 envelopes, current/previous state and witness
  HMAC domains, explicit previous-key CLI slots, key identifiers, parent hashes, and
  pre-listen records are inspectable.
- **Artifacts and reproducibility — strong locally, incomplete externally.** Three
  authority verifiers, manifest, evidence checker, deterministic bundle, clean-tree
  procedure, and 29/29 tests pass; no public immutable full-data rerun exists.
- **Limitations — exemplary.** Same-host, correctness-only containers, fixture keys,
  joint rollback, omitted payloads, one-host timing, and metadata gaps are explicit.

## 10. Multi-Reviewer Panel

### Reviewer 1 — Method and Soundness

- **Likely score / confidence:** 8/10, 5/5.
- **Main positive signal:** the trust transition is explicit, durable, and fail-closed.
- **Main negative signal:** no external key or freshness authority exists.
- **Evidence basis:** v6 server path, rotation record, revoked-byte fixture, limitations.
- **Score-change condition:** external monotonic/key authority with interruption faults.

### Reviewer 2 — Distributed Systems

- **Likely score / confidence:** 8/10, 5/5.
- **Main positive signal:** migration occurs before listen and preserves authenticated
  parent continuity.
- **Main negative signal:** primary, mirror, and witness remain one-host files without
  quorum, partitions, election, or remote failover.
- **Evidence basis:** platform fields and same-host artifact topology.
- **Score-change condition:** physical cross-host replicated authority campaign.

### Reviewer 3 — Evidence and Experiments

- **Likely score / confidence:** 8/10, 5/5.
- **Main positive signal:** old/new bytes, records, counters, failures, and three
  execution paths agree.
- **Main negative signal:** timing remains one-host.
- **Evidence basis:** three verifiers, 90-field parity, pinned reports, 29/29 tests.
- **Score-change condition:** native external complete-cost repetitions.

### Reviewer 4 — Novelty and Positioning

- **Likely score / confidence:** 8/10, 4/5.
- **Main positive signal:** the bounded protocol strengthens implementation credibility.
- **Main negative signal:** fixture-key migration is not a new production key-management
  contribution and should not be scored as one.
- **Evidence basis:** unchanged abstract/contribution list and explicit limitations.
- **Score-change condition:** differentiated externally validated authority mechanism.

### Reviewer 5 — Writing and Clarity

- **Likely score / confidence:** 8/10, 5/5.
- **Main positive signal:** mechanism, observed migration, revocation outcome, and scope
  boundary remain recoverable in the fixed page budget.
- **Main negative signal:** the manuscript remains dense because of breadth.
- **Evidence basis:** source audit, sentences of at most 18 words in revised passages,
  and rendered pages 7, 10, and 11.
- **Score-change condition:** no further local prose edit is decision-relevant.

### Reviewer 6 — Ethics, Security, and Limitations

- **Likely score / confidence:** 8/10, 5/5.
- **Main positive signal:** the paper refuses to call fixture keys production custody or
  same-host files physical replication.
- **Main negative signal:** compromise recovery, KMS/HSM, and independent custody are
  absent.
- **Evidence basis:** evidence zero fields, limitations, claim ledger, artifact guide.
- **Score-change condition:** operational lifecycle under independent custody.

### Reviewer 7 — Artifact and Reproducibility

- **Likely score / confidence:** 8/10, 5/5.
- **Main positive signal:** the artifact preserves before/after/revoked bytes and
  independently validates both HMAC domains.
- **Main negative signal:** no public immutable archive or independent operator exists.
- **Evidence basis:** manifest, bundle tooling, local/ARM64/amd64 artifact trees.
- **Score-change condition:** public deposit and independent full-data rerun.

### Reviewer 8 — Numerical-Solver Domain Application

- **Likely score / confidence:** 8/10, 5/5.
- **Main positive signal:** solver gains and regressions remain separated from authority
  correctness evidence.
- **Main negative signal:** customer-scale and external native deployment remain absent.
- **Evidence basis:** PDE, sparse, operator, DAE, router, and device sections.
- **Score-change condition:** representative external application traces.

### Reviewer 9 — Evidence and Ablation

- **Likely score / confidence:** 8/10, 5/5.
- **Main positive signal:** one-previous-key migration, new-only restart, and revoked-
  byte failure isolate the lifecycle transition cleanly.
- **Main negative signal:** arbitrary crash points during rotation are not injected.
- **Evidence basis:** preserved boundary artifacts and explicit scope fields.
- **Score-change condition:** systematic interruption matrix under operational custody.

### Reviewer 10 — Novice-Advocate Reader

- **Likely score / confidence:** 8/10, 5/5.
- **Main positive signal:** a reader can identify old trust, new trust, publication
  boundary, revocation outcome, and what is not claimed.
- **Main negative signal:** authentication and persistence background remains useful.
- **Evidence basis:** revised passages and visual inspection.
- **Score-change condition:** no further change required within 12 pages.

### Reviewer 11 — AC / Meta-Review

- **Likely score / confidence:** 8/10, 5/5.
- **Main positive signal:** Round 22 closes a real local fixture gap without overclaim.
- **Main negative signal:** no decisive external axis changes.
- **Evidence basis:** immutable Round 21 concerns, new artifacts, passing gates, scope.
- **Score-change condition:** satisfy native performance, physical authority/external
  freshness, or independent reproduction.

### Panel Synthesis

- **Agreement:** the bounded migration/revocation protocol is credible and claim-aligned.
- **Disagreement:** only whether production lifecycle should remain moderate or major;
  all reviewers agree it cannot raise the overall score by itself.
- **Decisive positive axis:** coherent design, broad evidence, fail-closed fixture
  migration, negative results, and excellent scope discipline.
- **Decisive negative axis:** absent native external performance, physical authority,
  external freshness, and independent reproduction.
- **Unresolved evidence:** production custody, arbitrary interruption, joint rollback,
  cross-host failover, public archive, independent rerun, and final metadata.
- **AC stance:** accept at **8/10**, confidence **5/5**.

## 11. Concerns Table

| ID | Severity | Concern | Evidence basis | Affected criterion | Fix class | Required action | Owner skill | Score-change condition |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| R22-1 | Minor | Closed locally: fixture-key rotation and revocation were untested | One explicit previous key per domain, current-key republication before listen, new-only restart, revoked-byte failure, three verifiers | Soundness, evidence | method/soundness | Preserve bounded semantics and fixtures | Security implementation + integrity audit | Reduces local risk only |
| R22-2 | Major | Joint state-and-witness rollback remains undetected | Same process/filesystem/host; no external anchor | Soundness, significance | experiment | Add independent monotonic authority and coordinated rollback faults | Distributed-systems implementation | Necessary path toward 9 |
| R22-3 | Major | No physical replicas, quorum, partitions, or remote failover | `multi_host=0`, `consensus_protocol=0` | Significance, evidence | experiment | Deploy cross-host replicas and failure campaign | Distributed-systems experiment | Necessary path toward 9 |
| R22-4 | Major | Native external performance remains absent | Authoritative timing is Apple M4; containers are correctness-only | Evidence, external validity | experiment | Repeat complete-cost campaigns externally | Benchmark owner | Performance path toward 9 |
| R22-5 | Major | No public immutable independent reproduction | Author-operated bundle; large payloads omitted | Reproducibility | reproducibility | Deposit artifact/data metadata and obtain independent rerun | Artifact owner | Reproducibility path toward 9 |
| R22-6 | Moderate | Production key lifecycle remains untested beyond fixtures | No provisioning, custody, KMS/HSM, compromise recovery, or arbitrary interruption matrix | Soundness, ethics | method/soundness | Deploy and fault-test an operational lifecycle | Security owner | May support 9 only with external authority |
| R22-7 | Moderate | Final administrative and policy metadata is incomplete | Placeholder author/funding/acknowledgment fields | Readiness | writing | Finalize metadata and submission checks | Author | Desk/readiness only |

## 12. AC / Meta-Review

Round 22 is a successful bounded scientific revision. It replaces an untested fixture
gap with a durable old-to-current trust transition, a new-key-only restart, and
revoked-byte fail-closed evidence across local, ARM64, and emulated x86-64 paths. The
independent verifier checks the actual preserved bytes and records rather than trusting
server counters.

The revision does not solve production key management. The keys remain fixtures; state,
mirror, and witness remain same-host; coherent joint rollback remains possible; and no
KMS/HSM, independent custody, physical replication, or external freshness exists.
These boundaries are stated consistently, so the new evidence strengthens soundness
without creating an overclaim.

The scientific ceiling is unchanged. The paper still lacks the external performance,
physical distributed-fault, independent-freshness, and public-reproduction evidence
that separates a strong local systems artifact from a 9--10 TPDS paper.

**AC stance:** **8/10 — accept**, confidence **5/5**. Credit the local protocol; do not
inflate the overall score from same-host fixtures.

## 13. Quantitative Scores

| Dimension | Score | Confidence | Evidence basis | Deduction and repair condition |
| --- | ---: | ---: | --- | --- |
| Quality | 4/5 | 5/5 | Coherent formal, implementation, experiment, and artifact package | External authority/performance incomplete; add native and distributed validation |
| Clarity | 4.74/5 | 5/5 | Bounded migration, revocation, and scope are concise and visually stable | Remaining density reflects breadth and fixed length |
| Significance | 4/5 | 4/5 | Broad numerical-service and verification relevance | Production distributed and customer-scale evidence absent |
| Originality | 4/5 | 4/5 | Verification-aware complete-cost fusion remains coherent | Fixture rotation strengthens implementation, not novelty |
| Soundness | 4/5 | 5/5 | Formal assumptions plus authenticated recovery, migration, and failure evidence | Joint rollback and external authority remain absent |
| Evidence | 4/5 | 5/5 | Paired statistics, negative results, three-path authority evidence, preserved bytes | Native external performance and physical faults absent |
| Reproducibility | 4/5 | 5/5 | Tests, manifest, locks, verifier, deterministic bundle | Public immutable archive and independent rerun absent |
| Ethics / Limitations | 5/5 | 5/5 | Fixture, same-host, emulation, custody, and rollback limits are explicit | Preserve final metadata consistency |

### Writing Review Scorecard

| Dimension | Weight | Score | Confidence | Evidence basis | Concrete repair |
| --- | ---: | ---: | ---: | --- | --- |
| Storyline and motivation | 12 | 5/5 | 5/5 | Problem, gap, insight, evidence, and boundary remain explicit | Preserve sequence |
| Contribution display | 12 | 5/5 | 5/5 | Contributions are visible and unchanged | Do not promote fixture protocol to novelty |
| Paragraph logic | 10 | 5/5 | 5/5 | New passages retain mechanism → result → boundary roles | Preserve progression |
| Claim-evidence alignment | 14 | 5/5 | 5/5 | Fixture claims map to preserved bytes and verifier checks | Keep ledgers synchronized |
| Method readability | 10 | 4/5 | 5/5 | Dense but reproducible systems detail | No further edit without new space/evidence |
| Results readability | 10 | 5/5 | 5/5 | Positive, negative, and fault outcomes remain separated | Preserve ordering |
| Experiment narration | 10 | 5/5 | 5/5 | Ten starts, six failures, and migration outcomes are concise | Preserve counts |
| Related-work positioning | 8 | 4/5 | 4/5 | Closest work is positioned on technical axes | Refresh only if claims broaden |
| Terminology and notation consistency | 8 | 5/5 | 5/5 | Current/previous, witness, fixture, same-host, and emulation terms are stable | Preserve terminology audit |
| LaTeX and format discipline | 8 | 5/5 | 5/5 | Exact 12 pages, no overfull boxes, balanced affected pages | Preserve format gate |
| Reviewer-facing risk | 8 | 4/5 | 5/5 | Fixture limits are prominent; external evidence remains decisive | Do not inflate local claims |

**Weighted writing score:** **4.74/5**. **Writing risk:** low.

- **Overall:** **8/10 — accept**.
- **Confidence:** **5/5**.
- **Score-change condition:** only new external evidence on native performance, physical
  authority/external freshness, or independent reproduction can support movement
  toward 9. More same-host fixtures or wording alone cannot.

No criterion is 3 or below; no fatal concern is averaged away.

## 14. Questions for Authors

1. Which independent service or hardware root will provision, retain, revoke, and
   recover production state and witness keys?
2. What crash/interruption matrix will define atomicity during operational key rotation?
3. What independent authority will preserve freshness during disconnection and detect
   coherent state-and-witness rollback?
4. Which native external systems will repeat complete verified cost?
5. What public archive and independent operator will reproduce the full-data campaign?

## 15. Score Revision Criteria

### Raising the Score Would Require

- Native external paired complete-cost evidence on representative systems.
- Physical cross-host state/witness authority with independent failure domains,
  partitions, quorum, split brain, leader loss, recovery, and remote failover.
- Externally anchored monotonic freshness detecting joint state/witness rollback.
- Public immutable preservation and an independent full-data rerun.
- Operational key provisioning, custody, rotation, revocation, and compromise recovery
  only if coupled to an independent authority rather than more same-host fixtures.

One major external axis could support 9 if soundness and claim discipline remain. A 10
requires several axes to converge without a new central weakness.

### Lowering the Score Would Be Triggered By

- Calling fixture migration a production key lifecycle or KMS/HSM integration.
- Calling same-host files physical replicas or an external monotonic anchor.
- Treating emulated x86-64 correctness as native performance.
- Generalizing one-generation witness recovery or one-previous-key migration beyond the
  tested protocol.
- Dropping negative results, finite-sample limits, or fixture-key status.
- Failing evidence, test, page, manifest, archive, or verifier gates.

### Concerns Unlikely to Change Before Submission

- Native external performance and independent reproduction require external resources.
- Physical replicated authority, external freshness, and production custody require
  deployment work.

## 16. Action Plan and CCFA Handoffs

1. **Priority:** immediate. **Action:** update latest-review pointers and freeze Round 22
   in the deterministic archive. **Owner skill:** artifact/integrity owner.
   **Input needed:** final source and evidence tree. **Expected output:** byte-stable
   archive, sidecar, contract, and clean-tree proof. **Handoff required:** no.
2. **Priority:** immediate. **Action:** rerun stale-term, duplicate-field, allowlist,
   claim, sentence, visual, page, and manifest audits. **Owner skill:** integrity audit.
   **Input needed:** final Round 22 tree. **Expected output:** zero stale or unsupported
   claims. **Handoff required:** no.
3. **Priority:** submission. **Action:** finalize author, affiliation, funding, conflict,
   acknowledgment, policy, and public-archive metadata. **Owner skill:** authors.
   **Input needed:** institutional and submission data. **Expected output:** complete
   submission metadata. **Handoff required:** yes.
4. **Priority:** decisive external. **Action:** run native performance and physical
   replicated-authority campaigns with external freshness and an independent operator.
   **Owner skill:** benchmark and distributed-systems owners. **Input needed:** external
   systems and independent custody. **Expected output:** evidence capable of moving the
   score toward 9. **Handoff required:** yes.

### Checks Run

- Passed local, Ubuntu ARM64, and emulated x86-64 authority verifiers:
  `snapshots=4 recoveries=2 failures=6`.
- Confirmed 90-field parity across the three ledgers; only deployment and namespace
  indicators differ as intended.
- Passed `paper/check_evidence.py`, `paper/check_artifact_manifest.py`, and
  `paper/check.sh`.
- Rebuilt the PDF at exactly 12 pages and 289,476 bytes without overfull boxes.
- Visually inspected affected rendered pages 7, 10, and 11.
- Confirmed revised manuscript sentences are at most 18 words.
- Passed 29/29 Release CTests.
- Confirmed no stale v5/eight-start/five-failure wording outside immutable Round 21.

### Checks Skipped

- Native external performance, physical cross-host authority, external monotonic
  freshness, production KMS/HSM custody, public independent full-data rerun, and final
  submission metadata.
- New public novelty search because the abstract and contribution list did not broaden.

### Unresolved Risks

- Joint state-and-witness rollback and absent external freshness.
- Physical replicas, partitions, quorum, election, remote failover, and native external
  performance.
- Production custody and compromise recovery.
- Public independent reproduction and final metadata.

## 17. Output Self-Check

- Overall score, criterion scores, writing score, and confidence are separated.
- R21-6 is credited only for its locally tested fixture subset.
- No scientific score is inflated from same-host files, emulation, or wording.
- No claim, count, negative result, or scope boundary was removed.
- Same-host files are not called physical replicas or external anchors.
- Fixture keys are not called production lifecycle, custody, or KMS/HSM.
- Emulated correctness is not called native performance.
- The exact 12-page constraint is preserved without a layout hack.
