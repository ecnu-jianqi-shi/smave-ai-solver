# CCF-A Full Review — Round 27

## 1. Report Metadata

- **Mode:** full scientific, writing, format, artifact, integrity, and reproducibility review.
- **Review date:** 2026-07-26.
- **Venue and assumptions:** IEEE Transactions on Parallel and Distributed Systems,
  regular paper, 2026 submission cycle, 12-page initial manuscript.
- **Paper title:** *Verification-Aware Expert Fusion with Parallel Original-Equation
  Gates for Repeated Numerical Solves*.
- **Materials reviewed:** current PDF and LaTeX sources; immutable Rounds 1--26; routing
  API and production implementation; routing unit tests; the independent four-stage
  exhaustive ordering evidence; generated manuscript values; claim ledger; artifact
  manifest and deterministic-bundle tooling; prior 29-test evidence; and the current
  selective post-change validation results.
- **Privacy boundary:** local unpublished manuscript and artifacts only. No private text
  was submitted to an external search service.
- **Search basis:** no new novelty search was required. Existing PhysicsCorrect and
  ANCHOR metadata had already been checked against their official DOI/arXiv records.
- **Report file:** `ccfa-review-reports/round-27-full-review.md`.

## 2. Desk Rejection Assessment

- **Paper length — pass.** The current PDF remains exactly 12 pages and 290,609 bytes.
- **Topic compatibility — pass.** Verification-aware numerical execution, complete-cost
  routing, parallel gates, and bounded authority evidence remain within TPDS scope.
- **Minimum quality — pass.** The theory-to-production routing path is now inspectable,
  and the broader benchmark, negative-result, fault, and artifact package remains intact.
- **Format and source integrity — pass.** `IEEEtran` journal/compsoc mode is unchanged;
  manuscript checks report no layout manipulation.
- **Policy/anonymity/compliance — administratively incomplete.** Author, affiliation,
  funding, conflict, acknowledgment, and final release metadata remain placeholders.
- **Prompt injection and hidden manipulation — pass locally.** No reviewer-directed
  instruction was found in the inspected manuscript or artifact sources.
- **Validation status — pass with a host-latency caveat.** Routing-specific tests,
  exhaustive ordering reproduction, all 29 CTests, network-authority reproduction,
  evidence/manifest checks, manuscript rebuilding, and clean extraction pass. One prior
  attempt incurred extreme macOS `syspolicyd`/`dyld` delay in FMI `dlopen`; sampling
  localized the delay outside the modified routing path, and the successful rerun
  completed all 29 tests in 85.91 seconds.

**Desk-rejection risk:** low scientifically; medium administratively until final
submission metadata is supplied. The observed policy-service latency is an operational
host caveat, not an unresolved correctness failure.

## 3. Paper Summary

The paper presents verification-aware expert fusion for repeated numerical solves.
Candidate generation, optional correction, mandatory original-equation verification,
and classical fallback are optimized under complete verified cost. Hybrid DAE IR,
role-restricted experts, immutable inputs, atomic publication, and independent gates
support a bounded commit-authority invariant. Experiments report complete paths,
thread/process gate scaling, external baselines, order sensitivity, negative device and
routing results, failure probes, and artifact reproduction.

The central contributions are:

1. verification-aware routing over candidate, transfer, correction, gate, rejection,
   and fallback cost;
2. an optimal ordering rule for a fixed eligible cascade under order-invariant stage
   statistics, without claiming globally optimal subset selection;
3. original-equation commit authority under explicit isolation and publication assumptions;
4. strict-equivalent parallel and process-isolated gate implementations;
5. complete-cost measurement and an inspectable, bounded reproduction artifact.

Round 27 repairs a central theory-to-implementation inconsistency. This is a scientific
soundness correction, not merely artifact polish.

## 4. Round 27 Scientific Change

The manuscript has long stated the reach-weighted complete-cost objective and the
fixed-cascade exchange rule, but the previous production router sorted stages
lexicographically by risk and then single-stage cost. That implementation did not realize
the stated complete-cost ordering theorem. Round 27 closes the mismatch:

1. `cascade_ordering_index(step)` validates stage cost and acceptance probability and
   returns `estimated_cost_us / pass_probability`.
2. The production stage cost contains setup, solve, correction, expected rejection cost,
   and the calibrated risk/OOD penalty before division by predicted acceptance probability.
3. `order_cascade_steps()` uses the complete-cost index as the primary key; risk and cost
   are tie-breakers only, so risk is no longer a lexicographic override of the objective.
4. `RuntimeRouter::route()` calls this shared ordering function before separating
   contract-required direct fallbacks and retaining the mandatory terminal fallback.
5. `expected_cascade_cost()` explicitly includes reach probability and terminal fallback
   cost and rejects invalid stage probabilities or terminal costs.
6. Routing unit tests distinguish a cheap/low-acceptance stage from a more expensive/
   high-acceptance stage and verify that sorting lowers expected complete cost.
7. A standalone evidence executable enumerates all 24 permutations of a four-stage
   contract with `terminal_cost_us=10`.
8. The selected order `c,b,d,a` has expected cost `5.9524999999999997`, exactly equal to
   the exhaustive minimum in the machine-readable evidence.
9. The manuscript states the theorem only for a fixed eligible cascade with order-
   invariant statistics and explicitly denies globally optimal top-$k$ subset selection.
10. The claim ledger and artifact checks bind this bounded result to
    `build/release/cascade-ordering/evidence.txt`.

This closes the prior major soundness concern. It does not validate probability
calibration, stage-statistic stationarity, global subset choice, or external deployment.

## 5. Likely Stance and Calibrated Score

- **Likely stance:** accept.
- **Overall score:** **8/10**.
- **Scholarly confidence:** **5/5**.
- **Main accept axis:** the central complete-cost claim now aligns across derivation,
  production routing, focused unit tests, independent exhaustive evidence, and bounded
  manuscript wording.
- **Main reject axis:** no genuine native external performance, physical replicated
  authority, external freshness, production custody, public immutable preservation, or
  independent full-data rerun.

The soundness dimension rises because a real implementation mismatch is closed. The
overall score does not rise to 9 because this repair makes an existing core claim credible;
it does not add the decisive external evidence axis required for a stronger TPDS stance.

## 6. Quantitative Scorecard

| Dimension | Score | Confidence | Evidence basis | Deduction / score-change condition |
| --- | ---: | ---: | --- | --- |
| Novelty | 4/5 | 4/5 | Verification-aware complete-cost fusion and bounded commit authority remain meaningful | Requires stronger external systems positioning and evidence, not another local mechanism |
| Soundness | 5/5 | 5/5 | Exchange derivation, production `cost/pass` ordering, explicit terminal cost, invalid-input rejection, focused tests, and exhaustive 24-permutation agreement now align | Preserve the assumptions and rerun the full suite on a healthy host |
| Evidence | 4/5 | 5/5 | Broad paired evidence, negatives, raw samples, fault probes, and exact ordering evidence | No provider-controlled hosted result or physical fault campaign |
| Significance | 4/5 | 4/5 | Repeated numerical solving and safe expert composition are relevant to TPDS readers | No customer-scale, native external, or physical distributed deployment |
| Clarity | 4.75/5 | 5/5 | Fixed-cascade theorem, implementation boundary, terminal fallback, and non-optimal-subset disclaimer are explicit | The 12-page manuscript remains dense |
| Reproducibility | 4/5 | 5/5 | Shared production helper, filtered tests, standalone exhaustive target, manifest, data locks, deterministic bundle, and successful current clean extraction | No public immutable deposit or independent rerun |
| Ethics / Limitations | 5/5 | 5/5 | Local, synthetic, hosted, VM, physical, attested, public, and independent claims remain separated | Preserve these boundaries after any external run |

**Overall:** **8/10** | **Scholarly Confidence:** **5/5**

**Recommendation:** accept.

**Verdict:** Round 27 materially improves technical soundness but does not supply the
external evidence needed for score movement beyond 8.

No criterion is 3 or below; no fatal concern is averaged away.

## 7. Major Strengths

1. **The central theorem now reaches production code.** The runtime uses the same
   complete-cost index that the paper derives.
2. **The objective includes rejection and fallback consequences.** Expected stage cost
   and terminal cost are represented rather than comparing raw kernel latency.
3. **Risk is integrated rather than lexicographically dominant.** The routing surrogate
   places the risk/OOD penalty inside stage cost before the acceptance-probability ratio.
4. **The evidence is independently checkable.** A small standalone executable and CMake
   verifier reproduce an exhaustive optimum without relying on the production test harness.
5. **The claim boundary is appropriately narrow.** The paper does not convert a fixed-
   cascade ordering theorem into a claim of globally optimal top-$k$ selection.
6. **Safety invariants remain intact.** Direct and terminal fallbacks stay contractually
   ordered after speculative stages.
7. **Generated numbers remain synchronized.** The calibrated-router speedup and interval
   were regenerated rather than manually copied after the routing change.

## 8. Major and Moderate Concerns

### Major Concerns

1. **No native external performance was measured.** All authoritative performance
   evidence remains tied to one Apple M4 host; the GitHub workflow has not been run.
2. **No physical distributed authority exists.** Containers and provider VMs do not
   establish independent failure domains, partitions, quorum, election, split-brain
   resistance, or remote failover.
3. **Freshness is not externally anchored.** State and witness remain jointly rollbackable.
4. **Public independent reproduction is absent.** There is no committed public snapshot,
   persistent identifier, immutable deposit, or independent full-data rerun.
5. **Operational key custody is absent.** Fixture controls do not establish entropy,
   generation, provisioning, KMS/HSM custody, compromise recovery, or production rotation.

### Moderate Concerns

1. **The host showed transient policy-service latency.** One clean-tree attempt spent
   hundreds of seconds in FMI `dlopen` while `syspolicyd` was active. Sampling showed a
   `dyld fcntl` wait, and the complete rerun passed 29/29 in 85.91 seconds. This is an
   operational reproducibility caveat, not a correctness failure.
2. **The ordering assumptions are empirical obligations.** Acceptance probabilities and
   conditional stage costs may change after earlier stages alter state or workload context.
3. **Top-$k$ selection remains heuristic.** Ordering the chosen set optimally under the
   stated assumptions does not prove that the chosen subset is globally optimal.
4. **External workload breadth remains limited.** No native external PDEBench, customer,
   accelerator, NUMA, networked, or distributed performance result exists.
5. **Submission metadata remains unfinished.** Required author and disclosure fields are
   still placeholders.

## 9. Claim-Evidence Audit

| Claim | Where stated | Evidence | Strength | Reviewer judgment |
| --- | --- | --- | --- | --- |
| The router optimizes reach-weighted complete cost rather than raw latency | Abstract; problem formulation; system design | `src/routing.cpp`, routing tests | Strong | Closed after Round 27 production alignment |
| Sorting a fixed eligible cascade by cost per acceptance is optimal under order-invariant statistics | Problem formulation | Adjacent-exchange derivation; exhaustive 24-permutation evidence | Strong | Correctly bounded; no global subset claim |
| Terminal fallback cost is part of the complete path | Problem formulation; runtime invariant | `expected_cascade_cost`; fallback-preservation tests | Strong | Implementation and wording align |
| Calibrated routing is `2.273× [2.243, 2.306]` faster than a fixed expert | Evaluation; generated values; claim ledger | `phase4/family-router-evaluation.txt` | Adequate | Evaluated family only; not a universal gain |
| Original-equation gates bound caller-visible commit authority | Formal proposition; design; fault evaluation | Gate equivalence and local authority fixtures | Adequate | Valid under explicit assumptions, not physical distributed authority |
| The artifact is independently reproducible | Limitations and artifact documents | Deterministic author-operated bundle only | Weak if generalized | Manuscript appropriately avoids the generalized claim |
| The workflow supplies external performance | Artifact documents | Local dry-run and synthetic contract only | Absent | Correctly denied; no hosted run exists |

No central current manuscript claim is unsupported. External deployment and independence
claims remain intentionally absent rather than rhetorically strengthened.

## 10. Experiment, Benchmark, and Reproducibility Audit

- **Focused production-path test — pass.** `SMAVE_TEST_FILTER=cascade-ordering` exercises
  the routing bundle and ordering assertions through the C++ test binary.
- **Focused calibrated-router test — pass.** `SMAVE_TEST_FILTER=competition-router`
  exercises the regenerated routing evaluation path.
- **Independent exhaustive evidence — pass.** The CMake target rebuilds the evidence and
  verifies four stages, 24 permutations, terminal cost 10, and exact optimum equality.
- **Generated value synchronization — pass.** The claim ledger, README, LaTeX macros,
  and operator speedup data use the regenerated `2.273166×` routing result and interval.
- **Paper evidence check — pass.** It verifies the cascade fields, equality tolerance,
  claim-ledger wording, generated values, and the broader pinned evidence package.
- **Manifest contract — pass.** Round 27 source, routing tests, evidence, and
  documentation are captured by the regenerated manifest.
- **Full local Release suite — pass with observed latency variance.** The successful
  clean-tree run passed 29/29 in 85.91 seconds. An earlier attempt reached the same 29/29
  result only after 477.47 seconds because several `dlopen` calls waited on macOS policy
  services.
- **Clean extraction — pass.** The verifier rebuilt Release, passed all 29 CTests, reran
  exhaustive cascade ordering and network authority, verified the local-only native-
  performance package and synthetic campaign contract, checked evidence/manifest, and
  rebuilt the 12-page PDF.

The focused evidence materially supports the changed routing code, and the successful
clean-tree run closes the current full-suite validation gate. The transient macOS policy-
service delay remains recorded rather than hidden.

## 11. Writing and Presentation Review

### Writing Scorecard

| Dimension | Score | Confidence | Evidence basis | Remaining issue |
| --- | ---: | ---: | --- | --- |
| Contribution visibility | 5/5 | 5/5 | Abstract now names fixed-cascade ordering and its assumptions | Dense contribution stack |
| Claim-evidence alignment | 5/5 | 5/5 | Method, code, evidence, and ledger use the same bounded claim | Preserve after future router changes |
| Section flow | 4.75/5 | 5/5 | Objective leads to exchange rule, implementation, then ablation evidence | Limited space compresses transitions |
| Terminology | 5/5 | 5/5 | Complete cost, stage cost, acceptance probability, direct fallback, and terminal fallback are distinct | None decision-relevant |
| Limitation visibility | 5/5 | 5/5 | Fixed cascade versus subset selection is explicit in method, design, and ablation | External limits remain numerous |
| Figure/table narration | 4.75/5 | 4/5 | Main numerical claims are linked to generated artifacts | No dedicated ordering figure, appropriately avoided for space |

### Writing Findings

1. The abstract no longer overstates routing optimality: it says fixed eligible cascades
   and order-invariant statistics.
2. The problem formulation gives a compact adjacent-exchange argument and states exactly
   what it does not prove.
3. The design section maps risk/OOD penalties into stage cost and explains fallback order.
4. The ablation section supplies one sentence of exhaustive evidence without displacing
   more consequential systems results.
5. The manuscript remains information-dense, but the repaired claim is recoverable after
   one careful pass.

## 12. Format and LaTeX Audit

- `IEEEtran` journal/compsoc mode remains active.
- The PDF remains exactly 12 pages.
- Current size is 290,609 bytes.
- Current SHA-256 is
  `664f75e189c5271f902e958d674a4c0ef1bda0265866ecfbe203c35becc77c39`.
- No evidence of margin, font, spacing, or float manipulation was found.
- Author, affiliation, funding, conflict, acknowledgment, and release metadata remain the
  only desk-facing placeholders.

## 13. Multi-Reviewer Panel

### Reviewer 1 — Method and Soundness

- **Lens:** derivation, assumptions, implementation alignment.
- **Score tendency:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** the complete-cost exchange rule now drives production routing.
- **Main negative signal:** order-invariant stage statistics are assumptions, not measured
  universally.
- **Evidence basis:** formulation, routing implementation, unit tests, exhaustive evidence.
- **Score-change condition:** retain 9 tendency if future routing changes remain bound to
  the same helper and evidence target.

### Reviewer 2 — Distributed Systems

- **Lens:** authority, failure domains, freshness, failover.
- **Score tendency:** 7/10.
- **Confidence:** 5/5.
- **Main positive signal:** claim boundaries for same-host authority remain unusually explicit.
- **Main negative signal:** no physical replicas, partitions, quorum, or external freshness.
- **Evidence basis:** authority artifacts and limitations.
- **Score-change condition:** physical independent-host partition/failover evidence could
  move this reviewer toward 9.

### Reviewer 3 — Evidence and Experiments

- **Lens:** baselines, statistics, completeness, external validity.
- **Score tendency:** 8/10.
- **Confidence:** 5/5.
- **Main positive signal:** the changed routing claim has a focused production test and an
  exact independent exhaustive check.
- **Main negative signal:** no provider-controlled native external result exists.
- **Evidence basis:** paired reports, negative results, routing evidence, dry-run non-claims.
- **Score-change condition:** preserve a genuine hosted campaign with raw artifacts and
  verifier output.

### Reviewer 4 — Novelty and Positioning

- **Lens:** contribution distinctness and related-work boundary.
- **Score tendency:** 8/10.
- **Confidence:** 4/5.
- **Main positive signal:** verification-aware complete-cost composition is coherent and
  better defended after the implementation repair.
- **Main negative signal:** the ordering rule itself is classical exchange reasoning and
  should not carry the novelty claim alone.
- **Evidence basis:** abstract, formulation, related work, contribution list.
- **Score-change condition:** external systems evidence should demonstrate why the full
  composition matters beyond the individual mechanisms.

### Reviewer 5 — Writing and Clarity

- **Lens:** argument recovery, terminology, claim calibration.
- **Score tendency:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** theorem, implementation boundary, evidence, and disclaimer now
  tell one consistent story.
- **Main negative signal:** the exact 12-page format remains dense.
- **Evidence basis:** full manuscript and claim ledger.
- **Score-change condition:** no further compression unless required by venue formatting.

### Reviewer 6 — Artifact, Security, and Reproducibility

- **Lens:** tamper resistance, deterministic build, independent audit.
- **Score tendency:** 8/10.
- **Confidence:** 5/5.
- **Main positive signal:** the standalone ordering evidence is included in manifest and
  clean-extraction contracts.
- **Main negative signal:** no public immutable archive or independent rerun exists;
  macOS policy-service latency also varied sharply across clean-tree attempts.
- **Evidence basis:** bundle scripts, manifest inputs, evidence checker, current host status.
- **Score-change condition:** healthy-host clean extraction plus public immutable deposit
  and independent rerun.

### Reviewer 7 — Domain Application

- **Lens:** numerical relevance and workload realism.
- **Score tendency:** 8/10.
- **Confidence:** 4/5.
- **Main positive signal:** complete-cost routing matches the realities of correction,
  verification, rejection, and fallback.
- **Main negative signal:** external payload and hardware breadth remain limited.
- **Evidence basis:** PDEBench-derived, SuiteSparse, operator, and routing evaluations.
- **Score-change condition:** native external full-payload measurements.

### Reviewer 8 — Novice Advocate

- **Lens:** whether the core idea and limits are understandable without hidden context.
- **Score tendency:** 8/10.
- **Confidence:** 4/5.
- **Main positive signal:** cost divided by acceptance probability is now explained and
  backed by a small exhaustive example.
- **Main negative signal:** the surrounding authority and artifact machinery is complex.
- **Evidence basis:** abstract, formulation, design, ablation, limitations.
- **Score-change condition:** preserve the current concise ordering explanation.

### Panel Synthesis

- **Agreement:** Round 27 closes a real soundness problem and makes the central routing
  claim credible.
- **Disagreement:** method and writing reviewers would score near 9; distributed-systems
  and external-evidence reviewers remain at 7--8.
- **Decisive accept axis:** derivation-to-production alignment plus broad bounded evidence.
- **Decisive reject axis:** absent external systems evidence and independent preservation.
- **Unresolved evidence:** genuine hosted run, physical authority, external freshness,
  production custody, public archive, and independent rerun.
- **Final calibrated stance:** accept, 8/10, confidence 5/5.

## 14. Concern-to-Action Table

| Concern | Severity | Evidence basis | Required action | Score impact condition |
| --- | --- | --- | --- | --- |
| Previous theory/implementation routing mismatch | Closed major | Old risk-first sort versus manuscript complete-cost claim | Keep production ordering bound to `cascade_ordering_index` and exhaustive evidence | Reopening lowers soundness and overall score |
| macOS policy-service latency variance | Low operational | Sampled FMI `dlopen -> dyld fcntl`; successful rerun passed 29/29 | Preserve the successful clean-tree record and prefer a healthy second host | No score movement unless it prevents reproduction |
| No native external performance | Major | Hosted workflow unexecuted; local flags zero | Run from immutable commit and preserve all raw/provider outputs | One decisive axis toward 9 |
| No physical authority/freshness | Major | Same-host containers/files only | Use independent hosts, partitions, failover, and external monotonic anchor | Alternative decisive axis toward 9 |
| No public independent reproduction | Major | Author-operated local archive only | Deposit immutable artifact and obtain independent full-data rerun | Alternative decisive axis toward 9 |
| Placeholder metadata | Moderate | Author and disclosure files | Supply final submission metadata | Removes administrative risk, not scientific score ceiling |

## 15. AC / Meta-Review

The most important Round 27 finding is not a new benchmark number. It is the discovery
that the production router had not implemented the paper's stated complete-cost ordering
rule. That would have justified a material soundness deduction if left unresolved because
the abstract and formulation present the rule as part of the system contribution.

The revision addresses the issue at the correct level. It introduces a shared validated
ordering index, uses it in `RuntimeRouter`, keeps risk inside stage cost rather than as a
lexicographic override, represents terminal fallback in expected-cost evaluation, adds
focused invalid-input and cost-reduction tests, and supplies an independent exhaustive
four-stage check. The paper also limits the claim to fixed eligible cascades with order-
invariant statistics and explicitly declines globally optimal subset selection.

This is sufficient to raise technical soundness to 5/5. It is not sufficient to raise the
overall score to 9. The remaining ceiling is external validity and independent systems
evidence, not another local correctness mechanism. The current clean extraction passes
29/29 and all downstream artifact checks; the unusually slow first attempt remains a
recorded macOS policy-service latency caveat.

**Meta-review recommendation:** accept, 8/10, confidence 5/5.

## 16. Score Revision Criteria

### Raise Toward 9

- Execute the native-performance workflow from an immutable review-accessible commit.
- Preserve the run URL, three replicate directories, exact aggregate, verifier log,
  uploaded artifact, and provider attestation receipt.
- Retain all slowdowns and intervals without a positive-speedup gate.
- Prefer representative complete payloads in addition to self-contained fixtures.
- Alternatively close another major external axis: physical authority and external
  freshness, or public immutable preservation with independent full-data reproduction.

One genuine major external axis could support 9. A 10 requires several such axes to
converge without reopening soundness, metadata, or claim-calibration weaknesses.

### Lower the Score

- Reintroducing risk-first or raw-cost-first production ordering while retaining the
  complete-cost manuscript claim.
- Generalizing the fixed-cascade result to globally optimal top-$k$ subset selection.
- Hiding the sampled policy-service delay or presenting the anomalous first-run latency as
  normal benchmark behavior.
- Calling the synthetic contract check a hosted experiment or provider attestation.
- Treating VM jobs or Docker namespaces as independent physical hosts.
- Generalizing self-contained timings to full payloads, customers, accelerators, NUMA,
  networked systems, or distributed execution.
- Failing routing, evidence, manifest, page, archive, or clean-tree gates.

## 17. Recommended Next CCFA Owner

1. **Immediate — artifact/integrity owner:** freeze the final Round 27 manifest, rebuild
   two deterministic archives, and preserve the successful clean-tree evidence.
2. **Optional portability — validation owner:** repeat the frozen archive on a healthy
   second host to distinguish macOS policy latency from ordinary runtime variance.
3. **Decisive external — repository/benchmark owner:** establish an immutable commit and
   remote, dispatch the real hosted campaign, and preserve provider outputs unchanged.
4. **Submission — authors:** finalize author, funding, conflict, acknowledgment, and
   public-archive metadata.
5. **Other external owners:** pursue physical authority, external freshness, operational
   custody, representative workloads, and independent reproduction.

## 18. Checks Run

- Production routing code uses `estimated_cost_us / pass_probability` and calls the shared
  ordering helper before fallback separation.
- The stage-cost surrogate includes setup, solve, correction, expected rejection cost,
  and calibrated risk/OOD penalty.
- `SMAVE_TEST_FILTER=cascade-ordering ./build/release/smave_tests` passed.
- `SMAVE_TEST_FILTER=competition-router ./build/release/smave_tests` passed.
- `cmake --build build/release --target reproduce-cascade-ordering` passed and emitted the
  exact four-stage/24-permutation optimum.
- Generated calibrated-router evidence reports `2.273166×`, 95% interval
  `[2.2425665, 2.3062146]`, and `97.96875%` run win rate.
- `python3 paper/check_evidence.py` passed before the Round 27 documentation freeze.
- The synthetic native-performance campaign contract passed with `external_evidence=0`.
- `paper/check.sh` passed at exactly 12 pages and 290,609 bytes.
- PDF SHA-256 is
  `664f75e189c5271f902e958d674a4c0ef1bda0265866ecfbe203c35becc77c39`.
- System sampling during an anomalously slow attempt located the wait at FMI
  `smoke_fmi2_co_simulation -> dlopen -> dyld fcntl`, while `syspolicyd` used sustained
  CPU. That attempt still reached 29/29 in 477.47 seconds before its post-test ordering
  target was manually interrupted.
- A complete rerun then passed 29/29 in 85.91 seconds and completed exhaustive ordering,
  network authority, local-only native-performance verification, synthetic campaign
  checks, paper evidence/manifest checks, and the 12-page PDF rebuild.
- Clean-tree evidence is stored at
  `build/core-repro-bundle/clean-tree-evidence.txt`.

## 19. Unresolved or Unverified

- Genuine GitHub-hosted native x86-64 run, artifacts, verifier log, and attestation receipt.
- Native external PDEBench/customer, accelerator, NUMA, network, or distributed performance.
- Physical replicas, quorum, partitions, election, split-brain handling, and remote failover.
- External monotonic freshness and joint state/witness rollback detection.
- Production key entropy, generation, custody, KMS/HSM, and compromise recovery.
- Public immutable archive, persistent identifier, independent full-data rerun, and metadata.

## 20. Output Self-Check

- Overall score, criterion scores, writing score, and confidence are separated.
- The prior central soundness mismatch is named and marked closed rather than hidden.
- Fixed-cascade ordering is not generalized to global subset selection.
- Focused evidence and the successful current full clean-tree pass are separately credited.
- Synthetic, local, hosted, attested, VM, physical-host, public, and independent states
  remain distinct.
- No hosted run, external performance, successful attestation, or independent reproduction
  is implied.
- No acceptance probability or unsupported novelty claim is stated.
- The exact 12-page constraint is preserved without a layout hack.
