# CCF-A Full Review — Round 35

## 1. Report Metadata

- **Review date:** 2026-07-26.
- **Target venue/year/track:** IEEE Transactions on Parallel and Distributed Systems;
  regular paper; repository-defined 12-page IEEE Computer Society journal contract.
- **Paper title:** *Complete-Cost Expert Fusion for Verified Repeated Numerical Solves*.
- **Input materials reviewed:** final manuscript sources and 12-page PDF; solver source,
  tests, generated values, claim ledger, artifact manifest, official HINTS native
  evidence and raw samples, 29-test Release build, and clean extracted-tree bundle.
- **Search basis:** public-safe searches for HINTS, the Greedy PDE Router, FCG-NO,
  neural/classical PDE routing, and learned-solver baseline rigor; primary paper and
  official repository records were preferred.
- **Report file:** `ccfa-review-reports/round-35-full-review.md`.
- **Reviewer mode:** full scientific, writing, format, multi-reviewer, and AC synthesis.
- **Scientific scope:** solver algorithms, numerical acceptance, correction, routing,
  complete-path cost, intra-node parallelism, heterogeneous placement, and solver
  evidence. Review concerns and score conditions must remain within this scope.

## 2. Desk Rejection Assessment

- **Paper length — pass:** the final PDF has 12 pages.
- **Topic compatibility — pass:** the work studies parallel and heterogeneous numerical
  solver execution, algorithm selection, learned candidates, and complete-path cost.
- **Minimum quality — pass:** theory, implementation, negative results, baselines, and
  machine-generated evidence are inspectable.
- **Policy/anonymity/compliance — uncertain:** `paper/authors.tex` still contains an
  explicit placeholder; the final author or blinded block is submission-critical.
- **Prompt injection and hidden manipulation — pass:** a source scan found no hidden
  reviewer instruction, score request, or model-directed text.
- **Ethics and reviewability — pass:** the manuscript makes bounded technical claims,
  retains failures and regressions, and does not claim independent reproduction.

## 3. Paper Summary And Contribution Map

The paper formulates repeated numerical acceleration as request-level selection over
typed candidate, correction, original-equation acceptance, continuation, and terminal
numerical fallback stages. The Router minimizes a reach-weighted complete-cost
objective rather than raw candidate latency. For fixed eligible cascades with
order-invariant stage statistics, the paper derives a cost-per-acceptance ordering
rule. For finite calibrated expert--budget actions, an exact bounded dynamic program
selects expert subset, one correction budget per expert, and order. A family-specific
residual, constraint, defect, or consistency check determines whether any result may
be returned.

- **Claimed problem:** learned and heterogeneous candidates can appear fast when
  correction, transfer, verification, rejection, and continuation are excluded.
- **Claimed gap:** conventional algorithm selection and hybrid learned-solver studies do
  not generally optimize a typed request-level cascade under a mandatory
  original-equation acceptance contract.
- **Method map:** typed equation IR; capability filtering; reach-weighted complete cost;
  fixed-cascade ordering; exact finite-action expert--budget optimization; role-separated
  candidate/corrector/verifier paths; paired complete-runtime measurement.
- **Evidence package:** seven PDEBench-derived workloads; SuiteSparse, PETSc, and
  OpenModelica breadth; fixed-cascade exhaustive enumeration; size, conditioning, and
  topology shift; held-out joint-policy study; two operator families; shared hybrid
  control; official native HINTS execution; gate, complete-path, and batch scaling;
  negative transfer and unprofitable device cases; deterministic artifact checks.
- **Stated limitations:** one authoritative timing architecture; a controlled
  joint-policy action set; calibrated rather than jointly predicted action statistics;
  one small official HINTS configuration; and no Greedy PDE Router execution.

## 4. Search And Related-Work Basis

- **Queries used:** HINTS neural operator relaxation PDE solver; Greedy PDE Router;
  FCG-NO neural operator conjugate gradients; neural/classical PDE routing; weak
  learned-solver baselines.
- **Sources searched:** arXiv, Nature Machine Intelligence records, PMLR/ICML records,
  and the official HINTS repository revision recorded by the artifact.
- **Closest works found:** HINTS; *A Greedy PDE Router for Blending Neural Operators and
  Classical Methods*; FCG-NO; learned preconditioners; classical algorithm portfolios.
- **Unverified related-work risks:** no public implementation was found and executed for
  the Greedy PDE Router; the search is not an exhaustive survey of every 2026 preprint.
- **Source-quality screening status:** the native baseline is tied to published HINTS,
  pinned public source, official pretrained weights, and the full official test set.

## 5. Expected Review Outcome

- **Expected outcome:** **9/10, strong accept**.
- **Main accept signal:** a coherent solver contribution connects a formal complete-cost
  objective, exact bounded optimization, mandatory original-equation acceptance,
  production execution, paired statistics, faithful HINTS execution, and retained
  negative results.
- **Main reject signal:** none is fatal, but the most ambitious policy and generality
  claims remain tested on controlled action sets and one timing platform.
- **Confidence:** **5/5**; manuscript, source, evidence, raw samples, tests, and clean-tree
  reproduction records are locally inspectable.

## 6. Strengths And Weaknesses

### Strengths

- The central object is a numerical solver cascade; `gate` and `fallback` have precise
  numerical meanings throughout the paper.
- Equation (1) charges stage reach probability, rejection continuation, transfer,
  correction, original-equation acceptance, and terminal fallback.
- The fixed-cascade ordering result is checked against all 24 permutations of a
  four-stage contract, and the finite-action dynamic program is checked against an
  independent exhaustive oracle.
- The official HINTS code, architecture, pretrained weights, full 750-case test set,
  and published 400-update configuration are executed without modifying tracked source.
- The HINTS comparison uses the exact exported equations and a common FP64
  `||Ax-b||∞/max(1,||b||∞)` measure; both methods have zero failures.
- Positive claims use paired estimators and bootstrap intervals, while device,
  nonlinear routing, topology transfer, and no-common-success cases are retained.
- The paper distinguishes gate-only parallel speedup from complete-solve scaling and
  therefore avoids advertising verification throughput as solver throughput.
- The clean bundle verifies 29/29 CTests, evidence schemas, paper macros, artifact
  hashes, the 12-page PDF, and the frozen native HINTS evidence contract.

### Weaknesses

**Weakness:** Joint expert--budget evidence remains deliberately small.

- **Evidence basis:** two 12-variable nonlinear families, two experts, three calibrated
  actions, and 32 training/32 held-out scenarios per family.
- **Reviewer deduction:** the exact optimizer is convincing, but policy usefulness on a
  realistic public solver portfolio remains under-demonstrated.
- **Required fix:** apply the frozen-profile/exhaustive-oracle protocol to a larger
  public workload with more eligible experts and correction budgets.

**Weakness:** Cost and acceptance are calibrated inputs rather than request-conditioned
joint predictions.

- **Evidence basis:** the Router consumes per-action profiles; the manuscript explicitly
  limits the current result.
- **Reviewer deduction:** the paper solves the downstream combinatorial decision but not
  the full statistical prediction problem implied by per-request adaptation.
- **Required fix:** train and calibrate cost/pass predictors from request features, then
  report held-out calibration and policy regret through the same exact optimizer.

**Weakness:** The faithful HINTS comparison is narrow despite being technically strong.

- **Evidence basis:** 750 official cases, but each is a 29-interior-unknown 1D Poisson
  system whose structure admits a direct tridiagonal expert.
- **Reviewer deduction:** the result closes the previous reproduction gap and validates
  complete-cost selection, but it does not establish broad dominance over neural--
  classical hybrids.
- **Required fix:** add a higher-dimensional or less structurally trivial published
  hybrid configuration, or reproduce another closest public method when code exists.

**Claim boundary:** Performance evidence is platform-specific.

- **Evidence basis:** all authoritative timing is on one Apple M4; Linux and emulated
  runs establish build/test portability only.
- **Reviewer deduction:** absolute and relative timing conclusions may depend on CPU,
  memory system, compiler, and accelerator stack.
- **Required fix:** retain platform-specific wording. Additional architectures are
  optional validation unless the paper makes a portability claim.

## 7. Potentially Missing Related Work

### Greedy PDE Router

- **Work:** *A Greedy PDE Router for Blending Neural Operators and Classical Methods*.
- **Status:** searched and cited in the revised manuscript.
- **Why relevant:** it routes between neural and classical solver actions.
- **Overlap:** both works select heterogeneous numerical actions.
- **Needed comparison:** retain the current distinction between iteration-level
  error-trajectory actions and request-level complete-cost cascades; execute it only if
  a faithful public implementation becomes available.

### Baseline-rigor literature

- **Work:** recent analysis of weak baselines and reporting bias in ML-based iterative
  solvers.
- **Status:** searched; not required for the core technical positioning.
- **Why relevant:** the paper's complete-cost and negative-result protocol addresses the
  same evaluation failure mode.
- **Overlap:** benchmark methodology rather than solver mechanism.
- **Needed comparison:** an optional citation could contextualize baseline design, but
  absence does not change the current score.

## 8. Claim-Evidence Audit

| Claim | Where stated | Evidence provided | Strength | Reviewer deduction | Required fix |
| --- | --- | --- | --- | --- | --- |
| Reach-weighted complete cost is the correct selection objective | Abstract, Secs. 1 and 3 | Formal cost decomposition and runtime traces | Strong | Assumes calibrated, conditionally stable stage statistics | Test learned request-conditioned profiles |
| Cost-per-acceptance sorting is optimal for a fixed eligible cascade | Sec. 3 | Pairwise exchange derivation and 24-permutation check | Strong within assumptions | Does not solve interacting-stage or unrestricted subset selection | Preserve assumption language |
| Exact bounded policy jointly selects expert, budget, subset, and order | Secs. 3, 4, and 7 | Dynamic program, independent oracle, held-out controlled families | Strong but bounded | Portfolio is small | Scale to public larger action set |
| Every returned result passes the original-equation contract | Secs. 3--5 | Control-flow proposition, family gates, mismatch/failure evidence | Strong implementation invariant | Not protection against a wrong equation callback or faulty arithmetic | Preserve boundary |
| Qualified workloads obtain stable complete-runtime acceleration | Abstract and Sec. 7 | Seven paired PDE workloads and two held-out operator families | Strong locally | One hardware platform and bounded families | Preserve platform-specific claim wording |
| Router selection beats official HINTS on its public 1D Poisson workload | Abstract and Secs. 6--7 | Official code/weights/data, 750 paired cases, `16.027× [15.654, 16.563]`, zero failures | Strong for this configuration | Structured 29-unknown problem is narrow | Add harder faithful hybrid workload |
| Gate parallelism can help while complete-path scaling remains bounded | Abstract and Secs. 7--8 | Linear/nonlinear gate scaling, Navier--Stokes complete-path and batch studies | Strong | Intra-node only | Add second architecture if broad portability is claimed |

## 9. Experiment / Benchmark / Reproducibility Audit

- **Baselines:** workload-specific classical solvers, fixed and calibrated Routers,
  shared candidate/correction controls, official native HINTS, and hindsight oracles
  are clearly distinguished. The HINTS comparison no longer relies on a schedule-only
  surrogate.
- **Ablations:** no gate, no correction, wrong correction, strict-gate fusion,
  correction budgets, candidate/transfer/correction/gate/fallback decomposition,
  device paths, and operator transfer failures are retained.
- **Datasets/benchmarks:** breadth is strong, but headline acceleration still combines
  derived or bounded families rather than one standard end-to-end solver benchmark
  covering the full claimed portfolio.
- **Metrics:** complete-runtime speedup, bootstrap interval, paired win rate, residual,
  QoI error, acceptance, fallback, failure, gate mismatch, break-even, and no-common-
  success counts are appropriate.
- **Statistical rigor:** paired repetition units are explicit; process-level PDE timing
  and counterbalanced order are strong. Operator timing remains primarily within one
  process, which limits independence claims but is not misrepresented.
- **Robustness/failure cases:** negative transfer, unprofitable device execution,
  nonlinear route regression, and unsupported comparisons are visible.
- **Implementation details:** the native HINTS revision, weights, schedule, compatibility
  adaptation, timing boundary, residual contract, and selected SMAVE expert are recorded.
- **Artifacts:** evidence macros, claim ledger, manifest, source bundle, raw native HINTS
  samples, and clean-tree verification are unusually inspectable.
- **Reproducibility boundary:** the core bundle verifies frozen HINTS evidence but does
  not install or rerun the external public source/PyTorch environment. It remains an
  author-operated local artifact, not an independent replication.
- **Limitations:** one-platform timing, controlled joint policy, narrow native HINTS
  workload, absent Greedy implementation, and unfinished author metadata are explicit.

## 10. Multi-Reviewer Panel

### Reviewer 1

- **Reviewer:** Numerical methods and solver correctness.
- **Likely score:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** original-equation acceptance remains authoritative after
  every candidate and correction path.
- **Main negative signal:** evaluated equation families do not establish universal
  robustness or general high-index DAE capability.
- **Evidence basis:** Secs. 3--5, failure semantics, residual checks, negative cases.
- **Score-change condition:** broader nontrivial families under the same acceptance
  contract could strengthen significance.

### Reviewer 2

- **Reviewer:** Scientific machine learning and hybrid PDE solvers.
- **Likely score:** 8/10.
- **Confidence:** 5/5.
- **Main positive signal:** faithful HINTS code, architecture, weights, and data replace
  the prior schedule surrogate.
- **Main negative signal:** the decisive native comparison is a small structured 1D
  problem and does not execute the Greedy PDE Router.
- **Evidence basis:** native HINTS evidence and Secs. 2, 6, and 7.
- **Score-change condition:** a harder published hybrid workload or second faithful
  closest method would move this reviewer toward 9.

### Reviewer 3

- **Reviewer:** Algorithm selection and optimization.
- **Likely score:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** the objective, ordering rule, exact bounded dynamic program,
  and exhaustive checks form a coherent algorithmic contribution.
- **Main negative signal:** action statistics are inputs rather than jointly learned
  request-conditioned quantities.
- **Evidence basis:** Sec. 3, cascade evidence, joint-route evidence.
- **Score-change condition:** held-out joint prediction/calibration/regret on a larger
  action set.

### Reviewer 4

- **Reviewer:** Parallel and heterogeneous solver systems.
- **Likely score:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** the paper separates gate scaling, complete-path scaling,
  batching, residency, transfer, and failed offload decisions.
- **Main negative signal:** authoritative timing is single-node and single-architecture.
- **Evidence basis:** RQ5, gate/parallel/batch/device evidence.
- **Score-change condition:** none within the current platform-specific claim; broader
  portability claims would require additional native measurements.

### Reviewer 5

- **Reviewer:** Experimental design and statistics.
- **Likely score:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** paired estimators, bootstrap intervals, process-level order
  control, failures, and negative results are consistently reported.
- **Main negative signal:** operator repetitions are within-process, and the HINTS
  configuration is narrow despite the full 750-case test set.
- **Evidence basis:** Secs. 6--8 and raw/evidence reports.
- **Score-change condition:** process-level operator pairs and a harder external hybrid
  configuration.

### Reviewer 6

- **Reviewer:** Reproducibility and artifact audit.
- **Likely score:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** 29/29 clean-tree tests, frozen raw HINTS samples, generated
  macros, claim ledger, manifest hashes, and PDF rebuild all pass.
- **Main negative signal:** external HINTS dependencies are frozen rather than rerun in
  the core bundle; no public immutable archive or independent rerun exists.
- **Evidence basis:** clean-tree evidence and artifact documentation.
- **Score-change condition:** none for the solver contribution; public archival release
  remains a publication-quality improvement.

### Reviewer 7

- **Reviewer:** Writing, clarity, and reviewer-facing risk.
- **Likely score:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** the manuscript now states that the HINTS result validates
  complete-cost selection rather than claiming a SMAVE latent operator win.
- **Main negative signal:** dense evidence and the long revision identifier still make
  the experimental section demanding.
- **Evidence basis:** abstract, contribution list, methodology, limitations, 12-page PDF.
- **Score-change condition:** no major scientific change required; final metadata and
  minor prose compression would reduce submission friction.

### Reviewer 8

- **Reviewer:** Skeptical novice advocate.
- **Likely score:** 8/10.
- **Confidence:** 4/5.
- **Main positive signal:** candidate--corrector--original-equation acceptance--fallback
  is understandable and operationally testable.
- **Main negative signal:** the large evidence matrix can obscure which two ideas are
  genuinely novel: complete-cost cascade selection and mandatory numerical acceptance.
- **Evidence basis:** architecture figure, Introduction, RQs, evaluation tables.
- **Score-change condition:** stronger visual prioritization of the two core ideas.

### Panel Synthesis

- **Agreement:** the solver mechanism is sound, the native HINTS evidence is a material
  advance, and there is no fatal correctness or evidence flaw.
- **Disagreement:** the ML reviewer discounts the narrow 1D HINTS workload more heavily
  than the systems and algorithm-selection reviewers.
- **Decisive positive axis:** formal complete-cost selection plus mandatory original-
  equation acceptance and unusually inspectable complete-path evidence.
- **Decisive negative axis:** external validity of the joint policy and timing results.
- **Unresolved evidence:** realistic larger action portfolio, joint cost/pass prediction,
  and a harder published hybrid baseline.
- **AC stance:** **9/10, strong accept**; the native HINTS result closes the previous
  largest gap but does not justify a 10/10 award-level score.

## 11. Concerns Table

| ID | Severity | Concern | Evidence basis | Affected criterion | Fix class | Required action | Owner skill | Score-change condition |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| R35-1 | Major | Joint policy is tested on a small controlled action set | Two families, two experts, three actions | Significance, evidence | Experiment | Apply frozen-profile/oracle protocol to a larger public portfolio | Solver experiments | Necessary for credible 10/10 |
| R35-2 | Major | Cost and pass probability are calibrated inputs, not jointly predicted | Router profiles and explicit limitation | Originality breadth, evidence | Method/soundness + experiment | Add request-feature predictors, calibration, and held-out regret | Solver modeling | Necessary for credible 10/10 |
| R35-3 | Moderate | Native HINTS comparison is one small structured 1D configuration | 29 unknowns, direct tridiagonal selection | External validity, significance | Experiment | Add harder published hybrid workload or second faithful method | Solver experiments | Could strengthen 9 and support 10 with R35-1/2 |
| R35-4 | Minor | Authoritative timing is one Apple M4 | Methodology and limitations | Claim boundary | Writing | Keep timing claims platform-specific | Paper writing | No scientific score change within the stated scope |
| R35-5 | Minor | External HINTS environment is not rerun by the core bundle | `external_public_code_required=1`, `core_bundle_rerun=0` | Reproducibility | Publication artifact | Preserve the explicit rerun boundary | Artifact/release | Not a solver score condition |
| R35-6 | Minor | Author/disclosure metadata remains unfinished | `paper/authors.tex` and acknowledgment placeholder | Compliance | Writing | Insert real or blinded metadata before submission | Author | Required for submission, not scientific score |

## 12. AC / Meta-Review

- **Reviewer consensus:** strong accept. The contribution is a solver-selection method
  with a numerical acceptance invariant and complete-path cost model.
- **Reviewer disagreement:** the main debate is whether the 1D native HINTS result is a
  decisive closest-method comparison or a narrow demonstration that a portfolio should
  choose a structured direct solver. The evidence supports the latter interpretation,
  which the manuscript now states explicitly.
- **Decisive acceptance axis:** theory, production mechanism, common-equation gates,
  faithful external baseline execution, paired complete-cost evidence, and retained
  failures form a coherent chain.
- **Decisive rejection axis:** none fatal. Policy breadth and closest-method breadth cap
  the score.
- **AC stance:** **9/10, strong accept, confidence 5/5**.
- **Discussion risks:** an enthusiastic reading of `16.027×` could overgeneralize beyond
  the official 1D Poisson configuration; the current limitation language must remain.

## 13. Quantitative Scores

| Criterion | Score (1–5) | Confidence | Evidence basis | Deduction / repair condition |
| --- | ---: | ---: | --- | --- |
| Quality | 5 | 5 | Coherent theory, implementation, and evidence chain | No material deduction |
| Clarity | 5 | 5 | Stable numerical terminology and explicit RQs | No material deduction |
| Significance | 4 | 5 | Broad solver evidence and real complete-cost effects | Joint policy and HINTS external validity are narrow |
| Originality | 5 | 5 | Typed complete-cost cascade, ordering rule, exact budget policy, mandatory numerical acceptance | Preserve bounded novelty language |
| Soundness | 5 | 5 | Derivation, exhaustive checks, common residuals, negative cases | No material deduction |
| Evidence | 5 | 5 | Paired statistics, full official HINTS test set, failures, ablations | Add larger public policy workload for award-level evidence |
| Reproducibility | 4 | 5 | Clean bundle, raw samples, manifests, deterministic checks | No external rerun, public archive, or independent reproduction |
| Ethics / Limitations | 5 | 5 | Honest scope and negative-result reporting | No material deduction |

### Writing Scorecard

| Dimension | Weight | Score (1–5) | Confidence | Evidence basis | Concrete repair |
| --- | ---: | ---: | ---: | --- | --- |
| Storyline and motivation | 12 | 5 | 5 | Problem→gap→complete-cost insight is explicit | None required |
| Contribution display | 12 | 5 | 5 | Four contributions and bounded conclusion are visible | Keep solver-only framing |
| Paragraph logic | 10 | 5 | 5 | Each section and RQ has a distinct role | None required |
| Claim-evidence alignment | 14 | 5 | 5 | Generated macros and ledger match final evidence | Preserve native HINTS boundary |
| Method readability | 10 | 4 | 5 | Equations are ordered, but the method remains dense | Optional compact pseudocode if page budget permits |
| Experiment narration | 10 | 5 | 5 | Text explains what positive and negative results establish | None required |
| Related-work positioning | 8 | 5 | 5 | HINTS, Greedy Router, FCG-NO, and portfolios are separated by routing level and objective | Retain technical-axis comparison |
| Terminology and notation consistency | 8 | 5 | 5 | Gate/fallback/complete cost remain numerical and stable | None required |
| LaTeX and format discipline | 8 | 5 | 5 | 12 pages, no undefined references or overfull boxes | Replace/blind author placeholder |
| Reviewer-facing risk | 8 | 4 | 5 | Scope is honest, but `16.027×` is easy to overread | Keep 1D-only caveat adjacent to result |

- **Weighted writing score:** **4.80/5**.
- **Writing risk band:** low.
- **Quality:** 5/5.
- **Clarity:** 5/5.
- **Significance:** 4/5.
- **Originality:** 5/5.
- **Soundness:** 5/5.
- **Evidence:** 5/5.
- **Reproducibility:** 4/5.
- **Ethics / Limitations:** 5/5.
- **Overall:** **9/10, strong accept**.
- **Confidence:** **5/5**.
- **Score-change conditions:** 10/10 would require a realistic larger held-out action
  portfolio, joint request-conditioned cost/pass prediction, and a harder faithful
  hybrid baseline or equivalent public solver evidence.

## 14. Questions For Authors

1. Which public workload can expose at least four eligible experts and multiple
   correction budgets while still permitting a complete held-out oracle enumeration?
2. Can cost and acceptance be jointly predicted from request features without changing
   the exact bounded optimizer or weakening original-equation acceptance?
3. Can the official HINTS comparison be extended to a less structurally trivial
   configuration, or can the Greedy PDE Router be faithfully executed if code appears?

## 15. Score Revision Criteria

### Raising the score would require

- realistic held-out expert--budget regret on a larger public portfolio;
- joint request-conditioned prediction and calibration of action cost and acceptance;
- preferably, a harder faithful published hybrid baseline or public Greedy Router run.

### Lowering the score would be triggered by

- any native HINTS rerun whose paired lower bound falls to or below one while the claim
  remains unchanged;
- gate mismatch, erroneous acceptance, hidden failure, or omitted continuation cost;
- wording that generalizes the 1D HINTS result to broad PDE or learned-solver dominance;
- stale generated macros, claim ledger, artifact manifest, or clean-tree verification.

### Persistent scientific boundaries

- bounded equation semantics and controlled joint-policy action sets;
- one structurally simple official HINTS configuration.

## 16. Action Plan And CCFA Handoffs

| Priority | Action | Owner skill | Input needed | Expected output | Handoff required |
| --- | --- | --- | --- | --- | --- |
| P0 | Scale held-out joint policy to a public larger portfolio | Solver experiments | Public workload and finite actions | Calibration/oracle/regret matrix | No |
| P0 | Jointly predict action cost and pass probability | Solver modeling | Request features and train/held-out split | Calibrated predictions and downstream regret | No |
| P1 | Extend faithful published hybrid coverage | Solver experiments | Public code and compatible equation workload | Common-equation complete-cost comparison | No |

- **Checks run:** official native HINTS target over 750 cases; native HINTS evidence
  verifier; Python compilation; Release build; 29/29 CTests; paper evidence check;
  artifact manifest check; 12-page LaTeX build with no undefined references or overfull
  boxes; deterministic core-bundle generation; clean extracted-tree build/test/evidence/
  PDF verification; hidden-instruction source scan; public-safe closest-work search.
- **Checks skipped:** no Greedy PDE Router execution and no exhaustive census of all
  2026 solver preprints.
- **Unresolved risks:** realistic policy breadth, joint prediction, and harder
  closest-method coverage.

## Output Self-Check

- Section order follows the standard full-review contract.
- Criterion scores, overall score, confidence, reviewer tendencies, and AC stance are
  internally consistent.
- Every score below 5 has a concrete deduction and repair condition.
- No acceptance probability, fabricated baseline, independent reproduction, or
  universal solver claim is asserted.
- The review evaluates only solver mechanisms, numerical claims, and their evidence.
