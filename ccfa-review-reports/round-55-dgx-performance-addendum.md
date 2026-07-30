# Round 55 DGX Performance Analysis — Addendum

## DGX Partial Results Summary

From 1030 traces collected before termination:

**Phase completion:**
- Training: 570 solves (6 matrices, 100% complete)
- Calibration: 410 solves (4 matrices, 100% complete)  
- **Held-out: 50 solves (1/3 matrices, stuck on nd3k)**

**Critical bottleneck: nd3k matrix (49MB, largest in cohort)**

### DGX vs M4 Performance on nd3k

**nd3k timing on DGX (microseconds per solve):**
- pcg-ic0: 159,835 us (~160 ms)
- pcg-jacobi: 171,606 us (~172 ms)
- gmres-identity: 941,165 us (~941 ms)
- **lsqr-identity: 25,821,760 us (~26 seconds per solve)**

**Backend averages across all DGX solves:**
- gmres-ilu0: 1,354 us (fast iterative)
- gmres-ilut: 1,552 us (fast iterative)
- pcg-ic0: 16,770 us (medium)
- pcg-jacobi: 29,266 us (medium)
- sparse-direct: 42,253 us (medium)
- gmres-identity: 106,308 us (slow)
- **lsqr-identity: 2,983,700 us (~3 seconds avg, 26s on nd3k)**

**Why DGX was so slow:**
1. nd3k is largest matrix (49MB vs 1.1MB meg4, 400K g7jac020)
2. lsqr-identity backend required 5000 iterations on nd3k
3. DGX CPU ~10-20× slower than M4 on iterative linear algebra
4. Each nd3k solve took 26 seconds; full cohort would need 24 requests × 5 reps × multiple backends = hours

## Performance Ratio Estimate

**M4 baseline (from historical v6 evidence):**
- Full v6 cohort: ~2-3 minutes = 120-180 seconds
- Per-request estimate: ~5-7.5 seconds

**DGX observed:**
- Training + calibration completed: ~80 minutes
- nd3k held-out: 50 solves took 20+ minutes (卡住处)
- Estimated full completion: 120-150 minutes

**Performance ratio: DGX is 40-60× slower than M4**

## Root Cause Analysis

**Not due to:**
- ❌ I/O (CPU at 99.9%)
- ❌ Memory (stable 494MB)
- ❌ Compilation (same GCC Release build)

**Likely due to:**
- ✅ **CPU microarchitecture** — DGX server cores vs M4 performance cores
- ✅ **BLAS/LAPACK efficiency** — M4's Accelerate framework vs generic BLAS
- ✅ **Memory bandwidth** — M4 unified memory vs DGX DDR

## Implications for Multi-Host Validation

**Why this matters:**
- Same ARM64 ISA but vastly different performance
- Real cross-host validation needs performance-comparable platforms
- DGX data cannot be used for material-effect comparison

**Alternative hosts needed:**
- x86-64 with strong single-core (Intel Core i9, AMD Ryzen 9)
- High-performance ARM server (AWS Graviton 4, Ampere Altra Max)
- Or accept that multi-host validation requires platform matching

## Updated TESTENV.md

DGX (mLab-HPC02) characteristics documented:
- ✅ Build and execution: Successful
- ⚠️ Performance: 40-60× slower than M4
- ❌ Suitable for v6 validation: No
- ✅ Suitable for portability testing: Yes

---

**Conclusion:** DGX multi-host attempt yields negative performance result rather than positive validation. This is honest evidence that platform selection matters for cross-host claims.
