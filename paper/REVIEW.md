# Solver-Focused Review Status

The full review history is stored in `../ccfa-review-reports/`.

## Current Calibrated Stance

- Latest completed review: `round-53-full-review.md`.
- Integrity audit: `round-53-integrity-audit.md`.
- Review date: **2026-07-29**.
- Overall score: **9/10, strong accept**.
- Confidence: **5/5**.
- Originality: **5/5**.
- Soundness: **5/5**.
- Evidence: **5/5**.
- Significance: **4/5**.
- Clarity, reproducibility, and limitations: **5/5**.

Round 53 preserves the exact interaction and tractability results and closes the only
local Round 52 diagnostic concern. Frozen model reconstruction is byte-identical. All
32 development-supported ordered GMRES ILU0/ILUT pairs are present among modeled
alternatives but disappear at unguarded top-3 selection. Five other unguarded candidate
identities occur in each version, none development-supported; the exact control-aware
route removes all, leaving zero final candidates and conditional timings. The result
diagnoses policy exposure without identifying a multiplier, policy gain, or interaction
benefit.

## Scientific Boundary

- `K/p` ordering, transition-aware test sequencing, and dynamic programming are prior
  art and are not claimed as first.
- The exact interaction model permits adjacent cost effects only; acceptance remains
  history-independent, and longer-memory effects are excluded.
- The exact preflight is a state-count/admission result, not a wall-time, memory,
  numerical-solver, workload-speedup, or external-validation claim.
- The 64-bit implementation supports at most 63 experts and rejects on state-cap
  overflow rather than returning an approximate plan.
- Every successfully returned result still passes the original-equation gate.

## Final Routing Evidence

1. V4 remains contract-invalid.
2. Frozen v5 remains a valid negative first run; the zero-execution current-policy
   replay switches 0/24 requests and remains `1.718370902×` global fixed.
3. Unique prefrozen v6 reaches `1.000030794×` oracle regret versus
   `1.001122945×` fixed and `1.756464745×` static: only `0.109%` below fixed.
4. V6 has 24/24 production successes and zero gate/order/DP mismatch, but selects no
   calibrated interaction transition.
5. V5/v6 each contain three held-out matrices and 24 requests on one Apple M4; neither
   is a population estimate or general routing-repair claim.
6. The Round 52 support-shift audit and Round 53 training-only attrition audit execute no
   solver and perform no policy tuning, cohort search, or conditional timing inference;
   held-out requests are excluded from every Round 53 count.

## Why the Overall Score Remains 9/10

The formal, implementation, and manuscript axes support full marks, but the decisive
significance axis does not. A defensible 10/10 requires independently prefrozen multi-host cohorts
showing a material repeatable gain over strong fixed controls, including real workloads
where interaction transitions are selected, plus public third-party reproduction.
Further proofs, wording changes, or synthetic property cases cannot substitute for
that external evidence.

## Current Validation Status

- Release configure/build and all 29 CTests pass.
- Independent and interaction property sweeps have zero maximum exhaustive-oracle gap.
- All 4,096 four-vertex graph reductions match the Hamiltonian threshold.
- The prefrozen Round 51 scaling target matches exact state/transition identities on
  nine executed profiles and performs three zero-visit cap rejections.
- The post-hoc Round 52 target reproduces 32 development pairs, zero candidates/timings,
  zero development--held-out overlap, and held-out Jaccard `0.143` from immutable v5/v6
  observations.
- The Round 53 target reconstructs both models byte-for-byte, reproduces all 32 supported
  pairs, localizes all 32 losses to unguarded top-3 selection, records five unsupported
  unguarded candidates per version, and verifies zero final candidates.
- `paper/check_evidence.py`, the artifact manifest checker, and the 12-page PDF build
  pass.
- The final deterministic archive is refreshed after the Round 53 reports; its external
  clean-tree evidence verifies frozen v6 and the zero-execution v5 replay without
  overwriting first-run directories.
- Verified author/disclosure/anonymity metadata remain unavailable and are not invented.
