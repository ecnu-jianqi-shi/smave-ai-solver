# CCF-A Full Review — Round 7

## Mode

Full scientific, writing, artifact, integrity, portability, and TPDS-fit re-review
after Linux/ARM64 and emulated Linux/x86-64 clean-build validation.

## Venue and Assumptions

- Target: IEEE Transactions on Parallel and Distributed Systems regular paper.
- Review date: 2026-07-24.
- Materials: current 12-page PDF and LaTeX sources; 217 pinned repeated-PDE reports;
  124 pinned order-sensitivity reports; gate, complete-path, and batch-scaling
  summaries; artifact manifest; portability scripts and summaries; source and tests;
  Rounds 1--6.
- Official-policy check: current IEEE Computer Society scope and regular-article page
  guidance were checked on 2026-07-24. The topic is in scope, and the current PDF is
  at the 12-page regular-paper boundary. Final metadata, biographies if used, and the
  selected single- or optional double-anonymous submission mode still require an
  author decision.
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
counterbalanced endpoint solver-order probes, routing studies, device probes,
ablations, negative results, and assumption-indexed implementation tests. Round 7
adds clean Ubuntu 24.04 ARM64 and emulated x86-64 builds, each passing 29/29 CTest
tests. The Linux work also exposed and repaired a non-Apple three-lane cyclic-solve
back-substitution defect and corrected optional Apple-backend test contracts.

## Likely Stance and Calibrated Score

**Overall: 8/10 — accept. Scholarly confidence: 5/5.**

Round 7 materially improves implementation portability and reviewer confidence. The
full SDK/test surface now builds and passes on macOS/ARM64, Linux/ARM64, and emulated
Linux/x86-64. Importantly, this was not a cosmetic CI exercise: the Linux run found a
real lane-3 double update in the non-Apple cyclic solver, demonstrating that the new
check can detect architecture-specific defects. Optional Accelerate and Metal paths
now verify fail-closed behavior when unavailable, and the timing-sensitive
cancellation wrapper is serialized.

The score remains 8 rather than rising to 9. Both Linux runs use containers on the
same Apple M4 physical host, the x86-64 run is emulated, and neither run measures the
authoritative workloads. They establish build/API/test portability, not native
performance portability or independent reproduction. The decisive 8-to-9 boundary
therefore remains native external-platform evidence plus a persistent public artifact
and independent rerun.

## Quantitative Scorecard

| Dimension | Score (1--5) | Confidence (1--5) | Evidence basis | Deduction / score-change condition |
|:---|:---:|:---:|:---|:---|
| Novelty | 4 | 5 | `paper/sections/01_introduction.tex:24`; `paper/sections/05_verification_aware_fusion.tex` | The transaction/objective/authority combination is differentiated. A 5 requires a broader scheduling or composition result beyond the current bounded protocol theorem. |
| Soundness | 4 | 5 | `paper/abstract.tex:11`; `paper/sections/09_discussion_limitations.tex:44`; three 29/29 platform suites | Formal scope and fail-closed tests are strong, but arbitrary callback errors, floating-point/hardware faults, and distributed commit remain outside the guarantee. Native external reruns would strengthen this dimension. |
| Evidence | 4 | 5 | `paper/abstract.tex:16`; 217 PDE reports; 124 order reports; two portability summaries | The local package is broad, paired, failure-inclusive, and statistically explicit. A 5 requires native cross-platform workload measurements, a common learned baseline, and independent reproduction. |
| Significance | 4 | 4 | `paper/sections/09_discussion_limitations.tex:75`; complete-path and batch results | The work is relevant to parallel verification and heterogeneous numerical runtimes, but multi-node, NUMA, and external deployment impact remain unmeasured. |
| Clarity | 5 | 5 | `paper/abstract.tex`; generated macros; claim ledger; explicit limitation sections | Claims, scopes, negative results, and performance/correctness portability are now clearly separated. No material deduction. |
| Reproducibility | 4 | 5 | `paper/ARTIFACT_SNAPSHOT.md`; `paper/ARTIFACT_MANIFEST.txt`; `benchmark/run_linux_portability_checks.sh` | Local reproducibility is unusually strong. A 5 requires committed history, a persistent public identifier, complete data acquisition instructions, and an independent rerun. |
| Ethics / Limitations | 5 | 5 | `paper/sections/09_discussion_limitations.tex:28`; `paper/sections/09_discussion_limitations.tex:34`; `paper/CLAIM_EVIDENCE.md` | Universal speedup, transfer, device, theorem, order, and portability limits are explicit. No material deduction. |

**Overall:** 8/10 | **Scholarly Confidence:** 5/5

**Recommendation:** accept.

**Verdict:** native Linux/x86-64 paired workload measurements plus an immutable
public artifact and independent rerun can raise the paper to 9. A credible 10 would
also require multi-hardware replication, discrete-accelerator evidence, stronger
learned baselines, broader order controls, and no unresolved submission metadata.

## Score-Change Conditions

| Change | Condition | Likely affected dimensions | Expected movement |
| --- | --- | --- | --- |
| Raise score | Reproduce the authoritative paired workloads on a native Linux/x86-64 host and one discrete accelerator with the same complete-cost contract | Evidence, significance, reproducibility | +1 overall if conclusions remain stable |
| Raise score | Publish an immutable archive with dataset/dependency instructions and obtain an independent rerun | Reproducibility, evidence | +0.5 to +1 overall |
| Raise dimension | Compare learned/hybrid methods under a shared training, correction, gate, fallback, and complete-cost protocol | Evidence, novelty | ML reviewer +1; possibly +0.5 overall |
| Raise dimension | Extend counterbalanced solver order from two endpoints to all seven PDE families | Evidence, soundness | Dimension-only unless conclusions change |
| Lower score | Native reruns reverse several speedups, fail original-equation gates, or cannot reproduce pinned reports | Evidence, soundness | -1 or more |
| No quick change | Demonstrate NUMA, multi-node, and distributed authority semantics | Significance, novelty | Requires new systems work |

## Top Strengths

1. **Bounded authority claim.** The paper proves a precise protocol property rather
   than claiming universal numerical correctness.
2. **Complete-cost evidence.** Candidate generation, correction, verification,
   fallback, transfer, and failure are retained in the measured contract.
3. **Failure-inclusive evaluation.** Device rejection, nonlinear regression,
   no-common-success cases, and failed operator transfer remain visible.
4. **Evidence governance.** Generated macros, pinned raw reports, analyzers, a claim
   ledger, and a manifest substantially reduce numeric drift.
5. **Portability discipline.** Linux validation both found a real defect and is
   described only as correctness portability, not performance evidence.

## Major and Fatal Concerns

No fatal scientific inconsistency was found in the current snapshot. The following
concerns remain decision-relevant for a stronger TPDS claim.

### P0 — Native Performance Portability

- Evidence basis: all authoritative timing remains Apple M4 data; the new ARM64 and
  x86-64 Linux runs are same-host correctness containers.
- Affected criterion: evidence, significance, reproducibility.
- Fix class: new external experiment.
- Score impact: required for a defensible 9; emulated build success cannot substitute.

### P0 — Public and Independent Reproduction

- Evidence basis: the manifest is local, the repository has no committed immutable
  revision, and no external party has rerun the package.
- Affected criterion: reproducibility and artifact credibility.
- Fix class: release engineering plus external validation.
- Score impact: required for 9--10.

### P1 — Shared Learned Baseline Protocol

- Evidence basis: the paper contains held-out learned/operator studies but not a
  common training/evaluation/verification/fallback protocol against the closest
  learned or hybrid solver alternatives.
- Affected criterion: novelty and evidence for the ML-facing reviewer.
- Fix class: new comparative experiment.
- Score impact: likely raises the ML reviewer from 7 to 8.

### P1 — Order-Sensitivity Breadth

- Evidence basis: counterbalancing covers only the minimum- and maximum-speedup PDE
  endpoints.
- Affected criterion: evidence robustness.
- Fix class: additional benchmark runs.
- Score impact: dimension-level improvement unless intermediate families change the
  aggregate conclusion.

## Writing and Presentation Concerns

- The new methodology sentence correctly separates container correctness from timing
  evidence (`paper/sections/06_experimental_methodology.tex:27`).
- The portability limitation now states that x86-64 is emulated and same-host
  (`paper/sections/09_discussion_limitations.tex:36`).
- The abstract still uses the concise and correct boundary “cross-platform
  performance remains open” (`paper/abstract.tex:30`).
- The PDF remains dense but readable at 12 pages. No undefined citation/reference or
  overfull-box failure was found.
- Final author metadata, funding, acknowledgments, and anonymity mode remain outside
  the scientific text and must not be invented.

## Format and Venue Concerns

- Topic compatibility: pass; parallel verification, heterogeneous execution,
  resource-aware routing, and runtime composition fit TPDS.
- Template: pass; IEEEtran Computer Society journal mode.
- Length: pass at exactly 12 pages, but with no margin for later material.
- Current policy: the general IEEE Computer Society regular-article limit is 12
  formatted pages including references and biographies; final author material should
  be checked before submission.
- Anonymity: unresolved author choice. Current IEEE Computer Society policy permits
  an optional double-anonymous request for eligible journals at editorial discretion;
  the source template now supports either path without asserting that TPDS is always
  single-anonymous.
- Desk rejection risk: low for scope and scientific completeness; medium if submitted
  with placeholder metadata or inconsistent anonymity.

## Multi-Reviewer Panel

### Reviewer R1 — Numerical Soundness

- Score tendency: 8/10; confidence 5/5.
- Main positive signal: bounded theorem, original-equation gates, negative cases, and
  three-platform test success.
- Main negative signal: arbitrary callbacks and hardware faults remain outside the
  proof; container success is not independent numerical replication.
- Score-change condition: native external reproduction with gate agreement.

### Reviewer R2 — Parallel and Distributed Systems

- Score tendency: 8/10; confidence 5/5.
- Main positive signal: gate scaling, complete-path bottleneck analysis, batch scaling,
  and fail-closed heterogeneous routing form a coherent systems contribution.
- Main negative signal: no NUMA, multi-node, distributed scheduler, or native
  non-Apple performance evidence.
- Score-change condition: native thread/NUMA and distributed scaling.

### Reviewer R3 — Scientific Machine Learning

- Score tendency: 7/10; confidence 5/5.
- Main positive signal: learned candidates never receive authority, and transfer
  failures are explicitly retained.
- Main negative signal: learned comparisons are not normalized under one shared
  training, correction, verification, and complete-cost protocol.
- Score-change condition: matched learned/hybrid baselines.

### Reviewer R4 — Artifact and Reproducibility

- Score tendency: 8/10; confidence 5/5.
- Main positive signal: 341 pinned raw timing reports, generated claims, manifest
  checks, dual-architecture Linux scripts, and three 29/29 suites.
- Main negative signal: no immutable public archive or independent rerun.
- Score-change condition: DOI-backed release and external reproduction.

## Panel Synthesis and AC Meta-Review

- Agreement: the paper is internally coherent, locally reproducible, and now
  demonstrably portable at the build/API/test level.
- Disagreement: the systems and artifact reviewers value the new portability work;
  the ML reviewer remains limited by baseline comparability.
- Decisive accept axis: bounded commit authority, complete-cost routing, parallel gate
  evidence, seven-family paired results, negative-result accounting, and strong local
  reproducibility.
- Decisive reject axis: overstating same-host containers as native performance or
  independent reproduction; the paper explicitly avoids this.
- Unresolved evidence: native cross-platform performance, public archive/rerun,
  discrete accelerators, all-family order sensitivity, shared learned baselines, and
  final metadata.
- Final calibrated stance: **accept, 8/10.**

## Concern-to-Action Table

| Priority | Concern | Required action | Expected effect | Status |
| --- | --- | --- | --- | --- |
| P0 | Native portability | Run paired workloads on native Linux/x86-64 and a discrete accelerator | Required for 9 | External hardware |
| P0 | Public artifact | Commit, archive, document datasets/dependencies, and obtain an independent rerun | Required for 9--10 | Open |
| P1 | Learned baselines | Use one training/evaluation/correction/gate/fallback protocol | Raises ML evidence | Open |
| P1 | Order breadth | Counterbalance all seven PDE families | Strengthens robustness | Two of seven complete |
| P2 | Metadata | Fill authors, affiliations, funding, acknowledgments, and anonymity mode | Removes submission risk | Author input required |

## Recommended Next CCFA Owner

- Evaluation maintainer with native Linux/x86-64, accelerator, NUMA, or cluster
  access.
- Artifact maintainer for a committed archival release and independent rerun.
- Scientific-ML maintainer for shared learned/hybrid baselines.
- Author for metadata and anonymity selection.

## Checks Run

- macOS/ARM64 Release build and CTest: 29/29 passed.
- Ubuntu 24.04 Linux/ARM64 clean Release/Ninja build and CTest: 29/29 passed.
- Ubuntu 24.04 Linux/x86-64 emulated clean Release/Ninja build and CTest: 29/29
  passed.
- Linux portability regression diagnosis: fixed duplicate lane-3 back-substitution;
  corrected unavailable Accelerate/Metal test contracts; serialized the wrapper that
  invokes the timing-sensitive cancellation probe.
- `paper/check_evidence.py`: pass.
- `paper/check_artifact_manifest.py`: pass after manifest refresh.
- `paper/check.sh`: pass; 12 pages; no undefined citation/reference or overfull box.
- Current-claim numeric, portability-scope, and hidden-manipulation audit: pass.
- Official IEEE Computer Society scope, page, and optional anonymity guidance checked
  on 2026-07-24.

## Unresolved or Unverified

- Native Linux/x86-64, discrete-accelerator, NUMA, and multi-node performance.
- Public persistent archive and independent rerun.
- Solver-order sensitivity for five remaining PDE families.
- Shared-protocol learned/hybrid baseline.
- Final author, affiliation, biography, funding, acknowledgment, and anonymity data.

## Output Self-Check

- The score was not raised merely because two more test environments passed.
- Correctness portability is not described as performance portability.
- Every score of 3 or below has a concrete deduction and repair condition.
- Score-change conditions are tied to inspectable evidence, not reviewer probability.
- No missing author information or external result was invented.
