# SuiteSparse Final Held-Out V4 Selection Freeze

This file freezes the v4 request-conditioned routing evaluation set before matrix
download, direct matrix inspection, solver action execution, terminal cascade
execution, routing, pass-rate measurement, regret measurement, or timing.

## Source And Exclusion Boundary

- Official SuiteSparse `ssstats.csv`, dated `31-Oct-2023 18:12:37`.
- Retrieved on `2026-07-27`; byte size `254328` and SHA-256
  `9bc797ab989331afbc9e0e51236d9145c2ac7f76c0913e9677ae07c4913a2aad`.
- Every collection group already present in `benchmark/data-lock/suitesparse.tsv`
  is excluded. This includes all local training, calibration, development, v1, v2,
  and v3 held-out groups and is stricter than excluding only prior held-outs.

## Deterministic Selection Contract

1. Start from real, square records with 5001--10000 rows.
2. Exclude descriptions containing `graph` or `duplicate`.
3. Exclude every group in `benchmark/data-lock/suitesparse.tsv`.
4. Assign numeric class using only source metadata: SPD requires positive-definite
   and numerical-symmetry flags equal to one; symmetric non-SPD requires numerical
   symmetry without the SPD flag; all remaining records are nonsymmetric.
5. In class order SPD, symmetric non-SPD, and nonsymmetric, choose the lowest-row
   record whose group has not already been selected.
6. After acquisition, direct numeric inspection must confirm each class. A mismatch
   invalidates the freeze; it does not authorize post hoc replacement after timing.

The 5001-row lower bound is fixed because exhaustive historical-group exclusion
leaves no unseen metadata-SPD candidate in the prior 512--5000 interval. The v4 set
therefore tests structural scale extrapolation rather than repeating the inspected
v3 scale band.

The frozen selection is `suitesparse-final-heldout-v4-selection.tsv`. The acquired
payload lock is `suitesparse-final-heldout-v4-payload.tsv`, and the post-inspection
freeze is `suitesparse-final-heldout-v4.tsv`.

The pre-first-run structure audit reports one SPD, one symmetric non-SPD, and one
nonsymmetric matrix, zero unresolved symmetric matrices, zero development matrices,
and `performance_measurements=0`. Its immutable hashes are:

- selection manifest: `a9de5767f3e2caeb9fc899507ca424d7bea5c31ffdb5e82bf7432b38cb5e065d`;
- payload manifest: `a9aa227a4e4b8b8b18da01ddf6ea3e97d756eee214c898500af2df10d2eb19c7`;
- final freeze manifest: `6f76e57edc198d4b33a488c98587041b8b045fbf1676b5898a69c89b142f8753`;
- structure-audit evidence: `3bc8184962f2319932f24e5555c90183628a52dbcbbb2489deff050ef7fc10ef`;
- structure-audit table: `98b5d2aff57decf96e72f5bf2244505b3e429de29f092ab6a1659ad137a78802`.

No solver action, terminal cascade, routing, pass-rate, regret, interaction, or
timing experiment had run on these payloads when the final manifest was written.
