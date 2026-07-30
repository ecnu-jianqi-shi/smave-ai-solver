# CCF-A Full Review — Round 15

## Mode

Full scientific, writing, artifact, integrity, reproducibility, and TPDS-fit
re-review after adding a deterministic core reproduction bundle and an
author-operated clean extracted-tree rerun.

## Venue and Assumptions

- Target: IEEE Transactions on Parallel and Distributed Systems regular paper.
- Review date: 2026-07-25.
- Materials: current 12-page PDF and LaTeX sources; implementation and tests;
  normalized archive generator; internal and external checksum verifiers; clean-tree
  run record; pinned evidence and 651 raw reports; Rounds 1--14.
- Current official policy check: the TPDS author page routes submissions through the
  IEEE Computer Society journal workflow; the current peer-review policy permits
  either single-anonymous review or optional double-anonymous review when authors
  remove identifying material. IEEE Computer Society journal submission guidance
  lists TPDS regular papers at 12 pages before overlength charges. The draft is 12
  pages, but author/funding/conflict and final review-mode metadata remain unresolved.
- Artifact boundary: the core archive excludes the approximately 47-GB PDEBench data
  and 37 non-fixture SuiteSparse system matrices. It is author-operated and local,
  without a public persistent identifier or an independent operator.
- Overall scale: CCFA 1--10; criterion scale: 1--5.

## Paper Summary

The paper presents verification-aware expert selection for repeated numerical
workloads. A typed transaction composes candidate generation, correction,
family-specific original-equation verification, publication, and classical fallback
under a reach-weighted complete-cost objective. An assumption-indexed proposition
makes commit authority conditional on isolation, immutable problem state, atomic
publication, and fallback behavior.

The evidence package spans seven paired PDE workloads, all-family order controls, two
held-out operator families with a shared control, routing studies, heterogeneous device
probes, thread/process scaling, complete-path and batch scaling, fault ablations,
negative transfer, same-host Linux portability, and phased TCP authority recovery.

Round 15 improves delivery rather than scientific scope. The packager allowlists
source, paper, pinned summaries, authority records, 217 PDE timing reports, 434 order
reports, and two Matrix Market fixtures. It normalizes path order, mtime, UID, and GID;
embeds SHA-256 for every file; writes a sidecar archive hash; and compares two archive
generations byte-for-byte. The verifier checks archive metadata and both hash layers,
extracts to a fresh temporary directory, configures Release with an explicit
core-bundle option, builds, confirms and passes 29/29 CTests, reruns the local TCP
authority target, checks manuscript evidence and artifact hashes, and rebuilds the
12-page PDF. The machine record explicitly sets `author_operated=1`,
`public_archive=0`, `independent_reproduction=0`, `native_external_performance=0`,
and `physical_multi_host=0`.

## Likely Stance and Calibrated Score

**Overall: 8/10 — accept. Scholarly confidence: 5/5.**

Round 15 raises reproducibility quality within the local environment. The archive is
small enough to inspect, deterministic under two consecutive generations, internally
self-checking, and actually exercised from a new extracted tree. Its exclusions are
explicit rather than silently converting a full benchmark claim into a fixture-only
claim. The normal build retains the 39-matrix assertion; only the named bundle mode
expects the two included parser fixtures.

The score remains 8 rather than 9. The new result is still produced and rerun by the
same author on the same Apple M4 system. It is not public, immutable through an
external repository, independently operated, or complete with the benchmark datasets.
It adds no native Linux/x86-64 performance, discrete-accelerator complete-cost evidence,
authenticated monotonic state, replication, physical cross-host failover, or network
performance. Raising the overall score for packaging alone would be score inflation.

## Quantitative Scorecard

| Dimension | Score (1--5) | Confidence (1--5) | Evidence basis | Deduction / score-change condition |
|:---|:---:|:---:|:---|:---|
| Novelty | 4 | 5 | `paper/sections/05_verification_aware_fusion.tex:4`; `paper/sections/03_problem_formulation.tex:23` | The contribution remains verification-aware transaction and complete-cost co-design, not the packaging mechanism. A 5 needs authenticated replicated or broader externally validated authority. |
| Soundness | 4 | 5 | `tests/gate_network_authority_evidence.cpp`; `artifact/verify_core_repro_bundle.py:89`; `paper/sections/09_discussion_limitations.tex:34` | Clean extraction checks the tested implementation, but coherent rollback, replicated recovery, partition, and failover remain outside evidence. |
| Evidence | 4 | 5 | `build/core-repro-bundle/clean-tree-evidence.txt`; `paper/CLAIM_EVIDENCE.md`; pinned raw reports | The evidence package is inspectable and failure-preserving; a 5 requires native external performance or independent physical-host reproduction. |
| Significance | 4 | 5 | `paper/sections/01_introduction.tex:45`; `paper/sections/09_discussion_limitations.tex:72` | TPDS relevance is credible, but multi-node resource management and recovery availability remain absent. |
| Clarity | 5 | 5 | `paper/sections/06_experimental_methodology.tex:171`; `paper/sections/09_discussion_limitations.tex:32` | Bundle success, dataset exclusion, authorship, public status, and independent-reproduction status are separated explicitly; no deduction. |
| Reproducibility | 4 | 5 | `artifact/make_core_repro_bundle.py`; `artifact/verify_core_repro_bundle.py`; `build/core-repro-bundle/contract.txt` | Deterministic generation and a successful clean extraction close local packaging ambiguity. A 5 requires a public immutable identifier, complete acquisition contract, and independent rerun. |
| Ethics / Limitations | 5 | 5 | `paper/sections/09_discussion_limitations.tex:16`; `paper/CLAIM_EVIDENCE.md` | Negative results and excluded claims remain visible; no deduction. |

**Overall:** 8/10 | **Scholarly Confidence:** 5/5

**Recommendation:** accept

**Verdict:** A public persistent release with licensed acquisition instructions and an
independent rerun, combined with native external performance or physical replicated
authority, could move the paper to 9. A hidden dataset dependency, non-deterministic
archive, fixture-only result mislabeled as full-data reproduction, or failed clean-tree
rerun would lower reproducibility and the overall stance.

## Top Strengths

1. The archive contract is executable: two generated payloads must be byte-identical,
   not merely claimed deterministic.
2. Internal per-file hashes and an external archive hash detect both member corruption
   and whole-archive mismatch.
3. The verifier starts from a new extracted directory and covers build, all 29 CTests,
   TCP fault/recovery, evidence consistency, artifact hashes, and the PDF.
4. Full local builds retain the 39-system-matrix assertion; bundle mode explicitly
   narrows only fixture discovery to the two files actually packaged.
5. The manuscript and machine record state that excluded datasets prevent full-data,
   public, independent, external-performance, or physical multi-host claims.
6. A hard manuscript gate now rejects any page count other than 12.

## Major / Fatal Concerns

1. **P0 — Public independent reproduction.** The archive remains a local build product
   without DOI, release tag, external immutable storage, independent operator, or
   public acquisition test.
2. **P0 — External performance authority.** All authoritative timing remains on one
   Apple M4. Native Linux/x86-64 and discrete-accelerator complete-cost replication
   remain the main scientific path to 9.
3. **P1 — Full-data acquisition.** The core archive intentionally omits PDEBench and
   most SuiteSparse data. A public artifact still needs dataset licenses, exact
   acquisition checksums, and a full-data reproduction path.
4. **P1 — Authenticated distributed authority.** One server and two unkeyed local files
   do not provide authenticated monotonic state, replication, partition handling,
   election, or recovery availability after loss of both generations.
5. **P1 — Submission readiness.** Author, affiliation, funding, conflict,
   acknowledgment, and final review-mode metadata remain placeholders or unresolved.

No new fatal scientific inconsistency was found. Round 15 closes local delivery
ambiguity but does not close external validity or distributed-systems scope gaps.

## Writing and Presentation Concerns

- The reproducibility paragraph is concise and reports both the successful checks and
  excluded data in the same passage.
- “Author-operated clean extraction” is the correct phrase; “independent” would be
  unsupported.
- The limitations section retains single-host, checksum, replication, and archive
  boundaries without burying negative performance results.
- The PDF remains exactly 12 pages without margin, font, or spacing manipulation.

## Format and Venue Concerns

- The IEEE Computer Society journal layout builds to exactly 12 pages; the new gate
  fails if that changes.
- No unresolved citations, references, undefined controls, or overfull boxes appear.
- TPDS topic fit is strong enough for review: parallel gate scaling, heterogeneous
  placement, transactional authority, and complete-cost system evaluation are central.
- Double-anonymous submission would require replacing the current placeholder with a
  blinded block and scrubbing identifying artifact metadata; single-anonymous review
  would require final author metadata instead.
- Desk-rejection risk is low for topic, structure, length, and readability, but medium
  until submission metadata and review mode are finalized.

## Multi-Reviewer Panel

### Reviewer 1 — Parallel and Distributed Systems

- **Score tendency:** 8/10; **confidence:** 5/5.
- **Positive:** complete-path/thread/process scaling and fail-closed authority have
  inspectable negative as well as positive results.
- **Negative:** deterministic local packaging does not add physical replication,
  failover availability, partition behavior, or network performance.
- **Score-change condition:** physical cross-host replicated authority with timed
  failover and partition tests.

### Reviewer 2 — Numerical Systems and SciML

- **Score tendency:** 8/10; **confidence:** 5/5.
- **Positive:** original-equation gates, shared controls, order counterbalancing, and
  explicit failed expert/offload paths support calibrated claims.
- **Negative:** authoritative timing remains single-machine and the core archive omits
  the large datasets needed to regenerate the main performance campaigns.
- **Score-change condition:** native external paired reruns with the full acquisition
  contract and retained negative outcomes.

### Reviewer 3 — Artifact and Reproducibility

- **Score tendency:** 8/10; **confidence:** 5/5.
- **Positive:** normalized metadata, sorted members, two byte-identical generations,
  internal hashes, sidecar hash, and clean-tree execution materially improve auditability.
- **Negative:** local author operation is neither archival persistence nor independent
  reproduction; optional local toolchain discoveries also remain environment-specific.
- **Score-change condition:** publish the frozen archive and acquisition scripts under
  a persistent identifier, then obtain and record an independent rerun.

## Panel Synthesis and AC Meta-Review

- **Agreement:** Round 15 is a genuine reproducibility improvement and correctly scoped.
- **Disagreement:** the artifact reviewer values deterministic clean-tree execution
  more than the systems reviewer, but no reviewer treats it as new distributed or
  performance evidence.
- **Decisive accept axis:** coherent verified-transaction design, complete-cost
  measurements, counterbalances, failure preservation, and machine-checked claims.
- **Decisive reject axis:** a reviewer demanding native external performance,
  replicated physical-host authority, or independent full-data reproduction can still
  downgrade the paper.
- **Unresolved evidence:** native x86-64 performance, discrete accelerator,
  authenticated monotonic authority, physical replication/failover/timing, dataset
  acquisition verification, public archive, independent rerun, and final metadata.
- **Final calibrated stance:** 8/10 accept. Reproducibility improves within the local
  trust boundary; the threshold for 9 remains external.

## Concern-to-Action Table

| Priority | Concern | Evidence basis | Concrete action | Owner | Score condition |
| --- | --- | --- | --- | --- | --- |
| P0 | Public independent reproduction | Local archive and author-operated clean run | Publish frozen bundle with persistent identifier; have a separate operator execute the verifier | Artifact owner + external operator | Required for robust 9--10 |
| P0 | Native external performance | Apple M4 timing authority only | Repeat representative paired workloads on native Linux/x86-64 and a discrete accelerator | External hardware owner | Main scientific path to 9 |
| P1 | Full-data acquisition | Core bundle excludes PDEBench and 37 matrices | Publish license-aware download scripts, exact dataset hashes, and a full-data profile | Artifact owner | Reproducibility +1 |
| P1 | Authenticated replicated authority | One server, two unkeyed local files | Add authenticated monotonic state and physical two-host replication; test failover and partitions | Distributed systems owner | Significance/Evidence +1 |
| P1 | Submission metadata | Placeholder author and acknowledgment sources | Finalize authors, funding, conflicts, acknowledgment, and review mode | Authors | Readiness only |
| Closed locally | Deterministic core delivery | Contract, sidecar, internal manifest, clean-tree record | Preserve normalized generation and verifier gates | Artifact owner | No remaining local packaging deduction |

## Recommended Next CCFA Owner

1. `ccf-paper-reviewer` remains the decision owner after external evidence changes.
2. The artifact owner should publish the frozen package, add license-aware acquisition
   scripts, and recruit an independent operator.
3. An external hardware operator should run native x86-64 and discrete-accelerator
   complete-cost experiments.
4. A distributed-systems owner should implement authenticated physical-host
   replication, failover, and partition tests.

## Checks Run

- Generated the core archive with two byte-identical normalized generations.
- Verified sorted file members, zero mtime/UID/GID, internal per-file SHA-256, and the
  external archive SHA-256.
- Extracted into a fresh temporary directory and completed a Release build.
- Confirmed and passed 29/29 CTests using two bundled Matrix Market fixtures.
- Reran the local TCP authority target, including exits 86/87/88, stale/corrupt
  generation handling, durable replay, and fail-closed dual corruption.
- Passed manuscript evidence and artifact-manifest checks from the extracted tree.
- Rebuilt the extracted-tree PDF to 12 pages.
- Rechecked the ordinary local 39-matrix sparse-suite test path.
- Added and passed an explicit 12-page manuscript gate.
- Scanned paper sources for prompt-injection/reviewer-manipulation phrases; none found.
- Verified current official TPDS/IEEE Computer Society submission and peer-review
  policy pages on 2026-07-25.

## Unresolved or Unverified

- Public immutable archive, release tag, and persistent identifier.
- Independent operator or independent-host reproduction.
- License-aware complete dataset acquisition and full-data clean rerun.
- Native Linux/x86-64 performance.
- CUDA or another discrete-accelerator complete-cost comparison.
- Authenticated external monotonic generation authority.
- Coherent malicious rollback of both local files.
- Recovery availability after loss of both local generations.
- Replication, partition, leader election, remote failover, or consensus.
- Two-physical-host network performance and variability.
- NUMA and multi-node complete-path scaling.
- Final author, affiliation, funding, conflict, acknowledgment, and review-mode
  metadata.

## Output Self-Check

- Scores and confidence remain separate and match the strongest unresolved concern.
- Every score below 5 includes a concrete deduction and repair condition.
- Deterministic author-operated extraction is not mislabeled as independent or public.
- The two-fixture bundle test is not mislabeled as full 39-matrix benchmark coverage.
- Docker namespaces are not mislabeled as physical hosts.
- Emulated x86-64 is not described as native performance.
- Existing negative results and complete-cost caveats remain visible.
- No score increase is granted for wording or packaging alone.
- No acceptance probability, invented result, or fabricated metadata is present.

## Official Policy Sources Checked

- IEEE TPDS author information: <https://www.computer.org/csdl/journal/td/write-for-us/15009>
- IEEE Computer Society peer-review policy: <https://www.computer.org/publications/author-resources/peer-review-policy>
- IEEE Computer Society journal author information and page limits: <https://www.computer.org/publications/author-resources/journals-information>
