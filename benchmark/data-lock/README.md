# Benchmark Data Lock

This directory separates reproducible acquisition metadata from the large benchmark
payloads. The lock covers the exact files consumed by the manuscript's seven
PDEBench-derived workloads and 66 SuiteSparse systems, including six paired
right-hand-side files.

## Sources and licenses

- **PDEBench data**: DaRUS dataset `doi:10.18419/DARUS-2986`, released under
  [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). `pdebench.tsv`
  records the dataset version, official data-file IDs, official filenames, local
  compatibility paths, sizes, repository MD5 checksums, and locally locked SHA-256.
- **SuiteSparse matrices**: SuiteSparse Matrix Collection, cited by Davis and Hu,
  DOI `10.1145/2049662.2049663`. The collection's current license page states that
  matrices are CC BY 4.0 and asks redistributors to preserve embedded metadata.
  `suitesparse.tsv` records collection group/name, official detail and Matrix Market
  archive URLs, exact extracted paths, sizes, and SHA-256. The local `.mtx` files are
  preserved byte-for-byte; no matrix values or metadata were modified.
- **Routing holdouts**: v1--v3 are retained as development evidence. V4 is preserved
  as a contract-invalid first run; v5 is the valid negative final run; and v6 is the
  unique favorable prefrozen run. V5/v6 selection, payload, final, and pre-first-run
  contracts freeze the group-disjoint sets before action timing. No additional
  held-out cohort is permitted for this paper. The current-rule v5 replay reads frozen
  observations only and performs no solver execution. The Round 52 interaction-support
  audit is explicitly post hoc: it diagnoses stable isolated-failure pair support and
  development-to-heldout shift without inferring conditional costs or changing policy.
  The Round 53 attrition audit is also post hoc and zero execution: it reconstructs
  frozen training plans exactly, excludes held-out requests from all counts, and
  identifies the first eligibility stage at which each supported transition disappears.

The locks verify locally present data without redistributing the roughly 52 GB of
payloads in the core archive. Acquisition is still network- and upstream-dependent;
this is not an immutable mirror.

## Commands

Verify all currently locked data:

```sh
python3 benchmark/data-lock/verify_data_lock.py --root . \
  --output build/release/data-lock/evidence.txt --verify-upstream
```

Download missing PDEBench files with the existing resumable downloader, then verify:

```sh
benchmark/pdebench/download_parallel_resume.sh
python3 benchmark/data-lock/verify_data_lock.py --root . --output build/release/data-lock/evidence.txt
```

Download a missing SuiteSparse system and its optional RHS into the lock path:

```sh
python3 benchmark/data-lock/acquire_suitesparse.py --root . --name west0479
```

The acquisition helper rejects archives whose extracted file set, sizes, or SHA-256
values differ from the lock. It does not bypass upstream license or availability
requirements.
