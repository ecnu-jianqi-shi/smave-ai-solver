# CCF-A Full Review — Round 51

## 1. Mode

Full scientific, theory, systems, evidence, novelty, writing, reproducibility, and AC
review under the CCF-A standard workflow.

## 2. Venue And Assumptions

- **Target:** IEEE TPDS-style CCF-A systems/numerical-software article.
- **Paper type:** verified numerical runtime with formal finite optimization,
  implementation, systems evaluation, and executable artifact.
- **Review state:** 2026-07-29, 12-page main paper plus supplementary theory artifact.
- **Administrative boundary:** author/disclosure/anonymity metadata remain unavailable
  and are not invented.
- **Search boundary:** no new public search was needed because Round 51 adds no
  originality claim; the Round 50 closest-work positioning remains unchanged.

## 3. Paper Summary

SMAVE selects candidate, correction, original-equation gate, continuation, and
terminal-solver paths by complete expected cost. The independent finite model uses the
classical `K/p` ordering and an exact subset/budget DP. The adjacent-interaction model
uses an exact Bellman recurrence over used experts and the immediately previous action
and has an NP-complete rational decision version.

Round 51 closes an operational gap between asymptotic complexity and deployment. The
production interaction optimizer now computes the exact reachable-state count before
memo traversal, exposes auditable state/transition/memo diagnostics, and rejects an
insufficient cap before visiting a DP state. A prefrozen planning-only study matches
the exact count against production recurrence visits through a 32-action stress profile
and records the deployed 12-action shape at only 49 states and 204 transitions.

Empirical workload evidence remains deliberately mixed and unchanged. V6 reaches
near-oracle regret but is only 0.109% below fixed; frozen v5 remains 1.718x fixed under
zero-execution replay. No calibrated interaction transition is selected in v6, and no
new host, cohort, numerical-solver timing, or independent reproduction is added.

## 4. Likely Stance And Calibrated Score

- **Likely stance:** strong accept.
- **Overall:** **9/10**.
- **Scholarly confidence:** **5/5**.
- **Round 51 movement:** no overall-score change. Engineering evidence and operational
  predictability improve materially, but the decisive significance axis remains 4/5.
- **Why not 10:** the new study proves deployable state accounting, not a material,
  repeatable external workload gain. The frozen final cohorts remain small, single-host,
  and mixed; interaction-aware routing still has no real-workload selection event.

## 5. Scorecard

| Dimension | Score (1-5) | Confidence (1-5) | Evidence basis | Deduction / score-change condition |
|:---|:---:|:---:|:---|:---|
| Novelty | 5 | 5 | Section 3 proposition; supplementary exact interaction proof; conservative Section 2 positioning | Full mark retained: the paper claims the verified composition and exact bounded model, not prior ratio ordering or sequencing |
| Soundness | 5 | 5 | Sections 3–4; exact preflight identity; 256+256 oracle sweeps; 4,096-graph audit | Full mark retained; lower only if longer-memory/history-dependent exactness is implied |
| Evidence | 5 | 5 | Sections 6–8; frozen v5/v6; Round 51 9-profile diagnostic study; 29/29 tests | Full mark for claim-calibrated evidence, including negative results; external significance is scored separately |
| Significance | 4 | 5 | Section 9; v6 0.109% fixed improvement; v5 negative; Round 51 planning-only counts | Deduction: no independently prefrozen multi-host material gain and no real workload selecting an interaction transition; those results are required for 5/5 |
| Clarity | 5 | 5 | Abstract through conclusion; explicit prior-art and scope sentences; dense but complete Section 8 paragraph | Retain if planning-only/preflight distinctions remain explicit |
| Reproducibility | 5 | 5 | Prefrozen contract, deterministic target/verifier, evidence checker, manifest, core bundle | Full mark for author-operated reproducibility; public independent replication remains a significance/external-validity gap |
| Ethics / Limitations | 5 | 5 | Section 9 and claim ledger retain one-host, cohort, negative-result, callback, interaction-memory, and metadata limits | Full mark because limitations are unusually explicit and no unavailable metadata is invented |

**Overall:** **9/10**  | **Scholarly Confidence:** **5/5**

**Recommendation:** strong accept.

**Verdict:** Round 51 removes the remaining practical-accounting uncertainty around the
exact DP but does not satisfy the external significance condition for 10/10. A one-point
increase requires independently prefrozen, multi-host evidence with a material fixed-
control gain and real selected interaction transitions; a regression that invalidates
the exact preflight or frozen v5/v6 boundaries would lower the score.

## 6. Top Strengths

1. **Exact, bounded optimization:** independent and adjacent-interaction models have
   explicit exactness, complexity, and hardness boundaries.
2. **Predictable deployment failure:** the interaction optimizer now knows the exact
   state requirement and rejects before memo traversal rather than failing after partial
   exponential exploration.
3. **Executable theory checks:** exhaustive finite cases, hardness reductions, and
   exact state/transition identities are independently encoded and verified.
4. **Verified numerical returns:** selected paths still return only through the
   original-equation gate and preserve terminal continuation.
5. **Negative-result discipline:** v4 invalidity, v5 failure, narrow v6 gain, and absent
   interaction selection remain visible rather than being optimized away.
6. **Strong artifact contract:** deterministic generated values, manifest hashes, and
   clean-tree reproduction make every central claim inspectable.

## 7. Major And Fatal Concerns

### M1 — External Significance Remains Below Full-Mark Level

- **Severity:** major score ceiling, not a correctness defect.
- **Evidence:** Section 9; v5/v6 each have three matrices and 24 requests on one Apple
  M4; v6 is only 0.109% below fixed; v5 is 1.718x fixed.
- **Affected criterion:** significance.
- **Fix class:** new external evidence.
- **Repair condition:** independently prefrozen multi-host cohorts with materially
  favorable fixed-control deltas and public third-party reproduction.
- **Expected movement:** significance 4→5 and overall 9→10 only if repeatable.

### M2 — Interaction-Aware Value Is Still Mechanistic

- **Severity:** moderate-to-major significance limitation.
- **Evidence:** the synthetic interaction sweep changes 126/256 plans, but v6 selects
  zero calibrated transitions and changes zero held-out plans through interactions.
- **Affected criteria:** significance and external validity.
- **Fix class:** new workload evidence, not prose.
- **Repair condition:** a prefrozen real workload where independently calibrated
  transitions survive uncertainty controls, are selected, and improve a strong fixed
  baseline without correctness regression.
- **Expected movement:** supports M1; insufficient alone for 10/10.

### M3 — Scaling Study Counts States, Not Resources

- **Severity:** minor because the manuscript states the boundary correctly.
- **Evidence:** Round 51 contract explicitly sets `timing_claim=0`; no peak-memory or
  wall-time measurement is reported.
- **Affected criterion:** practical significance, not soundness.
- **Fix class:** prefrozen planning benchmark.
- **Repair condition:** preregistered multi-host wall-time and peak-memory measurement
  of the same action grid, preserving the exact state-count oracle.
- **Expected movement:** strengthens deployability; unlikely to move overall score
  without M1.

No fatal scientific, implementation, citation, policy, or presentation concern is
identified.

## 8. Writing And Presentation Concerns

- Section 8 is dense because it preserves production-path, exhaustive-oracle,
  hardness, and scaling results in one paragraph to retain 12 pages. The logic remains
  progressive and every numeric result is generated.
- “Below the cap” must not be rewritten as “fully executed” for the 40-action profile;
  only the exact count and admission condition are established.
- “Before any DP-state visit” is the correct guarantee; broader “before allocation”
  wording would exceed the implementation evidence.

## 9. Format And Venue Concerns

- The main PDF is exactly 12 pages.
- No undefined citation/reference, duplicate label, or overfull box is reported.
- The final PDF is 324,118 bytes with SHA-256
  `d27da3ed8e58c179168ab91f03c853be77270f72f8a6d9c554332ee275420685`.
- Author, affiliation, disclosure, funding, conflict, acknowledgment, and anonymity
  facts remain unverified and are not invented.

## 10. Multi-Reviewer Panel

### Reviewer A — Theory And Algorithms

- **Score tendency:** 9/10.
- **Confidence:** 5/5.
- **Positive:** exact Bellman model, NP-completeness, and exact state-count identity are
  mutually consistent and executable.
- **Negative:** the adjacent-Markov/history-independent boundary remains essential.
- **Score-change condition:** any arbitrary-history exactness claim lowers soundness;
  current wording avoids it.

### Reviewer B — Systems And Runtime

- **Score tendency:** 9/10.
- **Confidence:** 5/5.
- **Positive:** exact preflight converts an exponential failure mode into a predictable
  admission decision and exposes auditable diagnostics.
- **Negative:** no wall-time or memory curve accompanies the state frontier.
- **Score-change condition:** prefrozen resource measurements improve practical impact,
  but not to 10 without external workload evidence.

### Reviewer C — Empirical Evaluation

- **Score tendency:** 8/10.
- **Confidence:** 5/5.
- **Positive:** unusually complete controls, negative evidence, frozen cohorts, and
  original-equation outcomes.
- **Negative:** the favorable final effect is only 0.109%, and v5 is strongly negative.
- **Score-change condition:** repeatable multi-host fixed-control improvement is
  required for a stronger score.

### Reviewer D — Reproducibility And Integrity

- **Score tendency:** 9/10.
- **Confidence:** 5/5.
- **Positive:** prefrozen contract, deterministic diagnostics, evidence checker,
  manifest, and core-bundle verifier align.
- **Negative:** author-operated local reproduction is not independent public reuse.
- **Score-change condition:** third-party public archive reproduction would remove the
  remaining external-reproducibility concern.

### AC / Meta-Review

- **Agreement:** all reviewers find the paper technically strong and the Round 51
  preflight/scaling repair valid.
- **Disagreement:** empirical impact remains 8-level for Reviewer C, while formal and
  systems reviewers support 9.
- **Decisive accept axis:** verified composition, exact bounded optimization, exhaustive
  checks, and unusually honest evidence boundaries.
- **Decisive reject axis:** none.
- **Unresolved evidence:** material multi-host gain, real selected interactions, and
  independent reproduction.
- **Final calibrated stance:** **9/10, strong accept**.

## 11. Concern-To-Action Table

| Concern | Severity | Exact action | Owner | Score-impact condition |
| --- | --- | --- | --- | --- |
| Single-host, tiny final cohorts | Major ceiling | Freeze a multi-host, collection-group-disjoint campaign before execution | External experiment owner | Material repeatable fixed-control gain can raise significance to 5 |
| No selected real interaction | Major ceiling | Freeze interaction-eligible workloads and calibration criteria before timing | External experiment owner | Selected beneficial transitions are necessary but not sufficient for 10 |
| No DP resource curve | Minor | Prefreeze wall-time and peak-memory collection for the existing deterministic grid | Artifact/benchmark owner | Improves deployability evidence; likely dimension-only |
| No public independent rerun | Major ceiling | Publish immutable archive and solicit third-party clean verification | Artifact owner / independent group | Required with external performance for 10 |
| Dense Section 8 paragraph | Minor | Preserve current generated-number structure; do not expand without page budget | Manuscript owner | Clarity remains 5 if boundaries stay explicit |

## 12. Recommended Next CCFA Owner

No further manuscript-only CCFA owner can defensibly deliver 10/10. The next owner is
an external experimental campaign using a prefrozen multi-host protocol, followed by
`ccf-integrity-auditor` and `ccf-paper-reviewer` once independent evidence exists.

## 13. Checks Run

- Release configure and full build.
- All 29 CTests: pass.
- `reproduce-joint-route-scaling-round51`: pass.
- Exact production diagnostic/state/transition checks over nine executed profiles.
- Three zero-visit preflight rejection checks.
- Existing interaction/property/hardness regressions through the full test suite.
- `paper/check_evidence.py`: pass.
- PDF rebuild: 12 pages, no undefined citation/reference or overfull box.
- Round 51 full integrity audit.

## 14. Unresolved Or Unverified

- Independent public reproduction.
- Multi-host, multi-architecture performance.
- Material repeatable improvement over fixed controls.
- A real workload selecting a calibrated interaction transition.
- Peak memory and wall time for the 40-action below-cap profile.
- Verified author/disclosure/anonymity metadata.

## 15. Output Self-Check

- Scores, confidence, and the 9/10 stance are internally consistent.
- Every 4/5 deduction has a concrete evidence basis and repair condition.
- No acceptance probability or unsupported field-first claim is made.
- The new exact preflight is distinguished from full execution and from timing evidence.
- No placeholder, invented metadata, hidden negative result, or broadened interaction
  exactness remains.

