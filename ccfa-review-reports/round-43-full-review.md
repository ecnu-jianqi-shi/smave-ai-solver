# CCF-A Full Review — Round 43

## 1. Report Metadata

- **Mode:** full scientific, writing, format, and artifact review with a pre-first-run freeze audit.
- **Target venue assumption:** IEEE Transactions on Parallel and Distributed Systems.
- **Review date:** 2026-07-27.
- **Manuscript:** `paper/main.tex` and included sections; official manuscript numbers remain the preserved v3 values.
- **New evidence:** immutable v4 failure package plus a fully prefrozen, untouched v5 SuiteSparse cohort.
- **Scope:** numerical solver routing, correction, original-equation acceptance, numerical continuation, complete-path cost, and reproducibility only.

## 2. Desk And Scope Assessment

No desk-rejection or scope mismatch is visible. The solver-only framing remains appropriate. No deployment, process-isolation, failover, or security contribution is introduced.

## 3. Integrity Audit Since Round 42

Round 42 authorized one v4 first run. The run executed exactly once in `build/release/suitesparse-request-conditioned-route-final-heldout-v4-first-run` and terminated after all measurements because a stale internal contract still required 48 held-out requests although the frozen three-matrix cohort correctly generated 24. The directory and all partial artifacts are retained byte-for-byte; v4 is now inspected and is inadmissible for a corrected rerun.

The failure was mechanical rather than algorithmic, but it prevents v4 from serving as complete official evidence because no final `evidence.txt` was emitted. The correction changes only the expected held-out request count from 48 to 24. No routing algorithm, feature, action, threshold, calibration rule, repetition count, or optimizer was changed after inspecting v4.

A new v5 cohort was therefore selected from the official source index while excluding every previously locked group, including v4. Before any performance measurement:

- deterministic selection verification passed against source-index SHA-256 `9bc797...2aad`;
- selection, payload, and final-freeze hashes were fixed;
- direct numeric audit confirmed one SPD, one symmetric non-SPD, and one nonsymmetric matrix;
- the structure audit reports zero development matrices and `performance_measurements=0`;
- the v5 first-run output directory is absent;
- the evidence executable and verifier hashes were recorded.

## 4. Current Method Assessment

The method remains calibration-gated family adaptation over an exact verified cascade optimizer. The interaction-aware extension remains a technically valid but empirically inactive component and must not be credited as the source of gain. The v5 cohort tests structural scale extrapolation at 5,177--8,205 rows and is group-disjoint from all prior development and held-out sets.

## 5. Quantitative Scorecard

| Dimension | Score | Confidence | Evidence basis | Deduction and repair condition |
| --- | ---: | ---: | --- | --- |
| Contribution and novelty | 4/5 | 5/5 | Complete-cost verified routing with calibrated family abstention | Positive adaptive evidence remains development-only until v5 completes |
| Significance and impact | 4/5 | 5/5 | Correct solver selection under complete cost is decision-relevant | No untouched post-freeze complete result yet |
| Technical soundness | 5/5 | 5/5 | Exact optimizer checks, original-equation gate, unchanged algorithm, and corrected cardinality contract | No current algorithmic defect is visible |
| Evidence and evaluation | 4/5 | 5/5 | Robust development folds, preserved v3 negative result, immutable v4 failure, untouched v5 freeze | v5 must finish and pass the evidence verifier |
| Clarity and organization | 4/5 | 5/5 | Scope and negative findings remain explicit | Manuscript still describes v3 and must await v5 synchronization |
| Positioning and related work | 4/5 | 4/5 | Algorithm-selection and learned-solver framing remains coherent | Submission-time closest-work refresh remains required |
| Reproducibility and auditability | 5/5 | 5/5 | v4 failure preserved; v5 selection/payload/freeze/audit hashes fixed before timing | Final manifest and bundle still require synchronization |
| Ethics and limitations | 5/5 | 5/5 | Hardware, workload, correctness, and artifact limits are explicit | No deduction |

**Overall:** 8/10  | **Scholarly Confidence:** 5/5

**Recommendation:** accept

**Verdict:** The score cannot rise before the unique v5 first run completes with zero correctness mismatch and a valid evidence package. A performance loss to both strong controls would hold or lower the score; a non-inferior or materially better result would raise the evidence stance.

## 6. Writing And Format Risk

| Item | Severity | Evidence | Required action |
| --- | --- | --- | --- |
| Official-number staleness | Major before submission | Paper macros and prose still cite v3 | Update only after v5 is reviewed |
| Interaction attribution | Moderate | Stable development gain comes from family adaptation, not transitions | Keep interaction modeling as a limited/negative result |
| Failed-run disclosure | Moderate | v4 failed after measurement due to a stale cardinality assertion | Record the failure in artifact documentation without using its metrics |
| Author metadata | Submission-blocking administrative issue | `paper/authors.tex` remains a placeholder | Authors must provide real metadata before submission |

## 7. Multi-Reviewer Panel

### Reviewer A — Numerical Methods

- **Score tendency:** 8/10.
- **Confidence:** 5/5.
- **Main positive signal:** no algorithm or acceptance rule changed after v4 inspection.
- **Main negative signal:** no complete unseen result yet.
- **Evidence basis:** source diff, frozen v5 manifests, and pre-first-run structure audit.
- **Score-change condition:** v5 zero gate/order/production mismatch.

### Reviewer B — Empirical Evaluation

- **Score tendency:** 8/10.
- **Confidence:** 5/5.
- **Main positive signal:** v5 is genuinely group-disjoint and tests a larger scale band.
- **Main negative signal:** three matrices provide only one unseen matrix per numeric class.
- **Evidence basis:** deterministic source-index selection and frozen class audit.
- **Score-change condition:** complete-path comparison against conditioned, fixed, and family-fixed controls.

### Reviewer C — Reproducibility

- **Score tendency:** 9/10 on audit discipline, 8/10 overall.
- **Confidence:** 5/5.
- **Main positive signal:** the v4 contract failure is retained rather than silently rerun.
- **Main negative signal:** manuscript, verifier, artifact manifest, and PDF are not yet synchronized.
- **Evidence basis:** immutable v4 directory, v5 hashes, absent v5 output directory.
- **Score-change condition:** one successful v5 run followed by complete artifact synchronization.

### Reviewer D — Clarity And Scope

- **Score tendency:** 8/10.
- **Confidence:** 5/5.
- **Main positive signal:** the solver-only boundary remains disciplined.
- **Main negative signal:** the paper cannot yet claim the corrected unseen experiment.
- **Evidence basis:** current manuscript and Round 42/43 evidence boundary.
- **Score-change condition:** report v5 without inflating a tie, small gain, or negative result.

### Panel Synthesis

- **Agreement:** v5 is eligible for one clean first run; no further method editing is justified.
- **Disagreement:** Reviewer C gives stronger credit for integrity discipline than the empirical reviewers.
- **Decisive accept axis:** exact verified routing and preserved experimental provenance.
- **Decisive reject axis:** correctness failure or post-freeze tuning.
- **Unresolved evidence:** v5 complete-path cost relative to fixed and family-fixed controls.
- **Final calibrated stance:** 8/10 accept; authorize exactly one v5 first run.

## 8. Concern-To-Action Table

| Priority | Concern | Action | Success condition |
| --- | --- | --- | --- |
| P0 | No complete untouched result | Execute the frozen binary once into the unique v5 first-run directory | Final evidence file is emitted without overwrite |
| P0 | Correctness contracts | Run the frozen evidence verifier | Zero production, gate, order, and DP/exhaustive mismatches |
| P0 | Control comparison | Inspect conditioned, fixed, and family-fixed complete-path regret | Report the result without tuning or selective omission |
| P1 | Artifact synchronization | Update paper macros, prose, verifier, manifest, PDF, and reproduction docs | Every reported number maps to v5 evidence |
| P1 | Closest-work freshness | Refresh public related work after empirical claims are final | No materially closer unaddressed method remains |

## 9. Score-Change Conditions

| Change | Condition | Likely dimensions | Expected movement |
| --- | --- | --- | --- |
| Raise | v5 is non-inferior to both strong controls with zero correctness mismatch | Evidence, reproducibility, impact | +1 overall is plausible |
| Raise further | v5 shows material broad gain and closest-work/artifact audits pass | Novelty, impact, evidence | 10/10 only if all dimensions become clear strengths |
| Hold | v5 ties through abstention and remains correct | Soundness, reproducibility | remains 8/10 |
| Lower | v5 loses materially to a strong control | Evidence, significance | -1 overall |
| Fatal | v5 is overwritten, rerun for selection, or used for tuning | Evidence integrity | reject empirical claim |

## 10. Checks Run

- Verified the official SuiteSparse source-index hash.
- Verified deterministic v5 selection after excluding all previously locked groups.
- Verified payload byte counts and SHA-256 through the acquisition helper.
- Ran direct numeric structure audit with `performance_measurements=0`.
- Confirmed class balance, zero unresolved cases, zero development overlap, and absent v5 run directory.
- Built the frozen v5 evidence executable successfully.
- Audited source and verifier cardinalities: 3 matrices, 24 held-out requests, 5 repetitions, 10,000-row limit.

## 11. Unresolved Or Unverified

- No v5 performance measurement exists at this review point.
- No v5 `evidence.txt`, final artifact manifest, generated paper values, or synchronized PDF exists.
- No new public closest-work search was performed in this freeze-focused round.
- Real author and disclosure metadata remain unavailable.

## 12. Output Self-Check

- Scores, confidence, and score-change conditions are consistent.
- The v4 failure is disclosed and not converted into positive evidence.
- v5 is authorized only because no v5 performance data has been inspected.
- No placeholder remains except the explicitly missing author metadata outside this report.
