# CCF-A Integrity Audit — Round 48

## Mode

Full claim, numeric, citation, terminology, figure/table, and artifact-consistency
audit for the 2026-07-27 TPDS manuscript state.

## Artifacts Checked

- `paper/main.tex`, `paper/abstract.tex`, all section and figure sources, and the
  generated value macros.
- `paper/CLAIM_EVIDENCE.md`, `paper/ARTIFACT_SNAPSHOT.md`, `paper/REVIEW.md`, and
  `paper/ARTIFACT_MANIFEST.txt`.
- Frozen SuiteSparse v5/v6 first-run evidence, the zero-execution v5 replay, routing
  observations/models/manifests, complete-cost evidence, scaling evidence, native
  HINTS evidence, portability evidence, and the 79-file data-lock record.
- `paper/references.bib`, every citation used by the manuscript, publisher/arXiv/PMLR
  metadata for the closest works, and the dated public-safe literature audit.
- Release build/CTest results, paper check logs, deterministic archive contract, and
  clean extracted-tree evidence.

## Claim-Evidence Matrix

| Claim family | Status | Evidence basis | Scope check |
| --- | --- | --- | --- |
| Complete-path cascade objective and return invariant | Supported | Problem formulation, exact finite optimizer implementation/tests, cascade-ordering exhaustive check, and original-equation gate traces | Presented as a tested control-flow and finite-model result, not universal formal verification |
| Optimal ordering for fixed eligible cascades | Supported | Proposition assumptions, four-stage exhaustive 24-permutation result, and exact optimum agreement | Restricted to order-invariant statistics and the declared cost model |
| Exact joint expert--budget selection | Supported | Independent exhaustive oracle, calibrated and shifted routing evidence, and zero DP mismatch | Restricted to finite actions and at most one budget per expert |
| V5 negative replay | Supported | Immutable v5 first-run inputs and byte-checked zero-execution replay | Explicitly counterfactual; no solver timing or repair claim |
| V6 near-oracle final cohort | Supported | Prefrozen v6 contracts, one first run, 24 production traces, and frozen verifier | Three held-out matrices and 24 requests on one Apple M4; not broad adaptation |
| Complete-cost bottleneck and budget findings | Supported | Two-family decomposition and rotated budget sweep | Family-specific; overlapping component timers are not treated as additive |
| PDE/operator/HINTS speedups | Supported | Generated macros, paired raw reports, baseline contracts, and evidence verifiers | Workload-qualified, one-host results with negative outcomes retained |
| Parallel and batch scaling | Supported | Paired worker/batch evidence with fixed thread controls and confidence intervals | Intra-node throughput/gate scaling; no cluster or strong-scaling claim |
| Data and artifact integrity | Supported | 79-file byte lock, deterministic archive generation, 29/29 clean CTests, frozen v6 verification, and v5 replay equivalence | Author-operated local reproduction; no public archive or independent rerun claim |
| Novelty boundary | Supported | Dated closest-work audit and citation contexts for HINTS, Greedy PDE Router, SPECTRA, data-driven solver selection, and Learning to Relax | No first claim; dynamic, RHS-conditioned, unseen-matrix, and online selection are acknowledged as prior art |

## Numeric Consistency Findings

- The v5 replay is consistent across evidence, generated macros, abstract, evaluation,
  discussion, conclusion, claim ledger, and review status: regret `4.4899717598`, fixed
  `2.6129235277`, ratio `1.7183709022`, and `0/24` switches with
  `solver_reexecution=0`.
- The v6 values are consistent across all locations: regret `1.0000307937`, fixed
  `1.0011229446`, static `1.7564647453`, raw conditioned `1.0452874360`, 24/24
  successes, eight terminal continuations, and zero failure/gate/order/DP mismatch.
- The reported `0.109%` fixed-control and `43.1%` static-control reductions reproduce
  the frozen regrets and are consistently described as narrow effect sizes.
- The data-lock totals agree: 66 system matrices plus six RHS files and seven PDEBench
  files, 79 files total; SuiteSparse bytes are `3,911,822,320`.
- The core-bundle contract now agrees with CTest usage: five included SuiteSparse test
  matrices and 61 excluded non-fixture systems. The clean extracted tree passes 29/29
  CTests and all downstream frozen checks.
- The PDF remains 12 pages and 311,604 bytes with no undefined citation/reference or
  overfull-box diagnostic in the checked log.

## Citation Metadata Findings

- All 19 cited keys exist; there are no duplicate keys or undefined citations.
- Crossref/publisher/PMLR/arXiv checks support the cited titles, years, authors, venues,
  and DOI/arXiv identifiers.
- One minor metadata defect was corrected: `zabegaev2026datadriven` now records article
  number/page `4` rather than treating it as issue `1`.
- The official DOI `10.1007/978-1-4612-1986-6_8` was added to the PETSc software-tools
  chapter entry.
- Seventeen uncited bibliography entries do not enter the rendered IEEE bibliography
  and create no citation-existence or context defect.

## Citation-Context Findings

- Classical solver citations support the stated numerical roles of GMRES, SuperLU,
  SUNDIALS, PETSc, and MUMPS.
- DeepONet, FNO, learned preconditioner, FCG-NO, HINTS, Greedy PDE Router, and
  Error-Conditioned Neural Solvers support the manuscript's learned/hybrid solver
  positioning.
- Rice, Kotthoff, Lighthouse, SPECTRA, the data-driven porous-media selector, and
  Learning to Relax support the algorithm-selection, request-feature, run-local
  adaptation, and sequential-tuning prior-art boundary.
- The manuscript does not attribute verified complete-path cascade optimization or the
  return invariant to those works and makes no unsupported exclusivity claim.

## Terminology and Figure/Table Consistency

- `raw conditioned`, `control-aware`, `training family-fixed`, `global fixed`,
  `static`, and `final policy` are distinguished consistently in the final-routing
  table and surrounding prose.
- V5 is consistently labeled a negative first run or zero-execution replay; v6 is
  consistently labeled the unique prefrozen first run and never a general repair.
- Table captions, text, generated macros, and evidence use the same metric direction:
  lower regret is better and speedup above one is favorable.
- No hidden reviewer instruction, prompt-injection text, private absolute path, or
  invented author identity appears in manuscript or artifact-facing sources.

## Severity

- Fatal: none.
- Major: none.
- Minor: two citation-metadata repairs completed in this round.
- Administrative: author, affiliation, correspondence, funding, conflict, and
  acknowledgment metadata remain unavailable and must be supplied by the authors.

## Safe Edit Suggestions

- Do not strengthen the v6 result beyond near-oracle control selection on one frozen
  cohort.
- Preserve the v5 negative replay and the `0.109%` fixed-control effect size.
- Regenerate the artifact manifest, PDF, and deterministic clean bundle after this
  report and the final panel report are added.
- Replace `paper/authors.tex` only with verified author-provided metadata.

## Next CCFA Owner

`ccf-paper-reviewer` for the Round 48 full panel and calibrated score decision.

## No-Invention Status

Pass. No result, citation, author identity, experiment, cohort, policy change, or
external validation was invented. Frozen v4--v6 first-run directories were not
modified or re-executed.
