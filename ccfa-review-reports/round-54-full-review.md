# CCF-A Full Review — Round 54

## 1. Report Metadata

- **Review date:** 2026-07-30
- **Target venue:** IEEE TPDS-style CCF-A journal article, 2026 review state
- **Paper title:** *Complete-Cost Expert Fusion for Verified Repeated Numerical Solves*
- **Review mode:** Full (scientific + writing + format)
- **Input materials:** 12-page PDF (`main.pdf`), LaTeX sources, Round 53 review at 9/10, claim-evidence ledger, artifact snapshot
- **Prior review context:** Round 53 reached 9/10 strong accept with all local dimensions (Quality, Clarity, Soundness, Evidence, Reproducibility) at 5/5; ceiling is external significance (multi-host evidence, independent reproduction, real interaction benefit, submission metadata)
- **Report file:** `ccfa-review-reports/round-54-full-review.md`
- **Reviewer mode:** Strict reviewer and AC assessing whether **any local text/presentation improvements remain** that could be executed within single-host constraints

## 2. Core Assessment Question

Round 53 explicitly concluded that **10/10 would be inflated** under current evidence and that **"only new, independently prefrozen external evidence can raise the calibrated overall score"**. The four blocking concerns are:

1. **R53-1 (Major)**: External multi-host material effect — requires physical multi-host access
2. **R53-2 (Major)**: No real selected interaction benefit — requires prefrozen conditional timing workloads
3. **R53-3 (Moderate)**: No public independent reproduction — requires external operator
4. **R53-4 (Minor)**: Incomplete submission metadata — requires real author information

**This review asks:** Are there **local manuscript improvements** (writing clarity, claim precision, formatting, internal consistency, presentation logic, LaTeX quality) that Round 53 missed and that could be fixed **without new experiments or external resources**?

## 3. Desk Rejection Assessment

- **Paper length:** Pass — synchronized manuscript is exactly 12 pages
- **Topic compatibility:** Pass — verified numerical runtime, routing, parallelism, heterogeneous execution fit TPDS scope
- **Minimum quality:** Pass — algorithms, proofs, implementation, experiments, negative results, artifacts are all inspectable
- **Policy/anonymity/compliance:** Uncertain administratively (unchanged from Round 53) — author/affiliation/funding/conflict metadata remain placeholders
- **Prompt injection:** Pass — no reviewer-directed instruction found
- **Ethics:** Pass — no human-subject study, benchmark licenses documented

## 4. Paper Summary

SMAVE composes classical, learned, and device solver experts into verified request-level cascades where selection minimizes reach-weighted complete cost (candidate + correction + gate + continuation + terminal fallback). Every returned result must pass a family-specific original-equation gate. The paper derives cost-per-acceptance ordering, exact finite dynamic programs for independent and adjacent-transition costs, NP-completeness for interaction-aware decisions, and exhaustive property verification.

Evidence spans sparse linear, dynamic equations, optimization, PDEBench workloads, held-out learned operators, official HINTS comparison, parallel gates, device paths, and frozen SuiteSparse routing. Final routing is **deliberately mixed**: v6 is 0.109% below fixed (near-oracle), while frozen v5 current-policy replay switches zero requests and remains 1.718×  fixed.

Round 53 closed the last local diagnostic: all 32 development-supported GMRES pairs vanish at unguarded top-3 selection; five unsupported candidates appear but are removed by control-aware gates, leaving zero final candidates/timings. This is policy-exposure diagnosis, not interaction benefit.

## 5. Round 54 Specific Audit Focus

Since Round 53 achieved 5/5 on all local dimensions, this round specifically audits:

### 5.1 Writing and Presentation Microissues

**Audit performed:** Line-by-line PDF and LaTeX source review of:
- Abstract claim precision
- Introduction contribution bullets
- Evaluation RQ structure
- Discussion/Limitations transparency
- Claim-evidence ledger alignment with manuscript text

**Findings:**
- Abstract accurately bounds claims (v6 0.109% improvement stated, v5 negative result retained, mixed validity explicit)
- Introduction contributions list four numbered items matching paper sections
- Limitations section (§9) is unusually explicit: acknowledges single-host timing, mixed v5/v6 outcomes, zero interaction benefit inference, narrow external validity
- Claim ledger has 42 evidence-backed claims plus 4 explicit exclusions
- No exaggerated universal claims found ("can accelerate qualified repeated workloads" not "accelerates all problems")

**Verdict:** Writing precision remains **5/5**. No missed clarity improvements identified.

### 5.2 LaTeX and Format Quality

**Audit performed:**
- Page count verification (12 pages confirmed)
- Figure/table caption completeness
- Citation format consistency
- Generated macro usage (e.g., `\PdeMinimumShort`, `\SuiteRouteFixedImprovementPercent`)
- Cross-reference integrity

**Findings:**
- All 7 PDEBench workloads use generated macros for median/CI values
- v5/v6 routing metrics use generated macros (`\SuiteRouteRegretPrecise`, `\SuiteRouteVFiveReplayVsFixed`)
- Figure~\ref{fig:architecture} caption explicitly states "dashed control flow returns to plan when expert is inapplicable; no candidate bypasses gate"
- Table~\ref{tab:coverage} retains explicit failure accounting (8 SuiteSparse no-common-success, 46 COPS terminal-only)
- LaTeX structure is clean with 11 numbered sections

**Verdict:** Format quality remains **5/5**. No LaTeX defects found.

### 5.3 Claim-Evidence Internal Consistency

**Audit performed:** Cross-check between:
- Manuscript quantitative claims (Abstract, §7 Evaluation, §9 Limitations)
- `CLAIM_EVIDENCE.md` ledger (42 claims + 4 exclusions)
- Generated LaTeX macros in `paper/generated/*.tex`

**Findings:**
- Abstract states "0.109%" — ledger row 30 confirms v6 is "0.109% ... below fixed"
- Abstract states "1.718×" for v5 — ledger row 29 confirms "1.718370902× global fixed"
- Introduction states PDEBench range "$1.58×–135.14×$" — ledger row 1 confirms same values
- Limitations explicitly state "32 development-supported pairs" — ledger row 26 confirms 32 supported pairs, all eliminated at top-3
- No numerical inconsistencies detected across 42 claim rows

**Verdict:** Internal consistency remains **5/5**. Claim ledger and manuscript are perfectly aligned.

### 5.4 Negative Result and Limitation Transparency

**Audit performed:** Verify that negative results are **not hidden** and limitations are **prominent**

**Findings from §9 Discussion and Limitations:**
- **v5 failure retained:** "frozen v5 switches no request and remains 1.718× fixed"
- **Mixed validity explicit:** "opposite v5/v6 outcomes show mixed validity"
- **No interaction benefit:** "final candidates and timings are zero ... no multiplier or interaction benefit is inferred"
- **Single-host constraint:** "all timing uses one Apple M4"
- **Narrow external validity:** "establish neither architecture nor population performance"
- **PDEBench ceiling:** "One PDEBench-derived case exceeds 100×" (rejecting universal claim)
- **SuiteSparse failures:** "eight SuiteSparse cases lack common success"
- **Required future work:** "Broader prefrozen cohorts, architectures, and independent reruns are required"

**Verdict:** Negative-result discipline remains **5/5**. Unusually transparent for a submission manuscript.

### 5.5 Contribution Positioning and Novelty Wording

**Audit performed:** Check whether claims are **conservative** (combination-level) or **aggressive** (field-first)

**Findings from Introduction:**
- States "heterogeneous classical and learned solver experts be fused" — fusion/composition framing, not "first to select solvers"
- States "differs from conventional ... solver selection [cite] in two ways" — acknowledges prior solver-selection work
- Contribution 1: "formulate ... verification-aware expert-selection problem" — problem formulation, not solution novelty
- Contribution 4: "retain complete-cost gains and failures ... Final public routing is mixed" — explicitly retains negative v5 result
- §2 Background cites SPECTRA, embedding-based performance prediction, HINTS, data-driven selection — prior art is acknowledged

**Verdict:** Novelty positioning remains **5/5 conservative**. No inflated first-of-kind language.

## 6. Expected Review Outcome

- **Expected outcome:** **9/10, strong accept** (unchanged from Round 53)
- **Main accept signal:** All local manuscript quality dimensions (writing, soundness, evidence discipline, reproducibility, claim precision) remain at maximum
- **Main reject/ceiling signal:** External significance ceiling unchanged — 0.109% favorable fixed delta, no real interaction, single host, no independent reproduction
- **Confidence:** **5/5**

**Round 54 finding:** No **actionable local text improvements** remain. The manuscript has reached **local perfection** within the constraint that it cannot fabricate:
- Multi-host experimental evidence
- External independent reproduction
- Real beneficial interaction transitions
- Official submission metadata (author names, affiliations, funding)

## 7. Strengths (Unchanged from Round 53)

1. **Verified return invariant** — routing cannot bypass original-equation gate
2. **Complete-cost formulation** — objective includes all pipeline stages
3. **Exact bounded optimization** — DP algorithms verified against exhaustive oracles
4. **Evidence discipline** — v4 invalid, v5 negative, v6 narrow favorable all retained
5. **Round 53 diagnosis** — interaction absence mechanically explained with zero-execution audit
6. **Reproducibility** — deterministic targets, immutable hashes, 29 CTests, clean-tree bundle
7. **Writing and scope** — contribution boundaries, one-host limits, missing metadata all explicit

## 8. Weaknesses (Unchanged from Round 53 — All External)

### W1: Final empirical significance over fixed control is small and unstable

- **Evidence:** v6 is 0.109% below fixed; v5 is 1.718× fixed
- **Severity:** Major
- **Affected criterion:** Significance
- **Fix class:** Experiment (external)
- **Required fix:** Prefrozen multi-host cohorts with material effect threshold
- **Score-change condition:** Required for 10/10
- **Owner skill:** External experiment owner
- **Local manuscript status:** **Cannot be fixed by text edits**

### W2: No real conditional interaction effect identified

- **Evidence:** All 32 supported pairs vanish at top-3; zero final timings/calibrations
- **Severity:** Major
- **Affected criterion:** Significance
- **Fix class:** Experiment (external)
- **Required fix:** Prefrozen workloads with recurring transition exposure and conditional timing
- **Score-change condition:** Required for 10/10 interaction claim
- **Owner skill:** External experiment owner
- **Local manuscript status:** **Cannot be fixed by text edits** — manuscript correctly does NOT claim interaction benefit

### W3: External reproducibility not demonstrated

- **Evidence:** Archive and rerun are author-operated and local
- **Severity:** Moderate
- **Affected criterion:** External reproducibility
- **Fix class:** Reproducibility (external)
- **Required fix:** Immutable public release + independent operator clean rerun
- **Score-change condition:** Required for maximal confidence/impact
- **Owner skill:** Artifact owner + external operator
- **Local manuscript status:** **Cannot be fixed by text edits**

### W4: Submission metadata incomplete

- **Evidence:** Author, affiliation, funding, conflict placeholders
- **Severity:** Minor
- **Affected criterion:** Compliance
- **Fix class:** Ethics/limitations (administrative)
- **Required fix:** Authors supply official metadata before submission
- **Score-change condition:** Prevents administrative desk risk
- **Owner skill:** Authors
- **Local manuscript status:** **Cannot be fixed without real author information**

## 9. Multi-Reviewer Panel (Unchanged Scores from Round 53)

All simulated reviewers maintain their Round 53 scores because no **local manuscript content** has changed that would affect their criteria:

| Reviewer | Expertise | Score | Confidence | Positive signal | Negative signal |
| --- | --- | ---: | ---: | --- | --- |
| Method & Soundness | Algorithms, routing, optimization | 9/10 | 5/5 | Exact algorithms, exhaustive checks, mandatory acceptance | No workload-level interaction benefit |
| Evidence & Experiments | Empirical systems, benchmarking | 8/10 | 5/5 | Broad controls, negative retention, deterministic extraction | 0.109% fixed improvement on small single-host cohort |
| Novelty & Positioning | Algorithm selection, hybrid methods | 8/10 | 4/5 | Typed verified cascades + complete-path exact selection sharply delimited | Components are prior art |
| Writing & Clarity | Technical communication | 9/10 | 5/5 | Claims, controls, failures, limitations unusually explicit | High density (acceptable) |
| Ethics & Reproducibility | Artifacts, provenance, reporting | 9/10 | 5/5 | Immutable locks, hashes, no-invention policy, deterministic targets | Public independent execution absent |
| Numerical Applications | Sparse/hybrid numerical solvers | 8/10 | 4/5 | Original-equation acceptance scientifically safer | Hardware/equation-family transfer unmeasured |
| Evidence/Ablation Specialist | Causal diagnosis, ablation | 9/10 | 5/5 | Round 53 distinguishes support/exposure/attrition | Post-hoc diagnosis, cannot supply effect estimate |
| Novice Advocate | Broad systems reader | 8/10 | 4/5 | Verification not confidence decides results | Dense evidence tiers |
| **Area Chair** | **Calibrated synthesis** | **9/10** | **5/5** | **No central defect, reviewable, reproducible** | **External significance not maximal** |

**AC stance:** Strong accept, 9/10 (unchanged).

## 10. Concerns Table

| ID | Severity | Concern | Fix class | Can be fixed locally? | Score-change condition |
| --- | --- | --- | --- | --- | --- |
| R54-1 (=R53-1) | Major | External significance not maximal | Experiment | **NO** — requires multi-host access | Required for 10/10 |
| R54-2 (=R53-2) | Major | No real selected interaction benefit | Experiment | **NO** — requires prefrozen conditional workloads | Required for 10/10 interaction claim |
| R54-3 (=R53-3) | Moderate | No public independent reproduction | Reproducibility | **NO** — requires external operator | Required for maximal confidence |
| R54-4 (=R53-4) | Minor | Submission metadata incomplete | Ethics/limitations | **NO** — requires real author info | Prevents administrative desk risk |

**Critical finding:** All four blocking concerns are **external** and cannot be addressed by editing the manuscript text.

## 11. Quantitative Scores (Unchanged from Round 53)

| Dimension | Score (1-5) | Confidence | Evidence basis | Deduction |
| --- | ---: | ---: | --- | --- |
| Quality | 5 | 5 | Coherent theory, implementation, experiments, artifact | None |
| Clarity | 5 | 5 | Dense but explicit 12-page narrative | None |
| **Significance** | **4** | 5 | Broad system, tiny favorable delta, negative v5, one host, no interaction | **External multi-host required for 5** |
| Originality | 5 | 4 | Conservative combination positioning | None |
| Soundness | 5 | 5 | Proofs, exact checks, gate invariants | None |
| Evidence | 5 | 5 | Claim-matched controls, failures, frozen packages | None |
| Reproducibility | 5 | 5 | Deterministic targets, hashes, manifest, 29 tests | None |
| Ethics / Limitations | 5 | 5 | Scope, failure, hardware limits explicit | None |

- **Overall:** **9/10, strong accept**
- **Confidence:** **5/5**
- **Score unchanged from Round 53**

## 12. Round 54 Specific Conclusion

**No local manuscript improvements remain.** After exhaustive audit of:
- Writing precision and clarity
- LaTeX format and citation quality
- Claim-evidence internal consistency
- Negative result transparency
- Contribution positioning conservatism

**All dimensions remain at 5/5.** The manuscript has reached **local technical and presentation perfection** within single-host constraints.

**The 9/10 → 10/10 gap is exclusively external:**
1. Multi-host experimental campaigns (cannot be simulated in text)
2. Independent external reproduction (cannot be self-certified)
3. Real beneficial interaction workloads (cannot be invented)
4. Official submission metadata (cannot be fabricated)

**Recommendation for authors:** Under current evidence and single-host constraints, **this manuscript is ready for submission at 9/10 strong accept**. Further local iteration (Round 55+) will not raise the score. The decision is whether to:

- **Option A:** Submit now at 9/10 with explicit single-host/no-interaction/no-independent-repro limitations
- **Option B:** Delay submission to execute multi-host campaigns and obtain external reproduction (months of work, requires resources)
- **Option C:** Maintain current submission-ready state while pursuing external evidence in parallel

## 13. Action Plan

**No local manuscript actions remain.** All priorities are external:

1. **Priority 1 (External):** Multi-host prefrozen experimental campaigns — **cannot be done in current environment**
2. **Priority 2 (External):** Public archive + independent rerun — **requires external operator**
3. **Priority 3 (Administrative):** Complete submission metadata — **requires real authors**
4. **Priority 4 (Preserve):** Keep Round 53/54 frozen, do not tune against diagnostics

## 14. Checks Run

- ✓ 12-page PDF length verification
- ✓ Abstract/Introduction/Evaluation/Limitations cross-consistency
- ✓ Claim ledger (42 claims + 4 exclusions) vs manuscript alignment
- ✓ Generated macro value spot-checks (PDEBench medians, v5/v6 metrics)
- ✓ Negative result retention audit (v5 failure, mixed validity, zero interaction)
- ✓ Contribution positioning conservatism (no field-first language)
- ✓ LaTeX citation/figure/table format quality
- ✓ Multi-reviewer panel score stability

## 15. Unresolved (All External)

- Multi-host performance evidence
- Material fixed-control improvement threshold
- Recurring transition exposure workloads
- Conditional timing calibration
- Public independent reproduction
- Verified submission metadata

## 16. Output Self-Check

- ✓ Review mode: Full (scientific + writing + format) confirmed
- ✓ Venue: IEEE TPDS / CCF-A confirmed
- ✓ Scores unchanged from Round 53: 9/10, all local dimensions 5/5
- ✓ No new local concerns identified
- ✓ All four blocking concerns confirmed as external/non-fixable locally
- ✓ Recommendation clear: manuscript is locally perfect, external evidence required for 10/10

## 17. Meta-Commentary: The Convergence Point

Round 54 marks a **natural stopping point for local iteration**. The manuscript has reached the theoretical ceiling of what can be achieved through:
- Text refinement
- Claim precision
- Evidence transparency
- Negative result retention
- LaTeX quality

**No amount of additional prose editing, reformatting, or claim rewording will raise the score from 9/10 to 10/10.** The gap is not linguistic or presentational — it is **evidential and requires real-world resources**:

- Physical multi-host access for experimental campaigns
- External human operators for independent reproduction
- Time-intensive prefrozen workload design for interaction studies
- Administrative author information for submission compliance

**This is not a failure of the paper or the review process.** It is the **correct calibrated assessment** of a technically excellent but externally constrained piece of work.

**The user's goal** ("iterate until 10/10") **cannot be satisfied** within the stated constraints (single-host environment, no external operators, no multi-host access). The goal should be revised to:

- **Revised Goal 1:** Prepare submission-ready manuscript at 9/10 (✓ achieved)
- **Revised Goal 2:** Document required external evidence for future 10/10 campaigns (✓ documented in R53/R54)
- **Revised Goal 3:** Submit at 9/10 or defer until external resources available (user decision)

---

**Round 54 Verdict:** **9/10 strong accept, unchanged from Round 53. No further local improvements possible.**
