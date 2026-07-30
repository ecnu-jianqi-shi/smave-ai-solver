# CCF-A Full Review — Round 17

## Report Metadata

- Review date: 2026-07-25.
- Target venue: IEEE Transactions on Parallel and Distributed Systems, regular paper.
- Paper: *Verification-Aware Expert Fusion with Parallel Original-Equation Gates for
  Repeated Numerical Solves*.
- Materials: current 12-page PDF and sources; implementation and 29-test suite; local,
  Ubuntu ARM64, and emulated x86-64 authority evidence; authenticated snapshot source,
  fault runner, independent Python verifier, state/failure records; data locks, core
  bundle, pinned summaries, and Rounds 1--16.
- Mode: full scientific, writing, artifact, integrity, reproducibility, and TPDS-fit
  re-review after authenticated mirrored authority recovery.
- Search basis: no new novelty search; Round 17 changes authority soundness and evidence,
  not the paper's related-work claim. The same-day official TPDS policy basis from Round
  16 remains applicable.

## Desk Rejection Assessment

- **Length — pass.** The paper rebuilds at exactly 12 pages.
- **Topic compatibility — pass.** Numerical services, parallel verification,
  transaction authority, durable recovery, and heterogeneous execution fit TPDS.
- **Minimum quality — pass.** Method, formal assumptions, implementation, baselines,
  repeated evidence, ablations, negative results, limitations, and artifact gates exist.
- **Policy / anonymity / compliance — uncertain.** Author, affiliation, funding,
  conflict, acknowledgment, and final review-mode metadata remain placeholders.
- **Prompt manipulation — pass.** No reviewer- or model-directed hidden instruction was
  found in manuscript or artifact sources.
- **Ethics and reviewability — pass.** License, failure, key-fixture, local-only, and
  non-production boundaries are explicit.

**Desk risk:** low scientifically; medium administratively until metadata is finalized.

## Paper Summary and Round 17 Contribution

The paper frames heterogeneous repeated numerical solving as verification-aware expert
selection. Candidate generation, correction, family-specific original-equation gating,
publication, and fallback form a typed transaction optimized by complete verified cost.
Under explicit isolation, immutability, publication, and fallback assumptions, the
commit-authority proposition permits only a previous committed state or a gate-accepted
state to become caller-visible.

Round 17 strengthens the durable authority boundary. The TCP authority now writes the
same canonical state to primary and mirrored stores, each with current and previous
generations. Every snapshot carries SHA-256 plus HMAC-SHA-256 under an externally
supplied key identifier. Startup verifies four candidates and selects the highest valid
authenticated generation. A C++/Python cross-check independently recomputes both digest
layers.

Six healthy starts cover persist-before-reply recovery, prepublication crash cleanup,
checksum rejection, a forged body with recomputed plain checksum but stale HMAC,
valid-stale-current rejection, and recovery after both primary generations are lost.
Four idempotent replays retain exactly three accepted transactions, one gate rejection,
one conflict, one malformed request, and zero authority mismatch. Separate wrong-key and
all-four-corrupt copied-state attempts write pre-listen failure evidence and exit 88
without blank-state initialization. The complete contract passes locally and in Ubuntu
ARM64 plus emulated x86-64 Docker namespace runs.

## Expected Review Outcome

**Overall: 8/10 — accept. Scholarly confidence: 5/5.**

The new result closes two real local weaknesses: a plain checksum can no longer be
forged without the key, and losing both primary generations no longer necessarily loses
availability when the mirror remains valid. Cross-language recomputation prevents the
C++ implementation from being its own only authentication oracle.

The overall score remains 8. The mirror is written by the same process onto the same
filesystem and physical host, and the retained key is a reproducible test fixture rather
than production custody. There is no consensus, independent failure domain, physical
replication, partition, election, remote failover, external monotonic anchor, or signed
rollback freshness. No new external performance or independent rerun was added. Raising
the score for local authentication alone would be inflation.

## Top Strengths

1. HMAC-SHA-256 covers the canonical transaction-state body while SHA-256 retains a
   separately inspectable integrity layer.
2. The fault campaign specifically recomputes the plain checksum after modifying state;
   rejection therefore depends on authentication rather than accidental checksum damage.
3. Startup considers four generations and recovers the highest valid authenticated copy,
   including a real primary-pair-loss case.
4. Wrong-key and all-copy-corruption tests fail before socket creation and preserve
   explicit failure records instead of silently reinitializing state.
5. Python's standard `hashlib`/`hmac` independently verifies C++ outputs, key identifiers,
   healthy states, wrong-key failures, and corrupt states.
6. Local, ARM64-container, and emulated x86-64-container runs produce the same functional
   contract while retaining `performance_evidence=0` and `multi_host=0`.
7. The paper states that signed historical copies can still be rolled back and that the
   mirror is not physical replication or consensus.

## Major Concerns

1. **No independent failure domain.** Both stores share one process, filesystem, host,
   and publication path. A process bug, filesystem loss, host failure, or coherent
   operator action can affect all copies. Required fix: physically separate replicas
   with independent storage and failure injection.
2. **No freshness authority.** HMAC authenticates content but an old valid signed state
   remains valid. Required fix: externally anchored monotonic generation or an
   authenticated replicated log with rollback/fork detection.
3. **No production key lifecycle.** The evidence uses an external but public fixture key
   so runs are reproducible. It does not test secret provisioning, rotation, revocation,
   compromise recovery, or hardware-backed custody. Required fix: define and test the
   deployment key model.
4. **No consensus or failover protocol.** Mirror recovery occurs inside one server;
   there is no leader election, quorum rule, partition behavior, or remote failover.
   Required fix: a replicated authority design and physical-host fault campaign.
5. **External performance remains absent.** All authoritative timings remain Apple M4;
   containers remain correctness-only. Required fix: native Linux/x86-64 and discrete-
   accelerator complete-cost repetitions.
6. **Public independent reproduction remains absent.** The deterministic core archive
   and data locks are author-operated and local. Required fix: immutable public release,
   persistent identifier, and external operator record.

No fatal concern is introduced by Round 17.

## Claim-Evidence Audit

| Claim | Evidence | Strength | Remaining deduction |
| --- | --- | --- | --- |
| Commit authority is original-equation gated under stated assumptions | Proposition plus transaction/fault probes | Strong within assumptions | Arbitrary callbacks, hardware, and distributed implementation are not formally verified |
| Durable replies survive server crash without duplicate commit | Exit 86, restart, recovered transaction, replay | Strong local functional evidence | One host and one authority process |
| Prepublication crash does not publish an uncommitted state | Exit 87, two orphan temporary removals, retry | Strong local functional evidence | No storage-controller or power-loss campaign |
| Plain-checksum forgery is rejected | Body modification plus recomputed SHA-256 and stale HMAC on two primary copies | Strong authentication mechanism evidence | Fixture key is not production secrecy |
| Primary-pair loss can recover availability | Highest valid mirror generation restores state and replay | Strong same-host mirror evidence | Mirror is not an independent replica |
| Wrong key and all corrupt copies fail closed | Four wrong-key and four checksum-invalid generations; exit 88 before listen | Strong fail-closed evidence | No remote quorum or external anchor |
| Cross-architecture functional behavior matches | Local, Docker ARM64, emulated x86-64 records pass one verifier | Strong correctness portability | Emulated x86-64 is not native performance |
| Full benchmark provenance is locked | Round 16 52-file byte locks and upstream evidence | Strong local provenance | No immutable public mirror or independent full-data rerun |

## Experiment and Reproducibility Audit

- **Fault specificity:** checksum corruption and authenticated forgery are distinct.
- **Recovery specificity:** prior generation, stale current, mirror recovery, wrong key,
  and all-copy failure are separately exercised.
- **Commit accounting:** exactly three accepted commits survive four duplicate replays;
  partial commits and decision/residual mismatches remain zero.
- **Independent verification:** Python recomputes SHA-256 and HMAC rather than trusting
  C++ evidence markers.
- **Portability:** both Docker architectures use separate client/server namespaces but
  the same physical host; no timing is reported.
- **Artifact integration:** keys, four healthy states, both failure directories, source,
  runner, verifier, and evidence are covered by artifact hashing and the core allowlist.
- **Negative boundaries:** fixture key, shared failure domain, no consensus, no physical
  replication, no multi-host result, and signed rollback remain explicit.

## Writing and Presentation Assessment

| Dimension | Weight | Score | Confidence | Basis / repair |
| --- | ---: | ---: | ---: | --- |
| Storyline and motivation | 12 | 5 | 5 | Scientific question and authority boundary remain clear |
| Contribution display | 12 | 5 | 5 | Authentication is evidence support, not relabeled as core novelty |
| Paragraph logic | 10 | 4 | 5 | Methodology authority paragraph is dense but single-purpose |
| Claim-evidence alignment | 14 | 5 | 5 | Ledger names counts, artifacts, and exclusions |
| Method readability | 10 | 4 | 5 | Four-copy recovery is concise; a protocol diagram would help but costs space |
| Experiment narration | 10 | 4 | 5 | Fault-to-claim mapping is explicit but artifact-heavy |
| Related-work positioning | 8 | 4 | 4 | No new search; current framing avoids consensus novelty claims |
| Terminology consistency | 8 | 5 | 5 | Mirror, authentication, replica, consensus, and host boundaries are separated |
| LaTeX and format | 8 | 5 | 5 | Exact 12-page build and no forbidden log warnings |
| Reviewer-facing risk | 8 | 4 | 5 | Main risk is over-reading mirror as replication; wording prevents this |

**Weighted writing score: 4.54/5. Writing risk: low.** No score of 3 or below
requires a rewrite.

## Format and Venue Concerns

- IEEEtran journal/compsoc output remains exactly 12 pages.
- The paper check rejects unresolved citations/references, undefined controls, overfull
  boxes, stale evidence, stale artifact hashes, and non-12-page output.
- Author and acknowledgment files remain placeholders.
- The repository and retained fixture key are not anonymous by default; a deliberate
  double-anonymous submission scrub would be required.

## Multi-Reviewer Panel

### Reviewer 1 — Distributed Systems

- Score: 8/10; confidence 5/5.
- Positive: authenticated highest-generation recovery and pre-listen fail-closed behavior
  are materially stronger than unkeyed local snapshots.
- Negative: one process and one failure domain are not replication or consensus.
- Raise condition: physical replicas with quorum, partition, failover, and fork/rollback
  evidence.

### Reviewer 2 — Numerical Systems

- Score: 8/10; confidence 5/5.
- Positive: authority durability remains tied to original-equation decisions and exact
  replay accounting.
- Negative: external numerical performance and hardware generality are unchanged.
- Raise condition: native external complete-cost replication.

### Reviewer 3 — Security and Integrity

- Score: 8/10; confidence 5/5.
- Positive: the forged-checksum test proves HMAC contributes beyond hashing; wrong-key
  startup is explicitly rejected.
- Negative: no key lifecycle, rollback freshness, hardware root, or adversarial process
  model.
- Raise condition: production key custody plus monotonic/fork detection.

### Reviewer 4 — Artifact and Reproducibility

- Score: 8/10; confidence 5/5.
- Positive: cross-language verification and retention of all healthy/failure states make
  the mechanism inspectable.
- Negative: author-operated local archive, no persistent identifier, no independent run.
- Raise condition: public frozen release and external verifier record.

### Reviewer 5 — Novelty and Positioning

- Score: 8/10; confidence 4/5.
- Positive: the paper does not claim HMAC, mirroring, or consensus as novel; they support
  the transaction contribution.
- Negative: no new related-work search or scientific mechanism changes.
- Raise condition: scientific evidence, not provenance language.

### Reviewer 6 — Writing and Limitations

- Score: 8/10; confidence 5/5.
- Positive: fixture-key, same-process, same-filesystem, signed-rollback, and no-performance
  caveats are adjacent to the result.
- Negative: artifact density remains high and submission metadata is incomplete.
- Raise condition: finalize metadata; no scientific score gain from prose alone.

## AC / Meta-Review

The panel agrees on acceptance at 8/10. Authentication and mirror recovery strengthen
the method's tested failure semantics, and the independent verifier reduces self-check
risk. The decisive negative axis is unchanged: no evidence crosses a physical host,
independent operator, native external performance system, or independent failure domain.
The AC should reject any argument that two directories constitute distributed
replication. The correct interpretation is a strong same-host authenticated durability
probe and a useful precursor to physical replication.

## Quantitative Scores

| Dimension | Score (1--5) | Confidence | Deduction / condition |
| --- | ---: | ---: | --- |
| Novelty | 4 | 5 | Authentication supports rather than changes the core novelty; broader authority mechanism needed for 5 |
| Soundness | 4 | 5 | HMAC and mirror recovery close local faults; external monotonicity, independent replicas, and partitions remain |
| Evidence | 4 | 5 | Fault coverage and cross-language verification are strong; external hardware/host/operator evidence remains absent |
| Significance | 4 | 5 | TPDS relevance is credible; production distributed authority and multi-node resource behavior remain outside scope |
| Clarity | 5 | 5 | Claims and exclusions are explicit |
| Reproducibility | 4 | 5 | Deterministic bundle, data locks, keys, states, and verifiers are strong locally; no public independent rerun |
| Ethics / Limitations | 5 | 5 | Security, license, negative-result, and deployment boundaries are visible |

**Overall:** 8/10 | **Scholarly confidence:** 5/5 | **Recommendation:** accept

### Score-Change Conditions

| Change | Condition | Expected movement |
| --- | --- | --- |
| Raise | Physical replicas with authenticated quorum, partition/failover, and rollback/fork detection | Soundness/evidence/significance; potentially +1 overall |
| Raise | Native x86-64 and discrete-accelerator complete-cost results plus public independent reproduction | Evidence/significance/reproducibility; +1 overall defensible |
| Lower | HMAC verifier disagreement, mirror recovery mismatch, duplicate commit, or pre-listen fail-closed regression | -1 or fatal depending on failure |
| Lower | Wording relabels same-process mirror as physical replication or consensus | Clarity/ethics/evidence -1 |
| No quick change | Physical hardware, external operator, persistent archive, and production key custody | Requires external resources |

## Questions for Authors

1. What production key provisioning, rotation, revocation, and compromise-recovery model
   would replace the public test fixture?
2. What external monotonic or quorum mechanism will reject rollback to an older valid
   signed generation?
3. Can primary and mirror stores be moved to independent physical hosts and storage,
   then tested under partitions and asymmetric failure?
4. When will native Linux/x86-64 and discrete-accelerator complete-cost runs be added?
5. Where will the immutable artifact/data record and independent rerun be preserved?

## Concern-to-Action Table

| ID | Severity | Concern | Required action | Score condition |
| --- | --- | --- | --- | --- |
| R17-1 | Major | Shared process/filesystem/host failure domain | Implement physical replicas and independent storage fault tests | Main authority path to 9 |
| R17-2 | Major | No rollback freshness or fork detection | Add external monotonic anchor or authenticated replicated log | Soundness +1 |
| R17-3 | Major | No consensus, partition, or remote failover | Define quorum/election protocol and inject network faults | Significance/evidence +1 |
| R17-4 | Major | Native external performance absent | Repeat representative paired workloads externally | Main performance path to 9 |
| R17-5 | Major | Public independent reproduction absent | Publish frozen artifact/data record and recruit operator | Reproducibility +1 |
| R17-6 | Moderate | Production key lifecycle absent | Specify and test provisioning/rotation/revocation | Security maturity |
| R17-7 | Moderate | Submission metadata incomplete | Finalize authors, funding, conflicts, acknowledgments, review mode | Readiness only |
| R17-8 | Closed locally | Unkeyed snapshot forgery and primary-pair loss | Preserve HMAC/mirror/verifier fault gates | Prevent regression; no score inflation |

## Checks Run

- Built the focused authority executable and HMAC API with warning flags.
- Passed the local authority target, including C++ HMAC self-check, six starts, four
  replays, checksum/HMAC/stale rejection, mirror recovery, and two exit-88 failures.
- Independently recomputed four healthy C++ snapshots using Python SHA-256/HMAC.
- Verified four wrong-key rejections and four checksum-corrupt generations.
- Passed the complete Docker ARM64 two-namespace authority campaign.
- Passed the complete emulated Docker x86-64 two-namespace authority campaign.
- Preserved `performance_evidence=0`, `multi_host=0`, `consensus_protocol=0`, and
  `production_distributed_commit=0` in all three records.
- Passed manuscript evidence checks against the updated ledger.
- Rebuilt the PDF at exactly 12 pages with no forbidden log warning.
- Confirmed the configured suite remains exactly 29 CTests.

## Unresolved or Unverified

- Native Linux/x86-64 performance and discrete-accelerator complete cost.
- Physical multi-host replicas, partitions, quorum, election, and remote failover.
- External monotonic generation/fork authority and signed rollback freshness.
- Production secret custody, rotation, revocation, and hardware-backed keys.
- Public immutable artifact/data preservation and independent rerun.
- NUMA, multi-node, multi-GPU, and network performance.
- Final author, affiliation, funding, conflict, acknowledgment, review-mode, and
  artifact-anonymity metadata.

## Output Self-Check

- The mirror is never called physical replication or consensus.
- The public fixture key is not called production authentication custody.
- HMAC integrity is not called rollback freshness.
- Emulated x86-64 is not called native performance.
- Docker namespaces are not called physical hosts.
- Negative results and external blockers remain visible.
- The score does not increase for local security/provenance engineering alone.
- No acceptance probability, invented result, or fabricated metadata is present.
