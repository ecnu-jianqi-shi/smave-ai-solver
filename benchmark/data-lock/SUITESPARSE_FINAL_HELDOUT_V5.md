# SuiteSparse Final Held-Out V5 Selection Freeze

This file freezes the v5 request-conditioned routing evaluation set before any
solver action, terminal cascade, routing, pass-rate, regret, interaction, or timing
measurement on the selected payloads.

## Source And Exclusion Boundary

- Official SuiteSparse `ssstats.csv`, dated `31-Oct-2023 18:12:37`.
- Retrieved on `2026-07-27`; byte size `254328` and SHA-256
  `9bc797ab989331afbc9e0e51236d9145c2ac7f76c0913e9677ae07c4913a2aad`.
- Every collection group present in `benchmark/data-lock/suitesparse.tsv` before
  the v5 rows were appended is excluded, including the measured but contract-failed
  v4 groups.

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

The frozen selection is `suitesparse-final-heldout-v5-selection.tsv`. The acquired
payload lock is `suitesparse-final-heldout-v5-payload.tsv`, and the post-inspection
freeze is `suitesparse-final-heldout-v5.tsv`.

The pre-first-run structure audit reports one SPD, one symmetric non-SPD, and one
nonsymmetric matrix, zero unresolved symmetric matrices, zero development matrices,
and `performance_measurements=0`. Its immutable hashes are:

- selection manifest: `a577561fdae10d9a2a835cdd9aa75da3ce2e2847fda101240e7ae36f1f96637f`;
- payload manifest: `f5310bcd7e5f28e5a178688235f78385c827a657e256441b0e1f4f03e82658b1`;
- final freeze manifest: `2faeecae412571d76321ef481b143c12e26f252ae099c84ec9992d211f4368c5`;
- structure-audit evidence: `8510afe254c1c1a3797e753f646cfecf62255adbb2f4d2408ea9073906452d54`;
- structure-audit table: `3165f61efb04560a8fa5d6676fab053c4fecfa63ff4bf6965ba27aa66af5af10`.

No solver action, terminal cascade, routing, pass-rate, regret, interaction, or
timing experiment had run on these payloads when the final manifest was written.
