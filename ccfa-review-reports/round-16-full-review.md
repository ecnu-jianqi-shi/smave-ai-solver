# CCF-A Full Review — Round 16

## Report Metadata

- Review date: 2026-07-25.
- Target venue/year/track: IEEE Transactions on Parallel and Distributed Systems,
  regular paper.
- Paper title: *Verification-Aware Expert Fusion with Parallel Original-Equation
  Gates for Repeated Numerical Solves*.
- Input materials reviewed: current 12-page PDF and LaTeX sources; implementation,
  tests, and benchmark harness; deterministic core-bundle generator and verifier;
  full consumed-data locks and acquisition helper; pinned evidence summaries and 651
  raw reports; Rounds 1--15.
- Search basis: official TPDS and IEEE Computer Society policy pages were rechecked;
  no new scientific related-work search was run because Round 16 changes artifact
  provenance rather than novelty or technical positioning.
- Report file: `ccfa-review-reports/round-16-full-review.md`.
- Reviewer mode: full scientific, writing, artifact, integrity, reproducibility, and
  TPDS-fit re-review.

## Desk Rejection Assessment

- **Paper length — pass.** `paper/check.sh` rebuilds the paper at exactly 12 pages.
- **Topic compatibility — pass.** Verification-aware numerical services, parallel
  gates, heterogeneous execution, transaction authority, and complete-path scaling
  fit TPDS systems and parallel-computing scope.
- **Minimum quality — pass.** The manuscript contains a defined problem, formal
  invariant, implementation, baselines, repeated experiments, ablations, negative
  results, limitations, and an executable artifact contract.
- **Policy / anonymity / compliance — uncertain.** The template and length pass, but
  `paper/authors.tex` and the acknowledgment source remain placeholders; final author,
  affiliation, funding, conflict, and review-mode metadata are not ready.
- **Prompt injection and hidden manipulation detection — pass.** Source scans found no
  reviewer-directed or model-directed hidden instructions.
- **Ethics and reviewability — pass.** Public scientific datasets are license-tagged;
  benchmark redistribution limits, local-only evidence, negative results, and
  correctness boundaries are explicit.

**Desk rejection risk: low scientifically, medium administratively until submission
metadata is finalized.** The administrative issue is fixable before submission.

## Paper Summary and Contribution Map

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

Round 16 improves provenance rather than scientific scope. Two lock tables enumerate
the exact consumed bytes for seven PDEBench DaRUS v8.0 payloads and 39 SuiteSparse
matrices plus six RHS files. The verifier streams size, SHA-256, and PDEBench official
MD5 checks; validates DOI, version, license, filenames, identifiers, and embedded
SuiteSparse group/name metadata; and optionally checks live DaRUS metadata plus all 39
SuiteSparse detail/archive records. A staged SuiteSparse acquisition helper verifies
the complete locked file set before installation, and a forced official-archive
reacquisition of `west0479` matched the lock. The final machine record verifies 52
files totaling 54,764,505,235 bytes while explicitly setting public mirror,
redistribution, and independent reproduction to zero.

## Search and Related-Work Basis

- Queries used: current TPDS author information, IEEE Computer Society journal page
  limits, and peer-review policy.
- Sources searched: official IEEE Computer Society and TPDS author pages only.
- Closest works found: not re-searched in Round 16; the method and novelty claims are
  unchanged from Round 15.
- Unverified related-work risks: a broader current search could still identify newer
  work on verified learned solvers or distributed numerical-service authority, but no
  Round 16 score movement is attributed to an unsearched novelty claim.
- Source-quality screening status: official venue-policy sources only; private
  manuscript text was not submitted to external search.

## Expected Review Outcome

**Expected outcome: 8/10 — accept. Scholarly confidence: 5/5.**

- **Main accept signal:** the central verification-aware transaction and complete-cost
  argument are backed by broad, failure-preserving evidence and increasingly strong
  executable provenance.
- **Main reject signal:** authoritative performance, distributed authority, and
  reproduction remain author-operated and confined to one physical Apple M4 host.
- **Confidence:** all material Round 16 claims are locally inspectable against lock
  tables, evidence records, manuscript text, source, tests, and the extracted-tree
  verifier.

Round 16 closes the locally actionable ambiguity about which full benchmark bytes were
consumed and how they can be reacquired. It does not make those bytes immutable,
publicly mirrored, bundled, or independently rerun. The score therefore remains 8
rather than rising through artifact wording or packaging alone.

## Strengths and Weaknesses

### Top Strengths

1. The lock covers the exact consumed set rather than a broad dataset citation: seven
   PDEBench files, 39 SuiteSparse matrices, and six RHS files.
2. PDEBench entries retain official data-file identifiers, released version, DOI,
   official filename/directory, repository MD5, local SHA-256, size, and CC BY 4.0.
3. SuiteSparse entries retain group/name metadata, archive/detail records, exact size,
   local SHA-256, collection DOI, and CC BY 4.0; embedded Matrix Market metadata is
   checked against the lock.
4. The acquisition helper stages and verifies every expected member before moving any
   file into place, avoiding partial installation as successful acquisition.
5. The core archive includes lock contracts, acquisition scripts, and final evidence
   without claiming that the 54.8-GB payloads are redistributed.
6. Negative performance results, same-host boundaries, mutable-upstream dependence,
   and missing independent reproduction remain visible in the paper and ledger.

### Major Weaknesses

1. **External performance remains absent.** Evidence basis: all authoritative timing
   is still Apple M4 (`paper/sections/09_discussion_limitations.tex:34`). Reviewer
   deduction: generality and TPDS systems significance remain below a 5/5. Required
   fix: native Linux/x86-64 and discrete-accelerator complete-cost repetitions.
2. **Distributed authority remains a same-host functional probe.** Evidence basis:
   Docker namespaces, local unkeyed generations, and explicit exclusions in
   `paper/sections/06_experimental_methodology.tex:120` and
   `paper/sections/09_discussion_limitations.tex:37`. Reviewer deduction: no physical
   multi-host replication, partition, or failover evidence. Required fix: authenticated
   monotonic state and replicated physical-host tests.
3. **The data source is locked but not immutable.** Evidence basis:
   `build/release/data-lock/evidence.txt` sets `public_immutable_mirror=0`, and
   `paper/sections/09_discussion_limitations.tex:45` states mutable-upstream
   dependence. Reviewer deduction: acquisition provenance is strong locally but still
   exposed to upstream disappearance or replacement. Required fix: a licensed public
   immutable mirror or preservation deposit with persistent identifier.
4. **No independent operator exists.** Evidence basis: both bundle and data-lock
   contracts set independent reproduction to zero. Reviewer deduction: the artifact
   has not crossed the author trust boundary. Required fix: independent full workflow
   execution with retained machine record.
5. **Submission metadata remains incomplete.** Evidence basis: `paper/authors.tex:1`
   and `paper/sections/11_acknowledgments.tex:1`. Reviewer deduction: administrative
   readiness only. Required fix: finalize authorship, affiliations, funding, conflicts,
   acknowledgments, and review mode.

No fatal scientific concern is introduced by Round 16.

## Potentially Missing Related Work

- **Work:** recent verified learned-solver and safeguarded numerical-ML systems.
  **Status:** unverified in this round. **Why relevant:** could refine novelty
  positioning. **Overlap:** candidate acceptance under numerical checks. **Needed
  comparison:** distinguish per-candidate original-equation authority, transaction
  fallback, and complete verified cost.
- **Work:** distributed state-machine or replicated authority designs applied to
  numerical services. **Status:** unverified in this round. **Why relevant:** frames
  the boundary between the current local authority probe and production distributed
  commit. **Overlap:** durable replay and fail-closed recovery. **Needed comparison:**
  authentication, monotonicity, replication, partitions, and availability.

Neither item changes the Round 16 score because the manuscript does not claim a new
distributed consensus protocol or universal learned-solver novelty.

## Claim-Evidence Audit

| Claim | Where stated | Evidence provided | Strength | Reviewer deduction | Required fix |
| --- | --- | --- | --- | --- | --- |
| Mandatory original-equation gating preserves commit authority under explicit assumptions | `paper/sections/05_verification_aware_fusion.tex`; `paper/sections/03_problem_formulation.tex` | Proposition, transaction implementation, fault and fallback probes | Strong within assumptions | Does not verify arbitrary callbacks, hardware, or malicious coherent rollback | Formalized callback trust or authenticated replicated authority for a 5 |
| Seven PDE workloads show stable complete-runtime benefit | Abstract and `paper/sections/07_evaluation.tex` | 30 run-level pairs, fixed-seed bootstrap intervals, order controls, raw reports | Strong locally | One host and workload-derived subsystems limit generality | Native external repetitions |
| Parallel gates preserve sequential decisions and residuals | Abstract; scaling evaluation | Thread and process evidence with authority equality | Strong locally | Process launch/pipe costs and same-host execution are not distributed scaling | Native multi-socket or physical-host evaluation |
| Held-out operators beat a shared hybrid control | Abstract; evaluation and ablation | Two operator families, 64 held-out scenarios each, shared baseline, failure retention | Strong locally | Scenario count bounds observed-error risk but cannot prove zero future failure | External operator family or independent rerun |
| TCP authority recovers durable state and fails closed | Methodology and limitations | Crash phases, current/previous generations, corrupt/stale selection, exit 88 | Moderate-to-strong functional evidence | No authentication, replication, partitions, remote failover, or timing | Physical replicated authority campaign |
| All consumed benchmark bytes are provenance-locked | `paper/sections/06_experimental_methodology.tex:181`; `paper/CLAIM_EVIDENCE.md` | Two TSV locks, 52-file verifier, upstream metadata check, forced reacquisition | Strong local provenance | Upstream remains mutable; payloads are absent from core archive | Immutable mirror and independent full-data run |
| The core artifact reproduces the tested implementation | `paper/ARTIFACT_SNAPSHOT.md`; bundle contract | Deterministic archive, internal/sidecar hashes, clean extracted-tree build and tests | Strong author-operated reproduction | Not public, independent, or complete-data reproduction | Persistent public release plus external operator |

## Experiment, Benchmark, and Reproducibility Audit

- **Baselines:** workload-specific classical solvers, routing alternatives, hindsight
  oracle with caveat, and a common held-out operator control are present.
- **Ablations:** gate, correction, fallback, routing, process/thread behavior,
  transaction failures, order effects, and negative transfer are retained.
- **Datasets:** the exact 52 consumed files are now byte-locked; licenses and upstream
  identifiers are explicit.
- **Metrics:** complete-runtime speedup, paired medians, fixed-seed bootstrap intervals,
  paired win rates, residual/QoI checks, fallback, failure, and break-even are defined.
- **Statistical rigor:** run-level pairing and scenario-level error bounds are stated;
  the manuscript avoids treating per-solve timing repetitions as independent claims.
- **Robustness and failures:** no-common-success cases, device regressions, nonlinear
  regressions, cross-topology failure, and authority corruption paths are reported.
- **Implementation details:** C++20 SDK, typed transaction roles, C ABI, install tests,
  frozen ABI host, evidence generators, and acquisition contracts are available.
- **Artifact reproducibility:** deterministic core packaging and full-data byte locks
  are strong, but public archival, independent execution, and immutable data hosting
  remain absent.
- **Limitations:** performance portability, callback trust, dynamic-equation scope,
  mutable upstream data, and distributed authority exclusions are explicit.

## Writing and Presentation Assessment

| Dimension | Weight | Score (1--5) | Confidence | Evidence basis | Concrete repair |
| --- | ---: | ---: | ---: | --- | --- |
| Storyline and motivation | 12 | 5 | 5 | `paper/sections/01_introduction.tex:24` | Preserve the scientific question and transaction distinction |
| Contribution display | 12 | 5 | 5 | `paper/sections/01_introduction.tex:66` | Preserve the current four-part contribution map |
| Paragraph logic | 10 | 4 | 5 | Dense methodology and limitation paragraphs | Split only if page-neutral; no urgent rewrite |
| Claim-evidence alignment | 14 | 5 | 5 | `paper/CLAIM_EVIDENCE.md`; generated values | Keep all new claims ledger-backed |
| Method readability | 10 | 4 | 5 | Formal assumptions plus staged pipeline | A compact algorithm listing could help but is not page-neutral |
| Experiment narration | 10 | 4 | 5 | Evaluation links results to complete-cost questions | Keep local/external boundaries adjacent to results |
| Related-work positioning | 8 | 4 | 4 | Technical rather than chronological positioning | Recheck recent verified learned-solver work before submission |
| Terminology and notation | 8 | 5 | 5 | Stable gate, transaction, authority, and complete-cost terms | Preserve terminology |
| LaTeX and format discipline | 8 | 5 | 5 | Exact 12-page build; no forbidden log warnings | Replace metadata placeholders only |
| Reviewer-facing risk | 8 | 4 | 5 | Strong caveats, but artifact density is high | Keep provenance details concise and in artifact documentation |

**Weighted writing score: 4.54/5. Writing risk: low.** The artifact paragraph is
dense but defensible; no score of 3 or below requires a manuscript rewrite.

## Format and Venue Concerns

- The IEEEtran journal/compsoc template builds successfully at exactly 12 pages.
- No undefined citations/references, undefined controls, or overfull boxes are accepted
  by `paper/check.sh`.
- The author block and acknowledgments are placeholders and must be finalized before
  submission.
- A double-anonymous submission would require a deliberate blinded artifact and
  document scrub; the current repository should not be assumed anonymous.
- The venue guide referenced by the reviewer skill was unavailable locally, so current
  official TPDS/IEEE Computer Society pages were used for the policy check.

## Multi-Reviewer Panel

### Reviewer 1 — Parallel and Distributed Systems

- Likely score: 8/10; confidence 5/5.
- Main positive signal: transaction authority, parallel gates, process isolation, and
  fail-closed recovery make the work relevant beyond raw solver selection.
- Main negative signal: no replicated physical-host authority, partition experiment,
  remote failover, or network performance.
- Score-change condition: authenticated physical replication plus failover/partition
  evidence could raise significance and soundness.

### Reviewer 2 — Numerical Systems and SciML

- Likely score: 8/10; confidence 5/5.
- Main positive signal: original equations retain authority; complete cost includes
  correction, verification, transfer, and fallback; failures are not hidden.
- Main negative signal: authoritative performance remains on one physical system.
- Score-change condition: representative native external repetitions and an external
  operator family would strengthen generality.

### Reviewer 3 — Evidence and Ablation

- Likely score: 8/10; confidence 5/5.
- Main positive signal: repeated paired runs, order controls, shared baselines,
  scenario-level error bounds, and negative outcomes provide unusually broad evidence.
- Main negative signal: several evidence axes remain local or same-host and cannot
  establish deployment-scale behavior.
- Score-change condition: external hardware plus physical multi-host evidence.

### Reviewer 4 — Artifact and Reproducibility

- Likely score: 8/10; confidence 5/5.
- Main positive signal: deterministic core packaging is now complemented by exact
  license-aware locks for every consumed benchmark byte and a staged acquisition path.
- Main negative signal: no public immutable mirror, persistent identifier, independent
  operator, or independent full-data rerun.
- Score-change condition: publish a frozen release and data-preservation plan, then
  retain an external verifier record.

### Reviewer 5 — Novelty and Positioning

- Likely score: 8/10; confidence 4/5.
- Main positive signal: the contribution is framed as transaction and cost co-design,
  not as invention of the underlying numerical experts.
- Main negative signal: Round 16 does not update the related-work search and cannot
  strengthen novelty by provenance alone.
- Score-change condition: a current closest-work comparison may reduce uncertainty but
  should not raise the score without scientific evidence.

### Reviewer 6 — Writing, Ethics, and Limitations

- Likely score: 8/10; confidence 5/5.
- Main positive signal: the manuscript clearly preserves negative results, licenses,
  local-only boundaries, and excluded claims.
- Main negative signal: submission metadata is incomplete and artifact detail is dense.
- Score-change condition: finalize metadata and preserve the current non-inflated scope.

## Panel Synthesis and AC Meta-Review

- **Agreement:** all reviewers support acceptance at 8/10 and agree that Round 16
  materially strengthens local provenance without changing scientific reach.
- **Disagreement:** the artifact reviewer views the data lock as a substantial
  reproducibility improvement; the novelty reviewer correctly assigns no novelty gain.
- **Decisive positive axis:** verification-aware complete-cost co-design backed by
  broad, inspectable, failure-preserving evidence.
- **Decisive negative axis:** all authoritative performance and authority validation
  remain within one author-operated physical environment.
- **Unresolved evidence:** public preservation, independent rerun, native external
  performance, discrete accelerator, and physical replicated authority.
- **AC stance:** accept at 8/10. Do not promote to 9 for provenance engineering alone;
  require evidence that crosses host, operator, or hardware boundaries.

## Quantitative Scores

| Dimension | Score (1--5) | Confidence (1--5) | Evidence basis | Deduction / score-change condition |
|:---|:---:|:---:|:---|:---|
| Novelty | 4 | 5 | `paper/sections/05_verification_aware_fusion.tex`; `paper/sections/03_problem_formulation.tex` | The novelty is verification-aware transaction and complete-cost co-design, not data locking. A 5 needs broader externally validated authority or a stronger distinct mechanism. |
| Soundness | 4 | 5 | Proposition, fault probes, `tests/gate_network_authority_evidence.cpp`, limitations | Assumption-bound safety is credible, but authentication, replication, partitions, and physical failover remain absent. |
| Evidence | 4 | 5 | Pinned reports, `paper/CLAIM_EVIDENCE.md`, data-lock evidence, clean-tree evidence | Evidence is broad and inspectable; a 5 requires native external performance or independent physical-host reproduction. |
| Significance | 4 | 5 | Introduction contribution map; TPDS implications | Systems relevance is credible, but production distributed resource management and recovery availability are outside evidence. |
| Clarity | 5 | 5 | Abstract, contribution list, methodology contract, limitations | Claims and exclusions are explicit; no deduction. |
| Reproducibility | 4 | 5 | Deterministic core bundle; lock TSVs; acquisition/verifier scripts; machine records | Full local acquisition ambiguity is closed. A 5 requires public immutable preservation and independent execution. |
| Ethics / Limitations | 5 | 5 | Licenses, negative results, `paper/sections/09_discussion_limitations.tex` | Data and claim boundaries are explicit; no deduction. |

**Overall:** 8/10 | **Scholarly Confidence:** 5/5

**Recommendation:** accept

| Change | Condition | Likely affected dimensions | Expected movement |
| --- | --- | --- | --- |
| Raise score | Native x86-64 and discrete-accelerator complete-cost repetitions plus independent public-artifact execution | Evidence, significance, reproducibility | +1 overall is defensible |
| Raise score | Authenticated physical-host replication with failover and partition evidence | Soundness, significance, evidence | +1 overall may be defensible |
| Lower score | A locked file fails reacquisition/hash verification or the clean extracted-tree verifier fails | Evidence, reproducibility | -1 overall |
| Lower score | Same-host or emulated results are presented as native multi-host performance | Clarity, ethics, evidence | -1 overall or stronger |
| No quick change | Public immutable preservation, independent operator recruitment, and external hardware access | Reproducibility, evidence | Requires external state, not prose |

## Questions for Authors

1. Where will the core archive, lock tables, and license-compliant acquisition metadata
   be deposited under a persistent identifier?
2. Can an independent operator reacquire all 52 files and execute the full benchmark
   profile, not only the core fixture profile?
3. Which native Linux/x86-64 and discrete-accelerator systems will be used for the
   first external complete-cost replication?
4. Is authenticated monotonic authority or physical replication planned, and what
   failure/partition model will it cover?
5. Which review mode, author list, funding statement, conflict statement, and artifact
   anonymity policy will be used at submission?

## Concern-to-Action Table

| ID | Severity | Concern | Evidence basis | Affected criterion | Fix class | Required action | Owner | Score-change condition |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| R16-1 | Major | Native external performance absent | Apple M4 timing authority only | Evidence, significance | Experiment | Repeat representative paired workloads on native Linux/x86-64 and a discrete accelerator | External hardware owner | Main path from 8 to 9 |
| R16-2 | Major | No physical replicated authority | Same-host Docker TCP probe | Soundness, significance | Method/soundness + experiment | Add authenticated monotonic state, two physical hosts, failover, and partitions | Distributed-systems owner | Could support 9 |
| R16-3 | Major | No public immutable mirror or independent rerun | Data-lock and bundle zero fields | Reproducibility | Reproducibility | Publish a frozen release/data record and recruit an external operator | Artifact owner + external operator | Required for robust 9 |
| R16-4 | Moderate | Related-work recency not re-searched | Artifact-only Round 16 review | Novelty | Related work | Run a public-safe closest-work search before submission | Literature owner | Reduces uncertainty; no automatic score gain |
| R16-5 | Moderate | Submission metadata incomplete | Placeholder author and acknowledgment files | Readiness | Writing/compliance | Finalize authors, affiliations, funding, conflicts, acknowledgment, and review mode | Authors | Readiness only |
| R16-6 | Closed locally | Full consumed-data acquisition ambiguity | 52-file byte locks and live checks | Reproducibility | Reproducibility | Preserve lock hashes, acquisition scripts, evidence, and exclusion language | Artifact owner | Prevents regression; no score inflation |

## Score Revision Criteria

**Raising the score would require:** evidence crossing at least one decisive external
boundary: native external hardware, an independent public-artifact/full-data rerun, or
authenticated physical-host replication. The strongest case for 9 combines at least
two of these.

**Lowering the score would be triggered by:** a failed locked-file reacquisition, stale
or inconsistent manifest, failed clean-tree build/test/paper gate, hidden payload
dependency, unsupported redistribution, or wording that relabels same-host/emulated
evidence as external performance or physical multi-host replication.

**Concerns unlikely to change before submission:** physical multi-host authority,
native accelerator access, independent rerun, and public preservation require external
resources or operators rather than manuscript edits.

## Action Plan and CCFA Handoffs

1. **Priority P0 — public independent reproduction.** Publish a frozen core archive,
   lock contracts, and preservation record; recruit a separate operator for core and
   full-data execution. Owner: artifact owner plus external operator. Handoff required:
   external, not a writing handoff.
2. **Priority P0 — native external performance.** Run representative paired workloads
   on native Linux/x86-64 and a discrete accelerator. Owner: experiment/hardware owner.
   Handoff required: yes, external hardware.
3. **Priority P1 — authenticated physical authority.** Add monotonic authenticated
   state and replicated two-host failover/partition tests. Owner: distributed-systems
   owner. Handoff required: yes, new system/evidence work.
4. **Priority P1 — submission metadata.** Finalize author, funding, conflict,
   acknowledgment, review-mode, and artifact-anonymity choices. Owner: authors.
   Handoff required: no.
5. **Decision owner.** `ccf-paper-reviewer` should re-score only after external
   evidence or a material scientific change. Wording-only changes should not trigger a
   higher score.

## Checks Run

- Verified all 52 consumed files and 54,764,505,235 bytes against lock size and local
  SHA-256; verified official PDEBench MD5, DOI, DaRUS v8.0 metadata, filenames,
  identifiers, and CC BY 4.0.
- Verified 39 SuiteSparse matrix pages/archive records, embedded group/name metadata,
  six companion RHS files, collection DOI, and CC BY 4.0.
- Confirmed lock-file SHA-256 values
  `6b4ef8172bd0280e2c8b64afcca9ba56e3e036b2148e8b79c40b60a99d464514`
  and `948a8c3348f80c03abfa02a8cf1cfb784ee9ef12d1b93083b52bc4b486e39e03`.
- Reviewed the recorded forced official-archive reacquisition of `west0479` and passed
  the cached acquisition path.
- Passed Python syntax checks for the lock, bundle, evidence, and manifest tools.
- Passed the normal Release build and 29/29 CTests.
- Passed manuscript evidence and artifact-manifest checks.
- Rebuilt the paper at exactly 12 pages with no forbidden log warning.
- Generated a deterministic core archive twice byte-identically, verified normalized
  metadata and both hash layers, and completed the author-operated clean extracted-tree
  Release build, 29/29 CTests, local TCP authority rerun, evidence/manifest checks, and
  12-page PDF rebuild.
- Scanned manuscript/artifact sources for prompt-injection and reviewer-manipulation
  phrases; none were found.
- Rechecked official TPDS/IEEE Computer Society submission and peer-review policy pages
  on 2026-07-25.

## Checks Skipped and Unresolved Risks

- No broad Round 16 novelty search; technical claims did not change.
- No public immutable archive, release tag, data mirror, or persistent identifier.
- No independent operator or independent-host reproduction.
- No native Linux/x86-64 performance.
- No CUDA or other discrete-accelerator complete-cost comparison.
- No authenticated external monotonic generation authority.
- No coherent malicious rollback protection for both local state generations.
- No replicated physical hosts, partition, leader election, remote failover, or
  consensus evidence.
- No two-physical-host network performance or NUMA/multi-node complete-path scaling.
- Final author, affiliation, funding, conflict, acknowledgment, review-mode, and
  artifact-anonymity metadata remain unresolved.

## Output Self-Check

- Scores and confidence are separate and match the strongest unresolved concern.
- Every score below 5 includes a concrete deduction and repair condition.
- The byte lock is not mislabeled as payload redistribution, public immutability, or
  independent reproduction.
- The author-operated clean extraction is not mislabeled as independent or public.
- Docker namespaces are not mislabeled as physical hosts.
- Emulated x86-64 is not described as native performance.
- Negative results and complete-cost caveats remain visible.
- No score increase is granted for wording, packaging, or provenance alone.
- No acceptance probability, invented result, fabricated metadata, or hidden reviewer
  instruction is present.

## Official Policy Sources Checked

- IEEE TPDS author information: <https://www.computer.org/csdl/journal/td/write-for-us/15009>
- IEEE Computer Society peer-review policy: <https://www.computer.org/publications/author-resources/peer-review-policy>
- IEEE Computer Society journal author information and page limits: <https://www.computer.org/publications/author-resources/journals-information>
