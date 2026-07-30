# CCF-A Integrity Audit — Round 52

## Mode

Full claim, numeric, terminology, implementation, citation-context, and artifact audit
for the 2026-07-29 TPDS manuscript state. Round 52 is a post-hoc diagnostic over
immutable v5/v6 observations, not a new performance experiment.

## Artifacts Checked

- Main manuscript sources, generated macros, references, and the synchronized 12-page
  `paper/main.pdf`.
- `paper/CLAIM_EVIDENCE.md`, `paper/ARTIFACT_SNAPSHOT.md`, `paper/README.md`, and the
  core-bundle documentation and allowlist.
- The Round 52 analysis contract, deterministic parser, CMake target, verifier,
  request/pair tables, generated evidence, and manuscript evidence checker.
- Frozen v5/v6 `action-observations.tsv`, `evidence.txt`, and empty
  `conditional-cost-observations.tsv` inputs; no first-run directory was overwritten.
- Existing Round 51 exact-preflight, exhaustive-oracle, hardness, and frozen-routing
  evidence to ensure that the diagnostic did not broaden prior claims.

## Claim-Evidence Matrix

| Claim family | Status | Evidence basis | Scope boundary |
| --- | --- | --- | --- |
| Development interaction support | Supported | 32 ordered GMRES ILU0/ILUT action pairs have stable isolated failures on at least two training and two calibration matrices | Isolated failure co-support only; no conditional timing or multiplier identification |
| Frozen plan eligibility | Supported | Both frozen v5 and v6 evidence files report zero plan-gated candidate transitions and zero calibrated transitions | The audit reads the frozen decision record; it does not tune or reconstruct a new policy |
| Conditional timing availability | Supported negative result | Both conditional-observation files contain headers and zero data rows | No conditional-cost estimate, uncertainty bound, or interaction gain can be inferred |
| Development-to-held-out transfer | Supported negative result | Development support has zero overlap with the v5 and v6 held-out observed pair sets | Held-out statuses are diagnostic only and excluded from selection/calibration |
| V5/v6 held-out stability | Supported negative result | V5/v6 contain 8/56 held-out ordered pairs; intersection 8, union 56, Jaccard `1/7 = 0.142857...` | Three matrices and 24 requests per cohort on one host; not a population estimate |
| Cause of absent real interaction selection | Partially diagnostic, correctly bounded | Development support exists, the frozen plan gate admits zero candidates, and held-out support shifts completely | Does not isolate a conditional multiplier, counterfactual policy gain, or causal value of any one gate predicate |
| Prior exact-interaction theory | Supported and unchanged | Bellman proof, 256 exhaustive interaction cases, 4,096-graph reduction audit, exact preflight | Adjacent cost effects and history-independent acceptance only |
| Frozen routing significance | Supported and unchanged | V6 is only `0.109%` below fixed; v5 replay remains `1.718370902×` fixed; no calibrated interaction transition is selected | One Apple M4 and two three-matrix final cohorts |

## Numeric Consistency Findings

- Each version contains 4,920 action rows, 984 request--action groups, and exactly five
  repetitions per group.
- Request counts are 48 training, 32 calibration, and 24 held-out for both versions.
- Requests with stable failures from at least two distinct experts are 38/24/8 for
  v5 training/calibration/held-out and 38/24/16 for v6.
- Training and calibration each expose 64 ordered failure pairs before the two-matrix
  support rule. The two-matrix counts are 40 and 32; their intersection is the reported
  32 development-supported pairs.
- All 32 development-supported pairs belong to one expert-pair class:
  `gmres-ilu0-cpu-v1 <-> gmres-ilut-cpu-v1` across the four frozen budgets.
- Held-out pair counts are 8 for v5 and 56 for v6. Their intersection is 8 and union is
  56, giving Jaccard `0.14285714285714285`, rendered as `0.143` in the manuscript.
- Development overlap is zero separately for v5 and v6 and zero against their union.
- Both frozen packages report zero plan candidates, zero conditional timing rows, zero
  calibrations, zero interaction-plan changes, and zero plans using a calibrated
  transition.
- An independent recount from `request-coverage.tsv`, `pair-support.tsv`, and
  `pair-classes.tsv` reproduces every manuscript-facing Round 52 number.
- The synchronized PDF has 12 pages, 324,200 bytes, and SHA-256
  `6d8765cfd65de19dc5f54326ea66ca7cdc38d09011f8a6725fa468bbf73e54d4`.

## Citation Metadata Findings

- Round 52 adds no citation and makes no new field-first claim.
- All 21 cited keys are defined; the bibliography has no duplicate keys, and the PDF
  build reports no undefined citation or reference.
- Public-safe exact-title checks of the recent solver-selection and neural-solver works
  found no contradiction to the manuscript's conservative citation forms. The Greedy
  PDE Router remains cited as an arXiv preprint rather than as an accepted venue paper.

## Citation-Context Findings

- The residual-versus-state-error limitation remains attached to the cited
  error-conditioned neural-solver work and is not used to claim a universal failure.
- Recent solver-selection citations support prior request-conditioned and adaptive
  selection; the manuscript explicitly denies novelty for solver selection itself.
- Test sequencing and ratio ordering remain cited as prior art; the contribution is
  bounded to verified complete-path composition and the stated finite models.

## Implementation And Artifact Findings

- The parser derives stable failures across all five repetitions and forms only ordered
  pairs whose actions belong to distinct experts.
- Development support requires at least two matrix identifiers in both training and
  calibration; held-out observations never enter that intersection.
- The summary expert-pair class and development-to-any-held-out overlap are computed
  from the observed sets rather than emitted as literals.
- The verifier locks the contract and both immutable v5/v6 action/evidence hashes, all
  key counts, all negative-inference flags, and the row counts of the three TSV outputs.
- The core archive includes the contract, parser, verifier, generated directory, and
  both frozen input packages needed for a clean zero-execution rerun.
- The artifact wording now uses the verified “zero DP-state visit” cap guarantee rather
  than implying a broader before-allocation guarantee.

## Severity

- Fatal: none.
- Major integrity defect: none.
- Moderate: none.
- Minor diagnostic limitation: the zero-candidate observation does not provide a
  stage-by-stage attrition count for each frozen eligibility predicate. No manuscript
  claim depends on such a decomposition.
- Residual scientific limitation: the available data identify neither conditional
  costs nor beneficial selected interactions and show complete development-to-held-out
  support shift.

## Safe Edit Suggestions

- Preserve “post-hoc,” “zero-execution,” “held-out diagnostic only,” and “no conditional
  multiplier or interaction benefit inference” in every summary.
- Do not convert isolated failure support into a transition population estimate or a
  claim that a conditional attempt would be beneficial.
- Keep the v5 negative result, v6 `0.109%` fixed improvement, zero selected transition,
  and one-host/three-matrix boundaries prominent.
- If a future diagnostic adds gate-stage attrition, freeze its predicates before
  inspecting held-out outcomes and keep it separate from policy tuning.

## Next CCFA Owner

`ccf-paper-reviewer` for the Round 52 decision panel. A score increase requires a new
prefrozen external campaign; manuscript-only edits cannot supply the missing evidence.

## No-Invention Status

Pass. No solver execution, conditional timing, multiplier, policy improvement, cohort
search, population effect, author identity, disclosure, funding, conflict, independent
reproduction, or external-performance fact was invented.
