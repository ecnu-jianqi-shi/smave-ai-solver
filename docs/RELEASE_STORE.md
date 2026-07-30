# Release Store Transaction Contract

`smave::ReleaseStore` is the local authority for signed immutable release payloads. It serializes activation and rollback across processes without weakening the runtime residual gate.

## Commit protocol

1. Verify the signed manifest, audit chain, bundle, model, expert, certificate, and optional dataset manifest before copying.
2. Copy into a private staging directory, verify the copied payload again, then atomically rename it into `releases/<release-id>`.
3. Write an immutable, checksummed and HMAC-authenticated `state-history/<generation>.state` record and persist it before replacing the `state` pointer.
4. On status, activation, or rollback, recover a missing, torn, or stale `state` pointer from the highest valid committed history generation.

If a process exits after the immutable release directory is committed but before state commit, retrying the same signed activation verifies and adopts that directory. A different payload using the same release id remains rejected.

## Process coordination

Release mutations use an operating-system file lock at `.release-lock-v2`. POSIX `flock` and Windows exclusive file handles are released by the kernel when a process exits, so a crashed owner cannot leave a permanent lock directory. Concurrent mutations wait in the kernel and each reread the latest committed generation while holding the lock.

Readers use `verified_active()` to capture the checksummed state, signed manifest, and immutable release directory from one generation while holding the same lock. `solve-release` consumes this snapshot instead of separately verifying one version and then re-reading a possibly switched active directory.

## Failure policy

- A corrupt primary `state` is repaired from committed history.
- A corrupt committed history generation or divergent records for one generation fail closed.
- Recomputing the public state hash cannot authorize a rollback: production verification and rollback also validate the generation signature and key id.
- Signed payload verification remains mandatory after state recovery.
- History records and release directories are immutable; recovery never overwrites a different signed release.

Legacy state schema v1 remains parseable for offline inspection and migration tooling, but `verified_active()` and rollback reject it because it has no transition signature. Activating a newly signed release writes schema v2 state history.

## Verification

```bash
ctest --test-dir build/release -R smave_release_store_recovery --output-on-failure
```

The focused test covers a torn primary state, retry after payload-directory commit, abrupt lock-owner exit, two concurrent rollback processes without lost generations, legacy-state production rejection, and a forged rollback that recomputes the public hash but cannot forge the state signature.

This contract is local-filesystem recovery, not distributed consensus, remote object-store coordination, public-key signing, KMS/HSM integration, certificate sharing for the experimental incremental gate, or a complete production control plane.
