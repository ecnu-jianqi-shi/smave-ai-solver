# CCF-A Full Review — Round 29

## 1. Report Metadata

- **Mode:** full scientific, writing, format, artifact, integrity, and reproducibility
  review.
- **Review date:** 2026-07-26.
- **Venue and assumptions:** IEEE Transactions on Parallel and Distributed Systems,
  regular paper, 2026 submission cycle.
- **Paper title:** *Complete-Cost Expert Fusion for Verified Repeated Numerical Solves*.
- **Materials reviewed:** current LaTeX manuscript, solver implementation, generated
  evidence, claim ledger, artifact manifest, clean reproduction bundle, Round 28 review,
  and public-safe related-work searches.
- **Privacy boundary:** unpublished manuscript text remained local; external searches used
  only public solver, neural-operator, correction, routing, and preconditioning keywords.
- **Desk rejection risk:** low. The manuscript is complete and builds to 12 pages, but
  author/disclosure placeholders must be replaced before submission.

## 2. Paper Summary

The paper studies repeated numerical solves with heterogeneous classical and learned
experts. It proposes a typed candidate--corrector--original-equation gate--fallback
pipeline and a Router that minimizes reach-weighted complete runtime, including transfer,
correction, verification, rejection, and terminal fallback. For a fixed eligible cascade
with order-invariant stage statistics, it derives the cost-per-acceptance ordering rule.
Every returned result must pass the family-specific original-equation acceptance test.

The evaluation covers seven PDEBench-derived repeated workloads, SuiteSparse, PETSc TS,
OpenModelica, COPS, held-out linear and nonlinear operators, exhaustive four-stage
ordering, routing calibration, a 5x5-to-6x6 router shift, complete-cost decomposition,
gate and complete-path parallelism, batch amortization, device placement, and explicit
negative results.

## 3. Likely Stance and Calibrated Score

- **Overall score:** **8/10, accept**.
- **Scholarly confidence:** **5/5**.
- **Rationale:** the central solver contribution is coherent and unusually well bounded.
  The new router-shift and two-family cost-decomposition evidence materially improve
  mechanism credibility. The paper does not yet reach strong-accept level because the
  correction policy is evaluated only at a fixed 32-step budget, the closest recent
  hybrid solver work is not fully positioned, and the strongest performance evidence
  remains author-operated on one Apple M4 system.

## 4. Scorecard

| Dimension | Score (1-5) | Confidence (1-5) | Evidence basis | Deduction / score-change condition |
|:---|:---:|:---:|:---|:---|
| Novelty | 4 | 5 | `paper/sections/01_introduction.tex`; `paper/sections/02_background_related_work.tex`; complete-cost objective and typed solver roles | The composition is distinctive, but correction, learned preconditioning, neural operators, and solver routing are known separately. Explicitly position FCG-NO and recent error-conditioned/solver-routing work, then compare against a stronger external hybrid path for 5/5. |
| Soundness | 5 | 5 | `paper/sections/03_problem_formulation.tex`; exhaustive 24-permutation evidence; family-specific gate and fallback traces | No major logical inconsistency is visible. The guarantee is correctly limited to the supplied equation contract and observed implementation paths. |
| Evidence | 4 | 5 | `paper/sections/07_evaluation.tex`; `paper/sections/08_ablation_analysis.tex`; router-shift and complete-cost-decomposition reports | The 5x5-to-6x6 shift and two-family timing decomposition are meaningful, but the fixed 32-step corrector does not expose the correction--acceptance--fallback frontier. Add a budget sweep with projected fallback-inclusive cost and break-even. |
| Significance | 4 | 4 | seven repeated PDE workloads, held-out operators, complete-path scaling, batch scaling, negative device and transfer results | Generality is constrained by one main timing host, limited external hybrid baselines, and small operator families. Native external runs and broader scale/topology shifts would support 5/5. |
| Clarity | 5 | 5 | solver-only scope, contribution list, acceptance terminology, explicit limitations, 12-page PDF | The earlier safety/high-availability ambiguity is removed. The paper remains dense but decision-relevant claims are traceable. |
| Reproducibility | 4 | 5 | deterministic bundle, manifest, frozen raw reports, clean extraction, 29/29 CTests, regenerated shift/decomposition evidence | The local clean bundle is strong, but there is no public immutable archive or independent full-data rerun. Deposit and independent reproduction would support 5/5. |
| Ethics / Limitations | 5 | 5 | `paper/sections/09_discussion_limitations.tex`; dataset licenses and claim exclusions | Limitations, unavailable comparisons, finite error-rate evidence, data exclusions, and non-universal claims are explicit. |

**Overall:** **8/10**  | **Scholarly Confidence:** **5/5**

**Recommendation:** accept

**Verdict:** a correction-budget frontier plus explicit closest-work positioning can
plausibly move the paper to 9/10. A 10/10 judgment still requires external evidence that
cannot be created honestly from the current local environment: a strong external hybrid
baseline, provider-controlled native execution, and public independent reproduction.

## 5. Top Strengths

1. **Complete-cost objective matches the production path.** Candidate, transfer,
   correction, gate, rejection, and fallback are charged through reach probabilities.
2. **Ordering theory is bounded and executable.** The pairwise rule is stated only for a
   fixed eligible cascade with order-invariant statistics, and all 24 four-stage
   permutations are checked.
3. **Numerical acceptance is separated from learned-model quality.** Router mistakes and
   rejected candidates affect cost, while successful returns still require the original
   equation gate.
4. **Distribution shift is no longer ignored.** Under the measured 5x5-to-6x6 shift, the
   winner is preserved but the source choice still has 1.213x held-out regret, preventing
   an overclaim of perfect transfer.
5. **Mechanism attribution is improved.** Raw candidates occupy only 1.46% and 0.20% of
   full verified time in the two operator families, while correction plus runtime gate
   dominates.
6. **Negative results are preserved.** Unprofitable device paths, failed transfer, and
   fallback-only comparisons remain visible rather than being removed from the story.
7. **Artifact discipline is unusually strong.** The clean bundle restores frozen analysis
   inputs, reruns the relevant solver analyses, passes 29/29 CTests, and rebuilds the PDF.

## 6. Major and Moderate Concerns

### M1 — Correction budget is fixed rather than explained

- **Severity:** major and locally actionable.
- **Evidence basis:** `paper/sections/07_evaluation.tex` and
  `build/release/{phase5,nonlinear-operator}/benchmark-traces/operator-ablation.txt` use a
  32-step external corrector limit.
- **Concern:** the paper shows that correction dominates runtime, but it does not show how
  acceptance, residual, iterations, fallback, and total cost change as the correction
  budget varies. A skeptical reviewer can argue that the reported method is one tuned
  operating point rather than a characterized mechanism.
- **Required action:** sweep budgets 0, 1, 2, 4, 8, 16, and 32 for both operator families;
  report correction time, acceptance, fallback, total iterations, maximum residual,
  fallback-inclusive projected cost, and the lowest-cost budget.
- **Score impact:** closing this concern supports Evidence 5/5 and an overall 9/10 stance.

### M2 — Closest recent hybrid-solver positioning is incomplete

- **Severity:** moderate.
- **Evidence basis:** current related work covers HINTS, PhysicsCorrect, ANCHOR, learned
  preconditioners, algorithm selection, FNO, and DeepONet.
- **Concern:** public-safe search also finds FCG-NO, which embeds neural operators in a
  flexible conjugate-gradient method, and recent error-conditioned neural solver work
  that selects solver behavior from error feedback. These works do not subsume SMAVE's
  complete-cost typed pipeline, but omitting them weakens the novelty boundary.
- **Required action:** add concise positioning that separates inner-iteration acceleration
  and error-conditioned solver choice from complete-cost cascade routing with mandatory
  original-equation acceptance and fallback-inclusive accounting.
- **Score impact:** reduces novelty-risk disagreement; dimension movement is more likely
  than a full overall-point change by itself.

### M3 — Distribution shift evidence remains one-dimensional

- **Severity:** moderate.
- **Evidence basis:** `build/release/router-shift/evidence.txt` covers one sparse family
  size/fingerprint transition.
- **Concern:** conditioning, topology, precision, hardware, and equation-family shifts are
  not yet jointly evaluated. The current result is useful but cannot establish broad
  Router generalization.
- **Required action:** add a predeclared shift matrix or explicitly retain the current
  one-axis limit in claims and future-work language.
- **Score impact:** broader measured shifts support Significance 5/5; honest bounding
  prevents a deduction but does not raise the score alone.

### M4 — External hybrid baseline and native execution remain absent

- **Severity:** major but externally dependent.
- **Evidence basis:** shared learned-candidate/Jacobi control is internal; hosted workflow
  has no provider-controlled artifact; authoritative timing is one Apple M4 system.
- **Concern:** the paper cannot yet demonstrate that the full approach outperforms a
  strong independently maintained hybrid solver or transfers across native hardware.
- **Required action:** run a provider-controlled x86-64/CUDA campaign and add one strong
  external hybrid learned-solver or learned-preconditioner baseline under the same
  complete-cost boundary.
- **Score impact:** one credible external axis supports 9/10; multiple aligned external
  axes are necessary for 10/10.

### M5 — Public and administrative completion is pending

- **Severity:** moderate reproducibility plus administrative desk risk.
- **Evidence basis:** the bundle is local and author-operated; authors, affiliations,
  funding, conflict, and acknowledgments remain placeholders.
- **Required action:** publish an immutable archive, obtain an independent rerun if
  possible, and replace all submission metadata placeholders.
- **Score impact:** raises Reproducibility to 5/5 and removes desk risk; it does not replace
  missing external numerical evidence.

## 7. Writing and Presentation Review

- The manuscript now consistently describes an original-equation acceptance gate rather
  than a security, authority, or high-availability mechanism.
- The introduction clearly states that SDK breadth is implementation support rather than
  the main novelty.
- The router-shift paragraph correctly distinguishes preserved winner identity from zero
  complete-cost regret.
- The cost decomposition is informative but should be followed by a budget frontier so
  the dominant correction term becomes an explanatory result rather than only a timing
  observation.
- No unresolved references or overfull boxes remain in the reviewed PDF.

## 8. Format and Desk Checks

| Check | Status | Evidence | Consequence / action |
| --- | --- | --- | --- |
| Paper length | pass under current assumption | 12-page IEEEtran journal draft | Verify the exact 2026 submission policy before submission. |
| Topic compatibility | pass | parallel numerical solving, heterogeneous execution, solver selection, batching, and device placement | Maintain the solver-only framing. |
| Minimum scientific quality | pass | complete method, theorem, evaluation, ablations, limitations, and references | No desk-level scientific omission. |
| Template/build | pass | `paper/check.sh`; no unresolved references or overfull boxes | Preserve the current build gate. |
| Prompt injection / hidden manipulation | pass | local source inspection found no reviewer-directed hidden instructions | No action. |
| Anonymity and metadata | uncertain | explicit author/disclosure placeholders | Replace according to the venue's review mode and submission rules. |
| Data and license disclosure | pass | byte locks, upstream records, licenses, and excluded payloads are stated | Preserve the claim boundaries. |

## 9. Multi-Reviewer Panel

### Reviewer R1 — Numerical Methods and Soundness

- **Lens:** mathematical formulation, acceptance semantics, and solver correctness.
- **Score tendency:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** the cost model, bounded ordering result, and acceptance
  invariant align with the implementation and evidence.
- **Main negative signal:** no correction-budget frontier explains why 32 iterations is
  the right operating point.
- **Score-change condition:** characterize the correction--acceptance--fallback frontier.

### Reviewer R2 — Parallel Solver Systems

- **Lens:** complete-path parallelism, batching, and heterogeneous placement.
- **Score tendency:** 8/10.
- **Confidence:** 4/5.
- **Main positive signal:** gate-only scaling is not confused with complete-path scaling,
  and batch amortization is measured separately.
- **Main negative signal:** one Apple M4 timing environment limits hardware conclusions.
- **Score-change condition:** provider-controlled native x86-64 or CUDA execution.

### Reviewer R3 — Experimental Evaluation

- **Lens:** baselines, statistics, shifts, ablations, and negative results.
- **Score tendency:** 8/10.
- **Confidence:** 5/5.
- **Main positive signal:** paired bootstrap intervals, order counterbalancing, shift
  regret, decomposition, and failed comparisons are all retained.
- **Main negative signal:** fixed corrector budget and internal shared control leave
  mechanism and baseline uncertainty.
- **Score-change condition:** budget sweep plus one stronger external hybrid baseline.

### Reviewer R4 — Novelty and Positioning

- **Lens:** relation to algorithm portfolios, neural operators, learned preconditioners,
  iterative hybrid solvers, and error-conditioned selection.
- **Score tendency:** 8/10.
- **Confidence:** 5/5.
- **Main positive signal:** complete-cost typed composition with mandatory per-request
  original-equation acceptance is not reduced to raw neural inference or a single learned
  preconditioner.
- **Main negative signal:** FCG-NO and recent error-conditioned neural solver work are not
  explicitly separated from the contribution.
- **Score-change condition:** add closest-work positioning and a matched external control.

### Reviewer R5 — Writing and Reproducibility

- **Lens:** scope, claim traceability, artifact, and submission readiness.
- **Score tendency:** 9/10 scientifically, 8/10 submission-ready.
- **Confidence:** 5/5.
- **Main positive signal:** solver scope, claim ledger, deterministic archive, clean
  extraction, and machine-generated values are unusually disciplined.
- **Main negative signal:** archive is not public or independently operated; metadata is
  unfinished.
- **Score-change condition:** immutable release, independent rerun, and completed metadata.

### Panel Synthesis

- **Agreement:** the paper is sound, solver-focused, and already above the acceptance
  threshold.
- **Disagreement:** R1 and R5 view the current mechanism and artifact as near-strong-accept;
  R2--R4 require stronger external and correction-budget evidence.
- **Decisive accept axis:** complete-cost solver fusion with original-equation acceptance,
  bounded claims, and explicit negative results.
- **Decisive reject axis:** no current fatal flaw; a future overclaim of broad hardware or
  Router generalization would materially lower the score.
- **Unresolved evidence:** correction frontier, closest-work positioning, external hybrid
  baseline, native external performance, public archive, and independent rerun.
- **Final calibrated stance:** accept, 8/10, confidence 5/5.

## 10. Concern-to-Action Table

| Concern | Severity | Fix class | Required action | Expected score movement |
| --- | --- | --- | --- | --- |
| Fixed correction budget | Major | local experiment and manuscript | Add 0--32 budget frontier for both operator families | Evidence +1 dimension; plausible overall 8 to 9 |
| Missing closest hybrid work | Moderate | local writing and references | Position FCG-NO and error-conditioned neural solvers | Novelty-risk reduction |
| One-axis Router shift | Moderate | new solver experiment | Add conditioning/topology/precision/hardware shift matrix | Significance or evidence +1 dimension |
| Internal hybrid baseline only | Major | external implementation/experiment | Add independently maintained hybrid baseline | Supports overall 9 and novelty 5 |
| One main timing host | Major | external execution | Archive provider-controlled native campaign | Supports significance and overall 9 |
| Local-only archive and metadata | Moderate | release/administrative | Publish immutable archive and complete submission fields | Reproducibility 5; desk risk removed |

## 11. Score-Change Conditions

| Change | Condition | Likely affected dimensions | Expected movement |
| --- | --- | --- | --- |
| Raise score | Correction-budget sweep plus closest-work positioning | Evidence, Novelty, Clarity | 8 to 9 is plausible |
| Raise score | Strong external hybrid baseline or provider-controlled native execution | Novelty, Significance, Evidence | reinforces 9; required for 10 trajectory |
| Raise score | Public immutable archive and independent rerun | Reproducibility | dimension 4 to 5 |
| Lower score | Budget sweep shows no stable complete-cost operating region | Evidence, Significance | 8 to 7 |
| Lower score | External baseline removes the reported advantage | Novelty, Significance | 8 to 6--7 depending on breadth |
| No quick change | Independent external execution and archival adoption | Significance, Reproducibility | cannot be created by local manuscript edits |

## 12. Checks Run

- `python3 -m py_compile` on router, evidence, and bundle verification scripts.
- `cmake --build build/release --target reproduce-router-shift`.
- `python3 paper/check_evidence.py` and `python3 paper/check_artifact_manifest.py`.
- `paper/check.sh`: 12 pages, no unresolved references, no overfull boxes.
- Deterministic bundle creation and clean extraction verification.
- Clean extraction: 29/29 CTests, cascade ordering, router shift, complete-cost
  decomposition, paper checks, and PDF rebuild all pass.
- Public-safe novelty search covering hybrid neural/numerical solvers, learned
  preconditioners, neural-operator Krylov methods, error-conditioned solvers, and solver
  routing.

## 13. Unresolved or Unverified

- No provider-controlled native performance artifact is available.
- No strong independently maintained hybrid solver has been executed under the same
  complete-cost boundary.
- No public immutable archive or independent full-data reproduction exists.
- Router shift evidence does not yet span conditioning, precision, hardware, or multiple
  equation families.
- Author, affiliation, funding, conflict, and acknowledgment metadata remain incomplete.

## 14. Output Self-Check

- Every score has an evidence basis and repair condition.
- The review does not use deployment infrastructure as a contribution or score gate.
- No external result, acceptance probability, or independent reproduction is invented.
- The overall score remains 8 because the strongest unresolved concerns are empirical,
  not because of missing non-solver infrastructure.
