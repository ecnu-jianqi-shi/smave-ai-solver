# CCF-A Full Review — Round 23

## 1. Report Metadata

- **Mode:** full scientific, writing, format, artifact, integrity, and reproducibility
  review.
- **Review date:** 2026-07-25.
- **Venue and assumptions:** IEEE Transactions on Parallel and Distributed Systems,
  regular paper, 2026 submission cycle, 12-page initial manuscript.
- **Paper title:** *Verification-Aware Expert Fusion with Parallel Original-Equation
  Gates for Repeated Numerical Solves*.
- **Materials reviewed:** current 12-page PDF and LaTeX sources; Round 23 C++ server,
  local and Docker campaigns, independent verifier, failure records, artifact registry,
  claim ledger, 29-test Release suite, and immutable Rounds 1--22.
- **Privacy boundary:** local unpublished manuscript and artifacts only. No private text
  was submitted to an external search service.
- **Search basis:** the core novelty and venue claims did not broaden. Same-day Round 22
  official TPDS scope and length checks therefore remain the policy basis; no new public
  novelty search was required.
- **Report file:** `ccfa-review-reports/round-23-full-review.md`.

## 2. Desk Rejection Assessment

- **Paper length — pass.** The PDF rebuilds at exactly 12 pages and 289,354 bytes.
  No margin, font, spacing, or float workaround was introduced.
- **Topic compatibility — pass.** Verification-aware numerical execution, parallel
  gates, complete-cost routing, and bounded authority evidence remain within TPDS scope.
- **Minimum quality — pass.** Formal assumptions, implementation probes, benchmark
  evidence, negative results, and artifact checks remain inspectable.
- **Format and source integrity — pass.** `IEEEtran` journal/compsoc mode is retained;
  compilation has no undefined references, citations, or overfull boxes.
- **Policy/anonymity/compliance — administratively incomplete.** Author, affiliation,
  funding, conflict, acknowledgment, and final release metadata remain unresolved.
- **Prompt injection and hidden manipulation — pass locally.** No reviewer-directed
  instruction was found in the inspected manuscript or artifact documentation.
- **Ethics and reviewability — pass with explicit limits.** Same-host storage, fixture
  keys, emulation, finite samples, licensing, and external-validity limits remain clear.

**Desk-rejection risk:** low scientifically; medium administratively until final
submission metadata and the TPDS checklist are complete.

## 3. Paper Summary

The paper presents verification-aware expert fusion for repeated numerical solves.
Candidate generation, optional correction, mandatory original-equation verification,
publication, and fallback form a typed transaction selected by complete verified cost.
Under explicit isolation, immutable-problem, atomic-publication, and fallback assumptions,
the commit-authority proposition restricts caller-visible state to a prior commit or a
state accepted by the family-specific gate.

The contribution map remains unchanged:

1. A reach-weighted complete-cost objective for heterogeneous expert cascades.
2. A candidate--corrector--gate--fallback transaction and bounded authority proposition.
3. A typed C/C++ runtime spanning algebraic and dynamic equation services.
4. A broad evaluation retaining regressions, unavailable comparisons, and failures.
5. An inspectable authority and reproduction artifact with explicit scope boundaries.

Round 23 does not add a manuscript-level novelty claim. It closes a local artifact gap:
key separation existed by convention, but duplicate configured key material was not
rejected.

## 4. Round 23 Scientific Change

Round 23 enforces one bounded startup policy across all configured authentication roles:

1. Current and optional previous state keys are compared with current and optional
   previous witness keys.
2. Every configured role key must be pairwise distinct.
3. A duplicate produces a durable `key-policy-failure.txt` record.
4. The record identifies both roles, a non-secret key identifier, and the policy stage.
5. Failure occurs with exit 88 before state load and before socket creation.
6. Epoch reuse is injected as `state-current == state-previous`.
7. Domain reuse is injected as `state-current == witness-current`.

The independent Python verifier checks the two role pairs and requires no state or
witness files in either failure directory. Local, Ubuntu ARM64, and emulated x86-64
campaigns each report `snapshots=4 recoveries=2 failures=8`. Their 97 key-value fields
match except the intended deployment and namespace indicators.

This is meaningful configuration validation. It is not key generation, entropy
validation, provisioning, custody, KMS/HSM integration, compromise recovery, external
authority, or physical replication.

## 5. Likely Stance and Calibrated Score

- **Likely stance:** accept.
- **Overall score:** **8/10**.
- **Scholarly confidence:** **5/5**.
- **Main accept axis:** coherent complete-cost verification design, explicit formal
  assumptions, broad auditable evidence, retained negative results, and fail-closed
  artifact behavior across three execution paths.
- **Main reject axis:** absent native external performance, physical replicated
  authority, external monotonic freshness, production key custody, public immutable
  preservation, and independent rerun.

Round 23 removes one preventable local misconfiguration. It does not change a decisive
external axis, so the overall score remains 8. Same-host fixtures and wording alone do
not support movement toward 9.

## 6. Quantitative Scorecard

| Dimension | Score | Confidence | Evidence basis | Deduction / score-change condition |
| --- | ---: | ---: | --- | --- |
| Novelty | 4/5 | 4/5 | Complete-cost verification-aware fusion remains the scientific contribution; the new key policy is implementation hardening | Requires stronger external systems evidence, not another local policy fixture |
| Soundness | 4/5 | 5/5 | Explicit proposition assumptions; authenticated state/witness recovery; pre-state-load collision rejection | Joint rollback, external freshness, and production custody remain absent |
| Evidence | 4/5 | 5/5 | Paired statistics, negative results, three authority paths, two new collision records, and 97-field parity | Native external performance and physical failure evidence remain absent |
| Significance | 4/5 | 4/5 | Broad numerical-service and verification relevance across several equation families | Production distributed and customer-scale evidence remains absent |
| Clarity | 4.74/5 | 5/5 | Mechanism, result, and limitation passages distinguish six authenticated-state failures from two policy failures | Breadth still creates unavoidable density at the fixed page limit |
| Reproducibility | 4/5 | 5/5 | Independent verifier, pinned reports, manifest, data locks, 29 tests, and deterministic bundle tooling | No public immutable archive or independent full-data rerun |
| Ethics / Limitations | 5/5 | 5/5 | Fixture, same-host, emulation, custody, rollback, and performance limits are explicit | Preserve final submission metadata and scope language |

**Overall:** **8/10** | **Scholarly Confidence:** **5/5**

**Recommendation:** accept.

**Verdict:** a major external evidence axis could support 9. More same-host fixtures,
test-key policies, or wording alone should not change the score.

No criterion is 3 or below; no fatal concern is averaged away.

## 7. Major Strengths

1. **The new policy is enforced before parsing persistent state.** The server compares
   all configured current/previous state/witness roles before recovery.
2. **Failure is durable and inspectable.** Each collision record states the two roles,
   stage, pairwise-distinct failure, exit code, and zero state/listen indicators.
3. **Epoch and domain separation are independently exercised.** The campaign injects
   same-domain temporal reuse and cross-domain reuse rather than only asserting flags.
4. **The verifier does not trust the C++ parser.** Python checks expected role pairs,
   duplicate key identifiers, record fields, and absence of loaded state/witness files.
5. **Cross-platform behavior is consistent.** Local, Ubuntu ARM64, and emulated x86-64
   ledgers differ only in deployment topology fields.
6. **Existing rotation and revocation evidence remains intact.** Current-key
   republication, current-key-only restart, and preserved old-byte rejection still pass.
7. **Claim discipline remains strong.** The manuscript calls the policy a same-host
   fixture and denies production lifecycle, custody, KMS/HSM, replication, and timing.
8. **Negative results remain visible.** One-workload `100×` success, device rejection,
   routing regression, transfer failure, Amdahl limits, and joint rollback remain clear.

## 8. Major and Moderate Concerns

### Major Concerns

1. **No independent monotonic authority.** State and witness remain jointly rollbackable
   on one process, filesystem, host, and failure domain. **Repair condition:** an
   independently administered monotonic anchor plus coordinated rollback faults.
2. **No physical replicated authority.** Primary and mirror remain files, not replicas
   under quorum, partitions, election, or failover. **Repair condition:** cross-host
   replicas and a physical fault campaign.
3. **No native external performance evidence.** Apple M4 remains the only authoritative
   timing host. ARM64 Docker and emulated x86-64 are correctness-only. **Repair
   condition:** repeat complete-cost campaigns on native external systems.
4. **No public independent reproduction.** The bundle is author-operated and excludes
   large benchmark payloads. **Repair condition:** immutable public deposit and an
   independent full-data rerun.

### Moderate Concerns

1. **Production key lifecycle remains absent.** Pairwise distinction does not establish
   key generation, custody, KMS/HSM, compromise recovery, or arbitrary interruption
   safety. **Repair condition:** deploy and fault-test an operational lifecycle under
   independent authority.
2. **Final administrative metadata remains incomplete.** Author, affiliation, funding,
   conflict, acknowledgment, and release metadata require completion.
3. **Recovery scope remains narrow by design.** One authenticated parent-linked witness
   advance is exercised; larger or unchained advances are not claimed.

## 9. Claim-Evidence Audit

| Claim | Where stated | Evidence provided | Strength | Reviewer deduction | Required fix |
| --- | --- | --- | --- | --- | --- |
| Complete verified cost should govern expert selection | Abstract; Introduction; Sections 3--5 | Reach-weighted objective, router comparisons, ablations, complete-runtime reports | Strong | Supported as a design and evaluation principle | Preserve complete-cost accounting |
| Mandatory original-equation gates bound caller-visible commits | Abstract; Proposition 1; implementation probes | Explicit assumptions, fresh buffers, atomic publication, fallback, gate equivalence | Strong within assumptions | Not formal verification of arbitrary callbacks or hardware | Preserve assumption language |
| Acceleration is qualified rather than universal | Abstract; Sections 7--10 | Seven paired PDE workloads, operators, negative device/router/transfer paths | Strong | One host and only one `100×` workload limit generality | Add native external performance for broader claims |
| Parallel gate throughput does not imply full-path scaling | Sections 6--9; Figures 4--5 | Thread/process gate studies, Amdahl separation, complete-path results | Strong | Same-host and partly gate-only | Add physical distributed performance before broadening |
| Configured fixture keys are pairwise distinct and collisions fail before load/listen | Sections 6.3, 7.5, and 9.3; claim ledger | C++ policy, two durable records, three campaigns, independent verifier | Strong for the fixture | No production lifecycle, custody, or external key authority | Keep claim bounded or deploy operational custody |
| Artifact state is locally reproducible and auditable | Section 6.6; artifact documents | Manifest, data locks, normalized archive, clean-tree verifier, 29 tests | Adequate | Author-operated and not publicly immutable | Deposit and obtain independent rerun |

No abstract, introduction, or conclusion claim was broadened in Round 23. The authority
change remains supporting artifact evidence rather than a new central contribution.

## 10. Experiment, Benchmark, and Reproducibility Audit

- **Statistical units — pass.** Paired process runs, bootstrap intervals, win rates,
  and the 64-scenario error bound remain distinguished from repeated timings.
- **Complete-cost accounting — pass.** Candidate, correction, gate, persistence, and
  fallback costs remain explicit where claimed.
- **Negative-result retention — pass.** Device, routing, transfer, and `100×` failures
  remain visible.
- **Key-policy campaign — pass.** Two new startup attempts exit 88 before state load and
  listen; the previous six authenticated-state failures remain separate.
- **Cross-path parity — pass.** All three verifiers report four snapshots, two
  recoveries, and eight failures; only deployment and namespace fields differ.
- **Independent checking — pass.** Python verifies healthy key-role distinction,
  collision role pairs, key identifiers, empty state/witness directories, and counters.
- **Performance interpretation — pass with limits.** Docker/emulation remain explicitly
  correctness-only and `performance_evidence=0`.
- **Physical replication — absent.** Separate namespaces on one bridge are not separate
  hosts or failure domains.
- **Public reproducibility — incomplete.** Data locks and a deterministic archive improve
  auditability but do not provide immutable public preservation or independent rerun.

## 11. Writing and Presentation Review

### Writing Scorecard

| Dimension | Weight | Score | Confidence | Evidence basis | Concrete repair |
| --- | ---: | ---: | ---: | --- | --- |
| Storyline and motivation | 12 | 5/5 | 5/5 | Problem, gap, insight, evidence, and boundaries remain explicit | Preserve sequence |
| Contribution display | 12 | 5/5 | 5/5 | Core contributions remain visible and unchanged | Do not promote key policy to novelty |
| Paragraph logic | 10 | 5/5 | 5/5 | Authority passages retain mechanism → outcome → boundary order | Preserve progression |
| Claim-evidence alignment | 14 | 5/5 | 5/5 | Six authenticated-state and two policy failures map to separate records | Keep ledgers synchronized |
| Method readability | 10 | 4/5 | 5/5 | Dense but reproducible systems detail | Avoid adding more fixture detail without space |
| Results readability | 10 | 5/5 | 5/5 | Positive, negative, recovery, and policy outcomes remain separated | Preserve counts |
| Experiment narration | 10 | 5/5 | 5/5 | Starts, recoveries, rotation, and failures remain concise | Preserve semantics |
| Related-work positioning | 8 | 4/5 | 4/5 | Closest algorithm-selection and learned-solver work remains positioned | Refresh only if claims broaden |
| Terminology consistency | 8 | 5/5 | 5/5 | Current/previous, state/witness, fixture, same-host, and emulation terms are stable | Preserve audit |
| LaTeX and format discipline | 8 | 5/5 | 5/5 | Exact 12 pages, no overfull boxes, clean affected pages | Preserve format gate |
| Reviewer-facing risk | 8 | 4/5 | 5/5 | Fixture limits are prominent; external evidence remains decisive | Do not inflate local claims |

**Weighted writing score:** **4.74/5**. **Writing risk:** low.

### Writing Findings

- **Main readability gain:** methodology, evaluation, and limitations now distinguish
  policy failures from authenticated-state failures in short declarative sentences.
- **Main reviewer-confusion risk:** `mirrored` and `witness` can still sound distributed
  if the same-host boundary is detached from the paragraph. The current text prevents
  that interpretation; it must remain adjacent.
- **Paragraph-level assessment:** no new paragraph mixes mechanism, result, and scope.
- **Sentence audit:** every revised manuscript sentence is at most 18 words.

## 12. Format and LaTeX Audit

- **Template:** `\documentclass[10pt,journal,compsoc]{IEEEtran}`.
- **Page count:** exactly 12.
- **PDF size:** 289,354 bytes.
- **Compilation:** clean under `latexmk -pdf -interaction=nonstopmode -halt-on-error`.
- **References/citations:** no undefined references or citations.
- **Boxes:** no overfull horizontal or vertical boxes.
- **Layout manipulation:** no new margin, font, spacing, or float workaround.
- **Visual inspection:** affected pages 6, 7, 9, 10, and 11 remain balanced and legible.
- **Administrative placeholders:** author and acknowledgment metadata remain incomplete.

**Format verdict:** scientifically submission-ready; administratively incomplete.

## 13. Multi-Reviewer Panel

### Reviewer 1 — Method and Soundness

- **Lens:** startup ordering, failure semantics, and key-role policy.
- **Score tendency:** 8/10.
- **Confidence:** 5/5.
- **Positive signal:** pairwise distinction is enforced before recovery and socket
  creation, with a durable failure envelope.
- **Negative signal:** the fixture does not address generation, custody, or compromise.
- **Evidence basis:** C++ policy path, two failure records, and Python absence checks.
- **Score-change condition:** external authority and operational lifecycle evidence.

### Reviewer 2 — Distributed Systems

- **Lens:** replication, failure domains, monotonicity, and failover.
- **Score tendency:** 7--8/10.
- **Confidence:** 5/5.
- **Positive signal:** same-host scope and pre-listen failure semantics are honest.
- **Negative signal:** state, mirror, witness, and keys remain jointly rollbackable.
- **Evidence basis:** topology fields, limitations, and artifact directories.
- **Score-change condition:** physical replicas, quorum/partition tests, and external
  monotonic freshness.

### Reviewer 3 — Evidence and Experiments

- **Lens:** campaign design, parity, counters, and claim support.
- **Score tendency:** 8/10.
- **Confidence:** 5/5.
- **Positive signal:** two distinct collision classes pass across all three paths with
  full evidence-map parity.
- **Negative signal:** no new external performance or physical-fault axis is added.
- **Evidence basis:** three verifier outputs and 97-field comparison.
- **Score-change condition:** native external timing or physical distributed faults.

### Reviewer 4 — Novelty and Positioning

- **Lens:** central contribution and closest-work differentiation.
- **Score tendency:** 8/10.
- **Confidence:** 4/5.
- **Positive signal:** complete-cost verification-aware fusion remains coherent.
- **Negative signal:** key-policy enforcement is engineering hardening, not novelty.
- **Evidence basis:** unchanged abstract, contribution list, and conclusion.
- **Score-change condition:** stronger systems evidence tied to the central design.

### Reviewer 5 — Writing and Clarity

- **Lens:** paragraph logic, counts, terminology, and rendered presentation.
- **Score tendency:** 8/10.
- **Confidence:** 5/5.
- **Positive signal:** six state failures and two policy failures are clearly separated.
- **Negative signal:** the fixed-length paper remains dense.
- **Evidence basis:** source audit, sentence audit, and rendered pages.
- **Score-change condition:** no local prose change should alter the scientific score.

### Reviewer 6 — Artifact, Security, and Reproducibility

- **Lens:** independent verification, custody boundaries, and archival status.
- **Score tendency:** 8/10.
- **Confidence:** 5/5.
- **Positive signal:** the verifier checks policy records and absence of state loading.
- **Negative signal:** no KMS/HSM custody, public archive, or independent operator.
- **Evidence basis:** verifier, manifest, bundle contract, and claim ledger.
- **Score-change condition:** operational custody plus public independent reproduction.

### Panel Synthesis

- **Agreement:** Round 23 closes a real local configuration gap without broadening the
  paper's central claims.
- **Disagreement:** the distributed-systems reviewer weights absent physical authority
  more heavily, but does not identify a contradiction in the bounded claim.
- **Decisive accept axis:** coherent method, explicit assumptions, broad evidence,
  negative-result discipline, and inspectable fail-closed artifacts.
- **Decisive reject axis:** no external performance, physical authority, external
  freshness, public independent rerun, or production custody.
- **Unresolved evidence:** all decisive external axes remain absent.
- **Final calibrated stance:** **8/10 accept, confidence 5/5**.

## 14. Concern-to-Action Table

| ID | Severity | Concern | Evidence basis | Fix class | Required action | Score impact condition |
| --- | --- | --- | --- | --- | --- | --- |
| R23-1 | Minor, closed locally | Configured key reuse was not rejected | Pairwise role scan plus two collision records | Method/soundness | Preserve policy and fixtures | No overall movement alone |
| R23-2 | Major | Joint state-and-witness rollback remains undetected | One process/filesystem/host; no external anchor | Distributed experiment | Add independent monotonic authority and coordinated rollback faults | Necessary path toward 9 |
| R23-3 | Major | No physical replicas, quorum, partitions, or failover | `multi_host=0`, `consensus_protocol=0` | Distributed experiment | Deploy cross-host replicas and failure campaign | Necessary path toward 9 |
| R23-4 | Major | Native external performance remains absent | Apple M4 timing; containers correctness-only | Performance experiment | Repeat complete-cost campaigns externally | Performance path toward 9 |
| R23-5 | Major | No public immutable independent reproduction | Author-operated bundle; large payloads omitted | Reproducibility | Deposit artifact and obtain independent rerun | Reproducibility path toward 9 |
| R23-6 | Moderate | Production key lifecycle remains untested | No generation, custody, KMS/HSM, or compromise recovery | Security deployment | Fault-test operational lifecycle under independent authority | Supports 9 only with external authority |
| R23-7 | Moderate | Submission metadata remains incomplete | Author and acknowledgment placeholders | Administrative | Finalize submission metadata and checklist | Removes desk risk, not scientific score |

## 15. AC / Meta-Review

The paper remains a strong accept-side TPDS submission under its stated scope. The
central contribution is not a distributed key-management protocol; it is a
verification-aware complete-cost runtime for heterogeneous repeated solves. Round 23
therefore deserves credit as artifact hardening, not as new novelty or production
security evidence.

The new policy is technically credible: it covers every configured current/previous
state/witness role, fails before state parsing and listening, emits durable records, and
is independently verified on local, ARM64, and emulated x86-64 paths. The manuscript
accurately reports two policy failures separately from six authenticated-state failures.

The decisive limitations are unchanged. Performance remains single-host, mirrors and
witnesses are not physical replicas, freshness is not externally anchored, archives are
not public and independently rerun, and keys are not under production custody. These
gaps cap the calibrated score at 8.

**Meta-review recommendation:** accept, 8/10, confidence 5/5.

## 16. Score Revision Criteria

### Raise Toward 9

- Native external paired complete-cost evidence on representative systems.
- Physical cross-host authority with independent failure domains, partitions, quorum,
  split-brain, leader-loss, recovery, and remote-failover tests.
- Externally anchored monotonic freshness detecting joint state/witness rollback.
- Public immutable preservation and an independent full-data rerun.
- Operational key provisioning, custody, rotation, revocation, and compromise recovery
  only when coupled to independent authority.

One major external axis could support 9 if soundness and claim discipline remain. A 10
requires several external axes to converge without introducing a central weakness.

### Lower the Score

- Calling pairwise fixture-key checks production lifecycle or KMS/HSM integration.
- Calling same-host files physical replicas or the witness an external anchor.
- Treating emulated x86-64 correctness as native performance.
- Generalizing one-generation recovery or one-previous-key migration beyond evidence.
- Dropping negative results, finite-sample limits, or fixture-key status.
- Failing verifier, evidence, test, page, manifest, archive, or clean-tree gates.

## 17. Recommended Next CCFA Owner

1. **Immediate — artifact/integrity owner:** update latest-review pointers, regenerate
   the manifest, freeze Round 23 in the deterministic archive, compare two generations,
   and rerun the clean-tree verifier.
2. **Immediate — integrity auditor:** run stale-term, duplicate-field, allowlist, claim,
   sentence, visual, page, and hash audits on the final tree.
3. **Submission — authors:** finalize author, affiliation, funding, conflict,
   acknowledgment, policy, and public-archive metadata.
4. **Decisive external — benchmark/distributed/security owners:** run native performance,
   physical authority, external freshness, production custody, and independent reruns.

## 18. Checks Run

- Local authority verifier passed:
  `SMAVE_GATE_NETWORK_AUTHORITY_CHECK 1 snapshots=4 recoveries=2 failures=8`.
- Ubuntu ARM64 Docker verifier passed with the same counts.
- Emulated x86-64 Docker verifier passed with the same counts.
- All 97 evidence fields match across paths except deployment and namespace indicators.
- Both collision records report `state_loaded=0`, `listen_socket_created=0`, and exit 88.
- `python3 paper/check_artifact_manifest.py` passed.
- `python3 paper/check_evidence.py` passed.
- `paper/check.sh` passed at exactly 12 pages and 289,354 bytes.
- `ctest --test-dir build/release --output-on-failure` passed 29/29 tests.
- Affected manuscript sentences are at most 18 words.
- Rendered pages 6, 7, 9, 10, and 11 were visually inspected.

## 19. Unresolved or Unverified

- Native external performance and independent full-data reproduction.
- Physical cross-host replicas, quorum, partitions, election, and remote failover.
- External monotonic freshness and joint state/witness rollback detection.
- Production key generation, custody, KMS/HSM, and compromise recovery.
- Public immutable archive, persistent identifier, and final submission metadata.

## 20. Output Self-Check

- Overall score, criterion scores, writing score, and confidence are separated.
- The new policy receives local soundness credit without score inflation.
- Same-host files are not called physical replicas or independent failure domains.
- Fixture keys are not called production lifecycle, custody, or KMS/HSM.
- Emulated correctness is not called native performance.
- Negative results and scope boundaries remain present.
- No acceptance probability or unsupported novelty claim is stated.
- The exact 12-page constraint is preserved without a layout hack.
