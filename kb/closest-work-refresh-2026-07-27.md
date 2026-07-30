# Closest-Work Refresh — 2026-07-27

## Scope and privacy boundary

This audit uses public, method-level queries only. It does not upload manuscript
text, unpublished measurements, cohort identities, or artifact contents.

The refresh targets three claim families:

1. machine-learned selection of sparse linear solvers and preconditioners;
2. online or request-dependent tuning across related linear systems; and
3. residual-monitored hybrid neural--numerical PDE solvers.

## Sources and execution

- Primary metadata: Crossref REST, arXiv API, DBLP, and publisher DOI records.
- Secondary discovery and cross-check: OpenAlex.
- The academic-search preflight passed for Crossref, arXiv, and PubMed.
- The bundled OpenAlex fallback script required Python 3.10+; it ran under
  Python 3.11 but returned low-relevance broad-query results, so exact-title and
  DOI lookups were used for the included records.
- The dedicated Semantic Scholar endpoint rate-limited most requests. No claim
  below depends on Semantic Scholar metadata.

Public-safe query concepts included:

- `sparse linear solver selection right hand side SuiteSparse machine learning`
- `automated linear solver selection machine learning multiphysics`
- `iterative solver preconditioner selection sparse matrix neural network`
- `learning solver parameters sequence linear system instances`
- `hybrid neural PDE solver residual reliability correction`

## Included closest work

| Work | Verified scope | Relationship to SMAVE | Positioning consequence |
| --- | --- | --- | --- |
| Bhowmick, Toth, and Raghavan (2009), *Towards Low-Cost, High-Accuracy Classifiers for Linear Solver Selection*, DOI `10.1007/978-3-642-01970-8_45` | Low-cost learned classifiers for choosing linear solvers | Establishes that learned sparse-solver selection is longstanding | Do not claim novelty for solver classification itself |
| Jessup et al. (2016), *Performance-Based Numerical Solver Selection in the Lighthouse Framework*, DOI `10.1137/15M1028406` | Taxonomy- and ML-based classification among PETSc/Trilinos sparse solvers | Closely matches numerical software discovery and performance-based selection | Position SMAVE around typed cascades, original-equation acceptance, and complete rejected-path cost |
| Eller, Cheng, and Maier (2012), *Dynamic Linear Solver Selection for Transient Simulations Using Multi-label Classifiers*, DOI `10.1016/j.procs.2012.04.167` | Per-system dynamic selection in transient sequences with low-cost attributes | Precedes request-varying solver selection across one simulation | Do not claim novelty for dynamic per-system selection |
| Khodak et al. (ICLR 2024), *Learning to Relax: Setting Solver Parameters Across a Sequence of Linear System Instances*, arXiv `2310.02246` | Bandit parameter tuning across related systems, with fixed-parameter and contextual regret guarantees for SOR | Strong theoretical comparator for sequential adaptation | Distinguish SMAVE's finite exact cascade optimizer and acceptance contract from online SOR parameter learning; do not imply broader theory |
| Zabegaev et al. (2024), *Automated Linear Solver Selection for Simulation of Multiphysics Processes in Porous Media*, DOI `10.1016/j.cma.2024.117031` | ML selection/tuning and solver switching as simulated physics changes | Close application-level dynamic selector | Cite as prior request-/state-dependent solver switching |
| Zabegaev, Berre, and Keilegavlen (2026), *Data-Driven Linear Solver Selection and Performance Tuning for Multiphysics Simulations in Porous Media*, DOI `10.1007/s44207-026-00013-y` | Online collection of run-local performance data and continuously updated selection policy | Closest online adaptation work in repeated multiphysics solves | SMAVE's final evidence is frozen offline routing with no online tuning; this is a difference, not superiority |
| Xiong et al. (2025), *MM-AutoSolver*, DOI `10.1016/j.jpdc.2025.105144` | Multimodal ML selection of iterative solvers and preconditioners | Directly overlaps learned solver/preconditioner selection | Novelty must rest on verified cascades and complete-path objective, not multimodal selection |
| Weng, Bungartz, and Dietrich (ICCS 2026), *Embedding-Based Methods for Linear Solver Performance Prediction*, DOI `10.1007/978-3-032-29921-5_32` | Learned embeddings predict performance over large PETSc configuration spaces and unseen matrices | Close unseen-matrix generalization comparator | Report SMAVE's mixed held-out result and small sample explicitly |
| Zhang et al. (2026), *SPECTRA: Revitalizing Image-Based Iterative Method Selection for Sparse Linear Systems*, DOI `10.1016/j.eswa.2026.132050` | Image-based iterative-method selection, including matrix/right-hand-side information and variable-RHS evaluation | Most direct recent comparator to request-conditioned sparse routing | State that request conditioning and unseen SuiteSparse selection are not unique to SMAVE |
| Zhang et al. (2024), HINTS, DOI `10.1038/s42256-024-00910-x` | Iteration-level neural-operator/relaxation blending | Close hybrid PDE composition baseline | Distinguish request-level typed pipeline selection and mandatory result acceptance |
| Rayan, Patel, and Tewari (2025/updated 2026), *A Greedy PDE Router*, arXiv `2509.24814` | Approximate greedy iteration-level routing among PDE solvers | Directly uses “router” for hybrid solver selection | Do not claim the solver-router concept; distinguish granularity and objective |
| Roy, Nayak, and Goswami (2025/updated 2026), ANCHOR, arXiv `2512.19643` | Residual-monitored adaptive classical correction for neural-operator rollouts | Close online monitor-and-correct mechanism | Distinguish trigger/correction from mandatory final acceptance and terminal fallback |
| Wu et al. (2026), *Are Deep Learning Based Hybrid PDE Solvers Reliable?*, DOI `10.1109/MCSE.2026.3696587` | Reliability analysis of HINTS-style hybrids under training/update choices | Reinforces the need to scope hybrid reliability claims | Cite as evidence that composition and residual behavior require adversarial evaluation |
| Jiang et al. (2026), *Error-Conditioned Neural Solvers*, arXiv `2606.27354` | Uses residual fields as learned correction inputs and warns that low residual may not imply low reconstruction error in ill-conditioned systems | Important distinction between an acceptance contract and an error surrogate | State explicitly that SMAVE certifies its declared equation tolerance, not unknown ground-truth solution error |

## Claim closure

The refreshed literature does **not** support claiming novelty for:

- learned or dynamic sparse linear-solver selection;
- request/right-hand-side-conditioned selection;
- online adaptation across related linear systems;
- solver routers or residual-triggered neural--classical correction; or
- unseen-matrix performance prediction.

The defensible contribution boundary is the combination of:

- typed candidate--corrector--gate--continuation pipelines;
- a reach-weighted complete-path objective that explicitly charges transfer,
  correction, mandatory original-equation acceptance, rejection, later attempts,
  and terminal fallback;
- exact optimization of the finite cascade under the declared model; and
- an implementation invariant in which routing can change cost but cannot bypass
  the original-equation acceptance contract.

This audit does not establish uniqueness or exhaustive absence of another method
with the same combination. The manuscript should say that the searched closest
work optimizes selections, parameters, iterations, or correction triggers, while
SMAVE studies verified complete-path cascade selection. It should not say “the
first” unless a separate systematic review establishes that claim.

## Evidence implications

- V5 remains a negative held-out result: the frozen control-aware replay switches
  zero requests and is `1.7183709022238944` times global fixed.
- V6 is one favorable untouched cohort: regret `1.0000307937350952`, only `0.109%`
  below global fixed and `43.1%` below static.
- The paper must describe mixed external validity and must not present v6 as a
  general repair of v5.
