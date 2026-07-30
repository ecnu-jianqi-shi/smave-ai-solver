# SuiteSparse Final Held-Out V2 Freeze

This file freezes the second final request-conditioned routing evaluation set
before any solver action timing is run on these six matrices.

## Selection contract

- Source index: the official SuiteSparse `ssstats.csv`, dated
  `31-Oct-2023 18:12:37`, downloaded on 2026-07-27; byte size `254328`, SHA-256
  `9bc797ab989331afbc9e0e51236d9145c2ac7f76c0913e9677ae07c4913a2aad`.
- Eligibility: real, square main matrices with coordinate storage and 512--5000
  rows; records described as duplicate problems or graphs are excluded.
- Leakage control: all collection groups used by training, calibration, the first
  development split, or held-out v1 are excluded. The six v2 groups are mutually
  distinct.
- Deterministic advancement: within each Cartesian cell of two row bands
  (`512--2047`, `2048--5000`) and three numeric classes (`SPD`, `symmetric
  non-SPD`, `nonsymmetric`), choose the lowest-row eligible record after applying
  the group exclusions and requiring a group not already selected for v2.
- Classification: direct numeric symmetry inspection is required. SPD additionally
  requires successful Accelerate sparse Cholesky plus the original-equation
  residual gate; symmetric non-SPD requires numeric symmetry and failed Cholesky
  factorization. No candidate-action timing, routing regret, pass-rate observation,
  or terminal-cascade measurement is used in selection.
- Frozen payloads: exact names, classes, dimensions, expanded nonzero counts, and
  SHA-256 values are recorded in `suitesparse-final-heldout-v2.tsv`; acquisition
  metadata is recorded in `suitesparse.tsv`.

The v1 matrices (`plbuckle`, `Si2`, `rotor2`, `ex10`, `laser`, and `Chebyshev2`)
became development data after `laser` exposed the missing identity-GMRES terminal
path. No v1 metric is admissible as final unseen evidence.
