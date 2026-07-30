# CCF-A Full Review — Round 25

## 1. Report Metadata

- **Mode:** full scientific, writing, format, artifact, integrity, and reproducibility review.
- **Review date:** 2026-07-25.
- **Venue and assumptions:** IEEE Transactions on Parallel and Distributed Systems,
  regular paper, 2026 submission cycle, 12-page initial manuscript.
- **Paper title:** *Verification-Aware Expert Fusion with Parallel Original-Equation
  Gates for Repeated Numerical Solves*.
- **Materials reviewed:** current 12-page PDF and LaTeX sources; immutable Rounds 1--24;
  three new raw-sample writers; native-performance runner, collector, shared contract
  validator, verifier, aggregator, GitHub workflow, local campaign output, documentation,
  artifact manifest, deterministic bundle registry, and Release test suite.
- **Privacy boundary:** local unpublished manuscript and artifacts only. No private text
  was submitted to an external search service.
- **Search basis:** no scientific novelty or venue-positioning claim changed. The only
  network checks confirmed the declared GitHub Action tags exist; prior official TPDS
  scope and length checks remain the policy basis.
- **Report file:** `ccfa-review-reports/round-25-full-review.md`.

## 2. Desk Rejection Assessment

- **Paper length — pass.** The PDF remains exactly 12 pages and 289,223 bytes. No
  manuscript source or layout setting changed in this round.
- **Topic compatibility — pass.** Verification-aware numerical execution, parallel
  gates, complete-cost routing, and bounded authority evidence remain within TPDS scope.
- **Minimum quality — pass.** Formal assumptions, implementation probes, benchmark
  evidence, retained negative results, and artifact checks remain inspectable.
- **Format and source integrity — pass.** `IEEEtran` journal/compsoc mode is retained;
  the manuscript check reports no undefined reference, citation, or overfull-box failure.
- **Policy/anonymity/compliance — administratively incomplete.** Author, affiliation,
  funding, conflict, acknowledgment, and final public-release metadata remain unresolved.
- **Prompt injection and hidden manipulation — pass locally.** No reviewer-directed
  instruction was found in the inspected manuscript or new workflow documentation.
- **Ethics and reviewability — pass with explicit limits.** Local, emulated, hosted,
  physical-host, archive, workload, and independence boundaries are stated directly.

**Desk-rejection risk:** low scientifically; medium administratively until final
submission metadata and the TPDS checklist are complete.

## 3. Paper Summary

The paper presents verification-aware expert fusion for repeated numerical solves.
Candidate generation, optional correction, mandatory original-equation verification,
and classical fallback are modeled as a complete-cost routing problem. Hybrid DAE IR,
role-restricted experts, immutable problem inputs, atomic publication, and independent
gates support a bounded commit-authority argument. Evaluation spans equation families,
parallel and process-isolated gates, complete paths, external baselines, negative device
and routing outcomes, order sensitivity, failure probes, and artifact reproduction.

The central contributions remain:

1. verification-cost-aware routing over heterogeneous candidate, correction, gate,
   transfer, and fallback stages;
2. an original-equation commit authority under explicit isolation and publication
   assumptions;
3. thread/process gate evidence with strict decision and residual equivalence;
4. complete-cost measurements that retain failures, slowdowns, and finite-sample limits;
5. an inspectable authority and reproduction artifact with explicit scope boundaries.

Round 25 adds no manuscript-level result and no new scientific claim. It builds an
auditable path for a future native external performance campaign.

## 4. Round 25 Scientific Change

Round 25 targets the decisive native-external-performance gap but completes only the
execution and evidence infrastructure:

1. Three existing performance executables now emit self-contained `raw-samples.txt`
   envelopes in addition to their prior summaries.
2. Each campaign replicate contains exactly 2,140 timing values: 300 threaded-gate,
   240 process-gate, 400 offline strict/adaptive, and 1,200 complete-path total/gate values.
3. A shared validator requires the exact raw key grids, finite positive measurements,
   and recomputes all raw-backed reported medians and selected speedups. Operator
   summaries are cross-checked across their evidence, performance, and statistics files.
4. Collection and later verification both use that validator. Verification also rejects
   component paths outside the evidence root and checks every component SHA-256.
5. Local collection is fail-closed as `external_provider=0`, `performance_evidence=0`,
   and `native_external_performance=0`.
6. Hosted collection requires GitHub Actions, GitHub-hosted Linux/X64 context, a reported
   x86-64 machine, full commit SHA, and repository/run/attempt/job/workflow provenance.
7. A manual workflow defines three Ubuntu 24.04 measurement jobs, preserves positive and
   negative timing directions, aggregates median/minimum/maximum across jobs, and requests
   a GitHub artifact attestation for the aggregate manifest.
8. The local Apple M4 campaign completed end to end. It retained a negative Operator
   result (`paired_median_speedup=0.9656`, `stable_speedup=0`) rather than failing or hiding it.
9. Local evidence failed the hosted expectation, recomputed-hash malformed and
   raw-summary-drift fixtures failed, ARM64 hosted-environment spoofing failed, and a
   synthetic three-replicate layout rejected divergent run provenance.

This is credible workflow and evidence-contract hardening. It is not a GitHub-hosted
run. There is no external run URL, hosted artifact, successful attestation, public commit,
or native x86-64 measurement. Therefore Round 25 supplies no external-performance result.

## 5. Likely Stance and Calibrated Score

- **Likely stance:** accept.
- **Overall score:** **8/10**.
- **Scholarly confidence:** **5/5**.
- **Main accept axis:** coherent complete-cost verification design, explicit formal
  assumptions, broad auditable evidence, retained negative results, and fail-closed
  reproducibility contracts.
- **Main reject axis:** absent native external performance, physical replicated
  authority, external monotonic freshness, production key custody, public immutable
  preservation, independent rerun, and final submission metadata.

Round 25 reduces execution friction and strengthens future evidence integrity. It does
not close the external evidence axis because the workflow was not executed. Tooling,
synthetic metadata, and local ARM64 timings cannot support movement toward 9.

## 6. Quantitative Scorecard

| Dimension | Score | Confidence | Evidence basis | Deduction / score-change condition |
| --- | ---: | ---: | --- | --- |
| Novelty | 4/5 | 4/5 | Complete-cost verification-aware fusion remains the scientific contribution; hosted-workflow scaffolding is not novelty | Requires stronger external systems evidence, not more workflow code |
| Soundness | 4/5 | 5/5 | Explicit proposition assumptions; strict gate equivalence; exact raw-summary reconciliation; fail-closed provenance rules | Physical authority, external freshness, and production custody remain absent |
| Evidence | 4/5 | 5/5 | Paired statistics, retained negative results, 2,140-value local package, tamper rejection, and 29 tests | No genuine hosted x86-64 run or physical-fault campaign exists |
| Significance | 4/5 | 4/5 | Broad numerical-service and verification relevance across several equation families | Production distributed and customer-scale evidence remains absent |
| Clarity | 4.75/5 | 5/5 | The manuscript is unchanged; new artifact docs separate readiness, evidence, and non-claims | Breadth still creates density at the fixed page limit |
| Reproducibility | 4/5 | 5/5 | Raw samples, shared validator, component hashes, provenance-bound workflow, manifest, data locks, and deterministic bundle | No public immutable archive, successful hosted artifact, or independent full-data rerun |
| Ethics / Limitations | 5/5 | 5/5 | Local, emulated, hosted, physical-host, workload, archive, and independence limits are explicit | Preserve these limits after any future hosted run |

**Overall:** **8/10** | **Scholarly Confidence:** **5/5**

**Recommendation:** accept.

**Verdict:** Round 25 earns artifact-integrity credit but no score increase. A real
external run or another decisive external axis is required for movement toward 9.

No criterion is 3 or below; no fatal concern is averaged away.

## 7. Major Strengths

1. **Raw timing evidence is now self-contained.** The three measurement families expose
   complete per-repetition samples instead of only aggregate summaries.
2. **Raw-backed summary values are independently reconstructable.** The shared contract
   validates exact grids and recomputes medians and selected ratios during both collection
   and later verification.
3. **Recomputed-hash tampering is not sufficient.** Missing fields and raw-summary drift
   fail even after a component digest is updated.
4. **Negative performance remains admissible.** The local Operator replicate is slower,
   records `stable_speedup=0`, and still yields a valid correctness-preserving artifact.
5. **Local evidence cannot become external by a flag.** Local mode fixes all external
   performance flags to zero; hosted mode also checks reported ISA and GitHub provenance.
6. **Aggregation binds the campaign.** It requires three unique replicate IDs and common
   repository, commit, run, attempt, and workflow reference before cross-job reporting.
7. **Non-claims are machine-readable.** Bare metal, physical independence, PDEBench,
   customer, accelerator, NUMA, distributed, archive, and independent-rerun claims remain zero.

## 8. Major and Moderate Concerns

### Major Concerns

1. **No native external performance was measured.** The only real Round 25 execution is
   Apple M4 local mode. The GitHub workflow cannot be triggered from the current tree
   because there is no usable commit, configured remote, or GitHub CLI/credential path.
2. **No physical distributed authority exists.** Namespaces, containers, files, and a
   provider VM do not prove independent failure domains, quorum, partition behavior,
   split-brain resistance, election, or remote failover.
3. **Freshness remains jointly rollbackable.** State and witness lack an external
   monotonic anchor capable of detecting joint rollback.
4. **Public independent reproduction is absent.** The deterministic bundle is local,
   payloads remain external, and no immutable public deposit or independent rerun exists.
5. **Operational custody remains absent.** Fixture keys do not establish entropy,
   generation, provisioning, KMS/HSM custody, compromise response, or production rotation.

### Moderate Concerns

1. **The future hosted scope is narrow.** Even a successful workflow covers
   provider-hosted VMs and self-contained fixtures, not PDEBench payloads or customers.
2. **Three jobs need not mean three physical hosts.** The provider may schedule jobs on
   distinct VMs without proving physical-host independence; the evidence correctly keeps
   `physical_host_independence_proven=0`.
3. **Attestation is post-manifest.** The aggregate records that attestation is requested,
   not that an attestation is embedded. Reviewers must inspect the successful action step
   and provider attestation separately.
4. **Hosted provenance is environment-derived.** The contract is meaningful only when
   read together with a successful provider-controlled GitHub run, not a local synthetic file.
5. **Submission metadata remains incomplete.** Administrative omissions can still delay
   or block submission despite scientific readiness.

## 9. Claim-Evidence Audit

| Claim | Evidence | Status | Boundary |
| --- | --- | --- | --- |
| Local campaign runs end to end | Local runner output and verified evidence envelope | Supported | Apple M4 local execution only |
| Each replicate has 2,140 raw values | Exact-key shared validator over three sample envelopes | Supported | Included fixture workloads only |
| Summaries match raw samples | Recomputed medians and ratios during collection and verification | Supported | Raw-backed metrics, not every confidence interval or Operator repetition |
| Negative outcomes are retained | Local Operator `0.9656`, `stable_speedup=0`; no threshold gate | Supported | One local campaign observation |
| Hosted mode is fail-closed | Required GitHub context, x86-64 machine, full provenance; ARM64 spoof rejection | Supported as contract | Not proof that a hosted run occurred |
| Three hosted jobs aggregate consistently | Synthetic layout and provenance-mismatch rejection | Workflow-readiness only | Synthetic metadata is not scientific evidence |
| Native external performance exists | No run URL, artifact, attestation, or hosted measurement | Not supported | Must remain false |
| Physical-host independence exists | Explicit machine-readable zero | Not supported | Provider VMs are not physical proof |
| Public independent reproduction exists | Local deterministic bundle only | Not supported | No public immutable deposit or independent operator |

The manuscript itself was not changed and makes no new hosted-performance claim. The
new documentation consistently labels the workflow as an unexecuted enabler.

## 10. Experiment, Benchmark, and Reproducibility Audit

- **Raw units — pass.** Thread and process scaling use 30 paired repetitions; adaptive
  and Operator studies retain their 100-repetition contracts.
- **Raw completeness — pass.** Exact expected keys reject missing, extra, non-finite, or
  non-positive values.
- **Summary reconciliation — pass.** Medians and selected ratios are recomputed from raw
  values rather than trusted from summaries.
- **Component integrity — pass.** Nine component files are constrained beneath the
  evidence root and bound by SHA-256.
- **Direction-agnostic policy — pass.** Correctness gates collection, while performance
  direction does not; the observed slower Operator path is retained.
- **Local boundary — pass.** Local evidence verifies only under `--expect-local` and
  fails `--expect-github-hosted`.
- **ISA spoof resistance — pass within scope.** GitHub-looking variables on Apple ARM64
  fail because the actual machine does not report x86-64.
- **Aggregation — pass synthetically.** Three valid synthetic hosted envelopes aggregate;
  changing one `run_id` fails. Synthetic success is not counted as external evidence.
- **Action availability — pass.** Declared `checkout`, artifact upload/download, and
  attestation v4 tags exist; YAML parses locally.
- **Hosted execution — absent.** No GitHub-run behavior, VM assignment, upload, or
  attestation was observed.
- **Public reproducibility — incomplete.** The local archive and data locks do not replace
  a public immutable deposit or independent full-data rerun.

## 11. Writing and Presentation Review

### Writing Scorecard

| Dimension | Score | Evidence | Residual risk |
| --- | ---: | --- | --- |
| Global argument | 4.8/5 | Problem, verified-cost insight, mechanism, evidence, and limitations remain ordered | Breadth remains high |
| Paragraph logic | 4.8/5 | Manuscript prose is unchanged from the polished Round 24 version | Evaluation remains information-dense |
| Claim-evidence visibility | 4.9/5 | Negative outcomes and external boundaries remain visible | External gaps cannot be repaired by workflow prose |
| Terminology consistency | 4.8/5 | Native, emulated, same-host, physical, and independent terms remain separated | Future hosted reporting must preserve distinctions |
| Figure/table narration | 4.7/5 | Results and limits remain connected to Tables 3--7 and Figures 3--5 | Fixed-length density persists |
| Accessibility | 4.5/5 | Layered contributions and explicit limits aid recovery | Broad systems/equations scope remains demanding |

**Writing quality:** **4.75/5**.

### Writing Findings

- No manuscript sentence, claim, figure, table, citation, or layout source changed.
- New documentation clearly distinguishes an executable enabler from measured evidence.
- The local campaign's slower Operator outcome is stated rather than suppressed.
- Scope exclusions cover physical hosts, payloads, customers, accelerators, NUMA,
  distributed execution, independent reproduction, public archiving, and production custody.
- Documentation is necessarily detailed, but it does not consume manuscript page budget.

## 12. Format and LaTeX Audit

- Exactly 12 pages, 289,223 bytes.
- PDF SHA-256: `97150ebc18a4845a01c346979ee45e8d86905eadeef34177461d57653522b278`.
- `IEEEtran` journal/compsoc mode remains unchanged.
- `paper/check.sh` reports all targets current and passes evidence and manifest checks.
- No manuscript file was modified in Round 25; the rendered PDF is byte-identical to Round 24.
- No spacing, margin, font-size, negative-vspace, or float-placement hack was introduced.

## 13. Multi-Reviewer Panel

### Reviewer 1 — Method and Soundness

- **Lens:** numerical systems, transaction invariants, and evidence derivation.
- **Score / score tendency:** 8/10.
- **Confidence:** 5/5.
- **Main positive signal:** raw samples now reconstruct the principal performance summaries.
- **Main negative signal:** workflow correctness does not provide external measurement.
- **Evidence basis:** sample writers, shared validator, local package, tamper tests.
- **Score-change condition:** a genuine hosted campaign tied to the reviewed source commit.

### Reviewer 2 — Distributed Systems

- **Lens:** replication, failure domains, monotonicity, and failover.
- **Score / score tendency:** 7--8/10.
- **Confidence:** 5/5.
- **Main positive signal:** provider-VM and physical-host claims are kept distinct.
- **Main negative signal:** no physical replicas, quorum, partition, election, or remote failover.
- **Evidence basis:** workflow topology fields, aggregate non-claims, authority limitations.
- **Score-change condition:** physical independent failure domains and fault campaigns.

### Reviewer 3 — Evidence and Experiments

- **Lens:** statistics, performance methodology, and external validity.
- **Score / score tendency:** 8/10.
- **Confidence:** 5/5.
- **Main positive signal:** exact raw grids and direction-agnostic acceptance reduce selective reporting risk.
- **Main negative signal:** all real authoritative performance remains on Apple M4.
- **Evidence basis:** 2,140-value local package, slower Operator result, no hosted artifact.
- **Score-change condition:** native hosted paired complete-cost measurements with raw artifacts.

### Reviewer 4 — Novelty and Positioning

- **Lens:** originality, contribution type, and TPDS fit.
- **Score / score tendency:** 8/10.
- **Confidence:** 4/5.
- **Main positive signal:** the complete-cost verification-aware fusion framing remains coherent.
- **Main negative signal:** Round 25 is reproducibility engineering, not new scientific novelty.
- **Evidence basis:** unchanged manuscript claims and artifact-only changes.
- **Score-change condition:** stronger systems evidence tied to the central design.

### Reviewer 5 — Writing and Clarity

- **Lens:** argument flow, terminology, and reviewer recoverability.
- **Score / score tendency:** 8/10; writing 4.75/5.
- **Confidence:** 5/5.
- **Main positive signal:** enabler, local dry-run, hosted evidence, and physical claims are separated.
- **Main negative signal:** the underlying manuscript remains dense at 12 pages.
- **Evidence basis:** unchanged PDF plus new bounded documentation.
- **Score-change condition:** no documentation-only change should alter the scientific score.

### Reviewer 6 — Artifact, Security, and Reproducibility

- **Lens:** provenance, tamper resistance, custody, and independent rerun.
- **Score / score tendency:** 8/10.
- **Confidence:** 5/5.
- **Main positive signal:** component hashes alone are no longer the only integrity check.
- **Main negative signal:** no public commit, successful provider attestation, or independent operator exists.
- **Evidence basis:** raw-summary reconciliation, path confinement, manifest, local bundle.
- **Score-change condition:** public immutable deposit plus independent full-data rerun.

### Reviewer 7 — Domain Application

- **Lens:** scientific-computing workload relevance and deployment realism.
- **Score / score tendency:** 7--8/10.
- **Confidence:** 4/5.
- **Main positive signal:** the workflow preserves complete-path and gate measurements.
- **Main negative signal:** hosted scope excludes PDEBench payloads and customer workloads.
- **Evidence basis:** campaign claim scope and machine-readable workload exclusions.
- **Score-change condition:** representative external payload and deployment evidence.

### Reviewer 8 — Novice Advocate

- **Lens:** accessibility, inspectability, and claim comprehension.
- **Score / score tendency:** 8/10.
- **Confidence:** 4/5.
- **Main positive signal:** explicit zero flags make the absence of external evidence hard to miss.
- **Main negative signal:** nine files and several provenance layers remain complex to audit manually.
- **Evidence basis:** campaign envelope and dedicated protocol document.
- **Score-change condition:** a public run page and compact reviewer verification recipe.

### Panel Synthesis

- **Agreement:** the new tooling materially improves future evidence integrity and retains
  negative outcomes, but does not itself constitute an external experiment.
- **Disagreement:** evidence reviewers value the raw contract strongly; distributed and
  application reviewers assign little scientific credit without external workloads or hosts.
- **Decisive accept axis:** mature verification-aware design, broad bounded evidence,
  honest negatives, and strong artifact discipline.
- **Decisive reject axis:** all decisive external systems axes remain unobserved.
- **Unresolved evidence:** native hosted run, physical replication, external freshness,
  public independent rerun, production custody, representative external workloads.
- **Final calibrated stance:** accept at 8/10, confidence 5/5.

## 14. Concern-to-Action Table

| ID | Severity | Concern | Evidence basis | Fix class | Required action | Score impact condition |
| --- | --- | --- | --- | --- | --- | --- |
| C1 | Major | No genuine native external performance | Local flags are zero; no run URL or artifact | External execution | Publish a real commit, dispatch the workflow, retain all jobs and attestation | Could support movement toward 9 if results and scope remain sound |
| C2 | Major | No physical distributed authority | Same-host and provider-VM evidence only | Distributed experiment | Use independent hosts; inject partitions, leader loss, split brain, and failover | Could support movement toward 9 |
| C3 | Major | No external rollback freshness | State and witness share rollback domain | Security architecture | Add an independently administered monotonic anchor and joint-rollback tests | Could support movement toward 9 |
| C4 | Major | No public independent reproduction | Local deterministic bundle only | Archival/reproduction | Deposit source/data locks/raw reports and obtain an independent full-data rerun | Could support movement toward 9 |
| C5 | Major | No operational key custody | Fixture keys and local policies only | Security operations | Demonstrate generation, custody, rotation, revocation, and compromise recovery | Supporting only unless coupled to external authority |
| C6 | Moderate | Hosted workload scope is narrow | Self-contained fixtures; payload flags zero | Benchmark design | Extend the same raw/provenance contract to representative payloads | Strengthens an external-performance case |
| C7 | Administrative | Submission metadata incomplete | Placeholders remain | Author action | Finalize authorship, funding, conflicts, acknowledgments, and release metadata | Removes desk/compliance risk, not scientific score |

## 15. AC / Meta-Review

The paper remains a strong, evidence-disciplined systems contribution centered on
verification-aware complete-cost runtime for heterogeneous repeated solves. Round 25
addresses the correct next blocker: external performance rather than another local
authority fixture. The implementation is carefully bounded and technically credible.

The strongest improvement is not the workflow YAML itself but the evidence contract.
Raw samples are complete and independently reconciled with summaries, component paths
and hashes are checked, negative timing directions are admissible, and provenance must
agree across three jobs. The local run validates the full collection path and preserves
a real slowdown. Adversarial checks reject malformed raw grids, raw-summary drift,
hosted expectation on local evidence, ARM64 context spoofing, and divergent run IDs.

However, no hosted execution occurred. Synthetic hosted envelopes demonstrate parser
and aggregator behavior only. They cannot stand in for provider-controlled execution,
artifact upload, VM provenance, or attestation. All authoritative performance therefore
remains single-host Apple M4 evidence. Physical authority, external freshness, public
independent reproduction, production custody, and metadata also remain absent.

The score must stay at 8. Raising it for an unexecuted workflow would violate evidence
calibration and reward readiness as if it were a result.

**Meta-review recommendation:** accept, 8/10, confidence 5/5.

## 16. Score Revision Criteria

### Raise Toward 9

- Execute the workflow from a public or review-accessible immutable commit and preserve
  the successful run URL, three raw replicate artifacts, aggregate, and attestation.
- Retain all slowdowns and confidence intervals; do not add a speedup threshold.
- Preferably extend external measurement to representative complete payloads rather than
  only self-contained fixtures.
- Alternatively, provide physical cross-host authority with independent failure domains,
  partition/failover tests, and externally anchored freshness.
- A public immutable archive plus independent full-data rerun remains another decisive axis.

One genuine major external axis could support 9 if soundness and claim discipline remain.
A 10 requires several external axes to converge without introducing a central weakness.

### Lower the Score

- Calling the local ARM64 dry-run native external performance.
- Calling synthetic hosted metadata or YAML validation an external experiment.
- Treating three provider jobs as proof of distinct physical hosts.
- Calling `artifact_attestation_requested=1` proof that attestation succeeded.
- Generalizing self-contained fixture timing to PDEBench, customers, accelerators, NUMA,
  networked systems, or distributed execution.
- Dropping the slower Operator result or adding a positive-speedup workflow gate.
- Failing raw-summary, manifest, evidence, test, page, archive, or clean-tree gates.

## 17. Recommended Next CCFA Owner

1. **Immediate — artifact/integrity owner:** update latest-review pointers, regenerate
   the manifest, freeze Round 25 in the deterministic archive, compare two generations,
   and rerun the clean-tree verifier.
2. **Immediate — integrity auditor:** run stale-term, duplicate-field, allowlist, claim,
   workflow, page, hash, archive, and clean-tree audits on the final tree.
3. **Decisive external — repository/benchmark owner:** establish a real immutable commit
   and remote, dispatch the hosted campaign, and preserve provider evidence unchanged.
4. **Submission — authors:** finalize author, affiliation, funding, conflict,
   acknowledgment, policy, and public-archive metadata.
5. **Other external owners:** pursue physical authority, external freshness, production
   custody, representative workloads, and independent reproduction.

## 18. Checks Run

- End-to-end local campaign completed in Release mode on Apple M4/ARM64.
- Local verifier passed with `external_provider=0`, `performance_evidence=0`, and
  `native_external_performance=0`.
- Raw component counts were 300 threaded, 240 process, and 1,600 adaptive values,
  totaling 2,140.
- Raw grids and summary medians/ratios reconciled through the shared validator.
- Recomputed-hash malformed-grid and raw-summary-drift fixtures were rejected.
- Local evidence was rejected under `--expect-github-hosted`.
- GitHub-looking hosted variables were rejected on the ARM64 machine.
- Three explicitly synthetic hosted envelopes aggregated; divergent `run_id` failed.
- Workflow YAML parsed; declared GitHub Action v4 tags exist.
- Python compilation and shell syntax checks passed.
- `python3 paper/check_artifact_manifest.py` passed.
- `python3 paper/check_evidence.py` passed.
- `paper/check.sh` passed at exactly 12 pages and 289,223 bytes.
- PDF SHA-256 remained
  `97150ebc18a4845a01c346979ee45e8d86905eadeef34177461d57653522b278`.
- `ctest --test-dir build/release --output-on-failure` passed 29/29 tests.

## 19. Unresolved or Unverified

- A genuine GitHub-hosted native x86-64 run, uploaded artifacts, and successful attestation.
- Native external PDEBench/customer payload and accelerator/NUMA/distributed performance.
- Physical cross-host replicas, quorum, partitions, election, and remote failover.
- External monotonic freshness and joint state/witness rollback detection.
- Production key entropy, generation, custody, KMS/HSM, and compromise recovery.
- Public immutable archive, persistent identifier, independent full-data rerun, and final metadata.

## 20. Output Self-Check

- Overall score, criterion scores, writing score, and confidence are separated.
- Workflow readiness receives integrity credit without being counted as external evidence.
- Local, synthetic, hosted, provider-VM, physical-host, and independent claims are distinct.
- Requested attestation is not called successful attestation.
- The slower Operator result is retained and explicitly credited as a negative result.
- Same-host files and namespaces are not called physical replicas.
- Emulated x86-64 correctness is not called native performance.
- No acceptance probability or unsupported novelty claim is stated.
- The exact 12-page constraint is preserved without a layout hack.
