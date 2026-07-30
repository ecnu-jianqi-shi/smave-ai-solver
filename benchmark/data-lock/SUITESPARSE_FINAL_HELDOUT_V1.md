# SuiteSparse Final Held-Out V1 Freeze

**Status after first execution:** development-only. The first run exposed that the
terminal numerical cascade lacked an identity-GMRES path for the consistent
singular `laser` system. The solver was changed in response, so no v1 timing or
routing metric is admissible as final unseen evidence.

This file freezes the final request-conditioned routing evaluation set before any
solver action timing is run on these matrices.

## Selection contract

- Source index: the official SuiteSparse `ssstats.csv`, dated
  `31-Oct-2023 18:12:37`, downloaded from
  `https://sparse.tamu.edu/files/ssstats.csv` on 2026-07-27; byte size `254328`,
  SHA-256 `9bc797ab989331afbc9e0e51236d9145c2ac7f76c0913e9677ae07c4913a2aad`.
- Eligibility: real, square matrices with 512--5000 rows; records described as
  duplicate problems or graphs are excluded.
- Leakage control: the entire collection groups used by training, calibration,
  or development (`Bai`, `Boeing`, `Gset`, `HB`, and `Nasa`) are excluded.
- Coverage: one matrix is frozen for each Cartesian cell of two row bands
  (`512--2047`, `2048--5000`) and three audited numeric classes (`SPD`,
  `symmetric non-SPD`, and `nonsymmetric`). All six selected matrices come from
  distinct collection groups.
- Classification: SuiteSparse symmetry/positive-definite metadata is checked by
  direct numeric symmetry inspection. SPD requires successful Accelerate sparse
  Cholesky plus the original-equation residual gate; symmetric non-SPD requires
  numeric symmetry and failed Cholesky factorization. No solver action timing,
  routing regret, or pass-rate observation is used to choose a matrix.
- Frozen payloads: exact names, classes, dimensions, expanded nonzero counts, and
  SHA-256 values are recorded in `suitesparse-final-heldout-v1.tsv`; acquisition
  metadata is recorded in `suitesparse.tsv`.

The prior `nasa4704`, `G26`, `rdb450`, and `fs_541_1` split was inspected during
router development and is also development-only.
