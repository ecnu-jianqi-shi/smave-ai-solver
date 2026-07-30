# Native External Performance Campaign

This campaign is a fail-closed enabler for performance measurements on GitHub-hosted
Ubuntu/x86-64 virtual machines. It does not convert local measurements, Docker runs,
or emulated x86-64 correctness checks into external performance evidence.

As of **2026-07-31**, the workflow has completed its first successful GitHub-hosted
campaign on `ecnu-jianqi-shi/smave-ai-solver` (run_id `30580876298`): three independent
`ubuntu-24.04` x86-64 jobs on Intel Xeon Platinum 8573C, AMD EPYC 7763, and AMD EPYC
9V74 runners, with aggregation, upload, and artifact attestation all succeeding. The
campaign verifier returns `SMAVE_NATIVE_EXTERNAL_PERFORMANCE_CAMPAIGN_CHECK 1`. The
hosted evidence covers only the self-contained gate and complete-path fixtures; it does
not extend to customer workloads, PDEBench payloads, accelerators, or NUMA performance.
A local dry-run remains available as a protocol self-check.

## Hosted workflow

The manually dispatched `.github/workflows/native-external-performance.yml` workflow:

1. checks out one GitHub commit in three independent `ubuntu-24.04` matrix jobs;
2. builds the self-contained Release targets natively on reported x86-64 runners;
3. runs fused-gate thread scaling, same-VM process scaling, risk-adaptive complete-path
   timing, and the second Operator-family replication;
4. preserves all positive and negative timing outcomes without a speedup threshold;
5. uploads each job's summaries, raw timing samples, hashes, and machine/provenance
   record;
6. validates common repository, commit, workflow-run, and workflow-ref provenance;
7. reports cross-job median, minimum, and maximum for the selected metrics;
8. independently verifies the exact aggregate schema, all referenced replicate files,
   replicate hashes and metadata, common provenance, and every cross-job statistic; and
9. requests a GitHub artifact attestation for the aggregate evidence manifest.

The workflow counts only when all measurement jobs, aggregation, upload, and attestation
steps succeed. A downloaded file without the successful GitHub run and its external
provenance is not equivalent evidence.

## Direction-agnostic contract

Correctness is mandatory, but performance direction is not:

- all original-equation gate decisions and residuals must remain equivalent;
- all Operator requests must retain zero failures, zero fallbacks, and the same accuracy;
- raw timing grids must be complete, finite, positive, and consistent with reported
  medians and speedups;
- a slowdown, confidence interval below one, or `stable_speedup=0` is retained rather
  than hidden or converted into a workflow failure; and
- no minimum speedup threshold gates artifact production.

Each replicate contains 1,900 raw timing values: 300 threaded-gate samples and 1,600
strict/adaptive offline and complete-path samples.
`benchmark/native_external_performance_contract.py` validates the exact grids and
recomputes the summary medians and reported ratios. The campaign verifier also checks
that every component stays beneath the evidence directory and matches its SHA-256.
`benchmark/verify_native_external_performance_campaign.py` then reconstructs the
aggregate from all three downloaded replicates. It rejects unreferenced or missing
replicates, path escapes, nonconsecutive IDs, digest or metadata drift, provenance drift,
cross-job statistic drift, and any unexpected campaign field.

The aggregate intentionally records `artifact_attestation_embedded=0`. The verifier can
prove that the pre-attestation manifest matches the downloaded artifacts; it cannot prove
that the later provider attestation step succeeded. That remains an external GitHub-run
check.

## Local dry-run

Use the local mode to validate build, collection, and rejection behavior:

```bash
benchmark/run_native_external_performance.sh \
  --provider local \
  --replicate-id 1 \
  --build build/native-external-dry-run \
  --output build/native-external-dry-run-output \
  --jobs 4

python3 benchmark/verify_native_external_performance.py \
  --evidence build/native-external-dry-run-output/evidence.txt \
  --expect-local
```

Local evidence must record:

```text
provider=local
external_provider=0
performance_evidence=0
native_external_performance=0
provider_hosted_vm=0
```

Running the same local evidence with `--expect-github-hosted` must fail. Supplying
GitHub-looking environment variables on the current ARM64 host must also fail because
the collector requires the machine itself to report `x86_64` or `amd64`.

## Hosted verification

After publishing a real commit to GitHub, dispatch
`smave-native-external-performance` from the Actions UI. For every downloaded replicate:

```bash
python3 benchmark/verify_native_external_performance.py \
  --evidence <replicate-directory>/evidence.txt \
  --expect-github-hosted
```

The aggregate job runs:

```bash
python3 benchmark/aggregate_native_external_performance.py \
  --input-root <download-root> \
  --output <summary-directory>/evidence.txt \
  --expected-replicates 3

python3 benchmark/verify_native_external_performance_campaign.py \
  --campaign <summary-directory>/evidence.txt \
  --input-root <download-root> \
  --expected-replicates 3
```

The local artifact includes a reproducible parser/integrity contract test using
explicitly synthetic hosted metadata:

```bash
python3 benchmark/check_native_external_performance_campaign_contract.py \
  --local-root build/native-external-dry-run-output
```

It validates one synthetic aggregate and requires metric, digest, schema, and provenance
tamper cases to fail. Its output states `synthetic=1` and `external_evidence=0`; it is a
software-contract test, not a hosted performance result.

Reviewers should verify the successful workflow URL, repository and full commit SHA,
job artifacts, aggregate manifest, and GitHub-provided attestation together. The current
local repository has no usable commit or configured remote, so it cannot produce that
provenance by itself.

## Claim boundary

A successful campaign can establish only three provider-hosted x86-64 VM executions of
the included self-contained workloads. It does **not** establish:

- bare-metal performance;
- PDEBench payload, customer, production, accelerator, or NUMA performance;
- independent reproduction by a separate organization;
- a public immutable archive or persistent identifier.

Accordingly, every replicate and aggregate keeps these non-claims machine-readable.
