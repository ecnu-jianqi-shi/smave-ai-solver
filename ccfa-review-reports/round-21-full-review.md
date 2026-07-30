# CCF-A Full Review — Round 21

## 1. Report Metadata

- **Review date:** 2026-07-25.
- **Target venue/year/track:** IEEE Transactions on Parallel and Distributed Systems,
  regular paper, 2026 submission cycle.
- **Paper title:** *Verification-Aware Expert Fusion with Parallel Original-Equation
  Gates for Repeated Numerical Solves*.
- **Input materials reviewed:** current 12-page PDF and LaTeX sources; implementation;
  local, Ubuntu ARM64, and emulated x86-64 authority records; independent verifier;
  29-test Release suite; artifact manifest and bundle tooling; visual renders of pages
  7, 9, 10, and 11; and immutable Rounds 1--20.
- **Search basis:** no new public novelty search. Round 21 changes presentation only and
  introduces no new scientific, performance, security, or distributed-systems claim.
- **Report file:** `ccfa-review-reports/round-21-full-review.md`.
- **Reviewer mode:** full scientific, writing, format, artifact, integrity, and
  reproducibility review.

## 2. Desk Rejection Assessment

- **Paper length — pass.** The manuscript rebuilds at exactly 12 pages and 289,255
  bytes without a layout hack.
- **Topic compatibility — pass.** Verification-aware numerical execution, parallel
  selection, and bounded authority/recovery remain appropriate for TPDS.
- **Minimum quality — pass.** Formal, implementation, benchmark, fault, negative-result,
  and artifact evidence remain inspectable.
- **Policy/anonymity/compliance — uncertain administratively.** Author, affiliation,
  funding, conflict, acknowledgment, and final artifact metadata remain incomplete.
- **Prompt injection/hidden manipulation — pass locally.** No reviewer-directed hidden
  instruction was found in the inspected manuscript or source paths.
- **Ethics and reviewability — pass with limits.** Same-host, emulation, fixture-key,
  finite-sample, licensing, and external-validity boundaries remain visible.

**Desk-rejection risk:** low scientifically; medium administratively until final
submission metadata and TPDS policy checks are completed.

## 3. Paper Summary and Contribution Map

The paper presents verification-aware expert fusion for repeated numerical solves.
Candidate generation, optional correction, original-equation gates, publication, and
fallback form a typed transaction optimized by complete verified cost. Under explicit
isolation, immutability, atomic-publication, and fallback assumptions, caller-visible
state is restricted to a prior commit or a state accepted by the mandatory gate.

The contribution map remains unchanged:

1. A reach-weighted complete-cost objective for heterogeneous expert cascades.
2. A candidate--corrector--gate--fallback transaction and bounded commit-authority
   proposition.
3. A typed C/C++ numerical-service implementation across equation families.
4. A broad evaluation retaining regressions, unavailable comparisons, and failures.
5. An inspectable local authority and reproduction artifact with explicit scope limits.

### Round 21 Writing Change

Round 20 identified one locally actionable presentation issue: the authority evidence
was accurate but compressed into sentences that braided mechanism, fault campaign, and
scope boundaries.

Round 21 reorganizes three passages without changing evidence:

1. **Methodology:** introduces the persistence mechanism before listing fault phases,
   then closes with the same-host claim boundary.
2. **Evaluation:** separates publication exits from cleanup/recovery/replay outcomes,
   then states authenticated rejection and non-distributed scope.
3. **Limitations:** separates observed local protections from absent external guarantees.

Every exit code, start count, cleanup count, replay count, failure class, one-generation
qualification, and external boundary is preserved. The manuscript remains exactly 12
pages with no margin, font, spacing, or float manipulation.

## 4. Search and Related-Work Basis

- **Queries used:** none in Round 21.
- **Sources searched:** no new public sources; previous verified bibliography remains.
- **Closest works found:** unchanged algorithm-selection, learned-solver,
  hybrid-correction, selective-prediction, and safety-architecture basis.
- **Unverified related-work risks:** external monotonic counters, transparency logs,
  replicated state-machine recovery, and production key management remain relevant only
  if future claims broaden.
- **Source-quality screening:** no citation or novelty assertion changed.

## 5. Expected Review Outcome

- **Expected outcome:** **8/10 — accept**.
- **Main accept signal:** coherent verification-aware complete-cost fusion, formal
  authority assumptions, broad inspectable evidence, negative results, and improved
  reviewer-facing claim/evidence flow.
- **Main reject signal:** no native external performance, physical replicated authority,
  independent monotonic freshness, public independent reproduction, or production key
  lifecycle.
- **Confidence:** **5/5**.

Round 21 closes R20-8 and raises the writing assessment. It cannot raise the scientific
overall score because no decision-level external evidence changed.

## 6. Strengths and Weaknesses

### Strengths

1. The revised methodology presents mechanism before campaign details, so the reader
   knows what each exit and recovery phase is testing.
2. The evaluation distinguishes the three publication boundaries from cleanup,
   recovery, replay, and fail-closed outcomes.
3. The limitations paragraph separates demonstrated local checks from absent external
   properties, reducing the risk that same-host files are read as replication.
4. All quantitative and negative boundaries remain unchanged and machine-checked.
5. Visual inspection shows balanced two-column pages, no stranded section, and no
   bibliography spillover.
6. Existing scientific strengths remain: complete-cost measurement, paired statistics,
   original-equation authority, broad interfaces, negative results, and reproducibility
   tooling.

### Major Weaknesses

1. **No independent rollback authority.** State and witness share one process,
   filesystem, physical host, operator, and fixture-key environment. **Required fix:**
   add an external monotonic service or quorum and coordinated rollback faults.
2. **No physical replication or failover protocol.** Primary/mirror/witness files are
   same-host. **Required fix:** cross-host replicas, independent storage/key domains,
   partitions, quorum, split brain, leader loss, recovery, and remote failover.
3. **No native external performance evidence.** Apple M4 remains the only authoritative
   timing host. **Required fix:** repeat complete-cost campaigns on external native
   Linux/x86-64 and representative accelerators or clusters.
4. **No public independent reproduction.** The deterministic bundle is author-operated
   and excludes large payloads. **Required fix:** immutable public deposit and an
   independent full-data rerun.
5. **No production key lifecycle.** Fixture keys do not establish provisioning,
   separation, rotation, revocation, or compromise recovery. **Required fix:** define
   and fault-test an operational lifecycle.

### Moderate Weaknesses

1. Final administrative and submission-policy metadata remains incomplete.
2. Recovery evidence remains intentionally limited to one authenticated parent-linked
   generation; larger or unchained advances are not claimed.

No material writing weakness remains from R20-8. The paper is still information-rich,
but the authority passages now have explicit paragraph-level progression.

## 7. Potentially Missing Related Work

- **External monotonic counters and transparency logs:** **unverified this round**;
  needed only for a future external-freshness claim.
- **Replicated state machines and consensus recovery:** **unverified this round**;
  needed only for future physical-replication, quorum, or remote-failover claims.
- **Production key-management systems:** **unverified this round**; needed if operational
  key custody becomes a contribution rather than a stated limitation.

No new related-work deduction is added because Round 21 changes no novelty boundary.

## 8. Claim-Evidence Audit

| Claim | Where stated | Evidence provided | Strength | Reviewer deduction | Required fix |
| --- | --- | --- | --- | --- | --- |
| Original-equation gates retain commit authority | Abstract, proposition, method, evaluation | Explicit assumptions, family gates, fallback and mismatch probes | Strong within assumptions | Does not verify arbitrary faulty callbacks or hardware | Preserve theorem boundaries |
| One-generation witness-lag catch-up occurs before listen | Methodology, evaluation, claim ledger | Exit-89 crash, snapshots, prepared witness, recovery record, three verifiers | Strong for tested one-step path | No larger/unchained path or external authority | Preserve exact qualification |
| Preserved witness rejects all-state rollback | Evaluation and limitations | Five pre-listen failure fixtures | Strong preserved-witness evidence | Joint state+witness rollback remains possible | Independent monotonic anchor |
| Wrong keys, corruption, and authenticated forks fail closed | Methodology and artifact ledger | Exit-88 failures with no blank reinitialization | Strong fixture evidence | No production custody or quorum | Operational security/distributed campaign |
| Complete-path speedups hold | Evaluation figures/tables and pinned reports | Paired runs, bootstrap intervals, retained failures | Strong for measured host/workloads | No native external performance | External complete-cost campaigns |
| Artifact is reproducible | Artifact documentation, manifest, bundle tools | Deterministic bundle, data locks, tests, verifier | Strong author-operated evidence | No public persistent archive or independent operator | Public deposit and independent rerun |

The wording changes improve navigation among these claims but do not enlarge any claim.

## 9. Experiment, Benchmark, and Reproducibility Audit

- **Baselines and ablations:** unchanged and explicit across classical, learned,
  routing, shared-control, fallback, worker/process, order, and component studies.
- **Metrics and statistics:** complete runtime, gate-only throughput, residuals,
  decisions, failures, and confidence intervals remain separated.
- **Robustness:** exits 86/87/89, four cleanups, one-generation catch-up, mirror
  recovery, four replays, and five fail-closed starts remain intact.
- **Implementation detail:** v5 envelopes, parent hashes, separate witness HMAC,
  publication order, and verifier checks remain inspectable.
- **Reproducibility:** three authority verifiers, manifest, evidence checker, paper gate,
  and 29/29 Release CTests pass after the prose change.
- **Writing validation:** all revised sentences remain at or below 30 words; affected
  rendered pages were visually inspected; the PDF stays exactly 12 pages.
- **Remaining external gaps:** native performance, physical distributed faults, public
  independent reproduction, and production key lifecycle.

## 10. Multi-Reviewer Panel

### Reviewer 1 — Method and Soundness

- **Likely score / confidence:** 8/10, 5/5.
- **Main positive signal:** mechanism, crash boundaries, recovery, and claim limits are
  now easier to distinguish.
- **Main negative signal:** same-host joint rollback remains possible.
- **Evidence basis:** revised methodology plus unchanged v5 fault evidence.
- **Score-change condition:** independent monotonic authority with rollback faults.

### Reviewer 2 — Distributed Systems

- **Likely score / confidence:** 8/10, 5/5.
- **Main positive signal:** the text is less likely to be misread as replication or
  consensus evidence.
- **Main negative signal:** no physical replicas, partitions, quorum, or remote failover.
- **Evidence basis:** explicit same-host wording and platform fields.
- **Score-change condition:** physical cross-host deployment and fault campaign.

### Reviewer 3 — Evidence and Experiments

- **Likely score / confidence:** 8/10, 5/5.
- **Main positive signal:** results now separate exit phases from recovery and rejection
  outcomes while preserving counts.
- **Main negative signal:** timing remains one-host.
- **Evidence basis:** revised evaluation, pinned reports, three verifiers, 29/29 tests.
- **Score-change condition:** native external complete-cost repetitions.

### Reviewer 4 — Novelty and Positioning

- **Likely score / confidence:** 8/10, 4/5.
- **Main positive signal:** contribution boundaries are displayed more cleanly.
- **Main negative signal:** Round 21 adds no novelty and should not be scored as such.
- **Evidence basis:** unchanged abstract, contribution map, and related work.
- **Score-change condition:** differentiated externally validated mechanism.

### Reviewer 5 — Writing and Clarity

- **Likely score / confidence:** 8/10, 5/5.
- **Main positive signal:** the mechanism → observed faults → scope-boundary sequence
  removes the prior local readability concern.
- **Main negative signal:** the paper remains dense overall because of its broad evidence
  package and fixed length.
- **Evidence basis:** source comparison, sentence audit, and rendered pages.
- **Score-change condition:** no further local edit is currently decision-relevant;
  scientific evidence, not more polishing, must drive the overall score.

### Reviewer 6 — Ethics, Artifact, and Reproducibility

- **Likely score / confidence:** 8/10, 5/5.
- **Main positive signal:** limitations are more visibly separated from demonstrated
  protections.
- **Main negative signal:** no public immutable archive or independent operator.
- **Evidence basis:** limitations, artifact ledger, manifest, and bundle tooling.
- **Score-change condition:** public deposit and independent full-data rerun.

### Reviewer 7 — Numerical-Solver Domain Application

- **Likely score / confidence:** 8/10, 5/5.
- **Main positive signal:** positive and negative solver outcomes remain prominent.
- **Main negative signal:** customer-scale and native external deployment remain absent.
- **Evidence basis:** PDE, sparse, operator, DAE, and device result sections.
- **Score-change condition:** representative external application traces.

### Reviewer 8 — Evidence and Ablation

- **Likely score / confidence:** 8/10, 5/5.
- **Main positive signal:** the broad ablation package is easier to interpret because
  authority evidence is no longer syntactically braided.
- **Main negative signal:** external performance and physical failure domains are absent.
- **Evidence basis:** ablation, worker/process, order, and authority sections.
- **Score-change condition:** external repetition under identical contracts.

### Reviewer 9 — Novice-Advocate Reader

- **Likely score / confidence:** 8/10, 5/5.
- **Main positive signal:** a reader can now identify what is persisted, what faults are
  injected, what recovers, and what is not claimed.
- **Main negative signal:** background knowledge of authentication and persistence is
  still useful.
- **Evidence basis:** revised passages and visual inspection.
- **Score-change condition:** no further change required within the current page budget.

### Reviewer 10 — AC / Meta-Review

- **Likely score / confidence:** 8/10, 5/5.
- **Main positive signal:** Round 21 removes an avoidable reviewer-facing ambiguity
  without deleting evidence or limitations.
- **Main negative signal:** none of the external decision axes changes.
- **Evidence basis:** unchanged evidence, passing gates, and improved writing score.
- **Score-change condition:** satisfy a major external evidence axis.

### Panel Synthesis

- **Agreement:** R20-8 is closed; the presentation is clearer and claim-neutral.
- **Disagreement:** none on overall score; only prioritization of external work differs.
- **Decisive positive axis:** coherent design, explicit assumptions, broad local evidence,
  negative results, and improved reviewer-facing structure.
- **Decisive negative axis:** absent native external performance, physical authority,
  external freshness, and independent reproduction.
- **Unresolved evidence:** joint rollback, cross-host failover, production key lifecycle,
  public archive, independent rerun, and final metadata.
- **AC stance:** accept at **8/10**, confidence **5/5**.

## 11. Concerns Table

| ID | Severity | Concern | Evidence basis | Affected criterion | Fix class | Required action | Owner skill | Score-change condition |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| R21-1 | Minor | Closed locally: authority presentation braided mechanism, faults, and boundaries | Three passages reorganized; sentence/page/visual gates pass | Clarity | writing | Preserve the new progression | Paper writer + integrity audit | Raises writing score only |
| R21-2 | Major | Joint state-and-witness rollback remains undetected | Same process/filesystem/host; no external anchor | Soundness, significance | experiment | Add independent monotonic authority and coordinated rollback faults | Distributed-systems implementation | Necessary path toward 9 |
| R21-3 | Major | No physical replicas, quorum, partitions, or remote failover | `multi_host=0`, `consensus_protocol=0` | Significance, evidence | experiment | Deploy cross-host replicas and failure campaign | Distributed-systems experiment | Necessary path toward 9 |
| R21-4 | Major | Native external performance remains absent | Authoritative timing is Apple M4; containers are correctness-only | Evidence, external validity | experiment | Repeat complete-cost campaigns externally | Benchmark owner | Performance path toward 9 |
| R21-5 | Major | No public immutable independent reproduction | Author-operated bundle; large payloads omitted | Reproducibility | reproducibility | Deposit artifact/data metadata and obtain independent rerun | Artifact owner | Reproducibility path toward 9 |
| R21-6 | Moderate | Production key lifecycle is untested | State/witness keys are fixtures | Soundness, ethics | method/soundness | Specify and test provisioning, rotation, revocation, and compromise recovery | Security owner | May support 9 with external authority |
| R21-7 | Moderate | Final administrative and policy metadata is incomplete | Placeholder author/funding/acknowledgment fields | Readiness | writing | Finalize metadata and submission checks | Author | Desk/readiness only |

## 12. AC / Meta-Review

Round 21 is a successful writing revision. The authority mechanism, fault campaign,
and claim boundary are now presented in that order across methodology, evaluation, and
limitations. This reduces reviewer confusion and lowers presentation risk without
removing any unfavorable result or scope restriction.

The scientific ceiling is unchanged. The paper still lacks independent physical and
operational evidence on the axes that distinguish a strong local systems artifact from
a top-tier externally validated distributed system.

**AC stance:** **8/10 — accept**, confidence **5/5**. Writing improves from 4.54 to
4.74/5; the overall score remains evidence-calibrated.

## 13. Quantitative Scores

| Dimension | Score | Confidence | Evidence basis | Deduction and repair condition |
| --- | ---: | ---: | --- | --- |
| Quality | 4/5 | 5/5 | Coherent formal, implementation, experiment, and artifact package | External authority/performance incomplete; add native and distributed validation |
| Clarity | 4.74/5 | 5/5 | Authority passages now have explicit mechanism, fault, and boundary progression | Remaining density reflects scope, not a decision-level writing defect |
| Significance | 4/5 | 4/5 | Broad numerical-service and verification relevance | Production distributed and customer-scale evidence absent |
| Originality | 4/5 | 4/5 | Verification-aware complete-cost fusion remains coherent | Round 21 adds presentation, not novelty |
| Soundness | 4/5 | 5/5 | Formal assumptions and extensive authenticated recovery/failure evidence | Joint rollback and external authority remain absent |
| Evidence | 4/5 | 5/5 | Paired statistics, negative results, and three-path authority evidence | Native external performance and physical faults absent |
| Reproducibility | 4/5 | 5/5 | Tests, manifest, locks, verifier, deterministic bundle | Public immutable archive and independent rerun absent |
| Ethics / Limitations | 5/5 | 5/5 | Scope boundaries are explicit and now easier to parse | Preserve final metadata consistency |

### Writing Review Scorecard

| Dimension | Weight | Score | Confidence | Evidence basis | Concrete repair |
| --- | ---: | ---: | ---: | --- | --- |
| Storyline and motivation | 12 | 5/5 | 5/5 | Problem, gap, insight, and evidence progression are explicit | Preserve sequence |
| Contribution display | 12 | 5/5 | 5/5 | Contributions are visible and supported | Preserve boundaries |
| Paragraph logic | 10 | 5/5 | 5/5 | Authority passages now use mechanism → faults → boundary | Preserve progression |
| Claim-evidence alignment | 14 | 5/5 | 5/5 | Strong claims map to formal or empirical evidence | Keep ledgers synchronized |
| Method readability | 10 | 4/5 | 5/5 | System detail is reproducible but necessarily dense | No further edit without new space/evidence |
| Experiment narration | 10 | 5/5 | 5/5 | Publication exits and outcomes are separated | Preserve result ordering |
| Related-work positioning | 8 | 4/5 | 4/5 | Closest work is positioned on technical axes | Refresh only if claims broaden |
| Terminology and notation consistency | 8 | 5/5 | 5/5 | Gate, witness, same-host, and emulation terms remain stable | Preserve terminology audit |
| LaTeX and format discipline | 8 | 5/5 | 5/5 | Exact 12 pages, no overfull boxes, balanced visual pages | Preserve format gate |
| Reviewer-facing risk | 8 | 4/5 | 5/5 | Local ambiguity is closed; external evidence remains decisive | Do not inflate local claims |

**Weighted writing score:** **4.74/5**. **Writing risk:** low.

- **Overall:** **8/10 — accept**.
- **Confidence:** **5/5**.
- **Score-change condition:** only new external evidence on native performance, physical
  authority/external freshness, or independent reproduction can support movement toward
  9. Further wording alone cannot.

No criterion is 3 or below; no fatal concern is averaged away.

## 14. Questions for Authors

1. What independent authority will hold freshness during production and disconnection?
2. Which native external systems will repeat complete verified cost?
3. What public archive and independent operator will reproduce the full-data campaign?
4. What provisioning, rotation, revocation, and compromise-recovery model will protect
   state and witness keys?

No remaining author question concerns the Round 21 prose itself.

## 15. Score Revision Criteria

### Raising the Score Would Require

- Native external paired complete-cost evidence on representative systems.
- Physical cross-host state/witness authority with independent failure domains,
  partitions, quorum, split brain, failover, and coordinated rollback faults.
- Externally anchored monotonic freshness detecting joint state/witness rollback.
- Public immutable preservation and an independent full-data rerun.

One major axis could support 9 if current soundness and claim discipline remain. A 10
requires several axes to converge without a new central weakness.

### Lowering the Score Would Be Triggered By

- Recombining mechanism, faults, and scope into ambiguous claims.
- Calling same-host files physical replicas or an external monotonic anchor.
- Treating emulated x86-64 correctness as native performance.
- Generalizing one-generation recovery to larger or unchained advances.
- Dropping negative results, finite-sample limits, or fixture-key status.
- Failing evidence, test, page, manifest, or archive gates.

### Concerns Unlikely to Change Before Submission

- Native external performance and independent reproduction require external resources.
- Physical replicated authority and production key lifecycle require deployment work.

## 16. Action Plan and CCFA Handoffs

1. **Immediate:** update latest-review pointers and freeze Round 21 in the deterministic
   archive. **Owner:** artifact/integrity owner. **Handoff required:** no.
2. **Immediate:** rerun claim, stale-term, duplicate-field, allowlist, page, and clean-
   tree audits. **Owner:** integrity audit. **Handoff required:** no.
3. **Submission:** finalize author, affiliation, funding, conflict, acknowledgment,
   policy, and public-archive metadata. **Owner:** authors. **Handoff required:** yes.
4. **Decisive external:** run native performance and physical replicated-authority
   campaigns with external freshness and an independent operator. **Owner:** benchmark
   and distributed-systems owners. **Handoff required:** yes.

### Checks Run

- Passed `paper/check.sh`, `paper/check_evidence.py`, and
  `paper/check_artifact_manifest.py`.
- Rebuilt the PDF at exactly 12 pages and 289,255 bytes without overfull boxes.
- Visually inspected affected rendered pages and confirmed balanced columns.
- Confirmed revised sentences are at most 30 words.
- Passed all three authority verifiers: `snapshots=4 recoveries=1 failures=5`.
- Passed 29/29 Release CTests.

### Checks Skipped

- Native external performance, physical cross-host authority, production key lifecycle,
  public independent full-data rerun, and final submission metadata.
- New public novelty search because Round 21 changes no novelty claim.

### Unresolved Risks

- Joint state-and-witness rollback and absent external freshness.
- Physical replicas, partitions, quorum, election, remote failover, and native external
  performance.
- Public independent reproduction, production key lifecycle, and final metadata.

## 17. Output Self-Check

- Overall score, criterion scores, writing score, and confidence are separated.
- R20-8 is closed locally; no scientific score is inflated from prose alone.
- No claim, count, negative result, or scope boundary was removed.
- Same-host files are not called physical replicas or external anchors.
- Emulated correctness is not called native performance.
- The exact 12-page constraint is preserved without a layout hack.
