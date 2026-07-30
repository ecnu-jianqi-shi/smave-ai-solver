# CCF-A Integrity Audit — Round 51

## Mode

Full claim, theorem, numeric, terminology, implementation, citation-context, and
artifact audit for the 2026-07-29 TPDS manuscript state.

## Artifacts Checked

- Main manuscript sources, generated macros, references, and the 12-page
  `paper/main.pdf`.
- `paper/SUPPLEMENTARY_THEORY.md`, `paper/CLAIM_EVIDENCE.md`, and artifact/status
  documentation.
- Production independent and interaction-aware recurrences in `src/routing.cpp` and
  their public diagnostics contract in `include/smave/routing.hpp`.
- The prefrozen Round 51 contract, scaling executable, CMake verifier, raw tables,
  generated evidence, and manuscript evidence checker.
- Existing Round 50 exhaustive-oracle, hardness, frozen v4/v5/v6, and limitation
  boundaries; no immutable timing cohort was rerun or reinterpreted.

## Claim-Evidence Matrix

| Claim family | Status | Evidence basis | Scope boundary |
| --- | --- | --- | --- |
| Independent cascade exactness | Supported and unchanged | Ratio ordering, exact sorted DP, production probe, 256 exhaustive cases | History-independent action statistics; one action per expert |
| Adjacent-interaction exactness and hardness | Supported and unchanged | Bellman proof, 256 exhaustive interaction cases, 4,096-graph reduction audit | Adjacent cost effects only; history-independent acceptance |
| Exact reachable-state preflight | Supported | Closed-form state identity, `O(mk)` subset-count implementation, executed diagnostic agreement | Interaction recurrence only; count does not claim low wall time or memory |
| Production-shape tractability | Supported | 12 actions, three experts, four budgets, `k=3`, 49 states, 204 recursive transitions | Deterministic planning shape, not a timed workload |
| Larger executed stress profile | Supported | 32 actions, 16 experts, two budgets, `k=6`, 158,209 states, 1,412,192 transitions | Largest fully executed Round 51 profile; one implementation/host |
| One-million-state boundary | Supported with explicit distinction | 40-action exact count is 666,561; 44-action exact count is 1,227,425 and rejects with zero visited states | The 40-action profile is counted below the cap, not reported as a timed full execution |
| Frozen workload significance | Supported and unchanged | v5 negative replay and unique v6 first run | V6 is only 0.109% below fixed; no calibrated interaction transition is selected |

## Formal Audit

- For expert action counts `b_e`, every nonempty interaction state is identified by a
  used-expert set `S` and a previous action belonging to one expert in `S`. The exact
  unpruned state count is therefore
  `1 + sum_S sum_{e in S} b_e` over `1 <= |S| <= min(k,m)`.
- Every counted state is reachable by ordering the experts in `S` and placing the
  chosen previous action last. The production recurrence explores every feasible next
  action and performs no cost-based pruning, so the identity matches actual visits.
- For uniform `b`, the count reduces to
  `1 + b * sum_l l * binom(m,l)`. This independently yields 49 for `(m,b,k)=(3,4,3)`,
  158,209 for `(16,2,6)`, 666,561 for `(20,2,6)`, and 1,227,425 for `(22,2,6)`.
- The preflight implementation maintains subset counts and previous-action weights by
  subset size in descending order, requiring `O(mk)` scalar operations and `O(k)`
  storage. Saturating addition/multiplication conservatively reject arithmetic overflow.
- The state-cap test occurs before the memo table and recurrence traversal. The claim
  was narrowed from “before allocation” to “before any DP-state visit,” which is exactly
  what diagnostics and the verifier establish.
- Existing Bellman exactness, NP membership, Hamiltonian reduction, and 63-expert mask
  bounds remain unchanged.

## Numeric Consistency Findings

- The deployed-shape row records 12 actions, 96 complete cross-expert conditional
  transitions, 49 interaction states, 204 recursive transitions, 156 memo hits, and 12
  capacity-terminal states.
- The largest executed uniform row records 32 actions, 960 calibrated transitions,
  158,209 interaction states, 1,412,192 recursive transitions, 1,253,984 memo hits, and
  96,096 capacity-terminal states.
- The graph identity
  `recursive_transitions = visited_states - 1 + memo_hits` holds for every executed
  interaction profile.
- Exact-limit execution succeeds when `maximum_states` equals the predicted state
  count; count-minus-one rejects before the first state visit for every executed profile.
- The three prefrozen preflight rows are 338,473 states at 18 experts, 666,561 at 20,
  and 1,227,425 at 22. The 22-expert row is rejected by the production one-million cap.
- `paper/check_evidence.py` reproduces every manuscript macro from evidence and rejects
  any changed field or scope boundary.
- The synchronized PDF has 12 pages, 324,118 bytes, and SHA-256
  `d27da3ed8e58c179168ab91f03c853be77270f72f8a6d9c554332ee275420685`.

## Citation Metadata And Context

- Round 51 introduces no new literature claim or citation. The exact state-count
  identity is derived for the paper's own unpruned Bellman implementation and is not
  presented as a field-first contribution.
- All 21 existing cited keys remain defined; the PDF build reports no undefined
  citations or references.
- Prior positioning remains conservative: ratio ordering, test sequencing,
  transition-aware sequencing, dynamic programming, and component solvers are prior
  art.

## Implementation And Artifact Findings

- Diagnostics are optional trailing API parameters, preserving existing callers and
  behavior when omitted.
- Empty interaction calibration delegates to the independent optimizer with the same
  diagnostics object.
- Input and conditional-calibration validation precede the exact preflight; invalid
  calibrations cannot be hidden by an oversized state space.
- The prefrozen contract fixes all profile sizes, budget multiplicity, top-`k` rule,
  complete cross-expert transitions, cap, metrics, and exclusions before execution.
- The study executes no numerical solver, searches no cohort, tunes no policy, and
  reports no wall-time or memory result.
- Release build and all 29 CTests pass after the production recurrence change.

## Severity

- Fatal: none.
- Major integrity defect: none.
- Moderate: none after narrowing “before allocation” to “before any DP-state visit.”
- Residual limitation: deterministic state/transition counts do not establish runtime,
  memory consumption, workload gain, cross-host behavior, or independent reproduction.

## Safe Edit Suggestions

- Preserve “planning-only,” “no timing claim,” and “before any DP-state visit” in all
  future summaries.
- Do not describe the 40-action profile as fully executed; only its exact preflight
  count and below-cap status are established.
- Keep frozen v5 negative evidence, the 0.109% v6 effect, and zero selected calibrated
  interaction transitions prominent.

## Next CCFA Owner

`ccf-paper-reviewer` for the Round 51 decision panel. Any future score movement on
significance requires a prefrozen external evidence campaign rather than further
wording changes.

## No-Invention Status

Pass. No author, affiliation, disclosure, funding, conflict, acknowledgment, external
host, independent reproduction, runtime, memory, or workload-gain fact was invented.

