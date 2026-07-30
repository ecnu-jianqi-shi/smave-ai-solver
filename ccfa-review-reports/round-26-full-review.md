# CCF-A Full Review — Round 26

## 1. Report Metadata

- **Mode:** full scientific, writing, format, artifact, integrity, and reproducibility review.
- **Review date:** 2026-07-25.
- **Venue and assumptions:** IEEE Transactions on Parallel and Distributed Systems,
  regular paper, 2026 submission cycle, 12-page initial manuscript.
- **Paper title:** *Verification-Aware Expert Fusion with Parallel Original-Equation
  Gates for Repeated Numerical Solves*.
- **Materials reviewed:** current PDF and LaTeX sources; immutable Rounds 1--25; replicate
  collector/verifier; aggregate generator; new independent campaign verifier; synthetic-
  only contract checker; GitHub workflow; local dry-run package; documentation, artifact
  manifest, bundle tooling, and 29-test Release suite.
- **Privacy boundary:** local unpublished manuscript and artifacts only. No private text
  was submitted to an external search service.
- **Search basis:** scientific novelty and venue positioning did not change. The official
  `actions/download-artifact@v4` contract was checked only to confirm that
  `merge-multiple: false` creates one named directory per downloaded artifact.
- **Report file:** `ccfa-review-reports/round-26-full-review.md`.

## 2. Desk Rejection Assessment

- **Paper length — pass.** The PDF remains exactly 12 pages and 289,223 bytes.
- **Topic compatibility — pass.** Verification-aware numerical execution, complete-cost
  routing, parallel gates, and bounded authority evidence remain within TPDS scope.
- **Minimum quality — pass.** Formal assumptions, broad benchmark evidence, negative
  results, fault probes, and reproducibility controls remain inspectable.
- **Format and source integrity — pass.** `IEEEtran` journal/compsoc mode is unchanged;
  manuscript checks pass with no layout manipulation.
- **Policy/anonymity/compliance — administratively incomplete.** Author, affiliation,
  funding, conflict, acknowledgment, and final release metadata remain unresolved.
- **Prompt injection and hidden manipulation — pass locally.** No reviewer-directed
  instruction was found in the inspected manuscript or artifact sources.
- **Ethics and reviewability — pass with limits.** Synthetic, local, hosted, provider-VM,
  physical-host, attestation, archive, and independence states are explicitly separated.

**Desk-rejection risk:** low scientifically; medium administratively until final
submission metadata and the TPDS checklist are complete.

## 3. Paper Summary

The paper presents verification-aware expert fusion for repeated numerical solves.
Candidate generation, optional correction, mandatory original-equation verification,
and classical fallback are optimized under complete verified cost. Hybrid DAE IR,
role-restricted experts, immutable inputs, atomic publication, and independent gates
support a bounded commit-authority invariant. Experiments report complete paths,
thread/process gate scaling, external baselines, order sensitivity, negative device and
routing results, failure probes, and artifact reproduction.

The central scientific contributions remain unchanged:

1. verification-cost-aware routing over candidate, transfer, correction, gate, and
   fallback stages;
2. original-equation commit authority under explicit isolation and publication assumptions;
3. strict-equivalent parallel and process-isolated gate implementations;
4. complete-cost measurement with failures, slowdowns, and finite-sample limits retained;
5. an inspectable authority and reproduction artifact with bounded claims.

Round 26 changes artifact verification only. It adds no manuscript result, method claim,
or scientific novelty claim.

## 4. Round 26 Scientific Change

Round 25 produced a provenance-bound aggregate but left reviewers to trust that the
aggregate program had encoded every downloaded replicate correctly. Round 26 closes that
reviewer-side verification gap:

1. The aggregate now sorts validated replicates by ID and requires exactly IDs 1, 2, and 3.
2. It records each replicate's relative evidence path, SHA-256, collection time, job,
   invocation URL, runner system/architecture, CPU, logical CPUs, memory, and virtualization.
3. `verify_native_external_performance_campaign.py` requires an exact campaign schema;
   missing fields, unexpected fields, or altered machine-readable non-claims fail.
4. Every referenced evidence path must remain beneath the download root, exist exactly
   once, and equal the complete one-directory-per-artifact file set.
5. Each replicate is revalidated through the Round 25 component/raw-sample verifier.
6. Campaign IDs, hashes, and metadata must match the replicate bytes exactly.
7. Repository, full commit SHA, run ID, run attempt, and workflow reference must agree
   across all replicates and with the aggregate.
8. Every selected cross-job median, minimum, and maximum is independently recomputed.
9. The GitHub workflow runs this verifier after aggregation and before upload/attestation.
10. A bundled synthetic-only contract check accepts one clean three-replicate fixture and
    requires metric, digest, schema, and provenance tamper cases to fail.

The contract check prints `synthetic=1` and `external_evidence=0`. The aggregate retains
`artifact_attestation_embedded=0`; the verifier cannot prove that the later provider
attestation succeeded. No GitHub-hosted run exists.

## 5. Likely Stance and Calibrated Score

- **Likely stance:** accept.
- **Overall score:** **8/10**.
- **Scholarly confidence:** **5/5**.
- **Main accept axis:** coherent verification-aware complete-cost design, explicit formal
  assumptions, broad bounded evidence, retained negative results, and unusually strong
  local artifact integrity.
- **Main reject axis:** absent native external performance, physical replicated authority,
  external freshness, production custody, public immutable preservation, independent
  rerun, representative external payloads, and final metadata.

Round 26 makes a future hosted result easier for a reviewer to verify independently. It
does not create that result. Parser tests, synthetic envelopes, and pre-attestation
verification cannot support movement toward 9.

## 6. Quantitative Scorecard

| Dimension | Score | Confidence | Evidence basis | Deduction / score-change condition |
| --- | ---: | ---: | --- | --- |
| Novelty | 4/5 | 4/5 | Complete-cost verification-aware fusion remains the contribution; aggregate verification is artifact engineering | Requires external systems evidence, not another verifier |
| Soundness | 4/5 | 5/5 | Explicit assumptions, strict gate equivalence, raw reconstruction, exact aggregate reconstruction | Physical authority, external freshness, and operational custody remain absent |
| Evidence | 4/5 | 5/5 | Paired statistics, negatives, raw samples, four campaign-level tamper rejections, 29 tests | No provider-controlled hosted execution or physical fault campaign |
| Significance | 4/5 | 4/5 | Broad relevance to repeated scientific solves and verified execution | Production distributed and customer-scale evidence remains absent |
| Clarity | 4.75/5 | 5/5 | Synthetic, local, hosted, attested, VM, and physical claims are separated | Fixed-length manuscript remains dense |
| Reproducibility | 4/5 | 5/5 | Replicate and campaign verifiers, exact schema, manifest, data locks, deterministic bundle | No public immutable deposit, hosted artifact, or independent full-data rerun |
| Ethics / Limitations | 5/5 | 5/5 | Attestation and external-evidence boundaries are explicit | Preserve them after any future hosted run |

**Overall:** **8/10** | **Scholarly Confidence:** **5/5**

**Recommendation:** accept.

**Verdict:** Round 26 earns reviewer-verifiability credit but no scientific score movement.
One genuine major external axis remains necessary for 9.

No criterion is 3 or below; no fatal concern is averaged away.

## 7. Major Strengths

1. **The aggregate is no longer trusted by construction.** A separate program rebuilds
   its schema, file set, provenance, metadata, hashes, and statistics.
2. **Replicate omission and substitution are detectable.** Referenced paths must equal
   the complete downloaded one-directory-per-artifact set.
3. **Ordering ambiguity is removed.** IDs must be exactly consecutive 1--3, independent
   of directory lexical order.
4. **Campaign statistic drift fails.** Median, minimum, and maximum are recomputed for
   every selected metric rather than accepted from the aggregate.
5. **Unexpected claims fail.** Exact schema validation rejects inserted fields instead of
   silently ignoring them.
6. **Tamper tests are reproducible.** The bundled checker covers metric, digest, schema,
   and provenance modifications in a temporary synthetic-only tree.
7. **Attestation semantics remain honest.** Pre-attestation verification is not called a
   successful provider attestation.

## 8. Major and Moderate Concerns

### Major Concerns

1. **No native external performance was measured.** The only real performance campaign
   remains the local Apple M4 dry-run with all external-performance flags zero.
2. **No physical distributed authority exists.** Provider VMs, namespaces, files, and
   containers do not prove independent failure domains, partitions, quorum, election,
   split-brain resistance, or remote failover.
3. **Freshness is not externally anchored.** State and witness remain jointly rollbackable.
4. **Public independent reproduction is absent.** The bundle is author-operated and
   local; no immutable public deposit or independent full-data rerun exists.
5. **Operational key custody is absent.** Fixture controls do not establish generation,
   entropy, provisioning, KMS/HSM custody, compromise recovery, or production rotation.

### Moderate Concerns

1. **Attestation still requires external inspection.** The verifier runs before the
   attestation action and deliberately cannot validate its later receipt.
2. **Synthetic fixtures prove software behavior only.** They do not authenticate GitHub,
   a VM, a commit, or a provider-controlled environment.
3. **Hosted scope remains narrow.** Self-contained fixtures do not establish PDEBench,
   customer, accelerator, NUMA, network, or distributed performance.
4. **Three jobs do not prove three physical hosts.** The correct machine-readable field
   remains `physical_host_independence_proven=0`.
5. **Submission metadata remains incomplete.** Administrative work can still block filing.

## 9. Claim-Evidence Audit

| Claim | Evidence | Status | Boundary |
| --- | --- | --- | --- |
| Aggregate matches all downloaded replicates | Independent exact-schema/file/hash/metadata/statistic verifier | Supported as software contract | Requires supplied artifact tree |
| Replicate IDs are complete and ordered | Aggregator and verifier require exactly 1, 2, 3 | Supported | Three-job campaign only |
| Campaign tampering fails | Reproducible metric, digest, schema, provenance rejection cases | Supported | Synthetic-only integrity tests |
| Artifact download layout matches verifier | Official action contract for `merge-multiple: false` | Supported | One named directory per artifact |
| Provider attestation succeeded | No run or receipt; aggregate says embedded zero | Not supported | Must remain externally verified |
| Native external performance exists | No hosted run URL, artifacts, or measurements | Not supported | Must remain false |
| Physical-host independence exists | Explicit zero | Not supported | Provider VM jobs are insufficient |
| Independent reproduction exists | Local author-operated bundle only | Not supported | No independent operator or public deposit |

No manuscript claim changed. New documentation consistently labels the campaign checker
as synthetic-only and non-evidence.

## 10. Experiment, Benchmark, and Reproducibility Audit

- **Replicate verification — pass.** Every replicate re-enters the full raw/component verifier.
- **File-set verification — pass.** Referenced and discovered evidence files must match exactly.
- **Path confinement — pass.** Relative evidence paths cannot escape the download root.
- **Schema verification — pass.** Missing and unexpected campaign fields fail.
- **Hash/metadata verification — pass.** Campaign fields must equal replicate bytes and values.
- **Provenance verification — pass.** Repository, commit, run, attempt, and workflow ref agree.
- **Statistics verification — pass.** All selected median/minimum/maximum values are recomputed.
- **Negative contract tests — pass.** Four synthetic-only tamper classes fail reproducibly.
- **Workflow sequencing — pass as configuration.** Verification occurs before upload and attestation.
- **Official layout compatibility — pass.** Separate artifact directories match the verifier glob.
- **Hosted execution — absent.** No provider-controlled behavior or attestation receipt was observed.
- **Public reproducibility — incomplete.** Local determinism is not independent reproduction.

## 11. Writing and Presentation Review

### Writing Scorecard

| Dimension | Score | Evidence | Residual risk |
| --- | ---: | --- | --- |
| Global argument | 4.8/5 | Manuscript contribution/evidence/limit order remains stable | Breadth remains high |
| Paragraph logic | 4.8/5 | Manuscript prose is unchanged from Round 25 | Evaluation remains information-dense |
| Claim-evidence visibility | 4.9/5 | New docs make pre-attestation and synthetic boundaries explicit | External gaps cannot be repaired by prose |
| Terminology consistency | 4.8/5 | Replicate, campaign, attestation, VM, physical, and independent terms remain distinct | Future run reporting must preserve distinctions |
| Figure/table narration | 4.7/5 | Existing results remain tied to Tables 3--7 and Figures 3--5 | Fixed-length density persists |
| Accessibility | 4.5/5 | Reviewer verification commands are explicit | Multi-layer artifact remains complex |

**Writing quality:** **4.75/5**.

### Writing Findings

- No manuscript sentence, figure, table, citation, or LaTeX layout source changed.
- Documentation explains the replicate verifier, aggregate verifier, and attestation boundary.
- Synthetic checks are labeled `external_evidence=0` rather than presented as experiments.
- The campaign recipe is now independently executable by reviewers after artifact download.
- Documentation detail does not consume the 12-page manuscript budget.

## 12. Format and LaTeX Audit

- Exactly 12 pages, 289,223 bytes.
- PDF SHA-256: `97150ebc18a4845a01c346979ee45e8d86905eadeef34177461d57653522b278`.
- `IEEEtran` journal/compsoc mode remains unchanged.
- `paper/check.sh` passes evidence, manifest, and PDF checks.
- The PDF remains byte-identical to Rounds 24 and 25.
- No spacing, margin, font-size, negative-vspace, or float hack was introduced.

## 13. Multi-Reviewer Panel

### Reviewer 1 — Method and Soundness

- **Lens:** numerical systems and evidence derivation.
- **Score / score tendency:** 8/10.
- **Confidence:** 5/5.
- **Main positive signal:** aggregate results are independently reconstructable.
- **Main negative signal:** no external measurement occurred.
- **Evidence basis:** campaign verifier and synthetic tamper checker.
- **Score-change condition:** provider-controlled run bound to the reviewed commit.

### Reviewer 2 — Distributed Systems

- **Lens:** replicas, failure domains, partitions, and freshness.
- **Score / score tendency:** 7--8/10.
- **Confidence:** 5/5.
- **Main positive signal:** VM and physical-host claims remain separate.
- **Main negative signal:** no physical authority or external monotonic anchor exists.
- **Evidence basis:** machine-readable zeros and authority limits.
- **Score-change condition:** independent hosts and fault/freshness campaigns.

### Reviewer 3 — Evidence and Experiments

- **Lens:** performance statistics and selective-reporting risk.
- **Score / score tendency:** 8/10.
- **Confidence:** 5/5.
- **Main positive signal:** every selected cross-job statistic is recomputed.
- **Main negative signal:** the job artifacts themselves do not exist.
- **Evidence basis:** verifier implementation and absent hosted provenance.
- **Score-change condition:** real raw hosted artifacts with retained slowdowns.

### Reviewer 4 — Novelty and Positioning

- **Lens:** originality and TPDS contribution type.
- **Score / score tendency:** 8/10.
- **Confidence:** 4/5.
- **Main positive signal:** core complete-cost verified-execution framing remains strong.
- **Main negative signal:** Round 26 is artifact engineering, not scientific novelty.
- **Evidence basis:** unchanged manuscript and new tooling-only diff.
- **Score-change condition:** external systems evidence tied to the central claim.

### Reviewer 5 — Writing and Clarity

- **Lens:** terminology and reviewer recoverability.
- **Score / score tendency:** 8/10; writing 4.75/5.
- **Confidence:** 5/5.
- **Main positive signal:** pre-attestation verification is precisely named.
- **Main negative signal:** artifact verification now spans several layers.
- **Evidence basis:** protocol document and command sequence.
- **Score-change condition:** no local documentation change should raise the scientific score.

### Reviewer 6 — Artifact, Security, and Reproducibility

- **Lens:** provenance, tamper resistance, and independent reproduction.
- **Score / score tendency:** 8/10.
- **Confidence:** 5/5.
- **Main positive signal:** exact schema and complete file-set checks close substitution gaps.
- **Main negative signal:** no public source commit, provider receipt, or independent operator.
- **Evidence basis:** verifier, manifest, bundle, and repository facts.
- **Score-change condition:** immutable public deposit and independent full-data rerun.

### Reviewer 7 — Domain Application

- **Lens:** workload representativeness and deployment realism.
- **Score / score tendency:** 7--8/10.
- **Confidence:** 4/5.
- **Main positive signal:** the future campaign preserves complete-path timings.
- **Main negative signal:** payload and customer performance remain explicit zeros.
- **Evidence basis:** campaign claim scope.
- **Score-change condition:** representative external payload measurements.

### Reviewer 8 — Novice Advocate

- **Lens:** inspectability and claim comprehension.
- **Score / score tendency:** 8/10.
- **Confidence:** 4/5.
- **Main positive signal:** one verifier command now checks the whole downloaded campaign.
- **Main negative signal:** external attestation still requires a separate provider step.
- **Evidence basis:** hosted verification recipe.
- **Score-change condition:** a real public run page and receipt.

### Panel Synthesis

- **Agreement:** independent aggregate verification materially improves auditability but is
  not an external result.
- **Disagreement:** artifact reviewers assign strong reproducibility credit; distributed
  and application reviewers see no decisive scientific-axis movement.
- **Decisive accept axis:** mature verified-execution design, broad honest evidence, and
  strong artifact discipline.
- **Decisive reject axis:** all major external axes remain absent.
- **Unresolved evidence:** hosted run/attestation, physical authority, external freshness,
  public independent rerun, production custody, representative payloads, final metadata.
- **Final calibrated stance:** accept at 8/10, confidence 5/5.

## 14. Concern-to-Action Table

| ID | Severity | Concern | Evidence basis | Fix class | Required action | Score impact condition |
| --- | --- | --- | --- | --- | --- | --- |
| C1 | Major | No native external performance | No run URL, artifacts, or receipt | External execution | Publish immutable commit, dispatch workflow, preserve all outputs | Could support 9 if sound and bounded |
| C2 | Major | No physical distributed authority | VM/same-host evidence only | Distributed experiment | Independent hosts plus partition/election/failover faults | Could support 9 |
| C3 | Major | No external freshness | Joint rollback domain | Security architecture | Independent monotonic anchor and rollback tests | Could support 9 |
| C4 | Major | No public independent rerun | Local bundle only | Archive/reproduction | Public immutable deposit and independent full-data rerun | Could support 9 |
| C5 | Major | No operational custody | Fixture keys only | Security operations | Generation, custody, rotation, revocation, compromise recovery | Supporting unless coupled to authority |
| C6 | Moderate | Hosted scope is self-contained | Payload/customer fields zero | Benchmark design | Extend raw/provenance contract to representative payloads | Strengthens external case |
| C7 | Administrative | Submission metadata incomplete | Placeholders remain | Author action | Finalize authorship and compliance metadata | Removes desk risk only |

## 15. AC / Meta-Review

Round 26 is a disciplined response to a moderate artifact concern. The authors do not
merely add more provenance fields; they provide a separate verifier that reconstructs
the aggregate from the downloaded artifacts and rejects omissions, substitutions,
metadata drift, provenance drift, statistic drift, and schema expansion. The workflow
runs that verifier before upload and attestation. A bundled temporary-fixture check makes
four negative cases reproducible without leaving synthetic artifacts in the archive.

This improves reviewer confidence that a future campaign summary will represent its
replicate artifacts accurately. It also reduces the risk that an added field could
silently broaden claims. The official artifact-download layout matches the verifier's
one-directory-per-artifact assumption.

Nevertheless, this is still pre-experiment infrastructure. No provider-controlled run,
VM execution, upload, or attestation receipt exists. The verifier itself states that the
manifest has no embedded attestation, and the synthetic checker states that it supplies
no external evidence. Physical authority, external freshness, public independent
reproduction, operational custody, representative payloads, and metadata remain open.

The calibrated score remains 8. Raising it for better verification of a nonexistent
external result would be evidence inflation.

**Meta-review recommendation:** accept, 8/10, confidence 5/5.

## 16. Score Revision Criteria

### Raise Toward 9

- Execute the workflow from an immutable review-accessible commit.
- Preserve the successful run URL, three replicate directories, exact aggregate,
  verifier log, upload artifact, and provider attestation receipt.
- Retain all slowdowns and intervals without a positive-speedup gate.
- Prefer representative complete payloads in addition to self-contained fixtures.
- Alternatively close another major external axis: physical authority and freshness, or
  public immutable preservation with independent full-data reproduction.

One genuine major external axis could support 9. A 10 requires several such axes to
converge without introducing a central weakness.

### Lower the Score

- Calling the synthetic contract check a hosted experiment.
- Calling pre-attestation verification proof that attestation succeeded.
- Omitting a slower replicate or adding a speedup threshold.
- Treating three VM jobs as three physical hosts.
- Generalizing self-contained timings to payloads, customers, accelerators, NUMA,
  networked systems, or distributed execution.
- Failing campaign schema, hash, provenance, statistic, manifest, test, page, archive,
  or clean-tree gates.

## 17. Recommended Next CCFA Owner

1. **Immediate — artifact/integrity owner:** update latest-review pointers, regenerate
   the manifest, freeze Round 26 in two deterministic archives, and rerun clean extraction.
2. **Immediate — integrity auditor:** rerun campaign-contract, workflow, stale-pointer,
   hash, page, archive, and clean-tree checks.
3. **Decisive external — repository/benchmark owner:** establish an immutable commit and
   remote, dispatch the real hosted campaign, and preserve provider outputs unchanged.
4. **Submission — authors:** finalize author, funding, conflict, acknowledgment, and
   public-archive metadata.
5. **Other external owners:** pursue physical authority, external freshness, operational
   custody, representative workloads, and independent reproduction.

## 18. Checks Run

- Python compilation passed for collector, replicate verifier, aggregator, campaign
  verifier, synthetic contract checker, manifest, and bundle tools.
- Workflow YAML parsed and places campaign verification before upload and attestation.
- Official `download-artifact@v4` documentation confirms separate named directories when
  `merge-multiple` is false.
- Local campaign package reverified as local-only.
- Synthetic clean aggregate passed exact campaign verification.
- Metric drift, replicate digest drift, unexpected schema field, and divergent run ID
  each failed through the reusable contract checker.
- `python3 paper/check_artifact_manifest.py` passed.
- `python3 paper/check_evidence.py` passed.
- `paper/check.sh` passed at exactly 12 pages and 289,223 bytes.
- PDF SHA-256 remained
  `97150ebc18a4845a01c346979ee45e8d86905eadeef34177461d57653522b278`.
- `ctest --test-dir build/release --output-on-failure` passed 29/29 tests.

## 19. Unresolved or Unverified

- Genuine GitHub-hosted native x86-64 run, artifacts, verifier log, and attestation receipt.
- Native external PDEBench/customer, accelerator, NUMA, network, or distributed performance.
- Physical replicas, quorum, partitions, election, split-brain handling, and remote failover.
- External monotonic freshness and joint state/witness rollback detection.
- Production key entropy, generation, custody, KMS/HSM, and compromise recovery.
- Public immutable archive, persistent identifier, independent full-data rerun, and metadata.

## 20. Output Self-Check

- Overall score, criterion scores, writing score, and confidence are separated.
- Independent aggregate verification receives reproducibility credit without score inflation.
- Synthetic, local, hosted, attested, VM, physical-host, and independent states are distinct.
- Pre-attestation verification is not called successful attestation.
- No hosted run or external performance is implied.
- Negative results and machine-readable non-claims remain preserved.
- No acceptance probability or unsupported novelty claim is stated.
- The exact 12-page constraint is preserved without a layout hack.
