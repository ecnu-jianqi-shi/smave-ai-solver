---
tags: [survey, neural-solver, speedup, evidence]
status: researched
updated: 2026-07-21
---

# AI 加速方程求解论文矩阵

## 研究问题与证据规则

本页关注 AI 是否真正加速 ODE/DAE/PDE 离散后产生的方程组求解，而不只比较预测误差。论文报告的“加速比”只有同时注明以下口径才可用于项目决策：

1. 求解对象：完整场、单个 QoI、单步线性系统或完整轨迹；
2. 精度门槛：相对 residual、解误差或任务指标；
3. 基线：Jacobi、AMG、FEM/CFD、GPU solver 或其他实现；
4. 硬件：CPU 对 CPU、GPU 对 GPU，还是 GPU AI 对 CPU solver；
5. 成本：是否包含装配、预处理构造、数据生成和训练；
6. 复用前提：单实例训练、同一方程族摊销，还是跨方程 zero-shot；
7. 正确性：只靠测试误差，还是每次仍由 residual/迭代收敛判据验收。

论文中的加速比不可直接横向排序。`10^4×` 摊销代理与 `2×` 保持 residual 收敛的预条件器解决的是不同问题。

## 已核验代表论文

| 工作 | 发表 | 方法与对象 | 论文报告性能 | 精度/保证口径 | 关键限制 | 对项目价值 |
|---|---|---|---|---|---|---|
| Fourier Neural Operator (FNO) | ICLR 2021 | 学习参数化 PDE 的解算子 | 最高约 `10^3×`；Navier–Stokes MCMC 示例单次 `0.005 s` 对传统 GPU solver `2.2 s`，约 `440×` | 示例解误差约百分比量级；训练集由数值求解器生成 | 针对固定 PDE 家族；高难例使用约 10,000 训练对；不是 residual-certified | Tensor Operator Expert；只适合已准入的重复多查询场景 |
| Learning Neural PDE Solvers with Convergence Guarantees | ICLR 2019 | 学习迭代更新算子，固定点保持原 PDE 解 | 卷积迭代器约 `2.5×` 算术操作加速；GPU 相对 64-core CPU 另有约 `30×`，CPU 墙钟与 FEniCS 相当或更快 | 通过固定点/谱半径条件讨论收敛；墙钟比较采用初始误差 `1%` 阈值 | 线性 PDE、小规模规则结构；硬件比较不完全同构 | Learned Iterator Expert；“学习误差传播算子而非直接猜解”的先例 |
| Geo-FNO | JMLR 2023 | 在学习形变后的规则潜网格上使用 FNO，支持复杂几何 | 摘要称最高 `10^5×`；Euler airfoil 同精度 cost-accuracy 实验正文称至少 `10^4×`；一次推理约 `0.01 s` 的示例 | 与所展示传统离散方案做同精度比较 | GPU 推理对 CPU time-stepping；离线训练未计入单次推理；固定 PDE 家族 | Geometry/Operator Family Expert；证明几何规范化与 FFT tensor 化价值 |
| Geometry-Informed Neural Operator (GINO) | NeurIPS 2023 | 图算子处理不规则几何，FNO 在规则潜网格计算 | 论文报告计算阻力系数相对优化 GPU CFD 约 `26,000×` | 只报告学习分布上的统计预测误差，不是每次 residual 认证 | 加速数字针对 drag coefficient/QoI，不等同于完整 CFD 场严格求解；500 个 3D CFD 样本 | QoI Expert；适合优化/筛选，不得用于 `0.01%` 方程解直接接受 |
| Learning Preconditioner for Conjugate Gradient PDE Solvers | ICML 2023 | GNN 预测稀疏预条件器，PCG 继续迭代到目标 residual | 热/波/Poisson 实验中降低总墙钟和迭代；如 heat-2d 到 `1e-12`：`3.377 s` 对 Jacobi `6.255 s`；Poisson-3d：`10.406 s` 对 Jacobi `17.080 s` | 总时间含预处理构造；最终精度由 PCG residual 控制 | 并非所有方程/精度都优于强基线；训练约分钟至小时，靠长轨迹摊销 | Neural Preconditioner Expert；与项目安全主线高度一致 |
| UGrid | ICML 2024 | 将 U-Net 嵌入 multigrid 迭代，学习 coarse correction/smoother | 大规模 Poisson 达相对 residual `≤10^-4`：示例约 `10–20 ms`，AMGCL 约 `129–424 ms`、AmgX 约 `64–175 ms`，约数倍至数十倍 | 保留 multigrid 迭代与 residual 停止条件；论文给出线性问题收敛/正确性分析 | 线性 PDE、网格结构和训练分布有假设；不能直接外推到任意非线性 DAE | Neural Multigrid Expert；应优先纳入大型线性 SCC 路径 |
| Graph Neural Preconditioners for Iterative Solutions of Sparse Linear Systems | ICLR 2025 | 从稀疏矩阵图构造通用 GNN 预条件器，交给 Krylov 求解 | 在 800+ 稀疏矩阵上，构造时间比 ILU/AMG 更可预测且可更短；在部分困难矩阵上总求解更快 | Krylov residual 仍是最终验收；论文不提供单一通用加速倍数 | 不是所有矩阵都优于经典预条件器；需要 Router 识别适用区域 | Sparse-Matrix Router + GNP Expert；直接支持“专家竞赛后路由” |
| Neural-Preconditioned Poisson Solver | SIGGRAPH 2024 | 神经网络作为 Poisson 预条件器，PCG/正交化确保收敛 | 论文示例：CG `790` 次、`0.5615 s`，方法 `19` 次、`0.05237 s`，约 `10.7×`；另例 `390→12` 次、`0.1125→0.027 s` | 最终 residual 约 `10^-6`；独立迭代器控制正确性 | 专用于混合边界 Poisson/流体压力投影；网络评估占其时间约 79.4% | GPU Preconditioner Expert；证明 tensor kernel 优化是主要工程瓶颈 |
| PDEformer | ICLR 2024 AI4DiffEq Workshop | 将 PDE 符号计算图与数值条件编码，跨多种 1D PDE zero-shot | 报告 zero-shot 可接近专用模型；重点是泛化，不是传统 solver 墙钟 | 预测误差评估，无逐调用 residual 认证 | 1D、预训练分布；非主会论文；未证明 `0.01%` | 编译期方程 embedding 和专家检索，不作为直接接受证据 |
| PROSE-PDE | Physical Review E 2025 | 方程符号模态 + 轨迹模态，多算子预训练与外推 | 多方程实验预测误差约百分比量级，展示未见方程外推 | 无传统求解器同精度墙钟证据 | 1D 常系数 PDE；当前精度远高于项目 `0.01%` 误差预算 | Router 表征、family expert 预训练与 equation-conditioned adapter |
| Poseidon | NeurIPS 2024 | PDE foundation model，预训练后以较少数据适配下游 PDE | 主要收益是数据效率和下游微调，不是统一 solver speedup | 下游预测误差，无原 residual gate | PDE 子集和数据分布限制；直接求解精度不足以承担安全路径 | Global/Family Expert 初始化；降低新方程训练成本 |

## 与 SMAVE 最直接的相关工作脉络

仅比较 neural operator 或 learned preconditioner 会高估 SMAVE 的单点原创性。SMAVE 还直接落在 algorithm selection、稀疏求解器自动选择、学习型迭代加速和 residual corrector 的交叉区域。下表用于回答“哪些思想已有、SMAVE 的差异在哪里、还缺什么证据”，而不是把所有前序工作都包装为弱相关背景。

| 脉络与代表工作 | 已有思想 | 与 SMAVE 的直接重叠 | SMAVE 当前可主张的差异 | 尚未闭环的证据 |
|---|---|---|---|---|
| Rice, *The Algorithm Selection Problem* (1976) | 从问题特征、算法集合与性能度量出发选择算法，是 portfolio/algorithm selection 的经典形式化起点 | 编译期/运行期 Router 根据 block profile、权限与代价选择专家 | 将选择对象扩展到 Hybrid DAE block、学习/经典专家、验证成本和 fallback 权限，而不只预测最快算法 | 仍需给出正式决策问题、损失函数、适用域和与标准 selector 的同协议比较 |
| Lighthouse (Sood et al., 2015)；Funk et al. (2022) | 以矩阵特征或机器学习自动选择稀疏线性求解器/配置 | 线性 SCC 的后端竞赛、family calibration 和请求条件 Router | Router 联合建模 expert--budget action 的完整成本、原方程通过率与 terminal fallback | 已增加 leave-one-scenario-out 1-NN、depth-3 Gini CART 与每 expert 对数 Runtime 岭回归结构成本模型，三者所选 expert 都执行完整 Runtime/gate；结构模型不使用查询 winner/查询计时，并显示 practical regret 比 exact label accuracy 更稳定，但训练/推理成本尚未计入，且仍缺公开 selector 代码在同一矩阵集合上的选择 regret 对照 |
| Zabegaev et al. (2024, 2026) | 面向多物理多孔介质模拟，使用数据驱动方法选择/调优线性求解配置 | 与 SMAVE 的 workload-aware backend selection、校准和请求条件 action model 最接近 | SMAVE 试图跨线性、非线性、ODE/DAE block 统一选择，并把逐调用原方程验收、校正预算和完整成本纳入路由合同 | 尚缺真实多物理 workload、迁移成本和相对现有 selector 的端到端收益证据 |
| Hsieh et al. (2019)；Arisaka & Li (2023) | 学习迭代更新或加速参数，同时保留原数值方法的固定点/收敛结构 | learned iterator、warm-start、preconditioner 与校正路径 | SMAVE 在同一 Router 中同时允许 learned、classic、direct 与拒绝路径，并统一计入 gate 成本 | 尚缺更强收敛分析以及跨非线性/事件 family 的稳定优势 |
| PIN (Luo & Cai, 2023) | 学习/预测预条件器以加速 inexact Newton，最终仍由 Newton/Krylov 收敛判据控制 | 非线性系统中的学习 warm-start、Jacobian/preconditioner 专家和原 residual 验收 | SMAVE 进一步联合选择专家、校正预算和后续经典路径，并记录完整 attempt trace | 当前非线性证据规模小，且 fixed-vs-online Router 未显示稳定显著优势 |
| Cao et al. (2023)；Jha (2024) residual-based error correction | 用原算子 residual 对 surrogate/neural operator 输出进行后处理或校正 | candidate 后的 correction 与原方程 gate | SMAVE 将 corrector 置于有序多专家数值求解路径中，并将 correction/gate/fallback 成本纳入路由 | Phase 5 已在同一 raw candidate 上加入 weighted diagonal residual/Jacobi corrector 文献算法重实现，并以同一 strict gate 报告成本、接受率和 mismatch；仍缺公开 corrector 代码、非线性/事件 workload 和复杂 OOD 压力测试 |

## SMAVE 创新归属矩阵

| 能力或主张 | 前序思想是否明确存在 | 当前创新归属 | 允许的学术表述 | 不允许的过度表述 |
|---|---|---|---|---|
| 依据问题特征选择求解算法 | 是 | 已有 algorithm selection/portfolio solver 思想的扩展 | Hybrid DAE block 级、类型兼容和完整成本感知的异构求解路由 | 首次提出 AI 自动选择数值求解器 |
| 用 residual/误差判据验收候选 | 是 | 数值计算传统与 surrogate correction 的系统集成 | family-specific 原方程 gate 与统一诊断/后续路径合同 | 首次提出 residual verification |
| 候选失败后继续下一条数值求解路径 | 是 | portfolio/cascade solver 的标准控制流 | 将 rejection probability 与 terminal fallback 成本纳入级联优化 | 首次提出 fallback 概念 |
| learned preconditioner/operator/iterator | 是 | 已有学习型数值方法的专家化封装 | 在统一 Router 中组合多类学习专家、经典校正器与预算 | 提出普适领先的新神经算子或基础求解算法 |
| Hybrid DAE IR 上统一线性、非线性、ODE/后续 DAE 的专家路由 | 部分 | **潜在系统创新** | 面向异构方程块的统一 profile、plan、gate 与 capability 合同 | 已证明覆盖完整 Modelica/Simulink/DAE 语义 |
| 将 gate/correction/fallback 成本纳入路由与 benchmark | 相关思想分散存在 | **当前最有辨识度的系统贡献候选** | 完整成本协同设计及其可证伪实验协议 | 已解决一般输入下数值验收的性能下限 |
| 请求条件 cost/pass 预测与 exact expert--budget DP | algorithm selection、budgeted search 各自存在 | **当前核心算法贡献候选** | 在同一 action 合同下联合选择专家、预算和顺序，并与 exhaustive oracle 对照 | 已证明真实公共 solver portfolio 上普遍最优 |
| 组合系统整体优于强基线 | 尚未由现有证据普遍证明 | 待验证的组合原创性 | 只能对已完成的具体 workload 报告结果与负复制 | 通用 `100×` 或所有方程族普遍加速 |

因此，论文最稳健的原创性定位不是“首次使用 AI 解方程”，而是：**在 Hybrid DAE block 级异构求解中，将算法选择、expert--budget 联合决策、学习候选、原方程验收、局部校正与 terminal fallback 形成统一完整成本优化问题。** 当前已在一个 Phase 5 线性 Operator workload 上完成同协议机器矩阵，并加入 1-NN、CART、结构成本 selector 与 residual/Jacobi corrector 对照；其中 1-NN、CART 和 corrector 仍是本地文献算法重实现，结构成本 selector 是 SMAVE 本地解释模型，均不是公开实现复现实验。仍必须通过公开 selector/corrector 代码、非线性/事件 family 复制和真实 workload 证明组合贡献，不能把单一线性 family 的消融外推为通用结论。

当前 selector 基线采用固定场景集合上的 leave-one-scenario-out 1-NN：每个查询只使用其他 63 个场景的 eligible winner 标签，查询自身标签不进入训练；所选专家在查询场景上仍执行完整 Runtime 与原方程 gate。标签生成和 1-NN lookup 成本未计入求解时间，因此它是 feature-based selector 的算法对照，不是完整端到端成本结论。重复运行显示 exact winner 会在实用等价专家间随系统负载变化，故 exact 分类准确率只作为 telemetry；主要报告失败、gate mismatch、相对事后参考的配对时间比和 5% 实用等价率。权威字段位于 `build/release/paired-oracle/evidence.txt` 与 `build/release/academic-evidence/summary.txt`。

同一证据现增加 leave-one-scenario-out depth-3 Gini CART：每个查询重新用其余 63 个场景训练，使用确定性特征/阈值决胜，预测 expert 后复用已测完整 Runtime/gate 样本。当前运行中 CART 零失败、零 gate mismatch，exact winner accuracy 与 5% 实用等价率优于 1-NN，但训练与推理成本仍排除；该实现只用于复现标准 CART 协议，`literature_cart_selector_public_code_used=0`，不得描述为原作者公开代码复现。

新增结构成本 selector 不再直接拟合 winner 标签，而是从场景向量提取 count、mean、variance、range、RMS 与 L1 等查询前可得聚合统计，对每个 eligible expert 分别拟合固定 `0.01` 岭正则的对数完整 Runtime 成本。每次 leave-one-scenario-out 都排除查询 winner 和查询计时，再选择预测成本最低的 expert，并复用该查询的完整 Runtime/gate 样本评价。重复运行均为零失败、零 gate mismatch 和中位 oracle 比 1，但选择分布从两个 expert 退化到只选 dense direct，且与 1-NN/CART 的 practical-equivalence 排名发生反转。该负结果支持将 regret/实用等价率作为主评价，也证明固定岭回归和简单聚合特征不足以稳定解释微秒噪声下的 expert 选择。模型训练与推理成本尚未计入，跨矩阵族稳定性也未证明。

Phase 5 correction 对照现增加 weighted diagonal residual/Jacobi corrector：在同一 raw latent candidate 上强制至少一步同步残差校正，最多 32 步，每步后使用原方程 fused strict gate，且与 scalar reference gate 检查 decision/residual 一致。当前线性 workload 中每个 candidate 一步即通过，零失败、零 gate mismatch，成本低于通用 Newton correction；该结果只说明固定线性 Poisson workload 上经典局部校正足够，不能外推非线性/事件方程，也不是公开 corrector 代码复现。

## 加速比的来源分解

### 1. 摊销整个求解过程

FNO、Geo-FNO、GINO 把多次网格迭代压缩为固定深度 Tensor 推理，因此在重复查询中能达到 `10^2–10^5×`。代价是：

- 需要同一方程族的大量高保真训练数据；
- 结果误差通常在 `10^-2–10^-1` 相对量级，而非 `10^-4`；
- 不保证每个实例满足离散方程 residual；
- 极高数字可能比较不同硬件，或只预测 QoI。

因此这类工作只能形成 `Tensor Operator/QoI Expert`，Router 必须限制在已验证分布和容差允许的任务。

### 2. 学习误差传播与多尺度修正

Learned Neural PDE Solver 和 UGrid 不直接生成最终解，而是学习比 Jacobi/smoother 更有效的迭代更新或 coarse-grid correction。原解仍是固定点，迭代 residual 决定停止。这类方法倍数较低，却更符合本项目 `fallback + 原方程验收` 的要求。

### 3. 学习预条件器和初值

NeuralPCG、GNP、Neural-Preconditioned Poisson Solver 学习 $P^{-1}r$、稀疏因子或子空间，使 Krylov/Newton 更快收敛。收益来自：

- 减少迭代次数；
- 降低 AMG/ILU 等预条件器的构造成本；
- 利用相似矩阵序列的重复结构；
- 最终 residual 与原求解器一致。

这是 Modelica/Simulink 每时间步反复出现相近 Jacobian 系统时最值得优先实现的方向。

### 4. 预训练跨方程表示

PDEformer、PROSE、Poseidon 的真正价值不是当前预测精度，而是证明方程计算图、符号 token 和解轨迹可以共同形成跨方程 embedding。该 embedding 可支持：

- 编译期 Router 的方程家族识别；
- 从专家库检索 warm-start/preconditioner；
- 用少量新模型数据适配 instance adapter；
- 预测哪个专家可能在目标 residual 下最省时。

## 对 `0.01%` 目标的判断

`0.01% = 10^-4`。当前高倍神经算子论文多数没有在完整解相对误差 `≤10^-4` 下证明 `10^3–10^5×`。UGrid 等工作能把 **relative residual** 推到 `10^-4`，但 residual 不等于解误差；对于条件数大的系统，有

$$
\frac{\|\hat x-x\|}{\|x\|}
\lesssim \kappa(A)\frac{\|b-A\hat x\|}{\|b\|}.
$$

因此项目不能把论文中的 residual `10^-4` 直接解释为解误差 `0.01%`。直接接受还需条件数/误差估计、QoI 检查和分支一致性；否则只允许作为预条件器或 warm start。

## 证据等级

| 等级 | 条件 | Router 权限 |
|---|---|---|
| E0 | 只有预测误差或宣传加速比 | 仅研究，不上线 |
| E1 | 明确基线、硬件和统计测试，但无原 residual | 可作 QoI/候选专家，必须校正 |
| E2 | 迭代 residual 控制最终解，报告总墙钟 | 可作 warm-start/preconditioner/iterator 专家 |
| E3 | 同硬件、同精度、含 setup，跨域测试且有 fallback | 可进入 Top-k 主路径 |
| E4 | 对声明连续域有误差/收敛证书 | 可在证书域内直接接受 |

当前最成熟的主线证据集中在 E2；高倍神经算子多数属于 E1。项目实验必须推动自己的专家达到 E3，而不能直接继承论文加速承诺。
