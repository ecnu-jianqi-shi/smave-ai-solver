# CCF-A Full Review — Round 18

## Report Metadata

- Review date: 2026-07-25.
- Mode: full scientific, writing, format, artifact, integrity, and reproducibility review.
- Target venue: IEEE Transactions on Parallel and Distributed Systems, regular paper.
- Paper: *Verification-Aware Expert Fusion with Parallel Original-Equation Gates for
  Repeated Numerical Solves*.
- Materials: current 12-page PDF and sources; implementation and 29-test suite; local,
  Ubuntu ARM64, and emulated x86-64 authenticated-authority evidence; primary/mirror
  current/previous snapshots; wrong-key, all-copy-corruption, and authenticated-fork
  failure records; independent Python verifier; deterministic core bundle; data locks;
  and Rounds 1--17.
- Search basis: no new public novelty search. Round 18 changes a local recovery
  soundness property, not the paper's novelty or related-work position.

## Venue and Assumptions

- The review assumes an IEEE TPDS regular-paper submission and the repository's explicit
  12-page manuscript constraint.
- Docker ARM64 and emulated x86-64 runs are correctness-only.
- Docker namespaces remain on one physical host.
- A mirrored file written by one server is not treated as physical replication,
  consensus, quorum, partition tolerance, or remote failover.
- The external test key is a public fixture, not production key custody.

## Desk Rejection Assessment

- **Length — pass.** The manuscript rebuilds at exactly 12 pages.
- **Topic fit — pass.** Parallel verification, numerical-service routing, durable
  authority, and heterogeneous execution fit TPDS.
- **Minimum scientific quality — pass.** The paper has a formal authority proposition,
  implemented mechanisms, baselines, repeated experiments, ablations, negative results,
  and explicit limitations.
- **Artifact inspectability — pass locally.** Source, tests, raw summaries, byte locks,
  fault fixtures, and independent parsers are available.
- **Administrative readiness — unresolved.** Author, affiliation, funding, conflict,
  acknowledgment, review-mode, and final artifact metadata remain incomplete.

**Desk risk:** low scientifically and medium administratively.

## Paper Summary

The paper presents verification-aware expert fusion for repeated numerical solves.
Candidate generation, correction, family-specific original-equation gates, publication,
and fallback form a typed transaction optimized by verified complete cost. Under stated
isolation, immutability, atomic-publication, and fallback assumptions, only a prior
committed state or a gate-accepted state may become caller-visible.

The empirical package spans classical and learned experts, complete-path timings,
strict-gate and certificate studies, intra-node thread/process scaling, data-provenance
locks, portability checks, and an untimed TCP authority probe. The paper preserves
negative device, routing, topology, and scaling results instead of generalizing from
successful cases.

## Round 18 Contribution

Round 17 selected the highest valid HMAC-authenticated generation across four
primary/mirror copies. It did not reject two valid copies that claimed the same highest
generation but contained different canonical bodies; iteration order could choose one.

Round 18 closes that ambiguity. Every successfully parsed snapshot now retains its
authenticated body SHA-256. Before selecting state, startup compares all valid copies at
the highest generation. Divergent authenticated bodies trigger
`authenticated-highest-generation-fork`, write a durable pre-listen failure record, and
exit 88 without deleting valid copies, opening a socket, or reinitializing blank state.

The fault runner copies all four healthy generations, modifies one highest-generation
body, and recomputes both SHA-256 and HMAC with the fixture key. The independent Python
verifier proves that all four copied snapshots authenticate, that primary and mirror
current copies share the same generation, and that their authenticated bytes differ.
The complete three-failure contract passes locally, in Ubuntu ARM64, and under emulated
x86-64.

This is authenticated same-generation fork detection, not rollback freshness. If every
valid signed copy is reverted consistently, the local authority has no external
monotonic observation with which to reject it. The evidence records therefore preserve
`external_monotonic_anchor=0` and `all_valid_copies_rollback_detection=0`.

## Likely Stance and Calibrated Score

**Overall: 8/10 — accept. Scholarly confidence: 5/5.**

Round 18 removes a real deterministic-selection flaw and strengthens fail-closed local
recovery. It improves the authority soundness evidence within the stated same-host
contract. It does not change the decision-level ceiling: the mirror still shares one
process, filesystem, host, key domain, and operator; no external monotonic anchor,
physical replicas, partition campaign, quorum, remote failover, native external
performance, public preservation, or independent rerun was added.

Raising the overall score for this local integrity refinement would over-credit artifact
engineering as distributed-systems evidence. The strongest defensible decision remains
8/10.

## Quantitative Scorecard

| Dimension | Score | Confidence | Evidence basis | Deduction and raise condition |
| --- | ---: | ---: | --- | --- |
| Novelty | 4/5 | 4/5 | Verification-aware expert fusion and typed commit authority are coherently combined | No new algorithmic theorem or external novelty result; raise with a clearly differentiated mechanism and broader external validation |
| Technical soundness | 4/5 | 5/5 | Formal assumptions, fail-closed recovery, independent HMAC verification, and authenticated-fork rejection | Same failure/key domain and no all-copy rollback freshness; raise with external monotonic or quorum authority |
| Experimental rigor | 4/5 | 5/5 | Repeated paired timing, confidence intervals, negative results, three-platform functional authority faults | Authoritative performance remains one-host; raise with native external systems and discrete accelerators |
| Significance | 4/5 | 4/5 | Broad numerical-service integration and explicit complete-cost framing | Production and customer-scale distributed operation remain unverified |
| Reproducibility | 4/5 | 5/5 | Deterministic archive, 29/29 tests, data locks, fault records, independent parser | No persistent public identifier or independent operator |
| Writing and presentation | 4.54/5 | 5/5 | Claims, evidence, and scope boundaries remain visible within 12 pages | Dense presentation remains, but no decision-level writing defect is present |
| Ethics and limitations | 5/5 | 5/5 | Fixture-key, same-host, emulation, negative-result, license, and rollback boundaries are explicit | Maintain these boundaries in submission metadata and artifact claims |

No criterion is scored 3 or below; therefore no hidden fatal scientific defect is being
masked by the overall recommendation.

## Top Strengths

1. The new fixture is cryptographically valid rather than merely checksum-corrupt, so
   rejection depends on an explicit fork invariant.
2. Fork detection runs before state mutation and before listen, preserving fail-closed
   authority semantics.
3. The Python verifier independently recomputes both digest layers and confirms the
   generation/body divergence instead of trusting C++ output labels.
4. The evidence separates local signed-fork detection from externally anchored rollback
   freshness.
5. Local, ARM64, and emulated x86-64 paths execute the same three negative-startup
   fixtures with `performance_evidence=0` and `multi_host=0`.
6. The manuscript adds the new result without dropping the same-process, fixture-key,
   physical-replication, consensus, or all-copy-rollback boundaries.

## Major Concerns

1. **No independent failure domain.** Primary and mirror remain files written by one
   process on one filesystem and host. A shared process, storage, host, or key failure
   can affect all copies.
2. **No all-copy rollback freshness.** Consistently reverting every valid signed copy
   remains undetectable because no external monotonic witness or replicated log exists.
3. **No consensus or remote failover.** The system has no quorum, leader election,
   partition behavior, split-brain protocol, or cross-host recovery campaign.
4. **External performance remains absent.** Native Linux/x86-64, NUMA, multi-node, and
   discrete-accelerator complete-cost evidence is missing.
5. **Independent reproduction remains absent.** The archive is author-operated and has
   no immutable public deposit, persistent identifier, or external rerun.
6. **Production key lifecycle remains absent.** The fixture does not establish
   provisioning, separation, rotation, revocation, compromise response, KMS, or HSM use.
7. **Submission metadata remains incomplete.** Administrative placeholders still create
   avoidable submission risk.

## Claim-Evidence Audit

| Claim | Evidence | Assessment | Boundary |
| --- | --- | --- | --- |
| Highest valid generation is selected | Four authenticated current/previous primary/mirror copies and replay checks | Strong local functional evidence | One process/filesystem/host |
| Plain-checksum forgery is rejected | Modified body, recomputed SHA-256, stale HMAC | Strong authentication evidence | Fixture key, not production custody |
| Same-generation authenticated fork is rejected | Valid HMAC on divergent highest-generation primary/mirror bodies; exit 88 before listen | Strong local fork-detection evidence | Does not detect consistent rollback of all copies |
| Primary-pair loss recovers from mirror | Both primary generations removed; replay recovers from mirror | Strong same-host availability evidence | Mirror is not an independent replica |
| Wrong key and all corrupt copies fail closed | Four wrong-key and four checksum-invalid copies | Strong pre-listen failure evidence | No external quorum or witness |
| Complete-path speedups hold | Pinned paired benchmark reports and intervals | Strong for measured host/workloads | No native external performance |
| Artifact is reproducible | Deterministic clean-tree archive and 29/29 tests | Strong author-operated local evidence | No public independent reproduction |

No current manuscript claim requires interpreting Docker namespaces as physical hosts,
HMAC as freshness, emulation as native performance, or the local mirror as consensus.

## Writing and Presentation Concerns

- The authority paragraph is necessarily dense because it carries mechanism, fault
  campaign, and scope boundaries in limited space.
- “Fork” is qualified as authenticated same-generation divergence; it is not used as a
  synonym for general distributed fork prevention.
- The limitations section explicitly states that consistent all-copy rollback remains
  undetectable.
- The added wording preserves the exact 12-page limit without font, margin, spacing, or
  float-placement hacks.

**Weighted writing score: 4.54/5. Writing risk: low.**

## Format and Venue Concerns

- Exactly 12 pages: pass.
- No unresolved citation/reference or overfull-box warning: pass.
- IEEE-style structure, figures, tables, and references: pass locally.
- Final author/funding/conflict/acknowledgment/review metadata: unresolved.
- Final TPDS policy and artifact-anonymity check: required immediately before submission.

## Multi-Reviewer Panel

### Reviewer 1 — Distributed Systems

- **Score / confidence:** 8/10, 5/5.
- **Positive signal:** highest-generation divergence now fails closed deterministically.
- **Negative signal:** no independent replicas, partition protocol, or quorum.
- **Score-change condition:** physical cross-host replicas with partition/failover tests.

### Reviewer 2 — Security and Integrity

- **Score / confidence:** 8/10, 5/5.
- **Positive signal:** the fork fixture carries valid SHA-256 and HMAC, and Python
  independently verifies it.
- **Negative signal:** shared fixture key and no external monotonic freshness authority.
- **Score-change condition:** separated production keys plus externally anchored
  rollback/fork detection.

### Reviewer 3 — Numerical Systems

- **Score / confidence:** 8/10, 5/5.
- **Positive signal:** original-equation authority, complete-cost reporting, and negative
  numerical results remain intact.
- **Negative signal:** external native and accelerator performance remains absent.
- **Score-change condition:** representative native x86-64 and discrete-accelerator
  complete-path campaigns.

### Reviewer 4 — Artifact and Reproducibility

- **Score / confidence:** 8/10, 5/5.
- **Positive signal:** fault inputs and failure outputs are included in the deterministic
  artifact contract and independently parsed.
- **Negative signal:** no immutable public deposit or independent operator.
- **Score-change condition:** persistent public archive plus documented independent rerun.

### Reviewer 5 — Writing and Scope

- **Score / confidence:** 8/10, 5/5.
- **Positive signal:** fork detection and rollback freshness are sharply separated.
- **Negative signal:** the manuscript is information-dense and metadata remains pending.
- **Score-change condition:** finalize metadata; prose-only changes do not raise the
  scientific score.

### Panel Synthesis

- **Agreement:** all reviewers regard the fork check as a real local soundness fix.
- **Disagreement:** none on the overall score; reviewers differ only on which external
  evidence should be prioritized first.
- **Decisive accept axis:** coherent verification-aware system, strong local evidence,
  explicit assumptions, preserved failures, and inspectable artifact.
- **Decisive reject axis:** a reviewer demanding production distributed authority or
  external performance could still downgrade the paper.
- **Unresolved evidence:** physical replication, monotonic freshness, native external
  performance, public preservation, independent rerun, and final metadata.
- **Final calibrated stance:** accept at 8/10, confidence 5/5.

## Concern-to-Action Table

| ID | Severity | Concern | Required action | Score impact condition |
| --- | --- | --- | --- | --- |
| R18-1 | Closed locally | Same-generation valid authenticated copies could diverge | Preserve fork selector, fixture, and Python checks | Prevents regression; no overall inflation |
| R18-2 | Major | Consistent all-copy rollback is undetectable | Add external monotonic witness or authenticated replicated log | Soundness path toward 9 |
| R18-3 | Major | Shared process/filesystem/host failure domain | Deploy physical replicas and independent storage faults | Distributed-evidence path toward 9 |
| R18-4 | Major | No quorum, partition, election, or remote failover | Implement and inject cross-host network failures | Significance/evidence path toward 9 |
| R18-5 | Major | Native external performance absent | Run paired complete-cost campaigns externally | Performance path toward 9 |
| R18-6 | Major | Public independent reproduction absent | Deposit frozen artifact/data and recruit operator | Reproducibility path toward 9 |
| R18-7 | Moderate | Production key lifecycle absent | Specify and test provisioning/rotation/revocation | Security maturity |
| R18-8 | Moderate | Submission metadata incomplete | Finalize administrative fields and policy checks | Readiness only |

## Recommended Next CCFA Owner

- **Primary:** artifact/experimental implementation for an external monotonic witness or
  physical cross-host replicated authority.
- **Secondary:** integrity audit after any new external evidence is imported.
- **Writing owner:** only after new evidence exists; wording alone cannot satisfy the
  score-change conditions.

## Checks Run

- Built the modified C++ authority executable with the existing warning policy.
- Passed Python syntax checks for local runner, independent verifier, manifest checker,
  and bundle generator.
- Passed local `reproduce-gate-network-authority` with C++ HMAC self-test, six healthy
  starts, and three exit-88 startup failures.
- Passed Ubuntu ARM64 two-container authority execution.
- Passed emulated x86-64 two-container authority execution.
- Independently verified all healthy, wrong-key, corrupt, and forked snapshots in Python.
- Confirmed forked current copies share a generation, differ in authenticated bytes, and
  produce a pre-listen failure record.
- Passed 29/29 Release CTests.
- Passed `paper/check_evidence.py` and `paper/check_artifact_manifest.py`.
- Rebuilt the manuscript at exactly 12 pages with no forbidden log warning.

## Unresolved or Unverified

- External monotonic generation/fork witness and consistent all-copy rollback rejection.
- Physical cross-host replicas, independent storage, partitions, quorum, election, and
  remote failover.
- Native Linux/x86-64, NUMA, multi-node, multi-GPU, and discrete-accelerator complete
  performance.
- Production key custody, rotation, revocation, compromise recovery, KMS, and HSM.
- Public immutable artifact/data preservation and independent rerun.
- Final author, affiliation, funding, conflict, acknowledgment, review-mode, and
  artifact-anonymity metadata.

## Output Self-Check

- The report separates criterion score, overall score, and confidence.
- Every score is tied to inspectable current evidence.
- No acceptance probability or fabricated external result is stated.
- Same-generation fork detection is not called rollback freshness.
- The mirror is not called physical replication, quorum, or consensus.
- Docker namespaces are not called physical hosts.
- Emulated x86-64 is not called native performance.
- The score remains 8 because decision-level external evidence is still absent.
