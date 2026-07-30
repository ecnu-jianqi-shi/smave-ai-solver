# CCF-A Full Review — Round 8

## Mode

Full scientific, writing, artifact, integrity, portability, and TPDS-fit re-review
after extending counterbalanced solver-order measurements to all seven PDE workloads
and regenerating the authoritative repeated-timing evidence.

## Venue and Assumptions

- Target: IEEE Transactions on Parallel and Distributed Systems regular paper.
- Review date: 2026-07-24.
- Materials: current 12-page PDF and LaTeX sources; 217 pinned repeated-PDE reports;
  434 pinned order-sensitivity reports; gate, complete-path, and batch-scaling
  summaries; artifact manifest; macOS and Linux portability results; source and
  tests; Rounds 1--7.
- Submission assumptions: the manuscript remains at the 12-page boundary. Author,
  affiliation, funding, and acknowledgment metadata are still placeholders and must
  be resolved by the authors before submission.
- Overall scale: CCFA 1--10; criterion scale: 1--5.

## Paper Summary

The paper frames repeated numerical acceleration as verification-aware expert
selection. Its central object is a candidate--correction--gate--fallback transaction
whose router minimizes reach-weighted complete verified cost. A bounded proposition
establishes caller-visible commit authority under explicit isolation, immutable-
problem, atomic-publication, and fallback assumptions. The implementation combines
classical solvers, learned candidates, device kernels, typed equation families,
parallel original-equation gates, an SDK, and failure-aware routing.

The evaluation includes seven 30-run paired PDE comparisons, two held-out operator
studies, two-family gate scaling, Navier--Stokes complete-path and batch scaling,
routing studies, device probes, ablations, negative results, and assumption-indexed
implementation tests. Round 8 adds 30 matched classical-first/SMAVE-first process
pairs for every PDE workload. The resulting 420 measured reports show median order
ratios from 0.991 to 1.040, a maximum absolute median shift of 3.9613%, six of seven
bootstrap intervals containing one, one small nonzero Diffusion--Sorption effect,
and no measured benefit reversal. The authoritative fixed-order timing study was
also regenerated: all seven bootstrap lower bounds remain above one, with each
family winning at least 29 of 30 paired runs.

## Likely Stance and Calibrated Score

**Overall: 8/10 — accept. Scholarly confidence: 5/5.**

Round 8 closes the previous P1 order-sensitivity breadth concern. The order study is
now workload-complete, process-paired, counterbalanced, independently analyzed, and
hash-pinned. It also reports the inconvenient result rather than hiding it:
Diffusion--Sorption has a small but statistically nonzero phase-order effect. The
paper accurately separates that effect from benefit reversal and does not claim
order invariance.

The score remains 8 rather than rising to 9 because the decisive limitations are no
longer locally statistical. All authoritative performance still comes from one
Apple M4 host. The Linux/ARM64 and emulated Linux/x86-64 runs establish correctness
and API portability only. There is no native Linux/x86-64 performance study, no
discrete-accelerator result under the complete-cost contract, no immutable public
archive, and no independent rerun. A shared learned/hybrid baseline protocol also
remains absent. These limitations affect external validity and reproducibility more
than wording, and the new order controls do not substitute for them.

## Quantitative Scorecard

| Dimension | Score (1--5) | Confidence (1--5) | Evidence basis | Deduction / score-change condition |
|:---|:---:|:---:|:---|:---|
| Novelty | 4 | 5 | `paper/sections/01_introduction.tex:24`; `paper/sections/05_verification_aware_fusion.tex:21` | The transaction/objective/authority combination is differentiated. A 5 requires a broader scheduling, composition, or distributed-authority result beyond the bounded protocol theorem. |
| Soundness | 4 | 5 | `paper/abstract.tex:11`; `paper/sections/06_experimental_methodology.tex:62`; `paper/sections/09_discussion_limitations.tex:22` | The theorem scope, original-equation authority, all-family order controls, and fail-closed tests are strong. Arbitrary callback errors, hardware faults, and distributed commit remain outside the guarantee; native external reruns would strengthen the score. |
| Evidence | 4 | 5 | `paper/sections/07_evaluation.tex:41`; `paper/sections/07_evaluation.tex:90`; 217 PDE reports; 434 order reports | Evidence is broad, paired, failure-inclusive, and now order-controlled across every PDE family. A 5 requires native cross-platform workload measurements, a shared learned baseline, and independent reproduction. |
| Significance | 4 | 4 | `paper/abstract.tex:16`; `paper/sections/09_discussion_limitations.tex:78` | The work is relevant to parallel verification and heterogeneous numerical runtimes, but NUMA, multi-node, discrete-accelerator, and external deployment impact remain unmeasured. |
| Clarity | 5 | 5 | `paper/abstract.tex`; `paper/CLAIM_EVIDENCE.md`; generated timing/order macros | Claims, scope, negative results, fixed-order results, and counterbalanced results are now clearly separated and machine-synchronized. No material deduction. |
| Reproducibility | 4 | 5 | `paper/ARTIFACT_SNAPSHOT.md`; `paper/ARTIFACT_MANIFEST.txt`; `benchmark/run_linux_portability_checks.sh` | Local reproducibility is unusually strong, including raw-report hashes and three passing platform suites. A 5 requires committed history, a persistent public identifier, complete release packaging, and an independent rerun. |
| Ethics / Limitations | 5 | 5 | `paper/sections/09_discussion_limitations.tex:22`; `paper/CLAIM_EVIDENCE.md:28` | Universal speedup, order, transfer, device, theorem, and portability limits are explicit; the significant order effect is disclosed. No material deduction. |

**Overall:** 8/10 | **Scholarly Confidence:** 5/5

**Recommendation:** accept.

**Verdict:** native Linux/x86-64 paired workload measurements plus an immutable
public artifact and independent rerun can raise the paper to 9. A credible 10 would
also require multi-hardware replication, discrete-accelerator evidence, stronger
shared-protocol learned baselines, and completed submission metadata.

## Score-Change Conditions

| Change | Condition | Likely affected dimensions | Expected movement |
| --- | --- | --- | --- |
| Raise score | Reproduce the authoritative paired workloads on a native Linux/x86-64 host and one discrete accelerator with the same setup+kernel+gate+fallback contract | Evidence, significance, reproducibility | +1 overall if conclusions remain stable |
| Raise score | Publish an immutable archive with dataset/dependency instructions and obtain an independent rerun | Reproducibility, evidence | +0.5 to +1 overall |
| Raise dimension | Compare learned/hybrid methods under one shared training, correction, gate, fallback, accuracy, and complete-cost protocol | Evidence, novelty | ML reviewer +1; possibly +0.5 overall |
| Raise readiness | Replace all author, affiliation, funding, and acknowledgment placeholders and freeze the submission mode | Clarity, format | Readiness only; no scientific score inflation |
| Lower score | Native reruns reverse several speedups, fail original-equation gates, or cannot reproduce pinned reports | Evidence, soundness | -1 or more |
| No quick change | Demonstrate NUMA, multi-node scaling, and distributed authority semantics | Significance, novelty | Required for a defensible 10 rather than the current qualified accept |

## Top Strengths

1. **The strongest remaining local confound is directly measured.** Every PDE
   benchmark supports both phase orders, and the study uses 210 matched process
   pairs rather than isolated one-shot timings.
2. **The analysis is statistically and scientifically honest.** Six intervals
   include one, while Diffusion--Sorption's `1.010 [1.001, 1.045]` result is reported
   as a small nonzero order effect rather than rounded into invariance.
3. **Benefit robustness survives the stronger control.** All 420 measured reports
   retain speedup above one; the largest median order shift is only 3.9613% and no
   family changes the qualitative conclusion.
4. **Authoritative timing was regenerated after implementation changes.** The new
   217-report run yields `1.583×--135.138×` medians, all bootstrap lower bounds above
   one, and a minimum of 29/30 paired wins rather than preserving stale perfect-win
   claims.
5. **Evidence synchronization is machine-enforced.** Generated LaTeX macros,
   evidence checks, raw-report counts, harness hashes, evidence hashes, and the
   12-page PDF all agree.
6. **Portability checks remain meaningful.** macOS/ARM64, Ubuntu/ARM64, and emulated
   Ubuntu/x86-64 each pass 29/29 tests after the Round 8 source changes.

## Major and Fatal Concerns

No new fatal internal-validity issue was found. The order-breadth concern from Round
7 is closed. The remaining score-limiting concerns are external-validity and release
concerns.

### P0 — Native Performance Portability

- Evidence basis: `paper/abstract.tex:16` and
  `paper/sections/09_discussion_limitations.tex:38` explicitly restrict
  authoritative performance to one Apple M4 host.
- Deduction: Linux containers do not establish native x86-64 performance, NUMA
  behavior, discrete-GPU behavior, or independent hardware reproducibility.
- Repair condition: rerun the same paired complete-cost workloads on native
  Linux/x86-64 and at least one discrete accelerator, retaining failures and all
  original-equation gates.
- Score impact: this is the main 8-to-9 boundary.

### P0 — Public and Independent Reproduction

- Evidence basis: `paper/ARTIFACT_SNAPSHOT.md:61` records no committed history,
  immutable release identifier, persistent archive, or independent-host rerun.
- Deduction: local hashes detect drift but do not provide public persistence or
  external reproducibility.
- Repair condition: publish a versioned archive with source, manifests, raw reports,
  acquisition/licensing instructions, dependency versions, and a persistent
  identifier; obtain an independent rerun.
- Score impact: required for 9 and indispensable for 10.

### P1 — Shared Learned Baseline Protocol

- Evidence basis: the paper cites and discusses learned operators and
  preconditioners, but the seven PDE results use workload-specific classical
  baselines and only two operator families have held-out learned comparisons.
- Deduction: a skeptical SciML reviewer cannot compare hybrid methods under one
  shared training, correction, gate, fallback, precision, and complete-cost
  contract.
- Repair condition: add at least one credible learned/hybrid baseline under the same
  held-out split, original-equation gate, failure accounting, and end-to-end timing
  definition.
- Score impact: evidence dimension +1; overall movement depends on result quality.

### P1 — Submission Metadata and Boundary

- Evidence basis: `paper/authors.tex` and
  `paper/sections/11_acknowledgments.tex` remain placeholders; the PDF is exactly 12
  pages.
- Deduction: scientific review is possible, but submission readiness is incomplete
  and any metadata expansion can disrupt the current page boundary.
- Repair condition: authors choose the submission mode, insert final metadata, and
  rebuild before the release manifest is frozen.
- Score impact: readiness risk rather than a scientific deduction.

## Writing and Presentation Concerns

1. The main order result is now appropriately compressed in the abstract and fully
   qualified in Evaluation and Limitations. No claim overstates invariance.
2. The phrase “all measured runs retain speedup” is correctly limited to the 420
   order-controlled reports; the separate authoritative fixed-order study correctly
   says at least 29/30 wins per family.
3. Generated macros prevent numerical drift between evidence, prose, table, figure,
   README, and claim ledger.
4. LaTeX still reports several underfull boxes, mostly in narrow tables and existing
   prose. No overfull box, undefined reference, missing citation, or page-count
   regression was observed.

## Format and Venue Concerns

1. The PDF remains 12 pages in IEEE Computer Society journal layout.
2. The paper is at the page boundary, so author metadata or final production edits
   may require small cuts.
3. Author identities, affiliations, funding, and acknowledgments must be finalized
   consistently with the chosen submission mode.
4. The topic remains a credible TPDS fit through verified parallel gates,
   heterogeneous runtime composition, worker/batch scaling, and failure-aware
   systems design; the absence of distributed experiments limits the strength of the
   fit rather than making it out of scope.

## Multi-Reviewer Panel

### Reviewer R1 — Numerical Soundness

- Score tendency: 8/10; confidence 5/5.
- Positive signal: original-equation authority, explicit theorem assumptions,
  failure-preserving comparisons, and all-family order controls.
- Negative signal: the theorem does not certify incorrect callbacks or hardware, and
  current performance evidence remains single-host.
- Score-change condition: native reruns with unchanged residual and fallback
  semantics.

### Reviewer R2 — Parallel and Distributed Systems

- Score tendency: 7/10 to 8/10; confidence 5/5.
- Positive signal: gate scaling, complete-path scaling, batch saturation, typed
  transactions, and the new phase-order control are systems-relevant.
- Negative signal: no native NUMA, multi-node, multi-GPU, or distributed authority
  experiment exists.
- Score-change condition: native cross-platform scaling or a distributed scheduling
  result under the same commit-authority contract.

### Reviewer R3 — Scientific Machine Learning

- Score tendency: 7/10; confidence 4/5.
- Positive signal: held-out operator studies include correction, gates, fallback,
  break-even, and negative transfer rather than raw inference latency alone.
- Negative signal: no common learned baseline spans the main PDE evaluation under a
  shared protocol.
- Score-change condition: add a credible learned/hybrid comparator with identical
  splits, precision, verification, fallback, and complete-cost accounting.

### Reviewer R4 — Artifact and Reproducibility

- Score tendency: 8/10; confidence 5/5.
- Positive signal: 651 pinned PDE/order reports, generated macros, manifest hashes,
  exact report counts, deterministic analyzers, and three passing platform suites.
- Negative signal: the artifact remains local, uncommitted, non-archival, and not
  independently reproduced.
- Score-change condition: persistent public archive plus an external rerun.

## Panel Synthesis and AC Meta-Review

- Agreement: all reviewers find the paper technically serious, unusually explicit
  about failures, and substantially stronger than Round 7 on internal timing
  validity.
- Disagreement: the numerical and artifact reviewers lean clear accept; the systems
  and SciML reviewers remain more conservative because native/distributed scaling
  and a shared learned baseline are absent.
- Decisive accept axis: verification-aware complete-cost composition backed by
  failure-inclusive, paired, order-controlled evidence.
- Decisive reject axis: a reviewer demanding native multi-platform performance or a
  unified learned baseline could still downgrade the paper.
- Unresolved evidence: native x86-64 performance, discrete accelerator, public
  archive, independent rerun, distributed scaling, and final metadata.
- Final calibrated stance: 8/10 accept. Round 8 removes a real P1 concern but does
  not cross the external-evidence boundary required for 9.

## Concern-to-Action Table

| Priority | Concern | Evidence basis | Concrete action | Owner | Score condition |
| --- | --- | --- | --- | --- | --- |
| P0 | Native performance portability | Single Apple M4 timing host | Repeat authoritative paired workloads on native Linux/x86-64 and one discrete accelerator | Authors / external hardware owner | Main path to 9 |
| P0 | Public independent reproduction | Local manifest only | Publish immutable archive and obtain independent rerun | Authors / artifact owner | Required for 9--10 |
| P1 | Shared learned baseline | Two operator families, workload-specific main baselines | Implement one shared learned/hybrid comparison contract | Experiment owner | Evidence +1; possible overall +0.5 |
| P1 | Submission metadata | Placeholder author/acknowledgment files | Finalize identities, funding, acknowledgments, and submission mode | Authors | Readiness only |
| Closed | Solver-order breadth | Seven families, 210 pairs, 434 reports | Preserve the current harness and manifest pin | Artifact owner | No remaining deduction |

## Recommended Next CCFA Owner

1. `ccf-paper-reviewer` should remain the decision owner after any new external
   evidence arrives.
2. Experimental ownership should move to an external/native hardware operator for
   Linux/x86-64 and discrete-accelerator measurements.
3. Artifact ownership should publish the frozen source, report trees, manifests, and
   dataset instructions under a persistent identifier.
4. If local work continues before external evidence, the highest-value scientific
   task is a shared learned/hybrid baseline protocol, not further wording changes.

## Checks Run

- Reanalyzed 434 order reports from
  `build/release/pdebench-order-sensitivity/20260724T035555Z`.
- Regenerated 217 authoritative PDE reports under
  `build/release/pdebench-repeated-timing/20260724T045454Z`.
- Verified every order report has `CROSS_SOLVER_AGREEMENT=1` and every measured
  speedup exceeds one.
- Verified all seven fixed-order bootstrap 95% lower bounds exceed one and each
  family wins at least 29/30 paired runs.
- Configured and rebuilt the Release tree; macOS/ARM64 CTest passed 29/29.
- Clean Ubuntu 24.04 ARM64 container passed 29/29.
- Clean Ubuntu 24.04 emulated x86-64 container passed 29/29.
- `python3 paper/check_evidence.py` passed.
- `python3 paper/check_artifact_manifest.py` passed with 217 PDE and 434 order
  reports.
- `paper/check.sh` passed; `paper/main.pdf` remains 12 pages.

## Unresolved or Unverified

- Native Linux/x86-64 performance.
- CUDA or another discrete-accelerator complete-cost comparison.
- NUMA, multi-node, and distributed authority scaling.
- Public immutable archive and persistent identifier.
- Independent-host reproduction.
- Shared-protocol learned/hybrid baseline across the main workload suite.
- Final author, affiliation, funding, acknowledgment, and submission-mode metadata.

## Output Self-Check

- Scores are separated from confidence and match the strongest unresolved concern.
- Every score below 5 has an explicit deduction and repair condition.
- The prior order-breadth concern is marked closed rather than repeated mechanically.
- No container result is described as native performance or independent
  reproduction.
- No wording change is credited as evidence improvement without a corresponding
  measurement or machine check.
- The report contains no placeholder, invented result, or acceptance probability.
