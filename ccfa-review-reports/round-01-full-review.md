# CCF-A Full Review — Round 1

## Mode

Full scientific, writing, LaTeX, and venue-fit review.

## Venue and Assumptions

- Target: IEEE Transactions on Parallel and Distributed Systems (TPDS), regular journal paper.
- Paper type: implemented heterogeneous numerical-solver runtime with learned components.
- Evidence basis: `paper/main.tex`, all files under `paper/sections/`, `paper/references.bib`, `paper/CLAIM_EVIDENCE.md`, machine reports under `build/release/`, and the compiled PDF.
- Unavailable evidence: independent Linux/x86-64 reproduction, discrete-GPU measurements, multicore/NUMA/multi-node scaling, and final author/funding metadata.

## Paper Summary

The paper formulates repeated numerical solving as verification-aware expert selection. A typed runtime combines classical solvers, learned candidates, correctors, heterogeneous-device kernels, an original-equation gate, and fallback. The key intended distinction from a conventional solver portfolio is that the router chooses a role-constrained transaction and minimizes complete verified cost rather than raw kernel latency. The evaluation preserves failures and negative results, including unsuccessful device paths and a learned-operator transfer case that does not break even.

## Likely Stance and Calibrated Score

**Overall: 6/10 — weak reject. Confidence: 5/5.**

The paper is unusually disciplined about failure-inclusive reporting and claim scope. However, its strongest implemented evidence is still a single-machine research platform evaluation, whereas TPDS reviewers are likely to expect a demonstrated parallel/distributed systems mechanism or a substantially stronger heterogeneous-runtime evaluation. The manuscript also lacks a formal statement proving the authority and failure-isolation property that currently carries much of the novelty claim.

## Quantitative Scorecard

| Dimension | Score (1–5) | Confidence | Evidence basis | Deduction and repair condition |
| --- | ---: | ---: | --- | --- |
| Contribution and novelty | 3 | 4 | Introduction, Related Work, Formulation | Complete-cost verification-aware fusion is promising, but the distinction from solver portfolios, runtime assurance, residual-corrected neural solvers, and recent online hybrid PDE methods is mostly verbal. Raise to 4 by adding a formal transaction model, closest-work comparison, and a non-obvious runtime rule or theorem. |
| Significance | 4 | 4 | Introduction, Evaluation, Limitations | Correctness-preserving acceleration is important, but broad impact is not yet established beyond qualified workloads. Raise to 5 with cross-platform evidence and a clearly reusable systems abstraction. |
| Technical soundness | 3 | 5 | Formulation, Fusion, machine evidence | Gate and fallback semantics are described but not proved as a compositional invariant. The expected-cost objective does not explicitly model cascade reach/acceptance probabilities. Raise to 4 with formal definitions, proposition, proof, and exact assumptions. |
| Experimental evidence | 3 | 5 | Methodology, Evaluation, claim ledger | Evidence is broad and honestly failure-inclusive, but headline PDEBench results lack paired intervals and use workload-specific baselines that are not visible in one comparison table. Raise to 4 with paired uncertainty, baseline identities/contracts, and cross-machine results. |
| Reproducibility | 4 | 5 | `paper/README.md`, `paper/check.sh`, machine reports | Strong local traceability. Raise to 5 with an environment lock, public archival identifier, one-command result regeneration, and independent reproduction. |
| Clarity and organization | 4 | 5 | Complete manuscript and PDF | The main question is clear, but Sections 4–5 still read partly as an implementation inventory and the abstract carries too many result classes. Raise to 5 by centering the transaction invariant and moving breadth behind it. |
| Venue fit | 3 | 5 | System Design, Evaluation, Limitations | Heterogeneous placement is present, but parallelism, contention, scaling, and distributed scheduling are not measured. Raise to 4–5 only with a demonstrated TPDS-facing execution mechanism and scaling study. |
| Ethics and limitations | 5 | 5 | Discussion and negative results | Claims explicitly reject universal acceleration, zero future risk, and unsupported accelerator generalization. |

## Top Strengths

1. The paper asks a falsifiable scientific question rather than listing repository features.
2. Complete-cost accounting includes correction, gate, fallback, setup, and transfer.
3. The original equation—not the learned model—is intended to remain the commit authority.
4. Positive, negative, fallback, failure, and no-common-success outcomes share one evidence policy.
5. The nonlinear raw-candidate/corrector result gives a concrete mechanism-level justification for fusion.
6. Machine-readable claim provenance is stronger than typical research prototypes.

## Major and Fatal Concerns

### C1 — Major: TPDS-specific systems contribution is not yet demonstrated

- Criterion: venue fit, significance, evidence.
- Evidence: authoritative timing comes from one Apple M4 host; the paper itself excludes multi-node, multi-GPU, NUMA, and cross-architecture claims.
- Reviewer deduction: the paper may be viewed as a careful solver framework rather than a parallel/distributed systems contribution.
- Repair condition: add multicore scaling, batch/concurrency scaling, device residency and transfer decomposition, resource contention, and at least one Linux/x86-64 plus discrete-accelerator reproduction. A full score is impossible without new evidence here.

### C2 — Major: the authority claim is not formalized

- Criterion: novelty, soundness.
- Evidence: `sections/03_problem_formulation.tex` and `sections/05_verification_aware_fusion.tex` describe fresh-state fallback and gated commit only in prose.
- Reviewer deduction: the central safety boundary is easy to agree with but difficult to audit or distinguish from generic runtime assurance.
- Repair condition: define transaction state, reach events, commit rule, assumptions, and prove that no candidate/router failure exposes an unverified state under those assumptions.

### C3 — Major: complete-cost routing is underspecified

- Criterion: novelty, soundness, reproducibility.
- Evidence: Equation (2) sums cost classes, but does not expose which cascade stages are reached or how acceptance/fallback probabilities enter the expectation.
- Reviewer deduction: the optimization objective could be interpreted as bookkeeping rather than an implementable routing principle.
- Repair condition: write the cascade expectation explicitly and state how calibration data estimates stage costs and reach probabilities without using held-out outcomes online.

### C4 — Major: closest recent hybrid-correction work is missing

- Criterion: novelty, related work.
- Evidence: the bibliography ends with broad neural-operator, learned-preconditioner, selective-prediction, and Simplex references. It does not discuss recent residual-corrected neural PDE solvers or online neural/classical mixture methods, including HINTS (2024), PhysicsCorrect (AAAI 2026), and the ANCHOR preprint (2025, revised 2026).
- Reviewer deduction: the novelty claim may appear outdated or insufficiently differentiated.
- Repair condition: add the closest works and compare task, authority, online selection, correction, fallback, and evidence scope directly.

### C5 — Major: headline PDEBench baselines and uncertainty are not inspectable enough

- Criterion: evidence, clarity.
- Evidence: Figure 3 reports a `1.57×–111.13×` range over designated baselines, but baseline identity, sample count, runtime components, and paired uncertainty are not visible together.
- Reviewer deduction: the `111.13×` case can dominate perception despite being a single workload under a special contract.
- Repair condition: add a compact table listing workload, baseline, number of same-input solves, included costs, speedup, and interval/status. If raw paired samples do not support intervals, label the result descriptive rather than statistically stable.

### C6 — Moderate: breadth exceeds direct evidence

- Criterion: clarity, evidence.
- Evidence: contributions claim transactional preservation across algebraic, ODE, DAE, event, hybrid, complementarity, and block-graph services, but acceleration experiments concentrate on sparse algebra and PDE-derived/operator workloads.
- Reviewer deduction: the paper risks combining correctness coverage with acceleration validation as if they were the same claim.
- Repair condition: explicitly separate contract-coverage evidence from performance evidence in the contributions, evaluation table, and conclusion.

### C7 — Moderate: zero-error statistical language is not operationalized

- Criterion: soundness, risk communication.
- Evidence: Methodology mentions a one-sided finite-sample upper confidence bound, but no formula, confidence level, or reported bound is shown in the manuscript.
- Reviewer deduction: the statement looks procedural rather than decision-relevant.
- Repair condition: give the exact zero-event bound, assumptions, confidence level, and either report it for each relevant experiment or remove the claim from the main methodology.

## Writing and Presentation Concerns

1. The abstract mixes the formulation, architecture, seven PDE workloads, two operator studies, routing, five suites, and multiple negative results. Retain one central claim, two decisive result classes, and one limitation.
2. The Related Work positioning paragraph says prior work “typically” optimizes one layer, but does not name the closest counterexamples.
3. The System Design section should distinguish essential mechanisms from engineering support. The public ABI paragraph is reproducibility material, not a core scientific mechanism.
4. The Fusion section needs a formal invariant before enumerating family-specific gates.
5. The conclusion should separate what is proven by design, what is observed empirically, and what remains untested.

## Format and Venue Concerns

- IEEE Computer Society journal template: pass.
- Current length: pass at 10 pages, but new formal material and comparison tables may approach the review limit.
- Undefined citations/references and overfull boxes: pass under `paper/check.sh`.
- Author/funding metadata: intentionally incomplete and must be replaced before submission.
- Topic fit: medium-to-high risk until a parallel/heterogeneous execution result becomes central rather than prospective.

## Multi-Reviewer Panel

### Reviewer R1 — Numerical soundness

- Score tendency: 6/10; confidence 5/5.
- Positive signal: the final original-equation residual remains visible and negative candidate results are retained.
- Negative signal: the commit-authority invariant is not formalized, and “independent gate” needs explicit implementation assumptions.
- Score-change condition: formal proposition plus gate/fallback composition tests and clearer floating-point boundaries.

### Reviewer R2 — Parallel and distributed systems

- Score tendency: 5/10; confidence 5/5.
- Positive signal: complete-cost routing includes transfer, residency, and heterogeneous-device rejection.
- Negative signal: no multicore, NUMA, multi-accelerator, contention, or multi-node result demonstrates a TPDS-level systems contribution.
- Score-change condition: end-to-end scaling and resource-management evidence under a unified protocol.

### Reviewer R3 — Scientific machine learning

- Score tendency: 6/10; confidence 4/5.
- Positive signal: the paper refuses to equate fast inference with a valid solve.
- Negative signal: related work does not yet address the closest residual-correction and online hybrid neural/classical methods.
- Score-change condition: current closest-work comparison and a sharper statement of why deterministic original-equation commit authority is different.

### Reviewer R4 — Artifact and reproducibility

- Score tendency: 8/10; confidence 5/5.
- Positive signal: deterministic reports, negative classifications, claim ledger, installed-host checks, and a manuscript validation script are unusually strong.
- Negative signal: reproduction remains local to one machine and lacks an archival release identifier.
- Score-change condition: public immutable artifact, environment lock, and independent machine rerun.

## Panel Synthesis and AC Meta-Review

- Agreement: the central problem is important and the evidence discipline is strong.
- Disagreement: artifact quality is near accept level, but systems venue fit is below accept level.
- Decisive accept axis: formal verification-aware transaction plus demonstrated heterogeneous scaling.
- Decisive reject axis: single-machine evaluation and missing closest-work positioning.
- Unresolved evidence: paired PDEBench distributions, cross-platform reproducibility, resource contention, and distributed scheduling.
- Final calibrated stance: **weak reject, 6/10**.

## Concern-to-Action Table

| Priority | Concern | Fix class | Required edit or evidence | Score-impact condition | Status |
| --- | --- | --- | --- | --- | --- |
| P0 | Missing TPDS scaling evidence | New experiment | Cross-platform, thread/batch scaling, transfer/residency, contention | Required for overall 9–10 | Requires new result |
| P0 | Missing formal authority result | Manuscript + test audit | Formal transaction invariant, assumptions, proof, test mapping | Can raise soundness by one point | Open |
| P0 | Incomplete cascade cost model | Manuscript | Reach-probability expected-cost equation and calibration description | Can raise novelty/soundness | Open |
| P1 | Closest recent work missing | Literature + manuscript | Add HINTS, PhysicsCorrect, ANCHOR, and direct comparison | Removes novelty-positioning deduction | Open |
| P1 | PDE baseline/CI opacity | Evidence + table | Baseline identities, paired samples/CI, or descriptive qualification | Can raise evidence score | Requires evidence audit |
| P1 | Correctness/performance breadth conflated | Manuscript | Split contract coverage from acceleration evidence | Raises clarity and claim precision | Open |
| P2 | Zero-event bound incomplete | Manuscript | Formula, assumptions, and reported bound or deletion | Removes risk-language concern | Open |
| P2 | Sections 4–5 read as inventory | Writing | Reorder around invariant and complete-cost mechanism | Raises clarity | Open |

## Recommended Next CCFA Owner

The requested combined workflow authorizes immediate manuscript revision. Implement the formal model, closest-work positioning, claim separation, and evidence-table improvements first. New TPDS scaling experiments remain a separate required evidence task and must not be fabricated.

## Checks Run

- Three-pass full-manuscript read.
- Claim-to-evidence ledger inspection.
- Current public related-work search using topic-only queries.
- IEEE/TPDS structure and LaTeX risk inspection.
- Existing compiled-PDF and machine-report review.

## Unresolved or Unverified

- No independent platform result exists in the inspected worktree.
- No paired raw distributions were found in the aggregate PDEBench report.
- “Independent gate” is not yet defined at the code-dependency level.
- Current author, affiliation, funding, conflict, and archival metadata are unavailable.

## Output Self-Check

- Scores separate criterion strength, overall stance, and confidence.
- Every score of 3 has a concrete deduction and repair condition.
- No acceptance probability or guaranteed score movement is claimed.
- No experiment, citation, author identity, or numerical result is invented.
