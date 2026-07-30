# SuiteSparse Final Held-Out V6 Selection Freeze

This file freezes the v6 request-conditioned routing evaluation set after the
control-aware family-anchor method was finalized and before any solver action,
terminal cascade, routing, pass-rate, regret, interaction, or timing measurement
on the selected payloads.

## Source And Exclusion Boundary

- Official SuiteSparse `ssstats.csv`, dated `31-Oct-2023 18:12:37`.
- Retrieved on `2026-07-27`; byte size `254328` and SHA-256
  `9bc797ab989331afbc9e0e51236d9145c2ac7f76c0913e9677ae07c4913a2aad`.
- Every collection group present in `benchmark/data-lock/suitesparse.tsv` before
  the v6 rows were appended is excluded, including all v1--v5 development,
  failed, and evaluated cohorts.

## Deterministic Selection Contract

1. Start from real, square records with 5001--10000 rows.
2. Exclude descriptions containing `graph` or `duplicate`.
3. Exclude every previously locked collection group.
4. Assign numeric class using only source metadata: SPD requires positive-definite
   and numerical-symmetry flags equal to one; symmetric non-SPD requires numerical
   symmetry without the SPD flag; all remaining records are nonsymmetric.
5. In class order SPD, symmetric non-SPD, and nonsymmetric, choose the lowest-row
   record whose group has not already been selected.
6. After acquisition, direct numeric inspection must confirm each class. A mismatch
   invalidates the freeze and does not authorize post hoc replacement after timing.

The frozen selection is `suitesparse-final-heldout-v6-selection.tsv`. The acquired
payload lock is `suitesparse-final-heldout-v6-payload.tsv`, and the post-inspection
freeze is `suitesparse-final-heldout-v6.tsv`.

The pre-first-run structure audit reports one SPD, one symmetric non-SPD, and one
nonsymmetric matrix, zero unresolved symmetric matrices, zero development matrices,
and `performance_measurements=0`. Its immutable hashes are:

- selection manifest: `b845e43fd9c35bc4b455906a202d0d93bc341825e03794c97082d892f4e0584f`;
- payload manifest: `cf666e8645238635051efc5f226229c87d5bbc530f4801cd2979ef71861f002c`;
- final freeze manifest: `87c4772409407ada2bc7b6748360238ef295e5c31168d253cd8e9fa8b23d20ba`;
- structure-audit evidence: `8f231dd2ef13f0b4c5de89d9ea2cc8a3fc5a3d89d50e95b2f51ed14af6823aaa`;
- structure-audit table: `532d94c946b29de65ed91d418422429de76b938de4188465305ae8041a86c2f1`.

No solver action, terminal cascade, routing, pass-rate, regret, interaction, or
timing experiment had run on these payloads when the final manifest was written.
