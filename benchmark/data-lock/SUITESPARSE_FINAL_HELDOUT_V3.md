# SuiteSparse Final Held-Out V3 Selection Freeze

This file freezes the third and final request-conditioned routing evaluation set
before downloading its matrix payloads and before running any solver action,
terminal cascade, routing, pass-rate, regret, interaction, or timing experiment.

## Source index

- Official SuiteSparse `ssstats.csv`, dated `31-Oct-2023 18:12:37`.
- Retrieved on `2026-07-27` from `https://sparse.tamu.edu/files/ssstats.csv`.
- Byte size: `254328`.
- SHA-256: `9bc797ab989331afbc9e0e51236d9145c2ac7f76c0913e9677ae07c4913a2aad`.
- The bytes are identical to the index used for the v1 and v2 freezes.

## Deterministic selection contract

1. Start from real, square records with 512--5000 rows.
2. Exclude records whose description contains `graph` or `duplicate`.
3. Exclude every collection group already present in
   `benchmark/data-lock/suitesparse.tsv`; this is stricter than excluding only the
   request-conditioned routing matrices.
4. Assign an index class using only source metadata: SPD requires positive-definite
   and numerical-symmetry flags equal to one; symmetric non-SPD requires numerical
   symmetry equal to one without the positive-definite flag; all remaining eligible
   records are nonsymmetric.
5. Within each class, select the lowest-row record from each size band
   (`512--2047`, `2048--5000`) while requiring a new collection group.
6. If a class has no eligible record in one size band, fill the missing slot with the
   globally lowest-row remaining record in that class from a new group. This rule is
   used once: the index contains no eligible small-band SPD record after the group
   exclusions, so `sts4098` fills that slot after `bibd_81_2`.
7. Direct matrix inspection must confirm dimensions, numeric symmetry, and numerical
   class after acquisition. A metadata-class mismatch advances deterministically to
   the next eligible record under the same rule.

The frozen names and index metadata are recorded in
`suitesparse-final-heldout-v3-selection.tsv`. Matrix bytes and SHA-256 values are
recorded in `suitesparse-final-heldout-v3-payload.tsv`; post-parse dimensions,
expanded nonzero counts, and numeric classes are frozen in
`suitesparse-final-heldout-v3.tsv`.

The pre-experiment structure audit reports exactly two SPD, two symmetric non-SPD,
and two nonsymmetric matrices, zero unresolved symmetric matrices, zero development
matrices, and `performance_measurements=0`. Its SHA-256 locks are:

- selection manifest: `d83ff0f0862a1006b99c9e681cb8c8e6fc0ac855ac5f26174b026ed94d71cad5`;
- payload manifest: `14252c64e2656f890262bf8afbba6f6cf7381303db0f46d3f028dd5a592b94ee`;
- structure-audit evidence: `4e63b1fbed4986ed09d69c3d04201368ea8adcd536dae9079650ac34fc57c447`;
- structure-audit table: `7becf33d8812c84bf7ad427b7373efa9d1ab3a67beacf2eb1be658d1d281031f`.

No solver action, terminal cascade, routing, pass-rate, regret, interaction, or
timing experiment had run on these payloads when this final manifest was written.
v1 and v2 are development data and are inadmissible as unseen evidence.
