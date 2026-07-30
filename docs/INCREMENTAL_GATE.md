# Experimental Incremental Gate

`smave::ExperimentalIncrementalGate` is a research-only gate path for repeated evaluation of an exactly identical immutable input. It does not replace `Runtime::evaluate_gate`, is not part of the stable C ABI, and must not be described as a general approximate or sampled residual gate.

## Contract

- The gate copies caller values into a library-owned `ImmutableGateInput`; callers cannot mutate the certified input after issuance.
- A new input object, a foreign-session input, or a change in `direct_permission` always executes the strict per-request FP64 original-expression gate.
- A repeated object may reuse the most recent strict aggregate decision and `residual_inf`; full residual vectors are not cached or returned on a reuse hit.
- Every configured interval performs a complete strict verification. A decision or `residual_inf` mismatch rejects the request and invalidates the cached certificate.
- `revoke()` invalidates the cached certificate; the next request requires a complete strict verification.
- NaN, infinity, missing unknowns, and declared bound violations are rejected before residual evaluation.
- Calls are mutex-serialized. This establishes thread safety for the experiment, not scalable production concurrency.

## Full Runtime Experiment

`Runtime` also accepts an explicitly injected `ExperimentalExactCandidateGatePolicy`.
The default constructor remains strict per request. The experimental policy only reuses a
certificate when the block, Direct permission, and every candidate value are bitwise identical.
Any value change executes the strict gate; every 16 repeats force a strict recheck, drift rejects
and revokes the certificate, and callers may explicitly call `revoke()`. Certificates hold a
non-reusable shared Runtime identity rather than a raw address, so allocator address reuse cannot
carry a certificate into a reconstructed model, artifact bundle, routing configuration, or
tolerance contract.

`build/release/risk-adaptive-gate/evidence.txt` includes a linear Operator, a small nonlinear
CubicCoupled control, and a generated 64-variable single-SCC nonlinear workload. Each uses 100
paired repetitions of 32 complete solves and a
fixed-seed 10,000-sample bootstrap for solve-internal total and gate timing. The verifier requires
all gate-time `95%` lower bounds above one, positive full-solve total intervals for the linear and
scaled nonlinear workloads, and exact agreement in success, path, plan, solution, gate decision,
and gate residual. The tiny nonlinear workload retains its total-time interval when machine load
makes it cross one; that control result is not hidden or used as the required second workload.
Concrete speedups remain machine-specific.

## Excluded Claims

The current evidence does not cover approximately equal inputs, mutable buffers, cross-artifact or cross-tolerance reuse, probabilistic residual sampling, ODE/DAE trajectories, event semantics, cross-process certificates, or production concurrency. Input issuance and copying are excluded from gate-only timing because issuance represents scenario preparation rather than a reuse lookup. Full-Runtime timing ends before trace I/O, so it is not a client total-cost or deployment-risk result.

The local stress probe runs eight threads with 128 complete solves per thread while another thread
issues 64 revocations. It requires exact authority equivalence, nonzero strict checks and reuse,
nonzero revocations, and zero periodic drift. A reconstruction probe destroys the first Runtime and
policy, creates a new process-local policy, and requires the first request to be strict again. These
tests cover mutex safety and fail-closed local reconstruction. The signed local `ReleaseStore` now
has a separate kernel-locked, crash-recoverable version transaction contract documented in
`docs/RELEASE_STORE.md`, but incremental-gate certificates remain process-local and are not restored
from that store. Real artifact-weight hot swap, cross-process certificate coordination, persistent
certificate recovery, and distributed deployment remain unsupported.

Reproduce the experiment with:

```bash
cmake --build build/release --target reproduce-risk-adaptive-gate -j2
```

The authoritative report is `build/release/risk-adaptive-gate/evidence.txt`; it must retain `deployment_promoted=0` until multi-process concurrency, real artifact/version hot swap, persistent crash recovery, and statistical-risk gates are satisfied.
