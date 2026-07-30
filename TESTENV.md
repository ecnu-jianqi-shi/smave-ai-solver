# Test Environment Inventory

## Available Hosts

### Primary Development Host
- **Type:** Apple M4 (local development machine)
- **Role:** Primary timing/benchmarking host for all Round 1-53 evidence
- **Status:** Active, all current evidence collected here
- **Performance baseline:** v6 cohort completes in 2-3 minutes
- **Constraints:** Single-host evidence only

### External Validation Host — DGX
- **Type:** Nvidia DGX Station (Spark)
- **Hostname:** mLab-HPC02
- **IP:** 10.111.100.16
- **Credentials:** `shijianqi` / SSH key installed (passwordless)
- **Architecture:** ARM64 (aarch64) — same ISA as M4
- **CPU:** 20 cores
- **GPU:** NVIDIA GB10 (Blackwell)
- **Memory:** 121GB (6.6GB free under load)
- **OS:** Ubuntu 24.04, Linux kernel 6.17
- **Compiler:** GCC 13.3.0
- **BLAS/LAPACK:** System default (not optimized)

**Round 55 validation results:**
- ✅ **Build status:** Successful (SMAVE Release compiled 100%)
- ✅ **Execution status:** Functional (v6 test started and ran)
- ❌ **Performance status:** **40-60× slower than M4**
- ❌ **Validation suitability:** Not practical for full v6 cohort

**Performance characteristics (from 1030 traces):**
- Training phase (570 solves): ~40 minutes
- Calibration phase (410 solves): ~30 minutes
- Held-out nd3k (50 solves): 20+ minutes before termination
- **Critical bottleneck:** nd3k matrix (49MB) with lsqr-identity taking 26 seconds per solve
- **Backend averages:** gmres-ilu0 1.4ms, pcg-ic0 17ms, lsqr-identity 3 seconds

**Why DGX is slow:**
- Server-grade ARM cores vs M4's high-performance cores
- Generic BLAS vs M4's Accelerate framework
- Memory bandwidth differences
- Not suitable for timing-sensitive benchmarks

**Added:** 2026-07-30, Round 55 multi-host campaign
**Status:** Available but not recommended for performance validation

## Multi-Host Evidence Gaps Assessment

With DGX performance issues identified, the following R54 concerns remain:

### R54-1: External Multi-Host Material Effect (Major) — ❌ NOT RESOLVED
- **Current status:** DGX validation attempted but blocked by 40-60× performance penalty
- **Blocking 10/10:** Yes
- **Now addressable:** ⚠️ Requires different host with comparable performance
- **Required work:**
  1. Identify x86-64 or high-performance ARM host with <5× M4 runtime
  2. Execute frozen v6 cohort (<15 minutes preferred)
  3. Compute DGX-only and cross-host regret vs fixed
  4. Document material effect threshold (e.g., "both hosts must exceed 1% improvement")
  5. Report as future multi-host validation campaign

### R54-3: Independent Reproduction (Moderate) — PARTIALLY ATTEMPTED
- **Current status:** DGX cross-host build successful but performance prevented completion
- **Blocking 10/10:** Partially (external confidence)
- **Now addressable:** ⚠️ DGX can serve as "independent host" for clean-tree rerun if runtime acceptable
- **Required work:**
  1. Transfer `smave-core-repro.tar.gz` to performance-suitable host
  2. Execute `artifact/verify_core_repro_bundle.py` 
  3. Run Release build + 29 CTests
  4. Document as "independent host clean rerun"

### R54-2: Real Interaction Benefit (Major) — NOT ATTEMPTED
- **Current status:** Zero final interaction candidates/timings (unchanged)
- **Blocking 10/10:** Yes
- **Now addressable:** ⚠️ Requires prefrozen conditional timing workloads (independent of host selection)

### R54-4: Submission Metadata (Minor) — NOT ATTEMPTED
- **Current status:** Placeholder author/affiliation info
- **Blocking 10/10:** No (administrative only)
- **Now addressable:** No — requires real author decision to disclose identity

## Recommended Next Steps

**For 9/10 submission (current state):**
- Document DGX attempt in limitations
- Submit with explicit single-host scope
- Acknowledge multi-host validation attempted but blocked by platform constraints

**For future 10/10 pursuit:**
- Obtain access to performance-comparable x86-64 or ARM host
- Execute v6 cohort in <15 minutes
- Demonstrate material effect (>1%) on both M4 and validation host

## Security Note

DGX SSH key-based authentication configured for project continuity. Access should be used only for:
- Authorized research reproduction
- Cross-host performance validation
- Clean-tree artifact verification

Do not use for:
- Modifying DGX system configuration
- Installing persistent services
- Accessing other users' data
- Exhausting shared compute resources
