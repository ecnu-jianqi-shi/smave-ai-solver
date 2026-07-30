# Core Reproduction Bundle

This bundle supports an author-operated clean extracted-tree build, the 29-test
CTest suite, fixed-cascade exhaustive ordering, independent and interaction-aware
property sweeps, the Hamiltonian reduction audit, manuscript checks, and PDF rebuilding.
It is deliberately smaller than the working tree. Legacy service
and release-engineering sources may remain for regression compatibility, but they are
outside the paper's claim surface and no corresponding build evidence is bundled.

## Included

- Project, test, example, benchmark-harness, paper, and review sources.
- The GitHub-hosted native-performance workflow and its reject-on-invalid collection,
  raw-sample validation, replicate/campaign verification, aggregation, and synthetic-only
  contract-check sources; no hosted run artifact.
- The `west0479`, `nasa2910`, `laser`, `M10PI_n1`, and `TS` Matrix Market
  fixtures exercised by CTest.
- Pinned evidence summaries, the four-stage/24-permutation cascade-ordering result,
  the router size/fingerprint and conditioning/topology shift analyses, the two-family
  layered complete-cost decomposition, correction-budget frontier, calibrated budget
  propagation evidence, exact joint expert--budget routing evidence with its
  independent exhaustive oracle, two deterministic property sweeps, and 4,096-graph
  hardness audit, the prefrozen planning-only state/transition scaling study and
  exact zero-DP-state-visit cap preflight, the post-hoc zero-execution frozen interaction-support
  and development-to-heldout shift audit, the exact frozen-plan transition attrition
  audit, held-out joint
  expert--budget shift evidence, request-conditioned 12-action routing evidence with
  frozen model parameters and 5,760 action observations, the preserved v4 contract
  failure, v5 negative first run, unique v6 first run, deterministic zero-execution
  v5/v6 control-aware replays, and the 651 raw
  reports covered by `paper/ARTIFACT_MANIFEST.txt`.
- The local native-performance dry-run evidence package, verified as local-only during
  clean extraction; it remains non-external performance evidence.
- Frozen native HINTS evidence, official and SMAVE raw samples, and the exact exported
  workload contract. The external HINTS checkout and PyTorch environment are not
  redistributed or executed by the core bundle.
- The reviewed `paper/main.pdf` and generated manuscript inputs.

## Excluded

- PDEBench datasets (`benchmark/pdebench`, approximately 47 GB).
- SuiteSparse benchmark matrices other than the five CTest fixtures
  (`benchmark/suitesparse`, approximately 5.1 GB in the working tree).
- Build products and unpinned exploratory outputs not named by the bundle
  allowlist.
- Locally installed optional solver, HDF5, Docker, and TeX dependencies.
- The official HINTS source checkout, pretrained artifact environment, and PyTorch
  installation; reproducing that baseline requires the separately pinned public code.

The explicit CMake option `SMAVE_CORE_REPRO_BUNDLE=ON` changes only the sparse
fixture discovery assertion from the full local inventory of 66 system matrices
to the five included parser and verified-fallback fixtures. It does not represent reproduction of the
excluded large-data benchmark campaigns.

## Verification

From the repository containing the archive:

```sh
python3 artifact/verify_core_repro_bundle.py \
  build/core-repro-bundle/smave-core-repro.tar.gz
```

The verifier checks the archive sidecar checksum and internal file manifest,
extracts into a new temporary directory, configures a Release build with the
bundle option, builds, runs all 29 CTests, reruns the independent exhaustive
cascade-ordering target, restores the four frozen analysis inputs into the clean build tree,
reruns router-shift, joint-route-budget-shift, request-conditioned joint routing, and
complete-cost-decomposition, reruns the prefrozen joint-route scaling study, verifies
the frozen interaction-support and transition-attrition audits, and verifies the frozen v6 SuiteSparse evidence without
re-executing a solver, reruns the deterministic v5 replay and requires byte-identical
evidence and decisions, verifies the bundled
joint expert--budget optimizers against independent exhaustive oracles, verifies both
256-case finite-action property sweeps and the 4,096-graph reduction audit,
local-only native-performance evidence, runs the synthetic-only campaign contract
check, checks the paper evidence and artifact manifest, and rebuilds the PDF when
`latexmk` is available.

The verifier checks the frozen native HINTS evidence contract but does not rerun the
external implementation (`external_public_code_required=1`, `core_bundle_rerun=0`).

For the synchronized final source, the focused cascade-ordering,
conditioning/topology-shift, calibrated correction-budget, exact joint expert--budget
Router, request-conditioned cost/pass model, frozen v6 verifier, and zero-execution
v5 replay checks pass, and the complete clean extraction
is required to pass 29/29 plus every downstream verifier. The default clean build excludes
non-solver infrastructure evidence executables, and no corresponding build evidence is
present in the archive. The machine record is
`build/core-repro-bundle/clean-tree-evidence.txt`.

This is an author-operated local rerun. It is not a public immutable archive,
independent reproduction, or native external performance result.
