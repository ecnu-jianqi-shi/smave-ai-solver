# CCF-A Full Review — Round 28

## 1. Report Metadata

- **Mode:** full scientific, writing, format, artifact, integrity, and reproducibility review.
- **Review date:** 2026-07-26.
- **Venue and assumptions:** IEEE Transactions on Parallel and Distributed Systems,
  regular paper, 2026 submission cycle.
- **Paper title:** *Complete-Cost Expert Fusion for Verified Repeated Numerical Solves*.
- **Materials reviewed:** current LaTeX manuscript, solver implementation, routing and
  gate tests, generated evidence, claim ledger, artifact scripts, core bundle contract,
  native-performance contract, and prior CCF-A review history.
- **Privacy boundary:** local unpublished manuscript and repository artifacts only; no
  private manuscript text was sent to an external search service.
- **Review purpose:** scope-corrected assessment after removing distributed/high-availability
  material from the manuscript claim surface.

## 2. Scope Correction

The project is a solver paper. POSIX process isolation, TCP state mirroring, HMAC/witness
protocols, key rotation, replication, failover, consensus, and distributed high
availability are not contributions, score gates, or required experiments. Legacy code may
remain for engineering regression, but it is excluded from the manuscript, claim ledger,
artifact evidence, and review criteria.

This correction closes the previous review's main framing error. The paper should now be
judged on complete-cost solver fusion, numerical acceptance, routing, correction, parallel
execution, heterogeneous placement, and external numerical evidence.

## 3. Paper Summary

The paper studies repeated numerical solves in which no single classical or learned
expert is uniformly best. It proposes a typed candidate--corrector--original-equation
gate--fallback pipeline and a router that minimizes reach-weighted complete runtime,
including correction, verification, rejection, and fallback costs. For a fixed eligible
cascade with order-invariant statistics, it derives the cost-per-acceptance ordering
rule. A verified-result publication invariant ensures that a successful return passes the
family-specific original-equation gate; this is a numerical control-flow guarantee, not a
security or distributed-systems theorem.

The evidence covers PDEBench-derived repeated solves, SuiteSparse/PETSc/OpenModelica
comparisons, held-out linear and nonlinear operators, complete-cost routing, exhaustive
four-stage ordering, gate and complete-path scaling, batch amortization, order
sensitivity, and negative transfer/device results.

## 4. Likely Stance and Calibrated Score

- **Overall score:** **8/10, accept**.
- **Confidence:** **5/5**.
- **Rationale:** the central solver composition is coherent, the prior routing
  theory--implementation mismatch is repaired, and the evidence is unusually explicit
  about complete cost and negative results. The remaining ceiling is empirical breadth and
  external validation, not missing distributed infrastructure.

## 5. Quantitative Scorecard

| Dimension | Score | Confidence | Evidence basis | Deduction / repair condition |
| --- | ---: | ---: | --- | --- |
| Novelty | 4/5 | 4/5 | Complete-cost fusion of learned candidates, correctors, original-equation gates, fallback, and routing; related-work positioning | The ordering rule is classical exchange reasoning and the components are individually familiar. A stronger external hybrid baseline and clearer mechanism-level attribution would support 5/5. |
| Soundness | 5/5 | 5/5 | `prop:verified-publication`, family-specific gates, fallback traces, exact 24-permutation cascade check, and implementation alignment | No current major soundness defect is visible. The guarantee remains bounded by the supplied equation/gate and finite precision, as stated. |
| Significance | 4/5 | 4/5 | Seven repeated workloads, held-out operators, complete-cost routing, and solver-level parallel/batch evidence | One Apple M4 host and limited external baseline breadth constrain generality. Provider-controlled native results plus broader workloads could support 5/5. |
| Experimental evidence | 4/5 | 5/5 | Paired 30-run timing, bootstrap intervals, order counterbalance, shared hybrid control, and retained negative results | Distribution-shift calibration, stronger external hybrid baselines, and more hardware/workload families remain open. |
| Clarity | 5/5 | 4/5 | Scope-corrected terminology, candidate--corrector--gate--fallback narrative, and explicit claim boundaries | Dense 12-page presentation still requires careful figure and table reading, but no major ambiguity remains. |
| Reproducibility | 4/5 | 4/5 | Deterministic archive, clean extraction, machine-generated evidence, manifest, and solver-focused bundle | No public immutable archive or independent rerun is currently available; deposit and independent reproduction would support 5/5. |
| External validity | 3/5 | 5/5 | One Apple M4 host, limited provider workflow status, and workload-specific gains/negative results | The deduction is due to hardware and workload breadth, not a missing distributed system. Native external execution, stronger baselines, and distribution-shift studies are the repair condition. |

Because the external-validity criterion is 3/5, the concrete deduction is limited
generalization evidence. It does not lower soundness: the paper correctly labels the
current results as workload- and platform-bounded. Completing the repair would move the
criterion toward 4--5/5 and could move the overall score from 8 toward 9.

## 6. Top Strengths

1. **The scientific scope is now coherent.** The paper asks a solver question and uses
   numerical evidence to answer it; irrelevant service-security machinery no longer
   competes with the contribution.
2. **Complete cost is treated as the optimization object.** Candidate generation,
   transfer, correction, gate, rejection, and fallback are not hidden behind raw model
   latency.
3. **Theory and implementation agree.** The fixed-cascade exchange rule is exercised by
   the production ordering path and checked against all 24 permutations of a four-stage
   synthetic contract.
4. **Acceptance semantics are simple and inspectable.** A candidate is not returned merely
   because a router selected it; successful returns pass the original-equation gate and
   failed paths fall through to another expert or the classical solver.
5. **The evaluation reports limits rather than only wins.** Gate-only scaling is separated
   from complete-path scaling, small gains and regressions are retained, and the order
   sensitivity study prevents a single timing order from carrying the result.

## 7. Major Concerns

### M1 — Native solver performance remains unverified

- **Severity:** major but non-fatal.
- **Evidence basis:** current performance evidence is primarily one Apple M4 host; the
  hosted native workflow is a contract and local dry-run rather than a completed provider
  campaign.
- **Affected criterion:** significance, external validity, reproducibility.
- **Action:** execute a provider-controlled native x86-64 campaign and preserve raw
  artifacts, runner metadata, and verifier output.
- **Score-change condition:** successful solver-relevant hosted evidence with complete-cost
  accounting can move overall confidence toward 9; it is not required to establish the
  current numerical soundness claim.

### M2 — External hybrid-solver baseline is still limited

- **Severity:** major but non-fatal.
- **Evidence basis:** the shared learned-candidate/Jacobi-correction/fallback control is
  transparent and useful, but it is not an external published hybrid implementation.
- **Affected criterion:** novelty, significance, experimental evidence.
- **Action:** add at least one stronger external learned-solver, neural-preconditioner,
  or algorithm-selection baseline under the same complete-cost and accuracy contract.
- **Score-change condition:** a controlled win or an informative negative result would
  materially strengthen the claim that the composition, rather than one local kernel,
  drives the result.

### M3 — Distribution-shift and calibration breadth

- **Severity:** moderate.
- **Evidence basis:** routing and operator results are family-specific; the fixed-cascade
  theorem assumes order-invariant stage statistics, and current calibration evidence does
  not span enough shifts in conditioning, scale, topology, or hardware.
- **Affected criterion:** significance, external validity.
- **Action:** report calibration error and complete-cost regret under held-out family,
  conditioning, size, topology, and hardware shifts.
- **Score-change condition:** robust calibration or clearly bounded failure regions can
  move external validity upward; unreported shift failures would reduce confidence.

### M4 — Mechanism-level attribution can be sharper

- **Severity:** moderate.
- **Evidence basis:** the paper has shared controls and negative results, but candidate,
  correction, gate, routing, batching, and placement contributions are not fully
  decomposed across all major workload families.
- **Affected criterion:** novelty, clarity, significance.
- **Action:** add a complete-cost bottleneck decomposition and correction--gate ablation
  that reports wall-clock contribution, acceptance rate, fallback rate, and break-even.
- **Score-change condition:** a clear attribution table would reduce the risk that the
  headline gains are over-attributed to the full method.

### M5 — Public independent reproduction is incomplete

- **Severity:** moderate.
- **Evidence basis:** the local deterministic archive and clean extraction are inspectable,
  but large payloads are excluded and no independent organization has rerun the full
  evidence.
- **Affected criterion:** reproducibility.
- **Action:** publish an immutable source/evidence archive, document payload acquisition,
  and obtain at least one independent solver-focused rerun.
- **Score-change condition:** independent reproduction can move reproducibility from 4/5
  to 5/5; the current bundle remains useful without it.

## 8. Writing and Presentation

- The revised title and abstract foreground complete-cost solver fusion rather than
  infrastructure.
- The proposition should remain labeled as verified-result publication or numerical
  acceptance; avoid reintroducing “authority,” “commit,” or transaction language unless it
  describes ordinary solver control flow.
- Keep the distinction between gate-only speedup and complete verified-path speedup in
  every scaling paragraph and figure caption.
- Keep the one-host, workload-bounded and finite-sample limits adjacent to headline speedup
  claims.
- The final submission still needs author, affiliation, funding, conflict, and disclosure
  metadata, but these are administrative rather than scientific score gates.

## 9. Multi-Reviewer Panel

### Reviewer R1 — Numerical Method and Soundness

- **Lens:** objective, assumptions, gate semantics, and implementation alignment.
- **Score tendency:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** complete-cost ordering and verified-result return are both
  explicit and tested.
- **Main negative signal:** order-invariant acceptance/cost statistics are bounded
  assumptions, not universal facts.
- **Evidence basis:** formulation, routing implementation, proposition, and exhaustive
  ordering evidence.
- **Score-change condition:** preserve the fixed-cascade boundary and add shift calibration.

### Reviewer R2 — Parallel Solver Systems

- **Lens:** solver-level parallelism, batch amortization, heterogeneous placement, and
  bottleneck accounting.
- **Score tendency:** 8/10.
- **Confidence:** 4/5.
- **Main positive signal:** the paper distinguishes fast gate scaling from weaker complete
  path scaling and reports batch effects.
- **Main negative signal:** hardware breadth and native external performance are limited.
- **Evidence basis:** gate-parallel, complete-path, batch, device, and negative-result
  evidence.
- **Score-change condition:** native x86-64/CUDA or broader placement evidence with the same
  complete-cost accounting.

### Reviewer R3 — Experimental Evaluation

- **Lens:** baselines, statistics, failure accounting, and external validity.
- **Score tendency:** 8/10.
- **Confidence:** 5/5.
- **Main positive signal:** paired timing, bootstrap intervals, counterbalanced order, and
  shared controls are stronger than a single aggregate speedup.
- **Main negative signal:** external hybrid baselines and distribution-shift matrices are
  incomplete.
- **Evidence basis:** claim ledger and benchmark evidence.
- **Score-change condition:** one stronger external baseline and a predeclared shift matrix.

### Reviewer R4 — Novelty and Positioning

- **Lens:** distinctness from algorithm selection, neural operators, learned
  preconditioners, and verification-only systems.
- **Score tendency:** 8/10.
- **Confidence:** 4/5.
- **Main positive signal:** the contribution is the complete verified composition and its
  cost model, not an unsupported claim of a new standalone numerical algorithm.
- **Main negative signal:** several components are known independently, so attribution and
  external comparison carry most of the novelty burden.
- **Evidence basis:** introduction, related work, contribution list, and ablations.
- **Score-change condition:** stronger comparison or a clearer mechanism-level result.

### Reviewer R5 — Writing and Reproducibility

- **Lens:** argument clarity, claim calibration, artifact, and reproduction path.
- **Score tendency:** 8/10.
- **Confidence:** 4/5.
- **Main positive signal:** scope correction removes the previous infrastructure detour and
  makes the paper easier to read.
- **Main negative signal:** public immutable release and independent rerun are still absent;
  the 12-page presentation remains dense.
- **Evidence basis:** manuscript, claim ledger, bundle scripts, and review boundary.
- **Score-change condition:** public archive plus independent solver-focused extraction.

### Panel Synthesis

- **Agreement:** the core numerical story is sound, the routing repair is real, and the
  paper no longer needs distributed-systems evidence to justify its contribution.
- **Disagreement:** reviewers differ mainly on how much external evidence is required for
  significance and novelty, not on the correctness of the current bounded claims.
- **Decisive accept axis:** complete-cost solver fusion with original-equation acceptance
  and explicit negative results.
- **Decisive reject axis:** a future failure to substantiate the claimed gains under a
  stronger baseline or materially broader workload matrix.
- **Unresolved evidence:** provider-controlled native performance, stronger external
  hybrid baseline, shift calibration, public archive, and independent rerun.
- **Final calibrated stance:** accept, 8/10, confidence 5/5.

## 10. Concern-to-Action Table

| Concern | Severity | Evidence basis | Required action | Score impact condition |
| --- | --- | --- | --- | --- |
| Hosted native solver performance absent | Major | Workflow exists; no provider-controlled result is in the current claim surface | Run and archive solver-focused hosted campaign | One concrete path toward 9 |
| External hybrid baseline limited | Major | Shared control is transparent but not external | Add stronger published or independently implemented baseline | Supports novelty/significance |
| Shift calibration incomplete | Moderate | Family-specific routing and fixed-cascade assumptions | Evaluate conditioning, size, topology, and hardware shifts | Raises external validity or bounds it honestly |
| Mechanism attribution incomplete | Moderate | Main controls exist but not all bottlenecks are decomposed | Add complete-cost bottleneck and correction--gate ablations | Reduces over-attribution risk |
| Public independent reproduction absent | Moderate | Author-operated bundle and excluded large payloads | Deposit immutable archive and obtain independent rerun | Raises reproducibility to 5/5 |
| Metadata placeholders | Administrative | Author and disclosure fields remain incomplete | Supply final submission metadata | Removes desk risk, not scientific ceiling |

## 11. Checks and Unresolved Items

- **Completed checks:** scope and claim-ledger audits; Python syntax checks; fresh
  solver-only native dry-run; synthetic campaign contract; paper evidence and artifact
  manifest checks; 12-page PDF build; focused routing tests; exhaustive 24-permutation
  reproduction; 29/29 CTests; deterministic bundle generation; and clean-extraction
  bundle verification.
- **Bundle scope:** no process/network build evidence is archived, and the default clean
  build no longer compiles the two legacy process/network evidence executables.
- **Unresolved:** the hosted campaign, public immutable archive, independent reproduction,
  stronger external baseline, and distribution-shift matrix remain future evidence.

## 12. Output Self-Check

- Scope is solver-only; process isolation and distributed high availability are not score
  gates.
- Scores include evidence basis and repair conditions; the only 3/5 criterion has an
  explicit deduction and movement condition.
- No claim of universal acceleration, zero future error, or independent reproduction is
  made.
- Historical security/release artifacts are not used as scientific evidence.
