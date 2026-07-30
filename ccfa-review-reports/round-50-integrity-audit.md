# CCF-A Integrity Audit — Round 50

## Mode

Full theorem, claim, numeric, citation, terminology, implementation, and artifact
audit for the 2026-07-28 TPDS manuscript state.

## Artifacts Checked

- Main manuscript, abstract, all section sources, generated macros, references, and
  the 12-page `paper/main.pdf`.
- `paper/SUPPLEMENTARY_THEORY.md`, `paper/CLAIM_EVIDENCE.md`, and the artifact/status
  documents.
- Independent and interaction-aware optimizer implementations in `src/routing.cpp`.
- `tests/joint_route_budget_evidence.cpp`, its CMake verifier, generated evidence,
  and evidence checker.
- Frozen v4/v5/v6 records and all Round 49 evidence boundaries.
- Public metadata/context for independent-test ordering and multimode transition-cost
  test sequencing.

## Claim-Evidence Matrix

| Claim family | Status | Evidence basis | Scope boundary |
| --- | --- | --- | --- |
| Independent finite cascade exactness | Supported | Adjacent exchange, sorted DP, exhaustive production probe, 256 six-action cases | History-independent costs/acceptance; one action per expert |
| Independent state/transition bound | Supported | Reachable capacity is determined by the used-expert set | Scalar values/backpointers; current map/vector overhead is not claimed constant |
| Adjacent-interaction exactness | Supported | Bellman recurrence over used experts and immediately previous action; 256 six-action/24-transition exhaustive cases | Only adjacent cost multipliers; acceptance remains history-independent |
| Interaction-aware NP-completeness | Supported | Directed Hamiltonian-path reduction in main proof sketch and full supplement | Rational decision problem; uniform costs/probabilities and binary multipliers suffice |
| Reduction implementation contract | Supported | All 4,096 directed simple graphs on four vertices satisfy threshold equivalence | Finite exhaustive audit of the reduction mechanics, not a proof substitute |
| Production optimizer behavior | Supported | 126/256 interaction cases change the independent plan; zero maximum oracle gap | Deterministic property evidence, not workload speedup or learned transfer |
| Verified-result return | Supported | Existing gate invariant and production rejection/acceptance trace | No claim about faulty callbacks or arithmetic hardware |
| Frozen final routing evidence | Supported and unchanged | v5 negative replay, unique v6 first run, frozen verifiers | Small one-host cohorts; no general repair claim |

## Formal Audit

- The independent exchange argument is valid because both adjacent orders reach a
  common unchanged suffix. Sorting therefore loses no optimal representative for any
  selected subset.
- The independent DP exhausts skip/take choices after sorting. Remaining capacity is
  `k-|S|` on reachable states, so the prior extra `k` factor was a loose rather than
  necessary state dimension.
- The interaction-aware state `(S,b)` is sufficient exactly when later attempt cost
  depends only on immediately previous action `b`, while acceptance and base cost do
  not otherwise depend on history.
- The state bound
  `(n+1) sum_{l=0}^{min(k,m)} binom(m,l)` counts every feasible used-expert set and a
  conservative previous-action value; scanning at most `n` next actions yields
  `O(n^2 2^m)` worst-case transitions.
- In the hardness reduction, choosing one more unused action and then stopping costs
  at most `2 + 0.5*5 = 4.5`, below terminal cost `5`; hence every optimum uses all
  vertices. A nonedge changes its multiplier from one to two and adds a strictly
  positive discounted term, establishing the Hamiltonian threshold equivalence.
- A proposed cascade is a polynomial certificate and its rational expected cost has
  polynomial bit length, so the decision problem is in NP.
- The main paper contains a complete proof sketch; `paper/SUPPLEMENTARY_THEORY.md`
  expands every induction, bound, and reduction step.

## Numeric Consistency Findings

- Independent property sweep: 256 cases, six actions, zero maximum oracle gap,
  expert exclusivity, and nondecreasing `K/p` order.
- Interaction property sweep: 256 cases, six actions, 24 transitions per case, zero
  maximum oracle gap, expert exclusivity, and 126 plans changed from the independent
  optimizer.
- Hardness audit: four vertices, 4,096 directed graphs, threshold `2.1875`, full
  four-action selections, and exact Hamiltonian-path equivalence.
- Production joint probe remains `4.5 us`, ratio `0.900` to best single action, with
  the original-equation gate accepting only the second attempted action.
- All prior v5/v6, PDE/operator, HINTS, scaling, and data-lock values remain unchanged.
- Final pre-archive PDF: 12 pages, 323,995 bytes, SHA-256
  `ea6a626307e39817e7733e470c68d01e79b9172c59911ed821269511815baea7`.

## Citation Metadata And Context

- All 21 cited keys exist; no undefined citation or duplicate cited key is reported.
- `ruan2004multimode` resolves to DOI `10.1109/TSMCB.2004.825940`, volume 34(3),
  pages 1490--1499.
- Its context supports the narrow statement that transition-aware diagnostic test
  sequencing and dynamic programming are prior art.
- The manuscript does not claim that transition-aware sequencing, Bellman recursion,
  Hamiltonian reductions, or NP-hard sequencing are first results.
- The novelty claim is restricted to the exact formal boundary and executable evidence
  for SMAVE's verified one-budget-per-expert cascade model.

## Terminology And Presentation

- The abstract, introduction, formulation, design, methodology, ablation, discussion,
  conclusion, claim ledger, and supplement consistently distinguish independent costs,
  adjacent cost interactions, and excluded longer-memory/history-dependent effects.
- The strict-gate figure was removed because its values are fully stated in text; no
  scientific evidence or negative result was removed.
- Direct LaTeX diagnostics show 12 pages and no undefined references/citations,
  multiply defined labels, or overfull boxes.

## Severity

- **Fatal:** none.
- **Major integrity defects:** none.
- **Round 50 repairs:** interaction exactness boundary, tight state counting,
  NP-completeness proof, transition prior-art citation, exhaustive interaction cases,
  exhaustive four-vertex reduction audit, and synchronized abstract/limitations.
- **Remaining non-integrity risks:** one-host/small-cohort external validity, no public
  independent reproduction, and unavailable verified author metadata.

## Safe Edit Suggestions

- Do not generalize the interaction theorem to longer memory or history-dependent
  acceptance.
- Do not call transition-aware sequencing or dynamic programming new.
- Preserve v5, the 0.109% v6-vs-fixed effect, and all one-host/cohort limits.
- Keep complete proofs in the supplementary artifact if the 12-page main limit applies.
- Never rerun or overwrite frozen v4/v5/v6 first-run directories.

## Next CCFA Owner

- `ccf-paper-reviewer` for the Round 50 decision panel.
- Artifact workflow for the post-review manifest and deterministic bundle refresh.
- Authors for verified identity/disclosure/anonymity metadata.

## No-Invention Status

- No new benchmark outcome, author identity, literature claim, acceptance probability,
  or external reproduction was invented.
- The formal claims are proved from the declared model and separately exercised by
  deterministic exhaustive code.
- Public searches used only generic method terms and did not expose private manuscript
  text or unpublished numerical results.

## Checks Run

- Release configure/build and 29/29 CTests.
- Focused independent/interaction evidence reproduction.
- 256 independent cases, 256 interaction cases, and 4,096 graph reductions.
- Paper evidence checker and direct 12-page LaTeX build.
- Theorem/code/claim/citation cross-check.

## Output Self-Check

- Every formal statement has assumptions, a proof basis, an implementation boundary,
  and executable evidence.
- Formal strengthening is not misrepresented as broader empirical validation.
- The integrity result supports a stronger originality score but not an unsupported
  10/10 overall decision.
