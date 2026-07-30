# SMAVE Benchmark Suite — Real-World Authoritative Test Sets

本目录保存权威测试资产及可复现执行入口。资产存在不等于已经完成 SMAVE 对照；任何“整体 benchmark 已完成”的结论都必须以 `build/release/benchmark-*/summary.txt` 为准。

**Goal**: Provide 10w-unknown-scale (≥ 100,000) complex equation systems from multiple physics domains to evaluate SMAVE AI Solver against industrial and academic standards.

## Quick Summary

| Test Set | Count | Size | Complexity | Top-Venue Papers Using It |
|---|---:|---:|---|---|
| **SuiteSparse Matrix Collection** | 39 systems + 6 RHS | 3.83 GB | real SPD/nonsymmetric/circuit/structural/CFD/reservoir | Davis&Hu (ACM TOMS 2011), Schenk&Gärtner (FGCS 2006 PARDISO), Amestoy et al. (SIMAX 2001 MUMPS), Li&Demmel (ACM TOMS 2003 SuperLU), Karypis&Kumar (SISC 1998 METIS) |
| **COPS 3.0** (Argonne) | 23 problems | 5.3 MB | nonlinear PDEs + optimization | Dolan&Moré (Math. Prog. 2002), Moré&Wild (SIOPT 2009), Conn et al. (Math. Prog. 2009) |
| **PDEBench** (ICLR 2023) | 7 PDE families | 50.94 GB | 1D/2D time-dependent PDEs | Takamoto et al. (ICLR 2023), Li et al. (ICLR 2021 FNO), Cao (ICLR 2023 Transolver), Li et al. (NeurIPS 2023 GNOT) |
| **PETSc TS** | 27 examples | 368 KB | DAE/ODE/PDE time-stepping | Balay et al. (ACM TOMS 1997, 2024 PETSc manual), Munson et al. (SISC 2012), Brown et al. (ACM TOMS 2012) |
| **Multiphysics MSL** | 7 examples | 58 KB | cross-domain coupling (thermo-electrical, electro-mechanical, fluid-thermal, magneto-mechanical) | Fritzson et al. (ACM TOMACS 2020), Modelica Conference, Cellier&Kofman (Springer 2006) |

SuiteSparse 数据锁包含 72 个 `.mtx` 资产：66 个系统矩阵和 6 个 RHS；COPS 当前是 22 个模型、68 个参数实例。PDEBench 清单对应约 51 GB 权威原文件，仓库中原有的 7 个小文件只是被截断的前缀，不能作为有效 HDF5 benchmark 输入。

## Executed Evidence (2026-07-20)

机器可读总报告 `build/release/benchmark-overall/summary.txt` 当前为 `OVERALL_SMAVE_VS_TRADITIONAL_COMPLETE 1`。完成语义是：清单中的案例和权威资产均已执行/验证，所有双方共同成功且适用的案例都完成同输入性能比较；失败、timeout、fallback-only、无适用调用和 no-common-success 仍保留为结果，不能解释为性能胜出。

100× 性能目标由独立的 `benchmark-100x-gate` 检查。截至 2026-07-20 的当前权威结果为 `1/7`：1D CFD `111.13×`、Darcy `4.33×`、shallow-water `2.58×`、2D NS `2.48×`、Burgers `2.36×`、Diffusion-Sorption `1.47×`、Advection `1.84×`。因此 benchmark 执行状态完整，但全量 100× 目标仍明确未完成。

| Family | Current authoritative result | Performance scope and limits |
|---|---|---|
| PETSc TS | 27/27 baseline；17 个独立方程案例和 10 个框架/布局自测完成分类；17/17 方程对照通过 | 17/17 报告 solve-time；10 个自测不制造重复速度比 |
| OpenModelica MSL | 7/7 轨迹与端到端对照；6 个适用模型共 535,728 次 SMAVE 调用、0 fallback | 7/7 报告端到端墙钟；无调用模型不计 SMAVE 内核性能 |
| COPS | Julia/Ipopt baseline 68/68；KKT 线性层 68 agreement/68 性能比较；MadNLP full-NLP 68/68 attempted、57 agreement | full-NLP 仅 12 个非 fallback-only 案例形成原生 SMAVE 性能比较；5 timeout、46 fallback-only、44 resource-gated、37,908 external fallback 明确排除 |
| SuiteSparse | 39/39 checkpoint、31 agreement、31 性能比较、8 no-common-success、0 invalid asset | 8 例双方无共同成功，不报告速度比；31 个可比案例保留 residual、已知解误差、时间、内存、迭代 |
| PDEBench | 7/7 文件通过 size+MD5+h5dump；七族对比全部完成 | solves：Advection 150、Burgers 150、Diffusion-Sorption 150、Darcy 3、shallow-water 60、2D NS 40、1D CFD 90；CFD 达到 100×，其余六族均超过 1×但未达到 100× |

PDEBench shallow-water、2D NS 和 1D CFD 的证据分别限定为线性化标量重力波、隐式黏性速度 Helmholtz、三场隐式耗散 Helmholtz 子系统；它们使用权威数据中的真实场作为 RHS，但不冒充完整 SWE/NS/可压缩 CFD 轨迹复现。所有性能字段排除 HDF5 I/O，并分别计量 traditional solve 与 SMAVE solve；speedup 小于 1 表示 SMAVE 更慢。

### Native external performance enabler

`run_native_external_performance.sh` 将 fused-gate thread scaling、同 VM 的
process scaling、risk-adaptive complete-path timing 和第二 Operator 家族复现组合成
一个来源绑定的 campaign。每个 replicate 保存 2,140 个原始 timing 值；
`native_external_performance_contract.py` 要求 raw grid 完整、有限、为正，并重算
summary median/ratio，`verify_native_external_performance.py` 进一步检查路径不能逃逸
artifact 根目录且所有文件 SHA-256 一致。`aggregate_native_external_performance.py`
只接受同一 GitHub repository、完整 commit、run、attempt 和 workflow ref 下的三个
连续编号 replicate，并保留跨 job median/min/max，不设置任何 speedup 成功阈值。
`verify_native_external_performance_campaign.py` 在上传前独立重建 exact aggregate
schema、replicate 文件集合、hash/metadata、共同 provenance 和全部统计；
`check_native_external_performance_campaign_contract.py` 使用显式 synthetic metadata
复验正常聚合，并要求 metric、digest、schema 和 provenance 四类篡改失败。该检查
始终标记 `external_evidence=0`，也不能证明后续 provider attestation 已成功。

`.github/workflows/native-external-performance.yml` 仅支持手动触发并要求 GitHub
托管 `ubuntu-24.04`、Linux/X64 上下文；本地模式始终记录
`performance_evidence=0` 和 `native_external_performance=0`。截至 2026-07-25
尚无真实 hosted run，因此它只是可验证的执行入口，不是新的外部性能结果，也不
证明 bare metal、物理 host 独立、PDEBench payload/customer workload、GPU/NUMA、
分布式性能、独立复现或公共不可变归档。完整协议见
[`../docs/NATIVE_EXTERNAL_PERFORMANCE.md`](../docs/NATIVE_EXTERNAL_PERFORMANCE.md)。

`cmake --build build/release --target pdebench-training-smoke` 会从六个尚未达到 100× 的权威文件提取统一的 FP32 `input/target` 连续 tensor、有限值检查和确定性 FNV-1a 校验和。Advection、Burgers、Diffusion-Sorption 输出 1D 场，Darcy 与 shallow-water 输出单通道 2D 场，NS 输出双速度通道。manifest 明确写入 `TARGET_KIND "authoritative-next-state-pretraining"` 与 `SOLVER_LABEL 0`：这些对只用于预训练/表征学习，不能冒充当前 benchmark 离散方程的 solver label；部署前必须用相同离散算子生成 solver label，并重新通过 FP64 原 residual、交叉求解和 100× 独立门禁。

## Directory Layout

```text
benchmark/
├── suitesparse/                  # 46 industrial sparse matrices (2.1 GB)
│   ├── small/                    # 11 matrices: Harwell-Boeing classics (west0479, fs_541_1, nasa2910, etc.)
│   ├── medium/                   # 10 matrices: Boeing bcsstk39, DNVS ship_001, Wang wang3
│   └── large/                    # 24 matrices: 10w+ (StocF-1465 1.46M, Freescale1 3.4M, rajat31 4.7M, invextr1_new 305k, Hamm add20/add32, Gset G10/G26, Bai bfwb398, etc.)
├── cops/                         # COPS 3.0 Argonne nonlinear optimization (5.3 MB)
│   └── cops-3.0/                 # 23 AMPL models: torsion, bearing, camshape, catmix, chain, channel, dirichlet, elec, gasoil, glider, henon, lane_emden, marine, methanol, minsurf, pinene, polygon, robot, rocket, steering, tetra, triangle, etc.
├── pdebench/                     # PDEBench ICLR 2023 (75 MB, 7 families)
│   ├── 1D/                       # Advection, Burgers, CFD, diffusion-sorption (.hdf5/.h5, 35 MB)
│   └── 2D/                       # DarcyFlow, NS_incom, shallow-water (.h5, 40 MB)
├── petsc-ts/                     # PETSc Time Stepping examples (368 KB, 27 files)
│   └── ex*.c                     # ex2–ex81: linear ODE, Bratu, advection-diffusion, DAE, chemical kinetics, porous medium, etc.
└── multiphysics-msl/             # Modelica Standard Library multiphysics coupling (58 KB, 7 examples)
    └── *.mo                      # HeatingMOSInverter (thermo-electrical), DCPM_Temperature (electro-mechanical-thermal), HeatExchanger (fluid-thermal), MovingCoilActuator (magneto-mechanical)
```

## 1. SuiteSparse Matrix Collection

**Source**: https://sparse.tamu.edu  
**License**: CC BY 4.0 for the matrices, with embedded source-specific metadata and
citation instructions preserved byte-for-byte  
**Citation**: Timothy A. Davis and Yifan Hu. "The University of Florida Sparse Matrix Collection." *ACM Transactions on Mathematical Software*, 38(1):1–25, 2011. DOI: [10.1145/2049662.2049663](https://doi.org/10.1145/2049662.2049663)

**Description**: The authoritative sparse matrix repository. Used as the standard benchmark in:
- Schenk & Gärtner (2006): PARDISO direct solver (Future Generation Computer Systems)
- Amestoy et al. (2001): MUMPS multifrontal solver (SIAM J. Matrix Analysis)
- Li & Demmel (2003): SuperLU (ACM TOMS)
- Karypis & Kumar (1998): METIS graph partitioning (SIAM J. Scientific Computing)
- Davis (2006): Direct Methods for Sparse Linear Systems (SIAM book)

**46 Matrix Market files are present**: 39 selected systems, six companion RHS
files, and one unselected `impcol_b` system whose name collides with the current
`*_b` RHS suffix rule. The evaluated 39 systems are structured by size:

### Small (11 matrices)
| Matrix | Rows | NNZ | Structure | Application |
|---|---:|---:|---|---|
| west0479 | 479 | 1,910 | unsymmetric | chemical engineering |
| fs_541_1 | 541 | 4,285 | unsymmetric | fluid structure |
| orsreg_1 | 2,205 | 14,133 | symmetric | oil reservoir simulation |
| saylr4 | 3,564 | 22,316 | unsymmetric | computational fluid dynamics |
| gre_1107 | 1,107 | 5,664 | unsymmetric | directed graph |
| impcol_e | 225 | 1,308 | unsymmetric | LP matrix |
| nasa2910 | 2,910 | 174,296 | symmetric SPD | structural (NASA truss) |
| nasa4704 | 4,704 | 104,756 | symmetric SPD | structural (NASA truss) |
| ct20stif | 52,329 | 2,600,295 | symmetric SPD | structural (cylindrical shell) |
| crystm01 | 4,875 | 146,653 | symmetric | FEM crystal vibration |
| msc00726 | 726 | 35,940 | symmetric SPD | structural (cylinder head) |

### Medium (10 matrices)
| Matrix | Rows | NNZ | Structure | Application |
|---|---:|---:|---|---|
| bcsstk39 | 46,772 | 2,089,066 | symmetric SPD | structural (Boeing 767 wing) |
| bcsstk36 | 23,052 | 1,143,140 | symmetric SPD | structural (car shock absorber) |
| ship_001 | 34,920 | 3,896,496 | symmetric SPD | structural (ship section, Det Norske Veritas) |
| ship_003 | 121,728 | 8,086,034 | symmetric SPD | structural (ship section) |
| shipsec1 | 140,874 | 7,813,404 | symmetric SPD | structural (DNVS ship) |
| troll | 213,453 | 11,174,811 | symmetric SPD | structural (oil platform Troll) |
| thread | 29,736 | 2,249,892 | symmetric SPD | structural (threaded connector) |
| trdheim | 18,036 | 1,200,606 | symmetric SPD | structural (Trondheim bridge) |
| fullb | 40,920 | 3,774,598 | symmetric SPD | structural (full bridge) |
| wang3 | 26,064 | 177,168 | unsymmetric | semiconductor device simulation |

### Large (18 matrices)
| Matrix | Rows | NNZ | Structure | Application | References |
|---|---:|---:|---|---|---|
| **Freescale1** | 3,428,755 | 18,920,347 | unsymmetric | circuit simulation (Freescale Semiconductor) | Davis&Hu TOMS 2011, Schenk&Gärtner 2006 |
| **rajat31** | 4,690,002 | 20,316,253 | unsymmetric | circuit simulation (Rajat Mittal, Univ. Florida) | Davis&Hu TOMS 2011 |
| **add20** | 2,395,130 | 13,151,496 | symmetric SPD | FEM (Michael Hamm, ETH) | Schenk&Gärtner 2006 |
| **add32** | 4,960 | 23,884 | symmetric SPD | structural | Schenk&Gärtner 2006 |
| **G10** | 3,257 | 9,583 | undirected graph | Gset benchmark (graph partitioning) | Karypis&Kumar SISC 1998 |
| **G26** | 5,000 | 33,068 | undirected graph | Gset benchmark | Karypis&Kumar SISC 1998 |
| af23560 | 23,560 | 460,598 | symmetric SPD | 2D/3D problem (Bai) | Davis&Hu TOMS 2011 |
| bfwb398 | 398 | 3,678 | unsymmetric | chemical process simulation (Bai) | — |
| ck400 | 400 | 3,200 | unsymmetric | chemistry (Bai) | — |
| ck656 | 656 | 5,248 | unsymmetric | chemistry (Bai) | — |
| dw1024 | 1,024 | 8,192 | symmetric SPD | least-squares problem (Bai) | — |
| dw256A | 256 | 2,048 | symmetric SPD | least-squares (Bai) | — |
| rdb450 | 450 | 75,954 | symmetric SPD | reaction-diffusion (Bai) | — |
| tols4000 | 4,000 | 12,021 | unsymmetric | fluid flow (Bai) | — |
| wang4 | 26,064 | 177,168 | unsymmetric | semiconductor device (Wang) | — |
| **StocF-1465** | 1,465,137 | 21,005,389 | symmetric SPD | SPE10 stochastic reservoir simulation (Janna, ConocoPhillips) | Karypis&Kumar SISC 1998, Amestoy et al. SIMAX 2001 |
| **invextr1_new** | 304,947 | 2,885,609 | symmetric | ANSYS Polyflow viscous fluid extrusion | Schenk&Gärtner 2006 |

**Note**: `G3_circuit`, `bayer10`, and `atmosmodd` were attempted but not retained
after 404/corrupt downloads. `Flan_1565` was subsequently restored and is now
byte-locked with the other 38 selected systems.

## 2. COPS 3.0 (Argonne National Lab)

**Source**: https://www.mcs.anl.gov/~more/cops/  
**License**: U.S. Department of Energy public release (see COPYRIGHT in cops-3.0/)  
**Citation**: Elizabeth D. Dolan, Jorge J. Moré, and Todd S. Munson. "Benchmarking Optimization Software with COPS 3.0." *Argonne National Laboratory Technical Report ANL/MCS-TM-273*, 2004.

**Description**: Canonical set of large-scale nonlinear constrained optimization problems. Widely used in:
- Dolan & Moré (2002): Performance profiles for benchmarking optimization software (*Mathematical Programming*)
- Moré & Wild (2009): Benchmarking derivative-free optimization (*SIAM J. Optimization*)
- Conn, Scheinberg, Vicente (2009): *Introduction to Derivative-Free Optimization* (MPS-SIAM book series)

**23 AMPL models** (`.mod` + `.dat` files), parameterized by mesh size N:

| Problem | Type | Max N Known | Equation | Application |
|---|---|---:|---|---|
| **torsion** | nonlinear PDE (2D) | 1000×1000 | elastic-plastic torsion variational inequality | solid mechanics |
| **bearing** | nonlinear PDE (2D) | 600×600 | elasto-hydrodynamic lubrication | journal bearing |
| **camshape** | optimal control | 800 timesteps | cam design (smooth displacement profile) | mechanical design |
| **catmix** | nonlinear algebraic | 100 species | catalytic cracking of gas oil | chemical engineering |
| **chain** | optimal control | 1000 links | hanging chain with obstacles | constrained dynamics |
| **channel** | nonlinear PDE (2D) | 400×400 | Navier-Stokes in channel with bump | CFD |
| **dirichlet** | nonlinear PDE (2D) | 1000×1000 | minimal surface with Dirichlet BC | geometry |
| **elec** | nonlinear PDE (2D) | 600×600 | electrostatic potential | electromagnetics |
| **gasoil** | nonlinear algebraic | 10000 pipes | steady-state natural gas pipeline network | pipeline optimization |
| **glider** | optimal control | 10000 timesteps | hang glider trajectory | aerospace |
| **henon** | nonlinear PDE (2D) | 1000×1000 | Hénon-Heiles system | dynamical systems |
| **lane_emden** | nonlinear ODE | 10000 | Lane-Emden equation (astrophysics) | stellar structure |
| **marine** | nonlinear algebraic | 10000 | steady marine ecosystem | ecology |
| **methanol** | nonlinear algebraic | 1000 | chemical equilibrium (methanol synthesis) | chemical engineering |
| **minsurf** | nonlinear PDE (2D) | 600×600 | minimal surface (Plateau problem) | geometry |
| **pinene** | nonlinear parameter estimation | 100000 data points | α-pinene chemical kinetics | chemistry |
| **polygon** | optimal control | 1000 vertices | obstacle avoidance polygon | robotics |
| **robot** | optimal control | 10000 timesteps | robot arm trajectory | robotics |
| **rocket** | optimal control | 1000 timesteps | rocket ascent trajectory | aerospace |
| **steering** | optimal control | 1000 timesteps | vehicle steering with obstacles | autonomous vehicles |
| **tetra** | nonlinear PDE (3D) | 50×50×50 | tetrahedral mesh elasticity | FEM |
| **triangle** | nonlinear PDE (2D) | 1000×1000 | triangular mesh elasticity | FEM |

## 3. PDEBench (ICLR 2023)

**Source**: https://github.com/pdebench/PDEBench, https://darus.uni-stuttgart.de (DaRUS repository)  
**License**: MIT (code), CC BY 4.0 (data)  
**Citation**: Makoto Takamoto et al. "PDEBench: An Extensive Benchmark for Scientific Machine Learning." *International Conference on Learning Representations (ICLR)*, 2023. arXiv: [2210.07182](https://arxiv.org/abs/2210.07182)

**Description**: The standard AI4Science PDE benchmark for physics-informed neural networks and neural operators. Used in:
- Li et al. (ICLR 2021): Fourier Neural Operator (FNO)
- Cao (ICLR 2023): Transolver
- Li et al. (NeurIPS 2023): Geometry-informed Neural Operator (GNOT)
- Gupta & Brandstetter (NeurIPS 2022): Towards multi-spatiotemporal-scale generalized PDE modeling

**7 PDE families are listed for authoritative restoration**. Expected sizes and MD5 values are stored in `pdebench/files.tsv`; the original 5–14 MB files were truncated prefixes and must not be treated as complete HDF5 data.

| PDE Family | Dimension | File | Size | Equation | Application |
|---|---|---|---:|---|---|
| **Advection** | 1D | 1D_Advection_Sols_beta0.1.hdf5 | 8.23 GB | u_t + β u_x = 0 | transport |
| **Burgers** | 1D | 1D_Burgers_Sols_Nu0.001.hdf5 | 8.23 GB | u_t + u u_x = ν u_xx | shock waves |
| **1D CFD** | 1D | 1D_CFD_Rand_Train.hdf5 | 12.41 GB | compressible Euler equations | fluid dynamics |
| **Diffusion-Sorption** | 1D | 1D_diff-sorp.h5 | 4.22 GB | u_t = D ∇²u - k u | contaminant transport |
| **Darcy Flow** | 2D | 2D_DarcyFlow_beta0.01_Train.hdf5 | 1.31 GB | -∇·(k(x) ∇p) = f | porous media |
| **Navier-Stokes (incompressible)** | 2D | ns_incom_2d_512-0.h5 | 9.91 GB | u_t + (u·∇)u = -∇p + ν∇²u, ∇·u=0 | CFD |
| **Shallow Water** | 2D | 2D_rdb.h5 | 6.63 GB | h_t + ∇·(hv) = 0, (hv)_t + ... = -gh∇h | geophysical flows |

**Note**: Full PDEBench contains additional families and resolutions. The manuscript
uses exactly these seven DaRUS v8.0 files; `benchmark/data-lock/pdebench.tsv` binds
their official IDs/names, sizes, MD5, and local SHA-256.

## 4. PETSc TS (Time Stepping)

**Source**: https://github.com/petsc/petsc (src/ts/tests/)  
**License**: BSD-2-Clause  
**Citation**: Satish Balay et al. "PETSc/TS: A Modern Scalable ODE/DAE Solver Library." *PETSc User Manual*, Argonne National Laboratory, 2024. URL: https://petsc.org/release/

**Description**: Official PETSc tutorial examples for time-dependent problems. PETSc is the de facto standard for large-scale scientific computing:
- Balay et al. (ACM TOMS 1997, 2024): PETSc foundational papers
- Munson et al. (SISC 2012): TAO optimization within PETSc
- Brown et al. (ACM TOMS 2012): Composable linear solvers in PETSc

**27 C source files** (ex2.c – ex81.c, 368 KB total):

| Example | Equation Type | Description |
|---|---|---|
| ex2.c | equation | linear ODE with fixed-step backward Euler |
| ex3.c | equation | 1D heat equation with FEM mass matrix |
| ex4.c | equation | 2D convection-diffusion equation |
| ex5.c | equation | nonlinear radiative surface-balance PDE |
| ex6.c | equation | index-1 DAE with reduced differential layout |
| ex7.c | equation | index-1 DAE with combined differential/algebraic layout |
| ex8.c | framework self-test | ROSW integration of the ex6/ex7 DAE for TS layout coverage |
| ex9.c | framework self-test | ROSW scatter-layout variant of the ex6/ex7 DAE |
| ex10.c | framework self-test | reduced/full TS DAE wrapper API |
| ex11.c | framework self-test | PETSc registration and memory-leak regression |
| ex12.c | equation | nonlinear diffusion PDE with fixed-step backward Euler |
| ex13.c | framework self-test | TSTrajectory interpolation and history API |
| ex14.c | equation | polynomial-chain ODE with fixed-step RK4 |
| ex15.c | equation | conservative reaction ODE with fixed-step BDF1 |
| ex17.c | framework self-test | TSResize restart and transfer consistency |
| ex18.c | equation | nontrivial mass-matrix DAE |
| ex21.c | equation | 2D time-dependent Bratu PDE |
| ex24.c | framework self-test | TSComputeIJacobian API |
| ex25.c | framework self-test | repeated PetscInitialize lifecycle |
| ex26.c | equation | mass-matrix ODE |
| ex27.c | equation | particle-basis Landau equation |
| ex28.c | equation | BGK kinetic equation |
| ex29.c | framework self-test | TS time-span delivery API |
| ex30.c | equation | grid-based Landau collision equation |
| ex35.c | framework self-test | colorized scatter-plot rendering |
| ex80.c | equation | second-order constant-acceleration equation |
| ex81.c | equation | first-order constant-velocity equation |

## Usage

### SuiteSparse Matrices

Convert `.mtx` to SMAVE Modelica format:
```bash
python3 benchmark/generators/mtx_to_mo.py \
  benchmark/suitesparse/medium/bcsstk39/bcsstk39.mtx \
  benchmark/suitesparse/medium/bcsstk39.mo \
  --model-name bcsstk39
./build/release/smave compile benchmark/suitesparse/medium/bcsstk39.mo \
  --top bcsstk39 --output build/release/bcsstk39.ir
```

Or use CSR-direct assembly (see `tests/large_sparse_evidence.cpp`).

### COPS Models

COPS problems are in AMPL format. To convert to SMAVE-compatible Modelica:
1. Export residual expressions from AMPL using `ampl -ogstub torsion.mod torsion.dat`
2. Parse the `.nl` stub and generate Modelica variables + equations
3. Or manually transcribe the PDE discretization into SMAVE Modelica subset

Example: `cops-3.0/models/torsion/torsion.mod` defines a 2D elastic-plastic torsion problem. The mesh size `nx`, `ny` can be set in `torsion.parN` files (N=1,2,3).

### PDEBench HDF5 Files

PDEBench `.hdf5`/`.h5` files contain spatiotemporal PDE solutions as NumPy arrays. Use `h5py` to extract initial conditions and snapshots for:
- Neural operator training (FNO, DeepONet, GNOT)
- PDE solver validation (finite difference, finite element)
- AI4Science generalization benchmarks

```python
import h5py
with h5py.File('benchmark/pdebench/1D/Burgers/1D_Burgers_Sols_Nu0.001.hdf5', 'r') as f:
    print(list(f.keys()))  # ['nu', 't-coordinate', 'tensor', 'x-coordinate']
    u = f['tensor'][:]     # shape (n_samples, n_t, n_x)
```

### PETSc TS Examples

Compile and run with PETSc:
```bash
cd benchmark/petsc-ts
mpicc -o ex10 ex10.c $(pkg-config PETSc --cflags --libs)
mpirun -n 4 ./ex10 -ts_monitor -ts_max_time 1.0 -ts_dt 0.01
```

Or extract the residual function and discretization stencil to generate SMAVE-compatible Modelica.

## Verification & Reproducibility

```bash
cmake --build build/release --target benchmark-readiness
cmake --build build/release --target benchmark-suitesparse-full
cmake --build build/release --target benchmark-petsc-ts-full
cmake --build build/release --target benchmark-multiphysics-msl-full
cmake --build build/release --target benchmark-cops-full
cmake --build build/release --target benchmark-cops-julia-baseline
cmake --build build/release --target benchmark-cops-kkt-comparison
cmake --build build/release --target benchmark-pdebench-download
cmake --build build/release --target benchmark-pdebench-verify
cmake --build build/release --target benchmark-data-lock
cmake --build build/release --target benchmark-cross-checks
```

These full targets are intentionally not normal CTest tests: SuiteSparse can consume tens of minutes and multiple GiB RAM; PDEBench downloads roughly 51 GB. SuiteSparse and PDEBench operations are resumable.

`benchmark-cross-checks` refreshes the routed SuiteSparse suite, all currently implemented PDEBench family adapters, the PETSc TS comparison and the seven-model OpenModelica MSL comparison before rewriting `benchmark-overall/summary.txt`. In contrast, `benchmark-report` only summarizes existing checkpoints and is intended for cheap status inspection.

Acquisition is scripted and byte-locked, but still depends on upstream availability:
- **SuiteSparse**: official Matrix Market archives linked by
  `https://sparse.tamu.edu/<GROUP>/<NAME>`, verified after extraction against SHA-256
- **COPS**: `curl -sL -o cops-3.0.zip https://www.mcs.anl.gov/~more/cops/cops-3.0.zip`
- **PDEBench**: DaRUS API `https://darus.uni-stuttgart.de/api/access/datafile/<ID>`
  with `pdebench/files.tsv` and `data-lock/pdebench.tsv`
- **PETSc TS**: `https://raw.githubusercontent.com/petsc/petsc/main/src/ts/tests/ex*.c`

The original matrices/models were downloaded on **2026-07-17**; on **2026-07-25**,
all 54,764,505,235 consumed bytes were locked and checked against current upstream
version/license/detail metadata. Re-acquisition must match the lock rather than
silently accepting a newer upstream payload.

## Multiphysics Coupling (NEW)

**7 cross-domain coupling examples** from Modelica Standard Library:
- **Thermo-electrical**: HeatingMOSInverter, HeatingRectifier (temperature-dependent semiconductors)
- **Electro-mechanical-thermal**: DCPM_Temperature (DC motor with winding heating)
- **Fluid-thermal**: HeatExchanger, HeatingSystem (convective heat transfer)
- **Magneto-mechanical**: MovingCoilActuator, SolenoidActuator (electromagnetic force)

These are **reference models** requiring MSL library components. See `multiphysics-msl/README.md` for detailed physics equations and SMAVE conversion workflow.

## Limitations & Future Work

- **SuiteSparse**: `Flan_1565` has been restored from the official source and is now a valid 1,564,794-order case. Several originally planned matrices remain unavailable at their old URLs; the checked-in 39-system manifest is the executed scope and includes documented replacements.
- **PDEBench**: All seven authoritative files, including the two formerly partial very large assets, are complete and verified by manifest size, MD5 and HDF5 readability. Downloads remain resumable and safe to run in the background; verified files are skipped, and staged data replaces a destination only after full verification.
- **Multiphysics MSL**: 7/7 models pass same-observable trajectory and end-to-end timing comparisons. Six models route 535,728 OpenModelica/DASSL `dgesv` calls through SMAVE with zero external fallback; MovingCoilActuator makes no applicable linear-solver call and is reported as such rather than counted as SMAVE kernel work. Full source-equation lowering to Hybrid DAE IR remains a separate compiler capability, not a prerequisite silently inferred from this solver comparison.
- **COPS**: The original AMPL runner retains its environment evidence (2 solved, 66 demo-license blocks). The pinned COPSBenchmark.jl/JuMP/Ipopt path independently solves all 68 corresponding parameter instances, and the exported KKT linear layer completes 68/68 SMAVE/SuperLU agreement and performance comparisons. The separate MadNLP full-NLP path attempts 68/68 cases with the same outer algorithm: 57 agreements, 12 native non-fallback-only performance comparisons, 5 timeouts, 46 fallback-only cases and 37,908 external fallback solves. Full-NLP fallback-only and timeout cases are execution evidence, not SMAVE speed comparisons.
- **MINPACK-2 / NIST Matrix Market**: Deferred to future work.

## Citations & Acknowledgments

This benchmark suite aggregates work from multiple research groups. Please cite the original papers when using these datasets:

**SuiteSparse**:
```bibtex
@article{davis2011university,
  title={The University of Florida Sparse Matrix Collection},
  author={Davis, Timothy A and Hu, Yifan},
  journal={ACM Transactions on Mathematical Software},
  volume={38},
  number={1},
  pages={1--25},
  year={2011},
  publisher={ACM}
}
```

**COPS**:
```bibtex
@techreport{dolan2004cops,
  title={Benchmarking Optimization Software with COPS 3.0},
  author={Dolan, Elizabeth D and Mor{\'e}, Jorge J and Munson, Todd S},
  institution={Argonne National Laboratory},
  number={ANL/MCS-TM-273},
  year={2004}
}
```

**PDEBench**:
```bibtex
@inproceedings{takamoto2023pdebench,
  title={PDEBench: An Extensive Benchmark for Scientific Machine Learning},
  author={Takamoto, Makoto and others},
  booktitle={International Conference on Learning Representations (ICLR)},
  year={2023}
}
```

**PETSc**:
```bibtex
@misc{petsc-web-page,
  author = {Satish Balay and others},
  title = {{PETS}c {W}eb page},
  url = {https://petsc.org/},
  year = {2024}
}
```

## License

The benchmark assets retain their original licenses. Manufactured known solutions are used only by the SuiteSparse correctness harness when a suite-provided RHS/reference solution is unavailable; reports label the RHS kind explicitly.
- **SuiteSparse**: CC BY 4.0; preserve embedded matrix metadata and citations
- **COPS**: U.S. Department of Energy public release (see `cops-3.0/COPYRIGHT`)
- **PDEBench**: MIT (code), CC BY 4.0 (data)
- **PETSc**: BSD-2-Clause

**SMAVE benchmark suite assembly**: No additional restrictions. Cite original sources.
