# CCF-A Full Review — Round 36

## 1. Report Metadata

- **Review date:** 2026-07-26.
- **Target venue/year/track:** IEEE Transactions on Parallel and Distributed Systems;
  regular paper; 12-page IEEE Computer Society journal format.
- **Paper title:** *Complete-Cost Expert Fusion for Verified Repeated Numerical Solves*.
- **Input materials reviewed:** final manuscript sources and 12-page PDF; production
  routing implementation; request-conditioned model API, training and prediction code;
  5,760 action observations; frozen 12-action model; disjoint train/calibration/held-out
  evidence; 29-test Release build; artifact manifest; and clean extracted-tree bundle.
- **Search basis:** the same-date Round 35 public-safe closest-work search for HINTS,
  Greedy PDE Router, FCG-NO, learned/classical PDE routing, and baseline rigor was
  retained; official IEEE Computer Society author guidance was rechecked for format.
- **Report file:** `ccfa-review-reports/round-36-full-review.md`.
- **Reviewer mode:** full scientific, writing, format, multi-reviewer, and AC synthesis.
- **Scientific scope:** numerical solver mechanisms, original-equation acceptance,
  correction, request-conditioned routing, complete-path cost, solver-internal
  parallelism, heterogeneous placement, baselines, and solver evidence only.

## 2. Desk Rejection Assessment

- **Paper length — pass:** the scope-corrected manuscript rebuilds as a 12-page PDF of
  303,665 bytes with SHA-256
  `92070a52435e32d0b3fa8f1a1f3617c2ab7ac60b7dae9c902b77645dbd8cb93b`.
- **Topic compatibility — pass:** repeated numerical solves, parallel complete-path
  execution, heterogeneous experts, and performance/correctness co-design fit TPDS.
- **Minimum quality — pass:** the paper contains a formal objective, exact finite-action
  optimizer, implementation, baselines, ablations, negative results, limitations, and
  machine-linked evidence.
- **Policy/anonymity/compliance — uncertain:** `paper/authors.tex` remains an explicit
  placeholder. Replacing or properly blinding it is required for submission but is an
  operational action, not a solver-science deduction.
- **Prompt injection and hidden manipulation — pass:** a source scan found no hidden
  reviewer instruction, model-directed text, or score manipulation.
- **Ethics and reviewability — pass:** claims are bounded, negative results are retained,
  and the manuscript distinguishes local author-operated verification from external
  validation.

**Desk rejection risk:** low after the author block is completed; medium if the current
placeholder is submitted unchanged.

## 3. Paper Summary And Contribution Map

The paper treats repeated numerical acceleration as a request-level cascade over typed
candidate, correction, original-equation gate, continuation, and terminal numerical
fallback stages. It minimizes reach-weighted complete cost rather than candidate-only
latency. For fixed order-invariant stages, it derives a cost-per-acceptance ordering
rule. For finite expert--correction-budget actions, an exact bounded dynamic program
selects the expert subset, one budget per expert, and cascade order. Round 36 adds a
production API that extracts request features, predicts each action's attempt cost and
gate-acceptance probability, incorporates disjoint calibration error as action risk,
and passes the resulting actions to the unchanged exact optimizer. Every returned result
still depends on the original residual, constraints, consistency condition, or
discretization defect rather than the Router prediction.

- **Claimed problem:** candidate-only speed can be misleading when setup, correction,
  transfer, verification, rejection, and continued solving are omitted.
- **Claimed gap:** prior algorithm-selection and learned-solver combinations do not
  generally optimize a typed request-level cascade under mandatory original-equation
  acceptance and complete verified runtime.
- **Method map:** typed equation IR; capability filtering; reach-weighted complete cost;
  fixed-cascade ordering; request-conditioned action cost/pass prediction; exact bounded
  expert--budget optimization; role-separated candidate/corrector/gate paths; terminal
  numerical fallback; paired complete-runtime evaluation.
- **Evidence package:** seven PDEBench-derived workloads; SuiteSparse, PETSc, and
  OpenModelica breadth; exhaustive fixed-cascade checks; size, conditioning, and topology
  shift; calibrated and request-conditioned expert--budget studies; two learned-operator
  families; a shared hybrid control; official native HINTS execution; gate and complete-
  path scaling; negative transfer/offload results; deterministic evidence extraction.
- **Stated limitations:** one authoritative timing architecture; controlled synthetic
  request-conditioned portfolio; order-invariant action model; one small official HINTS
  configuration; and no executed Greedy PDE Router comparison.

## 4. Search And Related-Work Basis

- **Queries used:** HINTS neural operator relaxation PDE solver; Greedy PDE Router;
  FCG-NO neural operator conjugate gradients; learned/classical PDE routing; complete-
  cost solver portfolios; official TPDS author guidance.
- **Sources searched:** primary papers, official project repositories, publisher records,
  and official IEEE Computer Society author pages. The manuscript text was not exposed in
  public queries.
- **Closest works found:** HINTS for scheduled neural/classical relaxation; Greedy PDE
  Router for iteration-level learned action selection; FCG-NO for neural-operator/Krylov
  coupling; classical algorithm-selection and portfolio methods for solver choice.
- **Unverified related-work risks:** no exhaustive census of all 2026 solver preprints was
  performed; the Greedy PDE Router remains positioned from its public description rather
  than an executed local artifact.
- **Source-quality screening status:** passed; no review conclusion depends on a blog,
  unsourced benchmark claim, or invented citation.

## 5. Expected Review Outcome

- **Expected outcome:** **9/10, strong accept**.
- **Main accept signal:** the paper now closes the previous statistical-routing gap with
  a production-trace-trained request-conditioned model, disjoint calibration/held-out
  splits, exact-DP/exhaustive agreement, lower held-out complete-cost regret than both
  controls, and unchanged original-equation acceptance.
- **Main reject signal:** external solver-policy validity remains bounded by a controlled
  three-family synthetic action portfolio and a narrow faithful HINTS configuration.
- **Confidence:** **5/5**, because the implementation, frozen model, raw observations,
  verifier thresholds, generated manuscript values, and clean-tree rerun are inspectable.

This score is solver-only. Distributed high availability, cross-host failover, process
isolation, dedicated physical hosts, remote replication, and service-level safe
switching are outside scope and cannot raise or lower the score.

## 6. Strengths And Weaknesses

### Strengths

- The paper's central object is unambiguously a numerical solver cascade. `gate` means
  original-equation numerical acceptance, and `fallback` means continuing with another
  numerical solve path after rejection.
- Equation (1) charges stage reach probability, setup, transfer, correction, gate,
  rejection continuation, and terminal fallback instead of reporting candidate latency.
- The request-conditioned path is production code rather than a post-hoc notebook:
  `RequestConditionedRoutingModel`, training, prediction, validation, and Runtime Router
  integration are exposed in `include/smave/routing.hpp:69` and `src/routing.cpp:1189`.
- The 12-action study uses 192 training, 96 disjoint calibration, and 192 disjoint
  held-out requests, yielding 2,304/1,152/2,304 action observations with three Runtime
  measurements per action/request.
- Held-out conditioned regret is `1.178×` realized exhaustive-oracle cost versus `1.451×`
  for both static-profile and fixed-action controls. The Router forms 44 distinct plans
  and changes plan on 92.2% of requests relative to the modal feature configuration.
- The exact bounded DP has zero mismatches against independent exhaustive cascade
  enumeration. The production run has 192 successes, zero failures, zero gate mismatch,
  and explicit preservation of the original-equation gate and terminal fallback.
- Pass prediction is well calibrated on the controlled study: Brier `0.0076`, ECE
  `0.0197`, and maximum per-action calibration error `0.0081`.
- The official HINTS code, architecture, pretrained weights, full 750-case test set, and
  published schedule are executed on the same exported equations and common FP64
  residual contract.
- Positive performance claims use paired statistics and bootstrap intervals, while
  topology shift, device regressions, no-common-success cases, and incomplete complete-
  path scaling are retained.
- The deterministic archive is byte-stable. Its clean extraction passes 29/29 CTests,
  all routing targets including the 12-action study, evidence/manifest checks, and the
  12-page PDF rebuild; the final archive digest is recorded outside the archive.

### Weaknesses

**Weakness:** The request-conditioned action portfolio remains controlled and synthetic.

- **Evidence basis:** three 12-variable quadratic/cubic/quartic nonlinear families,
  three experts, four budgets, and four declared context features.
- **Reviewer deduction:** the study convincingly establishes the mechanism and closes the
  static-input gap, but it does not yet show that request features predict complete-cost
  decisions across a realistic public portfolio of materially different solvers.
- **Required fix:** execute the same disjoint fit/calibration/held-out and realized-oracle
  protocol on a public workload with multiple genuinely distinct solver families and
  correction budgets.

**Weakness:** Cost-prediction tails are substantially weaker than the median.

- **Evidence basis:** median relative cost error is `0.225`, but p95 is `1.352` and the
  maximum is `2.515`; the main manuscript reports the median and policy regret.
- **Reviewer deduction:** low policy regret shows that decision quality survives these
  errors on the evaluated distribution, but the action-cost model is not uniformly
  accurate and could be brittle under a harder portfolio or shift.
- **Required fix:** report the tail errors adjacent to policy regret and evaluate whether
  uncertainty-aware, quantile, or heteroscedastic cost prediction improves worst-case
  route regret without changing the exact optimizer.

**Weakness:** The faithful HINTS comparison remains structurally narrow.

- **Evidence basis:** all 750 official cases are 29-interior-unknown 1D Poisson systems,
  and the production Router selects a direct tridiagonal expert.
- **Reviewer deduction:** this is a valid complete-cost selection result and a faithful
  baseline execution, but it cannot support general claims over neural/classical hybrid
  PDE solvers.
- **Required fix:** add a higher-dimensional or less structurally trivial published
  hybrid workload, or execute another closest public method when a faithful artifact is
  available.

**Weakness:** The bounded policy assumes order-invariant action statistics and does not
model expert interactions.

- **Evidence basis:** `paper/sections/09_discussion_limitations.tex:37` explicitly limits
  the exact policy to order-invariant actions.
- **Reviewer deduction:** exactness is correctly scoped to the finite stated model, but
  warm-cache, shared-correction, correlated-failure, or stateful expert interactions may
  change optimal ordering in realistic portfolios.
- **Required fix:** retain the boundary now; for broader claims, add an interaction-aware
  state model and compare its value against the current exact independent-action policy.

**Claim boundary:** authoritative timing remains platform-specific.

- **Evidence basis:** `paper/sections/09_discussion_limitations.tex:19` states that all
  timing comes from one Apple M4.
- **Reviewer deduction:** this limits performance transfer but not numerical correctness
  or the scientific validity of the solver mechanisms on the measured platform.
- **Required fix:** preserve platform-specific wording. Additional architecture results
  are optional validation, not a separate research contribution or score gate.

## 7. Potentially Missing Related Work

### Greedy PDE Router

- **Status:** searched; not locally executed.
- **Why relevant:** it is the closest named iteration-level neural/classical routing
  mechanism.
- **Overlap:** both choose among numerical actions using request/iteration state.
- **Needed comparison:** preserve the current objective-and-granularity distinction;
  execute it only if a faithful public artifact and compatible workload are available.

### HINTS And FCG-NO

- **Status:** searched; HINTS executed, FCG-NO positioned from the primary record.
- **Why relevant:** both combine learned components with classical iterative solving.
- **Overlap:** hybrid learned/classical acceleration under numerical residual evaluation.
- **Needed comparison:** keep the paper's statement that SMAVE contributes request-level
  complete-cost composition rather than a universally stronger corrector.

### Classical Solver Portfolios And Algorithm Selection

- **Status:** searched and cited at the category level.
- **Why relevant:** the Router is an algorithm-selection mechanism over solver actions.
- **Overlap:** cost-sensitive selection and portfolios.
- **Needed comparison:** emphasize that the differential contribution is typed cascades,
  original-equation acceptance, correction budgets, and reach-weighted complete cost.

No newly identified citation omission is fatal. The remaining issue is stronger
experimental comparison, not a missing bibliography entry.

## 8. Claim-Evidence Audit

| Claim | Where stated | Evidence provided | Strength | Reviewer deduction | Required fix |
| --- | --- | --- | --- | --- | --- |
| Complete verified runtime is the routing objective | `paper/sections/03_problem_formulation.tex:19` | Complete-cost equation, decomposition evidence, paired runtimes | Strong | Candidate-only speed is not substituted for solver speed | None |
| Fixed order-invariant cascades follow cost per acceptance | `paper/sections/03_problem_formulation.tex:60` | All 24 permutations of four stages | Strong | Correct for the stated assumptions | Preserve assumptions |
| Exact finite-action expert--budget routing | `paper/sections/03_problem_formulation.tex:78` | Exact DP and independent exhaustive enumeration | Strong | Exactness is credible for the bounded action model | Do not generalize to interacting actions |
| Request-conditioned cost/pass prediction improves policy | `paper/abstract.tex:14`; `paper/sections/07_evaluation.tex:132` | 192/96/192 split, 5,760 observations, frozen model, regret `1.178×` vs `1.451×` | Strong but controlled | The Round 35 static-input gap is closed | Add realistic public portfolio |
| Every returned result passes original-equation acceptance | `paper/sections/03_problem_formulation.tex:119` | Family gates, zero conditioned gate mismatch, negative-path tests | Strong | Prediction does not determine correctness | None |
| Seven PDEBench-derived workloads improve complete runtime | `paper/sections/07_evaluation.tex:29` | 30 paired runs/family and bootstrap intervals | Strong on one host | Valid measured-platform claim | Preserve platform boundary |
| Official HINTS comparison is faithful | `paper/sections/07_evaluation.tex:193` | Official code/weights/schedule, 750 cases, common equations/residual | Strong but narrow | Valid for one 1D Poisson configuration | Add harder hybrid only for broader significance |
| Gate scaling is not complete-solver scaling | `paper/sections/07_evaluation.tex:244` | Gate-only and complete-path scaling separated | Strong | Parallel claims are appropriately bounded | None |

## 9. Experiment / Benchmark / Reproducibility Audit

- **Baselines:** strong relative to the bounded claims: classical family baselines,
  shared hybrid control, official native HINTS, fixed action, and static profile. The
  remaining closest-method gap is Greedy PDE Router or an equivalent public router.
- **Ablations:** correction budgets, complete-cost layers, ordering, routing shifts,
  gate architecture, batching, placement, and negative devices are present.
- **Datasets/benchmarks:** broad interface and equation-family coverage is present, but
  the new statistical policy evidence is synthetic and the faithful hybrid baseline is
  small.
- **Metrics:** complete wall time, residual/gate status, acceptance, regret, Brier/ECE,
  plan diversity, bootstrap intervals, and break-even are decision-relevant. Adding p95
  and maximum cost prediction error to the manuscript would improve transparency.
- **Statistical rigor:** disjoint train/calibration/held-out splits, repeated action
  timing, paired estimators, and exhaustive realized oracle are strong. The synthetic
  generator remains the main external-validity limit.
- **Robustness/failure cases:** topology and conditioning shifts, erroneous candidates,
  rejected device paths, no-common-success cases, and fallback preservation are retained.
- **Implementation details:** the feature contract, normalization, ridge/logistic models,
  validation, and DP integration are production C++ APIs with strict negative tests.
- **Artifacts and reproducibility:** source, raw observations, frozen model, generated
  values, manifest, and clean-tree verifier align. The core archive intentionally omits
  large payloads and external HINTS dependencies; this bounds convenience rather than
  changing the scientific claims.
- **Limitations:** the manuscript states the controlled portfolio, one-host timing,
  order-invariant model, small HINTS workload, and unexecuted Greedy Router boundaries.

## 10. Multi-Reviewer Panel

### Reviewer 1

- **Expertise:** numerical algorithms and solver composition.
- **Likely score:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** complete-cost cascade formulation and mandatory original-
  equation acceptance are coherent and well evidenced.
- **Main negative signal:** action independence limits exactness in stateful portfolios.
- **Evidence basis:** formulation, 24-permutation check, exact DP/exhaustive agreement.
- **Score-change condition:** interaction-aware evidence would strengthen the generality
  of the optimizer but is not required for the current bounded claim.

### Reviewer 2

- **Expertise:** machine learning for algorithm selection.
- **Likely score:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** Round 36 closes the request-conditioned prediction gap with
  disjoint calibration and held-out regret through the production Router.
- **Main negative signal:** cost prediction has heavy relative-error tails and only four
  synthetic context features.
- **Evidence basis:** Brier/ECE, cost errors, plan diversity, held-out regret.
- **Score-change condition:** realistic portfolio calibration and OOD regret would support
  movement toward 10.

### Reviewer 3

- **Expertise:** PDE and hybrid learned/classical solvers.
- **Likely score:** 8/10.
- **Confidence:** 4/5.
- **Main positive signal:** HINTS is executed faithfully under a common equation and
  residual contract.
- **Main negative signal:** the 29-unknown 1D Poisson structure favors a direct expert and
  does not stress hybrid learned iteration.
- **Evidence basis:** native HINTS evidence and manuscript limitation language.
- **Score-change condition:** a harder published hybrid configuration would raise this
  reviewer's score.

### Reviewer 4

- **Expertise:** parallel and heterogeneous systems.
- **Likely score:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** gate-only scaling, complete-path scaling, batching, residency,
  and transfer are separated rather than conflated.
- **Main negative signal:** complete-path Navier--Stokes scaling remains modest and some
  placement/offload paths regress.
- **Evidence basis:** evaluation and retained negative results.
- **Score-change condition:** stronger complete-path parallelism on a harder workload
  would improve significance.

### Reviewer 5

- **Expertise:** experimental methodology and statistics.
- **Likely score:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** the conditioned study has disjoint splits, repeated action
  measurements, realized exhaustive oracle, and explicit controls.
- **Main negative signal:** the action generator is controlled, so confidence intervals
  do not imply real-portfolio transfer.
- **Evidence basis:** observation TSV, evidence contract, verifier thresholds.
- **Score-change condition:** preregistered public-portfolio held-out evaluation would
  strengthen external validity.

### Reviewer 6

- **Expertise:** artifacts and reproducibility.
- **Likely score:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** byte-stable archive, internal manifest, clean build, 29/29
  tests, rerun routing evidence, manuscript checks, and PDF rebuild.
- **Main negative signal:** large payloads and external HINTS execution are not embedded.
- **Evidence basis:** `build/core-repro-bundle/clean-tree-evidence.txt`.
- **Score-change condition:** none for solver science; payload-complete packaging would
  improve reproduction convenience only.

### Reviewer 7

- **Expertise:** technical writing and presentation.
- **Likely score:** 9/10.
- **Confidence:** 5/5.
- **Main positive signal:** the abstract, method, evaluation, and limitations use stable
  numerical meanings for gate, fallback, complete cost, and request conditioning.
- **Main negative signal:** the main text omits the large p95/maximum cost-prediction
  errors, and the method remains dense.
- **Evidence basis:** 12-page source, generated macros, evidence file.
- **Score-change condition:** one compact sentence on prediction tails would remove the
  main reporting risk.

### Reviewer 8

- **Expertise:** broad systems reader / novice advocate.
- **Likely score:** 9/10.
- **Confidence:** 4/5.
- **Main positive signal:** the paper answers a clear practical question: which complete
  verified solver path should run for this request?
- **Main negative signal:** the number of mechanisms and benchmark families can obscure
  which result establishes the central claim.
- **Evidence basis:** abstract, RQs, contribution list, and evaluation organization.
- **Score-change condition:** keep the 12-action result adjacent to the exact-DP claim and
  avoid expanding peripheral compatibility material.

### Panel Synthesis

- **Agreement:** all reviewers accept the bounded numerical correctness contract and the
  exact finite-action evidence; most view the request-conditioned result as closing the
  main Round 35 methodological gap.
- **Disagreement:** the PDE reviewer assigns more weight to the narrow HINTS workload;
  artifact and method reviewers assign more weight to the complete-cost formulation and
  clean evidence chain.
- **Decisive positive axis:** production request-conditioned action prediction plus exact
  complete-cost optimization under unchanged original-equation acceptance.
- **Decisive negative axis:** realistic public policy breadth and harder closest-method
  evidence.
- **Unresolved evidence:** public multi-expert/multi-budget regret, prediction-tail
  robustness, action interactions, and a harder faithful hybrid comparison.
- **AC stance:** 9/10 strong accept; the paper is stronger than Round 35 but the new result
  closes a prior gap rather than eliminating the remaining external-validity limits.

## 11. Concerns Table

| ID | Severity | Concern | Evidence basis | Affected criterion | Fix class | Required action | Owner skill | Score-change condition |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| C1 | Major | Controlled synthetic request-conditioned portfolio | Three 12-variable families and 12 actions | Significance / evidence | Experiment | Run disjoint held-out policy regret on a realistic public solver portfolio | Solver experiment design | Necessary component for 10 |
| C2 | Moderate | Cost-prediction tail error | Median `0.225`, p95 `1.352`, max `2.515` | Soundness / evidence / clarity | Method and writing | Report tails and evaluate uncertainty-aware prediction or worst-case regret | Routing model + paper writing | Could strengthen 9 and support 10 |
| C3 | Moderate | Faithful HINTS baseline is structurally narrow | 29-unknown 1D Poisson | Significance | Experiment | Add harder published hybrid workload if feasible | Solver baseline experiment | Supports movement toward 10 |
| C4 | Moderate | Closest learned Router is not executed | Greedy PDE Router discussed only | Positioning / evidence | Related-work and experiment | Execute a faithful public artifact if available; otherwise retain limitation | Literature/baseline owner | Supports movement toward 10 |
| C5 | Minor | Action interactions are outside the exact model | Order-invariant assumption | Generality | Method/soundness | Preserve scope or add interaction-aware state study | Routing method | No quick score change |
| C6 | Minor | Author block is unfinished | `paper/authors.tex` placeholder | Submission compliance | Writing | Replace or blind before submission | Authors | No scientific score change |

## 12. AC / Meta-Review

- **Reviewer consensus:** the paper presents a coherent solver contribution with unusually
  strong claim-to-machine-evidence linkage. The numerical acceptance contract is clear,
  and the request-conditioned study is methodologically stronger than the static-profile
  evidence available in Round 35.
- **Reviewer disagreement:** reviewers differ mainly on how heavily to penalize the
  controlled action portfolio and small HINTS configuration, not on correctness.
- **Decisive acceptance axis:** complete verified runtime is formulated, optimized, and
  measured consistently; request-conditioned cost/pass predictions now drive the exact
  production optimizer and outperform both controls on disjoint held-out requests.
- **Decisive rejection axis:** there is no fatal correctness flaw. The strongest negative
  signal is limited external validity for policy learning and closest hybrid comparisons.
- **AC stance:** **9/10, strong accept, confidence 5/5**.
- **Discussion risks:** reviewers may overread the controlled 12-action result as a broad
  solver-portfolio learning claim or overread the HINTS result as hybrid-solver
  dominance. The limitation language must remain adjacent to both results.

## 13. Quantitative Scores

### Scorecard

| Dimension | Score (1-5) | Confidence (1-5) | Evidence basis | Deduction / score-change condition |
|:---|:---:|:---:|:---|:---|
| Novelty | 5 | 5 | Complete-cost typed cascade, exact expert--budget DP, request-conditioned production routing | No material deduction within the bounded claim |
| Soundness | 5 | 5 | Formal objective, exact enumeration checks, strict model validation, zero gate mismatch | Interaction assumptions and cost tails must remain explicit |
| Evidence | 5 | 5 | Paired statistics, disjoint splits, raw action observations, strong controls, negative results | Realistic public policy portfolio needed for award-level breadth |
| Significance | 4 | 5 | Broad solver pipeline and workload coverage | Controlled policy study and small faithful hybrid workload prevent 5/5 |
| Clarity | 5 | 5 | Stable terminology, RQs, bounded conclusion, claim ledger | Add prediction-tail sentence if space permits |
| Reproducibility | 4 | 5 | Deterministic bundle, manifest, 29/29 tests, clean evidence and PDF rebuild | Large payloads and external HINTS environment are not packaged |
| Ethics / Limitations | 5 | 5 | Explicit negative results, bounded claims, platform and model limits | No material deduction |

**Overall:** 9/10  | **Scholarly Confidence:** 5/5

**Recommendation:** strong accept

**Verdict:** A realistic public multi-expert/multi-budget held-out policy study plus a
harder faithful hybrid comparison could justify 10/10. Evidence leakage, DP-oracle
mismatch, gate mismatch, or broader-than-evidence wording would lower the score.

| Change | Condition | Likely affected dimensions | Expected movement |
| --- | --- | --- | --- |
| Raise score | Public realistic action portfolio with disjoint calibration/held-out regret and decision-relevant oracle, plus stronger closest-hybrid evidence | Significance, Evidence | +1 overall possible |
| Raise dimension | Report and reduce prediction-tail error under shift | Soundness, Evidence, Clarity | Dimension-only or supports +1 package |
| Lower score | Held-out leakage, stale evidence, gate mismatch, hidden failure cost, or unsupported generalization | Soundness, Evidence, Ethics | -1 or fatal depending on severity |
| No quick change | General interaction-aware policy across stateful experts | Novelty, Significance | Requires new method/evidence |

### Writing Scorecard

| Dimension | Score (1-5) | Confidence | Evidence basis | Concrete repair |
| --- | ---: | ---: | --- | --- |
| Storyline and motivation | 5 | 5 | Candidate-only latency to complete-cost problem is explicit | None |
| Contribution display | 5 | 5 | Exact DP, conditioned routing, gate contract, and evidence are visible | Keep solver-only scope |
| Paragraph logic | 5 | 5 | Problem, method, RQs, and limitations have distinct roles | None |
| Claim-evidence alignment | 5 | 5 | Generated macros and ledger match latest evidence | Preserve automated checks |
| Method readability | 4 | 5 | Dense but technically ordered | Optional compact predictor/DP pseudocode |
| Experiment narration | 5 | 5 | Positive and negative results state what they establish | Add cost-tail sentence |
| Related-work positioning | 5 | 5 | HINTS, Greedy Router, FCG-NO, and portfolios separated by technical axis | Retain distinctions |
| Terminology consistency | 5 | 5 | Gate and fallback remain numerical | None |
| LaTeX and format discipline | 5 | 5 | 12 pages, no overfull boxes or undefined references | Complete/blind author block |
| Reviewer-facing risk | 4 | 5 | Synthetic policy and 1D HINTS results are easy to overgeneralize | Keep caveats adjacent |

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
- **Score-change conditions:** the Round 35 request-conditioned prediction condition is
  closed. Movement to 10 now depends on realistic solver-policy breadth and harder
  closest-method evidence, not additional non-solver infrastructure.

## 14. Questions For Authors

1. Which public workload can expose multiple materially different experts and multiple
   correction budgets while permitting a trustworthy held-out realized oracle or a
   tightly bounded approximation?
2. Why do p95 and maximum cost-prediction relative errors remain high despite strong
   policy regret, and which uncertainty model best predicts when route ranking is fragile?
3. Can the official hybrid comparison be extended beyond the 29-unknown 1D Poisson case,
   or can the Greedy PDE Router be executed faithfully on a compatible public workload?
4. Which realistic expert interactions violate order invariance first: shared setup,
   warm caches, correlated failure, or state changes after correction?

## 15. Score Revision Criteria

### Raising the score would require

- held-out request-conditioned regret on a realistic public multi-expert, multi-budget
  solver portfolio under the same original-equation acceptance contract;
- preferably, a harder faithful published hybrid workload or an executed closest learned
  Router;
- evidence that cost-prediction tails or action interactions do not erase the policy gain
  under the harder workload.

### Lowering the score would be triggered by

- any train/calibration/held-out leakage in the conditioned experiment;
- a DP/exhaustive mismatch, gate mismatch, erroneous acceptance, omitted continuation
  cost, or removable terminal fallback;
- wording that generalizes the controlled policy study or 1D HINTS result beyond its
  evidence;
- stale generated macros, claim ledger, artifact manifest, or clean-tree verification.

### Concerns unlikely to change before submission

- action-interaction modeling beyond the current order-invariant exact policy;
- broad public solver-portfolio coverage if no compatible benchmark and oracle are ready;
- execution of a closest method whose faithful public artifact is unavailable.

## 16. Action Plan And CCFA Handoffs

### Priority 1

- **Action:** design a realistic public request-conditioned expert--budget portfolio with
  disjoint training/calibration/held-out requests and a decision-relevant oracle.
- **Owner skill:** solver experiment design.
- **Input needed:** public workload, eligible experts, budgets, gate contract, and timing
  protocol.
- **Expected output:** held-out regret, calibration, plan diversity, failures, and complete
  Runtime traces.
- **Handoff required:** no for this review; yes before implementing the next experiment.

### Priority 2

- **Action:** analyze and reduce cost-prediction tail error without weakening the exact DP
  or original-equation gate.
- **Owner skill:** routing model implementation and experiment design.
- **Input needed:** current TSV/model plus harder shifted requests.
- **Expected output:** p50/p95/max cost error, rank error, uncertainty calibration, and
  policy regret under shift.
- **Handoff required:** no for this review.

### Priority 3

- **Action:** add one harder faithful published hybrid baseline or execute the closest
  public learned Router when technically possible.
- **Owner skill:** literature search and solver baseline experiment.
- **Input needed:** primary method specification, official code/weights, workload, and
  common original-equation acceptance rule.
- **Expected output:** same-equation paired complete-path comparison and bounded claim.
- **Handoff required:** yes before external artifact acquisition.

### Priority 4

- **Action:** add a compact main-text disclosure of p95/maximum cost-prediction error if it
  fits without weakening the current 12-page presentation.
- **Owner skill:** paper writing.
- **Input needed:** frozen Round 36 evidence values.
- **Expected output:** one bounded sentence adjacent to conditioned regret.
- **Handoff required:** no.

- **Checks run:** Release configure/build; request-conditioned target; 29/29 CTests;
  Python compilation; paper evidence and artifact-manifest checks; 12-page LaTeX build;
  no undefined references or overfull boxes; deterministic core-bundle creation; clean
  extracted-tree build/test/evidence/PDF verification; scoped hidden-instruction scan.
- **Checks skipped:** no new Greedy PDE Router execution, no harder HINTS configuration,
  and no exhaustive census of all 2026 solver preprints.
- **Unresolved risks:** realistic policy breadth, cost-prediction tails, order-dependent
  expert interactions, and harder closest-method coverage.

## Output Self-Check

- Section order follows the standard full-review contract.
- Every score of 4 or below has a concrete deduction and repair condition.
- The previous request-conditioned prediction gap is marked closed rather than repeated.
- No result, baseline, independent rerun, or performance claim is invented.
- Score conditions are limited to solver mechanisms, numerical evidence, and claim scope.
