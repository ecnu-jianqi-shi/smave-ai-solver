# SMAVE 求解器研究目标

> 本文件是 README 当前最高优先级、研究缺陷、验收标准和机器证据的唯一问题基线。
> 它只定义求解器机制、数值正确性、完整路径性能和可证伪实验；兼容性与发布工程
> 只有在直接支撑这些目标时才保留。

## 1. 项目定位

SMAVE 研究重复数值求解中的异构专家融合：经典直接法与迭代法、学习型候选、
corrector、原方程 gate 和候选拒绝后的数值续接如何在同一运行时中协同，使系统
最小化完整验证成本，而不是只追求候选 kernel 延迟。

这里的 `gate` 仅指由原方程 residual、约束、分支或离散缺陷定义的数值验收。候选
未通过验收时，求解器从原始请求状态继续执行下一条数值路径；本文统一将这一机制
称为“数值续接”，历史字段名 `fallback` 不改变其语义。

核心科学问题是：

> 在方程结构、数值状态、硬件和输入分布发生变化时，如何选择并排序候选求解路径，
> 同时保证每个成功返回的结果都满足声明的原方程 residual、约束和分支条件？

## 2. 当前问题与缺陷

以下四项缺陷直接对应 README 的四项当前最高优先级。缺陷只能在规定的机器证据、
统计口径和适用边界全部满足后关闭；已有局部实验不等于缺陷关闭。

### D-P0-1：correction-budget 与完整成本耦合未闭合

README 要求完成 correction-budget sweep，解释 candidate、corrector、原方程 gate
和数值续接的成本耦合及 break-even。当前已有 correction budget、请求条件路由和
完整成本分解机制，但尚未形成覆盖候选族、预算、校正器和后续路径的统一 sweep。

关闭条件：

1. 对每个纳入主张的 correctable expert，至少覆盖 no-correction、低预算、中预算和
   调用方上限四类预算，并在同一请求集合上交错重复；
2. 对每个预算记录 candidate/inference、transfer、corrector、gate、数值续接、
   terminal fallback 和总墙钟，不能用单一 attempt time 代替分项成本；
3. 报告接受率、校正收敛率、续接概率、complete cost、cost-per-acceptance 和
   break-even，并给出配对 bootstrap 区间、尾延迟和负结果；
4. 至少有一个可解释消融证明预算变化如何改变 routing、接受率和完整成本；
5. 证据必须由 `reproduce-calibrated-correction-router`、
   `reproduce-joint-route-budget-shift`、`reproduce-complete-cost-decomposition`
   及其 verifier 共同支持。

### D-P0-2：distribution shift 下的 calibration、regret 和 OOD 结构过滤未闭合

README 要求在 distribution shift 下加强路由校准和 complete-cost regret，并执行 OOD
结构过滤。当前已有 family/conditioning shift、成本与通过率校准、OOD 距离和保守
回退，但尚未把结构过滤定义为独立的硬门禁，也未统一报告 shift 维度上的 coverage、
误拒、危险放行和 regret。

关闭条件：

1. 至少覆盖 family、conditioning、规模、拓扑/结构和适用时的 tolerance/hardware
   shift；训练、校准、held-out 和 OOD 集按完整实例或方程族隔离；
2. 对成本和 gate probability 报告 calibration error、Brier/ECE 或等价指标、尾部
   误差，并同时报告 static、fixed、conditioned 与 realized-oracle 的 complete-cost
   regret；
3. 结构过滤必须在 learned route 获得权限前执行，明确结构不兼容、未验证拓扑、
   OOD 超界和未知硬件的拒绝原因；不能用 embedding 相似度或 Router 置信度替代过滤；
4. 报告 OOD coverage、false-accept/false-reject、保守 fallback 率、失败和
   negative transfer，不得只报告平均 speedup；
5. 证据必须包括 `reproduce-router-shift`、`reproduce-router-shift-matrix`、
   `reproduce-joint-route-budget-shift`、
   `reproduce-request-conditioned-joint-route` 和
   `reproduce-suitesparse-request-conditioned-route` 的机器报告与 verifier。

### D-P0-3：强外部 hybrid 基线与原生 x86-64/CUDA 证据未闭合

README 要求增加强外部 hybrid solver/preconditioner 基线，以及原生 x86-64/CUDA
性能证据。当前已有传统 solver、内部学习专家和官方 HINTS native baseline 接口，
但 Apple/ARM 本机结果不能替代 provider-hosted x86-64，也不能替代原生 CUDA 离散
GPU 的完整路径证据。

关闭条件：

1. 至少一个公开、可固定 revision 的外部 hybrid learned-solver、learned-
   preconditioner 或算法选择方法，在相同离散系统、RHS/初值、精度、容差、验证
   gate、计时边界和失败协议下与 SMAVE 对比；
2. 外部基线必须报告准备、推理、校正、gate、传输、fallback、内存和完整墙钟，
   不能只比较模型 kernel 或 solve-only 子区间；
3. 在 provider-controlled 原生 x86-64 主机上完成可重放 campaign，并保留 workflow、
   commit、机器、job、artifact 和 SHA-256 provenance；本地 ARM64 dry-run 不算关闭；
4. 在原生 CUDA 离散 GPU 上完成同一数值合同下的 single/batch、冷/热、传输、驻留、
   corrector、gate 和 fallback 证据；CUDA 仿真、远程非原生执行和纯 kernel benchmark
   均不算关闭；
5. 证据至少包括 `reproduce-hints-native-baseline`、provider-hosted x86-64
   workflow，以及待新增的 native-CUDA campaign。未具备 CUDA 证据前，D-P0-3 保持
   `OPEN`，不得把 Metal/ANE 或 ARM64 结果写成 CUDA 结果。

### D-P0-4：大规模 workload、完整路径并行、batch 摊销和异构 placement 未闭合

README 要求同时扩展大规模稀疏、非线性、ODE/DAE 和 operator workload，并完善完整
路径并行、batch 摊销和异构 placement。当前已有受限线性、非线性、DAE、operator、
CPU 线程、batch 和 Apple 设备证据，但覆盖边界、资源对称性和完整路径退出门禁尚未
统一。

关闭条件：

1. 四类 workload 各自至少有一个独立、可重放、跨规模或跨难度的证据集，并报告
   setup、assembly、iteration、memory、P99、失败、fallback 和端到端扩展；
2. 并行证据必须分别报告 candidate、corrector、verification 和数值续接的并行度，
   同时保留 gate-only 与 full-path 结果；SMAVE 与 baseline 必须使用对称资源；
3. batch 证据必须覆盖 batch size、冷/热状态、等待、组装、推理、传输、gate、
   corrector 和 fallback，给出 amortization 与 break-even，而不是只报告 kernel；
4. placement 证据必须覆盖 CPU 与至少一个原生 CUDA 离散 GPU（如已开放该 capability），
   报告 transfer、residency、queue、内存和失败时回到原始系统的行为；
5. 证据必须包括 `large-sparse`、`large-nonlinear`、`large-dae`、
   `operator-shared-baseline`、`gate-parallel-scaling`、`parallel-scaling`、
   `batch-scaling` 和 `device-execution`，并明确未覆盖的方程语义。

## 3. 核心研究难点

### G1：完整成本专家融合

- 建模候选生成、数据搬运、校正、gate、拒绝和后续数值路径的 reach-weighted 成本；
- 让生产 Router 与固定级联的 cost-per-acceptance 排序规则保持一致；
- 明确固定 eligible set、顺序无关统计、概率校准和 correction budget 的理论边界。

### G2：校正与原方程 Gate 协同

- 研究学习型候选需要多少局部校正才能达到最低完整成本；
- 分解候选误差、corrector 收敛、gate 严格度和数值续接概率之间的耦合；
- 保持成功返回必须通过 family-specific 原方程 gate 的控制流不变量。

### G3：分布偏移下的路由校准与结构过滤

- 评估 family、conditioning、规模、拓扑、容差和硬件 shift；
- 校准接受概率、成本、风险、complete-cost regret 和 OOD 信号；
- 用结构硬过滤阻止未验证拓扑或 OOD 实例取得 learned expert 权限；
- 保留 negative transfer、错误路由、误拒和不达 break-even 的负结果。

### G4：完整路径并行与异构 Placement

- 保留线程内 gate 并行、candidate/corrector/verification/续接的完整路径 scaling；
- 分析 batch amortization、等待、传输、驻留和设备队列形成的 break-even；
- 研究 CPU/GPU/NPU placement，但只有原生、完整路径和同资源证据才能形成性能主张；
- 并行贡献限于求解器内部线程、批处理和异构计算机制。

### G5：外部求解器基线与泛化

- 对比强外部 hybrid learned-solver、learned-preconditioner 和算法选择基线；
- 扩展 workload、规模、condition、precision、硬件和方程家族，同时逐项报告失败与覆盖率；
- 在 provider-controlled x86-64 与 native CUDA 可用时补充完整路径测量，不把 dry-run 当证据。

### G6：可复现的 Claim–Evidence 闭环

- 所有数字由机器证据生成并绑定 claim ledger、artifact manifest 和 provenance；
- 保留配对计时、bootstrap 区间、顺序敏感性、预算 sweep 和完整失败统计；
- 让核心算法实验可由普通开发机重跑；发布归档和第三方复现属于质量约束，不冒充算法贡献。

## 4. 当前优先级

### P0：README 四项最高优先级

1. `D-P0-1`：完成 correction-budget sweep，解释 candidate–corrector–gate–fallback
   的成本耦合与 break-even；
2. `D-P0-2`：完成 distribution-shift calibration、complete-cost regret 和 OOD
   结构过滤；
3. `D-P0-3`：完成强外部 hybrid solver/preconditioner baseline，以及原生 x86-64/CUDA
   完整路径证据；
4. `D-P0-4`：扩展大规模稀疏、非线性、ODE/DAE 和 operator workload，并完成完整路径
   并行、batch 摊销和异构 placement 证据。

### P1：扩大求解器适用性

1. 增加更多稀疏线性、非线性、ODE/DAE 和 operator workload；
2. 扩大 problem size、conditioning、precision 和硬件矩阵；
3. 在 P0 合同通过后推进更多 GPU/NPU 原生 kernel、传输摊销和 heterogeneous placement；
4. 改进工业稀疏后端与学习型 preconditioner/multigrid 的组合路径。

### P2：兼容边界

Modelica、Simulink、FMI、SSP 和复杂 hybrid semantics 只在它们产生新的求解器难点、
外部 workload 或验证需求时推进。兼容性扩展本身不应挤占 P0 算法与实验资源。

## 5. 缺陷—目标追踪总表

| 缺陷 | README 目标 | GOAL 目标 | DESIGN 需求 | 主要机器证据 | 当前状态 |
|---|---|---|---|---|---|
| `D-P0-1` | correction-budget sweep、成本耦合、break-even | `G1/G2` | `REQ-CORR-001`、`REQ-COST-001` | `calibrated-correction-router`、`joint-route-budget-shift`、`complete-cost-decomposition` | `EVIDENCE-COMPLETE` |
| `D-P0-2` | shift calibration、complete-cost regret、OOD 结构过滤 | `G3` | `REQ-SHIFT-001`、`REQ-OOD-STRUCT-001` | `router-shift`、`router-shift-matrix`、`joint-route-budget-shift`、`structural-ood-filter`、公共 SuiteSparse route | `EVIDENCE-COMPLETE` |
| `D-P0-3` | 外部 hybrid baseline、native x86-64/CUDA | `G5` | `REQ-EXT-BASE-001`、`REQ-NATIVE-X86-001`、`REQ-NATIVE-CUDA-001` | HINTS native baseline、hosted x86 workflow、native CUDA campaign | `CUDA-DONE-X86-PENDING` |
| `D-P0-4` | workload 扩展、full-path parallel、batch、placement | `G4/G5` | `REQ-WORKLOAD-001`、`REQ-FULLPATH-PAR-001`、`REQ-BATCH-001`、`REQ-PLACEMENT-001` | large sparse/nonlinear/DAE/operator、parallel、batch、device reports | `EVIDENCE-COMPLETE` |

## 6. 范围边界

项目只纳入能够形成求解器科学贡献的工作：算法、数值正确性、路由与校正机制、
完整路径成本、求解器内部并行、异构计算和可复现实验。论文贡献、实验主表、review
score gate、core bundle claim surface 和下一阶段任务均按这一边界组织。

任何工作只有在直接解决求解器机制、数值正确性、完整路径成本或可证伪实验问题时，
才可进入论文主张、评审条件和下一阶段任务。候选被原方程 gate 拒绝后继续另一条
求解路径，只表示同一请求内的数值续接。

动态库、C ABI、安装后宿主和错误栈只服务于调用兼容性、验证和实验复现，不是 P0
研究目标；它们必须复用同一 `EquationAssessment → SolvePlan → backend → gate →
fallback` 数值路径。

## 7. 阶段完成判定

本阶段只有在 `D-P0-1` 至 `D-P0-4` 全部关闭后才算完成，同时满足：

1. 完整成本 Router 的理论、实现和 exhaustive ordering evidence 一致；
2. candidate–corrector–gate–numerical-continuation 的关键耦合有 budget sweep 和
   可解释消融；
3. correction budget 与 routing 决策能够解释接受率、regret、break-even 和完整成本变化；
4. family、condition、规模、拓扑或硬件分布偏移上的泛化和校准显著强于当前证据，
   OOD 结构过滤不会把未验证实例授予 learned 权限；
5. 至少一个强外部求解器方法在同一数值合同下比较，并完成 provider-controlled native
   x86-64 与 native CUDA 证据；
6. 稀疏、非线性、ODE/DAE、operator workload 均有完整路径、失败、规模、P99、并行、
   batch 和 placement 报告；
7. 所有主张均报告完整路径成本、失败、数值续接、区间、硬件和适用边界。

## 8. 权威结果与证据规则

以下路径只表示报告约定，不表示当前文件已宣称目标关闭：

- `build/release/calibrated-correction-router/evidence.txt`
- `build/release/joint-route-budget-shift/evidence.txt`
- `build/release/request-conditioned-joint-route/evidence.txt`
- `build/release/suitesparse-request-conditioned-route-v6-reproduction/evidence.txt`
- `build/release/router-shift/evidence.txt`
- `build/release/router-shift-matrix/evidence.txt`
- `build/release/structural-ood-filter/evidence.txt`
- `build/release/complete-cost-decomposition/evidence.txt`
- `build/release/hints-native-baseline/evidence.txt`
- `build/release/native-cuda/evidence.txt`
- `build/release/large-sparse/evidence.txt`
- `build/release/large-nonlinear/evidence.txt`
- `build/release/large-dae/evidence.txt`
- `build/release/operator-shared-baseline/evidence.txt`
- `build/release/gate-parallel-scaling/evidence.txt`
- `build/release/parallel-scaling/evidence.txt`
- `build/release/batch-scaling/evidence.txt`
- `build/release/device-execution/evidence.txt`

provider-hosted x86-64 证据必须来自 `.github/workflows/native-external-performance.yml`
的成功运行、完整 artifact、provenance 和 attestation；本地 dry-run 只能证明协议，
不能关闭 `D-P0-3`。native CUDA 证据由 `reproduce-native-cuda-campaign` target 与
`benchmark/run_native_cuda_campaign.sh` 在原生离散 GPU 上生成，记录冷/热 setup、
transfer、residency、candidate、corrector、gate 和 fallback；CUDA 仿真、远程非原生
执行或纯 kernel benchmark 不满足要求。provider-hosted x86-64 证据仍须来自
`.github/workflows/native-external-performance.yml` 的成功运行。

所有报告都必须同时记录 workload、硬件、精度、计时边界、统计单位、失败/续接、
验证契约、数据 split 和证据版本。任何局部 kernel、非对称资源、未计 setup/transfer/
gate/fallback 的数字都不能关闭本目标。
