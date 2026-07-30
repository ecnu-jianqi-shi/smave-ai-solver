# CCF-A Integrity Audit — Round 49

## Mode

Full claim, numeric, citation, terminology, figure/table, theorem, and
artifact-consistency audit for the 2026-07-28 TPDS manuscript state.

## Artifacts Checked

- `paper/main.tex`, `paper/abstract.tex`, all section and figure sources, generated
  value macros, and the rebuilt `paper/main.pdf`.
- `paper/CLAIM_EVIDENCE.md`, `paper/ARTIFACT_SNAPSHOT.md`, `paper/REVIEW.md`, and
  `paper/ARTIFACT_MANIFEST.txt`.
- `include/smave/routing.hpp`, `src/routing.cpp`,
  `tests/joint_route_budget_evidence.cpp`, and the joint-routing evidence verifier.
- Frozen SuiteSparse v4/v5/v6 records, zero-execution v5 replay, request-conditioned
  routing evidence, complete-cost evidence, scaling evidence, native HINTS evidence,
  portability evidence, and data-lock records.
- `paper/references.bib`, all manuscript citation keys, DOI metadata for the newly
  cited ordering paper, and the public-safe closest-work record.
- Release configure/build, 29 CTests, focused evidence reproduction, paper evidence
  checks, and direct LaTeX diagnostics.

## Claim-Evidence Matrix

| Claim family | Status | Evidence basis | Scope check |
| --- | --- | --- | --- |
| Complete-path cascade objective | Supported | Problem formulation, complete-cost decomposition, routing traces, and generated values | Includes rejection and terminal continuation; not raw-kernel cost |
| Fixed-cascade cost-per-acceptance ordering | Supported | Adjacent-exchange derivation and exhaustive 24-permutation probe | Explicitly prior art and restricted to order-invariant statistics |
| Exact finite expert--budget recurrence | Supported | Proposition `prop:exact-finite-cascade`, production optimizer, exhaustive oracle, and 256 deterministic property cases | Exact only for finite history-independent action/suffix statistics with one action per expert |
| Recurrence state bound | Supported | States are `(index, used-expert set, capacity)`; terminal index included in `(n+1)(k+1)2^m` bound | Bound is for scalar values/backpointers, not a claim that the current `std::map` implementation has constant wall-clock overhead |
| Implementation limit behavior | Supported | 64-bit expert mask, 63-expert guard, and explicit maximum-state exception | Proposition applies only when the state cap admits all reached states; overflow rejects rather than truncates |
| Verified-result return invariant | Supported | Proposition, production Runtime paths, gate records, and negative/rejection evidence | Control-flow invariant under the declared callback/hardware contract, not formal verification of arbitrary external code |
| V5 negative replay | Supported | Immutable v5 first-run inputs and byte-checked zero-execution replay | Counterfactual decision replay only; no solver re-execution or repaired-performance claim |
| V6 near-oracle final cohort | Supported | Prefrozen v6 contracts, unique first run, 24 traces, and frozen verifier | Three matrices and 24 requests on one Apple M4; not population-level adaptation |
| Performance, scaling, and external baselines | Supported | Paired timing, intervals, controls, HINTS native evidence, and retained failures | Workload-qualified and host-qualified; no universal or cross-platform claim |
| Artifact integrity | Supported at source/release level | Current release checks pass; the deterministic archive refresh is sequenced after this report | Clean-tree evidence is authoritative for the post-review archive step |

## Theorem And Proof Findings

- The adjacent-exchange ordering is no longer presented as novel. Related work cites
  prior independent-test ordering with precedence constraints, and both the
  introduction and claim ledger identify the ratio rule as prior art.
- The proposition now requires finite nonnegative action and terminal costs,
  positive acceptance probabilities, and action/suffix statistics unchanged by
  previously rejected actions. These are the assumptions needed by the common-suffix
  exchange and backward-induction arguments.
- The empty cascade is represented by the terminal value and is therefore included
  in the global minimum rather than excluded by construction.
- The explicit state count was corrected from an off-by-one expression to
  `(n+1)(k+1)2^m`, accounting for the terminal action index. The manuscript retains
  the asymptotic `O(n k 2^m)` scalar-state and transition statement.
- The implementation boundary is explicit: at most 63 experts, exact execution only
  when the configured state cap is sufficient, and exception rather than approximate
  truncation on overflow.
- No theorem claim extends to interaction-aware costs, history-dependent acceptance,
  contract-fixed order, or action statistics changed by earlier attempts.

## Numeric Consistency Findings

- The focused joint-routing evidence records 256 six-action property cases, zero
  maximum exhaustive-oracle gap, expert exclusivity, and nondecreasing ordering.
- The production probe remains `4.5 us` versus `5.0 us` for the best single action,
  ratio `0.900`, with rejection of the first action, acceptance of the second after
  two iterations, and no terminal fallback.
- V5 remains unchanged: regret `4.4899717598`, fixed `2.6129235277`, ratio
  `1.7183709022`, and `0/24` decision switches with `solver_reexecution=0`.
- V6 remains unchanged: regret `1.0000307937`, fixed `1.0011229446`, static
  `1.7564647453`, raw conditioned regret `1.0452874360`, 24/24 successes, and zero
  gate/order/DP mismatch.
- The manuscript consistently reports the favorable v6 effect as only `0.109%`
  below fixed and preserves the adverse v5 result in evaluation, discussion, and
  conclusion.
- `python3 paper/check_evidence.py` passes after regenerating the joint-routing
  macros used in the methodology and ablation text.
- The final pre-archive PDF is 12 pages and 315,831 bytes with SHA-256
  `1100c41537c84878b291a15c39ce383c4e55bad15b29e0e80888a96bea9cb8f0`.

## Citation Metadata Findings

- All 20 cited keys exist; BibTeX reports no undefined citation or duplicate cited
  key.
- The new record `berend2014tests` resolves to DOI
  `10.1016/j.dam.2013.07.014`, *Discrete Applied Mathematics* 162 (2014), pages
  115--127, with title and author initials consistent with Crossref metadata.
- Existing Round 48 citation repairs remain intact, including the PETSc chapter DOI
  and the data-driven solver-selection article number.
- No new citation is inferred from title similarity alone; the ordering citation was
  checked against its stated independent-test/precedence scope.

## Citation-Context Findings

- The new citation supports the narrow statement that independent early-stop tests
  admit ratio-based ordering and that ordering remains studied under precedence
  constraints.
- The manuscript does not attribute SMAVE's cardinality and one-budget-per-expert
  recurrence to that paper, and it does not claim that recurrence as a first result.
- Existing HINTS, Greedy PDE Router, learned-preconditioner, request-conditioned
  solver-selection, and reliability citations remain within their verified contexts.
- No citation is used to support an unpublished performance number, broad consensus,
  or claim stronger than the cited source.

## Terminology, Figure, And Format Findings

- `gate`, `numerical continuation`, `terminal solver`, `fixed`, `static`, `raw`,
  `v5`, and `v6` remain consistent across the manuscript and claim ledger.
- The compressed discussion removes repetition without deleting the v5 failure,
  one-host limitation, small-cohort limitation, calibration weakness, or correctness
  boundary.
- The direct LaTeX build reports no undefined reference/citation, multiply defined
  label, or overfull box. The paper remains 12 pages after adding the theorem and
  prior-art citation.
- No hidden reviewer/LLM instruction was found in manuscript text, captions, or source
  comments.

## Severity

- **Fatal:** none.
- **Major integrity defects:** none.
- **Minor defects corrected in this round:** prior-art ambiguity, missing state-cap
  and 63-expert assumptions, property-sweep invisibility, and the terminal-index
  off-by-one in the explicit state count.
- **Unresolved non-integrity risks:** limited external validity, no independent public
  reproduction, and unavailable verified author/disclosure metadata.

## Safe Edit Suggestions

- Do not strengthen the theorem beyond the finite history-independent model.
- Do not describe the `K/p` ordering as new, first, or unique.
- Keep the v5 negative replay and the `0.109%` v6-vs-fixed effect visible.
- Replace author/disclosure placeholders only with verified metadata supplied by the
  authors.
- Regenerate the artifact manifest and deterministic bundle after this report; never
  rerun or overwrite frozen v4/v5/v6 first-run directories.

## Next CCFA Owner

- `ccf-paper-reviewer` for the Round 49 decision-level full review.
- Artifact/reproducibility workflow for final manifest, deterministic archive, and
  clean extracted-tree verification.
- Authors for identity, affiliation, funding, conflict, acknowledgment, and anonymity
  metadata.

## No-Invention Status

- No author identity, new experiment, benchmark outcome, citation, score change,
  acceptance probability, or independent-reproduction claim was invented.
- Public novelty checking used only method-level queries and did not expose private
  manuscript prose or unpublished results.
- The audit separates current verified evidence, prior Round 48 clean-bundle evidence,
  and the post-review Round 49 archive refresh.

## Checks Run

- Release configure/build and 29/29 CTests.
- `reproduce-joint-route-budget` and its independent exhaustive verifier.
- `python3 paper/check_evidence.py`.
- Direct `latexmk` rebuild and log scan.
- DOI metadata and citation-context verification for the new ordering citation.
- Theorem assumption, proof, complexity, code-limit, and claim-ledger cross-check.

## Output Self-Check

- Every central claim is tied to evidence and a stated scope boundary.
- All numerical values in this report come from verified local evidence or the rebuilt
  PDF.
- The report does not convert improved formalization into an unsupported 10/10 claim.
