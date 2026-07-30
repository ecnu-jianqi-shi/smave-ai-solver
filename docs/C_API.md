# SMAVE C ABI v1

`include/smave/c_api.h` 是 SMAVE 首批稳定外部接口。当前 capability 包括 dense/CSR 线性系统、经原方程 gate 验证的调用方线性/非线性 solver fallback 与无事件 ODE/DAE stepper fallback、强单调线性互补问题、callback 非线性代数系统、平滑显式 ODE、受限显式 ODE 事件、受限 index-1 fully implicit DAE event/reinit、无事件且一致初始化的 Jacobian-backed Hessenberg-like index-2 DAE、受限显式与 fully implicit DAE 多模式 hybrid、受限标量多速率块图，以及 per-call cooperative cancellation 与 relative timeout/deadline。

## 构建与安装

```bash
cmake --preset release
cmake --build build/release --target smave_sdk -j2
cmake --install build/release --prefix build/release/sdk-install
```

安装产物包括：

- `include/smave/c_api.h`
- `include/smave/cpp_api.hpp`
- macOS `lib/libsmave.dylib`
- Linux `lib/libsmave.so`
- `lib/cmake/SMAVE/SMAVEConfig.cmake`、`SMAVEConfigVersion.cmake` 与
  `SMAVETargets.cmake`，导入目标为 `SMAVE::smave`（C）和 `SMAVE::cpp`（C++20 RAII）
- `lib/pkgconfig/smave.pc`
- Windows 支持尚未通过验收，不属于 ABI v1 当前声明范围。

CMake 下游只需：

```cmake
find_package(SMAVE 0.1 CONFIG REQUIRED)
target_link_libraries(my_solver_host PRIVATE SMAVE::smave)
```

C++20 宿主可使用：

```cmake
find_package(SMAVE 0.1 CONFIG REQUIRED)
target_link_libraries(my_cpp_solver_host PRIVATE SMAVE::cpp)
```

非 CMake 下游可使用：

```bash
pkg-config --cflags --libs smave
```

两份元数据均以安装位置相对路径计算 prefix。安装 verifier 会先把完整 SDK 复制到另一目录，
再从 relocated tree 配置、编译并运行独立 CMake consumer，同时仅用 relocated
`PKG_CONFIG_PATH` 编译运行纯 C consumer；任何源树 include、构建树 library 或原安装绝对路径泄漏
都会使门禁失败。

完整安装后宿主证据：

```bash
cmake --build build/release --target reproduce-c-api-sdk -j2
cmake --build build/release --target reproduce-c-api-abi-matrix -j2
cat build/release/c-api-sdk/evidence.txt
cat build/release/c-api-sdk/installed-evidence.txt
cat build/release/c-api-abi-matrix/evidence.txt
```

## C++20 RAII 包装

`smave/cpp_api.hpp` 是仅包含公共 C 头的 header-only 包装，不把内部 C++ 类、STL 容器或异常
放入动态库 ABI。`smave::sdk::Library`、`Problem`、`Solver`、`CancelToken` 和 `Result` 都是
move-only；内部所有权保持 solver → problem → library、token → library、result → library，因此
父 wrapper 在子对象存活期间离开作用域不会提前销毁 C handle。`Result::info()` 和
`Result::provenance()` 复制 C 字符串，不返回可能在 result 销毁后悬空的指针。

`solve`、`solve_for`、取消以及线性/非线性 caller fallback 返回 `smave::sdk::SolveOutcome`。`OK`、
`SOLVE_FAILED`、`CANCELLED` 和 `DEADLINE_EXCEEDED` 都保留非空 RAII result 供诊断；参数、ABI、
状态或 capability 违约抛出带稳定 `smave_status` 的 `smave::sdk::Error`。包装不改变 C API 的
callback 生命周期、同步执行、cooperative cancellation 或 original-gate 规则。

`smave_cpp_api_raii_host` 与安装后公共头宿主验证父/结果生命周期、cancel/deadline outcome、
external linear/nonlinear fallback、状态异常和调用方 allocator 平衡；relocated CMake consumer 通过
`SMAVE::cpp` 验证导出的 C++20 interface target。

## CLI/SDK 公共线性服务

线性 CLI 与 C ABI 不再各自维护候选排序或正确性门禁，而是共同调用
`include/smave/solve_service.hpp` 定义的 `smave.verified-linear-solve.v1` 服务。
该服务从矩阵构造 `SparseLinearProfile`，调用正式 `EquationAssessment → SolvePlan`
Router，再按 plan 顺序执行结构化、工业稀疏、SuperLU、内置稀疏和 dense
fallback 适配器，并统一执行独立原矩阵 residual/backward-error gate。

`smave_solver_solve_linear_with_fallback` 可为线性 problem 注册一次调用期的
`smave_linear_fallback_desc`。共享服务先按正式 plan 尝试全部已验证内建候选；只有它们全部失败或
被原矩阵 gate 拒绝后，才在当前 solve 线程同步调用一次 caller fallback。callback 从独立输出
buffer 开始，只写固定维度 solution；返回成功仍必须通过同一原矩阵 residual/backward-error gate，
否则返回 `SMAVE_DIAGNOSTIC_ORIGINAL_GATE_REJECTED`，callback 非零返回则分类为
`SMAVE_DIAGNOSTIC_CALLBACK_FAILURE`。接受时 backend 为 `caller-linear-fallback-v1`，
`used_fallback=1`，service id 仍为 `smave.verified-linear-solve.v1`。可选 token/relative timeout
在 callback 前后检查，但正在执行的 callback 不被抢占；descriptor 和 `user_data` 必须保持存活到
调用返回。该 capability 当前仅适用于线性问题，传入其他 problem 稳定返回
`SMAVE_STATUS_UNSUPPORTED`。

CLI 可直接接收 dense 或 CSR 结构化输入，例如：

```bash
build/release/smave solve-linear \
  --storage dense \
  --matrix 4,-1,0,-1,4,-1,0,-1,3 \
  --rhs 3,2,2 \
  --output build/release/solve-service/manual.txt
```

以下门禁使用同一 fixture 分别调用 CLI 和纯 C ABI 宿主，并要求 service id、
equation family、plan id、backend、fallback 标记、residual、backward error、
solution 和 diagnostic 全部一致：

```bash
cmake --build build/release --target reproduce-solve-service -j2
cat build/release/solve-service/evidence.txt
```

## CLI/SDK 公共非线性服务

表达式 CLI 与 callback C ABI 共同调用 `smave.verified-nonlinear-solve.v1`。
两种输入均形成 `NonlinearAlgebraicProfile`，经正式 `EquationAssessment → SolvePlan`
Router 生成相同 plan；plan 决定先尝试调用方/表达式方向导数 Jacobian，失败时从
原始初值重建有限差分 damped Newton，最终始终调用原 residual 独立验收。

`smave_solver_solve_nonlinear_with_fallback` 接受一次调用期的
`smave_nonlinear_fallback_desc`；RAII 对应 `Solver::solve_nonlinear`。共享服务先逐个执行并
独立验收全部内建候选，只有它们全部失败或被拒绝后，才把原始 initial state 复制到新鲜
solution buffer 并同步调用 caller solver。callback 成功输出仍必须通过同一原 nonlinear
residual gate；错误 candidate 返回 `SMAVE_DIAGNOSTIC_ORIGINAL_GATE_REJECTED`，callback
非零返回分类为 `SMAVE_DIAGNOSTIC_CALLBACK_FAILURE`。接受 backend 为
`caller-nonlinear-fallback-v1`。token/relative timeout 在 callback 前后和最终 gate 检查，
但不能抢占 callback；descriptor 与 `user_data` 必须存活到调用返回。该合同只验证固定维度
非线性代数终态，不是 ODE/DAE trajectory 或 stepper 合同。

```bash
build/release/smave solve-nonlinear \
  --unknowns x,y --initial 1.5,1.5 \
  --residual 'x*x+y-5' --residual 'x+y*y-3' \
  --jacobian directional \
  --output build/release/solve-service/manual-nonlinear.txt
```

`reproduce-solve-service` 同时验证线性与非线性入口，并要求每一类 CLI/C ABI 的
equation family、plan id、backend、fallback、gate 指标、solution 和 diagnostic
分别完全一致。

## C ABI 无事件 ODE/DAE caller stepper

`smave_solver_solve_ode_with_fallback` 与 `smave_solver_solve_dae_with_fallback` 只接受既有
无事件 ODE/DAE problem；eventful descriptor 和其他方程族稳定返回
`SMAVE_STATUS_UNSUPPORTED`。两者只在正式 plan 的全部内建候选失败或被 gate 拒绝后执行，
每次 callback 的输入来自最后完整提交状态，输出 buffer 预填同一状态，不暴露失败 candidate。

ODE `smave_ode_dense_step_fallback_desc` 必须为固定 `to_time` 同时返回 quarter、midpoint、
three-quarter 和 endpoint state。SMAVE 独立调用原 RHS 于起点和四个输出节点，以两个半步
composite-Simpson 积分 defect 按 solver absolute/relative tolerance gate；接受 backend 为
`caller-ode-dense-stepper-fallback-v1`。这比只检查 endpoint RHS finite 更强，但仍是固定节点
dense-output 合同，不是任意自适应轨迹证明。

DAE `smave_dae_step_fallback_desc` 同时获得 differential mask 与上一状态/导数，返回 endpoint
state/derivative。SMAVE 首先检查 differential 分量的 backward-Euler kinematic relation 和
algebraic derivative 为零，再调用原 `F(t,y,ydot)` residual；index-2 子集继续执行 hidden-rank
与 hidden-residual gate。接受 backend 为 `caller-dae-stepper-fallback-v1`。

两类 callback 前后以及 gate/commit 边界检查 token 与 relative timeout，但不能抢占正在执行的
callback。首版不支持事件 root/reinit、hybrid mode change、callback 内 cancellation context、
caller-requested adaptive step size、异步/跨进程 stepper 或 rollback。

## C ABI 互补服务

`smave_complementarity_desc` 直接接收 dense row-major 或 canonical CSR 的矩阵
`M`、offset `q` 和可选初值，表达 `z >= 0`、`w=M z+q >= 0`、`z_i w_i=0`。
Builder 复制全部数组，生成与文本前端相同的 `ComplementarityIR`，并只接受
对称部数值正定且对角为正的强单调 LCP。非强单调矩阵返回
`SMAVE_STATUS_UNSUPPORTED`；非法/非 canonical CSR、NaN/Inf 和错误 ABI 版本稳定
拒绝，且不会转入方程字符串解析。

公共 `smave.verified-complementarity-solve.v1` 服务复用正式
`EquationAssessment → SolvePlan`，依次执行 projected Gauss–Seidel、
Fischer–Burmeister 半光滑 Newton 和 20 变量以内 enumerated active-set terminal
fallback。每个候选都从复制的原始 LCP 重新计算 gap expression、primal/dual
inequality 与 complementarity product gate。`smave_result_get_complementarity_info`
返回三类 violation 和实际尝试数，`smave_result_copy_complementarity_gap` 两阶段复制
最终 gap。

`smave_c_api_complementarity_service_probe` 覆盖 dense/CSR 等价、强制 terminal
fallback、输入复制、非强单调拒绝、非法 CSR、ABI mismatch 和扩展 struct 尾部保持；
`reproduce-c-api-sdk` 还从安装目录分别编译纯 C 和纯 C++ 宿主，只包含公共
`smave/c_api.h` 并链接安装后的共享库。当前合同不支持一般非线性互补、摩擦锥、
混合整数接触、用户 fallback callback 或大规模生产 MCP 后端。

## C ABI 块图服务

`smave_block_graph_desc` 使用节点和连接的整数索引直接描述闭合块图，不接收文件路径、
方程文本或内部 C++ 对象。当前节点包括 scalar `constant`、`gain`、weighted `sum`、
`unit_delay`、三种 scalar `switch`，以及具有固定输入/输出端口数的 callback 子模型。
每个节点声明 sample time/offset；二者必须是 `base_step` 的整数倍，offset 必须小于
period，运行期按稳定拓扑顺序执行到 `end_time`，未到期节点使用 zero-order hold。
除进入 `unit_delay` 的状态反馈外，same-rate/same-offset scalar 代数 SCC 使用稳定节点
顺序的 Jacobi fixed-point iteration；各轮从同一前轮输出读取全部反馈输入，只有最大
输出变化满足 solver absolute/relative tolerance 才提交。不同 period/offset 的反馈 SCC
在 create 时拒绝，非收敛 SCC 在 `maximum_iterations` 后返回稳定
`SMAVE_DIAGNOSTIC_ITERATION_LIMIT`。每个输入端口必须恰有一个 driver，输出按照
descriptor 节点顺序展平。

callback 节点把 primary evaluator、独立 original-output gate 和可选 local fallback
作为三个不同函数指针。公共 `smave.verified-block-graph-solve.v1` 服务先执行 primary，
再由 gate 返回非负 residual；callback 失败、非有限输出或 gate 超过 solver tolerance
时才执行 local fallback，并用同一独立 gate 重新验收。任一节点最终失败时当前 tick 的
内部 delay/output 状态不提交。callback 可能因 gate 与 fallback 被多次调用，必须可重入、
不得依赖一次性副作用，并且其 `user_data` 至少存活到所有 solver/result 调用完成。

`smave_result_get_block_graph_info` 返回 tick 数、节点执行数、fallback 次数、执行的
feedback component/iteration 数、最终 fixed-point residual、最大连接误差和接受输出的
最大 original-gate residual；`smave_result_copy_solution` 返回展平输出，
`smave_result_copy_block_output_offsets` 给出每个节点的半开区间 offset，
`smave_result_copy_block_commit_order` 给出稳定节点索引顺序。Builder 复制节点参数、sum
weights 和连接拓扑，callback/function pointer 与 `user_data` 按既有回调存活期合同借用。

`smave_c_api_block_graph_service_probe` 覆盖整数多速率/offset、zero-order hold、稳定
commit order、独立 gate 触发 local fallback、输入复制、收敛反馈的解析固定点、发散
反馈 iteration-limit、mixed-rate SCC/非法连接/ABI mismatch 拒绝和扩展 struct 尾部保持；
`reproduce-c-api-sdk` 从安装目录分别编译只包含公共头的
纯 C 与纯 C++ 宿主。当前 capability 不代表一般连续多物理网络、向量/bus/unit 传播、
跨速率/非收缩/带不连续事件的反馈求解、跨组件事件迭代、FMI/SSP rollback 协商、异步调度、分布式执行或
任意 Simulink/Modelica 块语义。

## CLI/SDK 公共 ODE 服务

表达式 CLI 与 callback C ABI 共同调用 `smave.verified-explicit-ode-solve.v1`。
两种输入均形成 `ExplicitOdeProfile` 并复用正式 `EquationAssessment → SolvePlan`；
主路径使用 adaptive RK4 step-doubling，失败时从原始初值启动 Heun/Euler fallback。
每个接受步必须通过 embedded scaled local-error gate，终点还会再次调用原 RHS，
拒绝非有限状态或导数。

```bash
build/release/smave solve-ode \
  --states x --initial 1 --rhs=-x \
  --start 0 --end 1 --max-step 0.1 \
  --absolute-tolerance 1e-10 --relative-tolerance 1e-8 \
  --output build/release/solve-service/manual-ode.txt
```

当前 ODE result 的 `residual_inf` 与 `backward_error` 均表示最大 scaled local error，
不是代数 residual。该接口只返回终态，不返回轨迹或事件记录，也不支持事件定位、
reset、DAE 约束或反向积分。

ODE 调用方应使用 `smave_result_get_ode_info` 读取类型化元数据：终止时间、最大
scaled local error、接受步数和拒绝步数。对线性或非线性代数 result 调用该函数会
返回 `SMAVE_STATUS_UNSUPPORTED`，避免把 ODE 局部误差语义误用于代数 residual。

## C ABI 显式 ODE 事件服务

`smave_event_ode_problem_desc` 在不改变原 `smave_ode_problem_desc` 的前提下增加
`smave_event_desc[]`。每个事件具有方向 `-1/0/+1`、整数优先级、guard callback、
reset callback 和独立 `user_data`。Router 将该对象识别为 `explicit-ode-with-events`，
计划使用 RK4 step-doubling、方向 crossing、bracketed bisection root、同刻优先级排序、
事务式 reset，以及 RHS/guard 有限性 gate；主路径失败时从原初值以减半最大步长重跑。

当前机器 probe 覆盖上升沿、下降沿、两个同刻事件的优先级顺序、atomic reset、root
时刻、最终状态和 stuck reset 拒绝。reset 后必须离开对应 guard 面，否则整个候选被
拒绝，避免零时间重复触发。`smave_result_get_ode_info` 的尾部新增 `event_count` 与
`last_event_time`；旧 ODE 结果前缀尺寸仍可查询，且尾随哨兵保持不变。

当前显式事件合同只覆盖标量 guard、离散 state reset 和单进程同步执行；不支持
DAE 一致性语义、superdense-time 多轮固定点、动态事件增删、轨迹/事件表导出、
rollback、异步 callback 或跨组件事件仲裁。

## C ABI fully implicit DAE 服务

`smave_dae_problem_desc` 直接接收 `F(t,y,ydot)=0` residual、可选组合 Jacobian
`dF/dy + alpha*dF/dydot`、初始状态/导数、时间区间和 `differential_mask`。mask 为
`1` 的分量按 backward-Euler 构造导数，mask 为 `0` 的代数分量导数固定为零，因此
该接口能表达包含代数约束的 index-1 DAE；在更窄的结构门禁下也能执行下述 index-2 子集，而不是把显式 ODE 改名为 DAE。

公共服务 id 为 `smave.verified-fully-implicit-dae-solve.v1`。服务使用正式
`FullyImplicitDaeIR → EquationAssessment → SolvePlan` 生成 plan，每步以调用方组合
Jacobian 执行 damped Newton；若该 Jacobian 路径失败，则从未修改的 step predictor
重跑有限差分 Jacobian fallback。初始一致性和每个接受步都重新调用原 DAE residual
独立验收。`smave_result_get_dae_info` 返回终止时间、最大原 residual、接受/拒绝步数，
并在扩展尾部返回 event count、last event time、识别出的 differentiation index、
hidden-rank 检查数、最小 rank margin 与最大 hidden residual。

机器 probe 使用两变量系统 `ydot + z = 0, z - y = 0`，明确包含一个微分状态和一个
代数变量；同时验证错误 Jacobian fallback 与不一致初始导数拒绝。

当调用方提供可用的组合 Jacobian 时，服务分别以 derivative scale `0` 与 `1` 求值，
恢复 `dF/dydot` 并按 `differential_mask` 识别 dynamic/algebraic 行列。若代数直接块奇异，
但 differential derivative block 与 Schur 型 hidden multiplier block 均满秩，则该问题被
分类为受限 Hessenberg-like index-2。初值和每个接受步除原 `F(t,y,ydot)` gate 外，还必须
通过微分代数约束得到的 hidden residual 与 hidden-rank gate；rank 缺失稳定拒绝，原 residual
为零但 hidden derivative 不一致的初值也稳定拒绝。公共 capability 为
`SMAVE_CAPABILITY_INDEX_TWO_DAE`，equation family 为
`dae-fully-implicit-hessenberg-index2`，仍复用
`smave.verified-fully-implicit-dae-solve.v1`，不新增平行 descriptor 或旁路服务。

机器 probe 使用仿射系统 `qdot-v-lambda=0, vdot+q=0, q=0`，从一致初值
`[q,v,lambda]=[0,1,-1]` 推进后保持解析解，并验证 hidden-inconsistent 初值、hidden-rank
奇异模型、ABI mismatch、既有 event-result prefix 和未知扩展尾部保持。安装门禁分别从安装
目录用纯 C 与纯 C++、仅包含 `smave/c_api.h` 的宿主重新编译运行该能力。该合同不执行
自动初值投影、符号约束微分、Pantelides、dummy derivatives 或一般 index reduction；不支持
高指数事件、跨 mask hybrid、一般非线性高指数保证或 index-3 多体系统。

`smave_event_dae_problem_desc` 在不修改既有 DAE descriptor 的前提下增加
`smave_dae_event_desc[]`。guard 同时读取 state 与 derivative；reset/reinit 同时输出
post-state 与 post-derivative。每个二分中点均从当前已提交状态重新执行完整
backward-Euler/Newton 隐式子步，不使用线性插值伪造 DAE 轨迹。最早同刻事件按
priority/source index 在临时事务状态中执行，只有全部 post 值有限、代数分量导数为零、
原 DAE residual 通过且各事件离开 guard 面后才提交。

DAE event probe 使用同一两变量系统，定位下降沿 root 约 `0.33333333337`，验证同刻
优先级、事务 reinit、错误方向不触发，并要求不一致 residual、非零代数导数、stuck
guard 与 callback failure 全部拒绝。同一 problem/solver 还由 8 个线程各重复求解 16 次，
event time 和终态必须与串行基准逐位一致；自定义 allocator 在全部结果销毁后必须平衡，
且安装后公共头/动态库会重新编译运行同一 probe。旧 `smave_dae_result_info` 前缀尺寸仍
可查询，尾随哨兵保持不变。当前 event/reinit 实现只支持固定最大步长 backward-Euler、同步标量
guard、调用方给定一致初值/reinit 和 index-1 候选；已识别的 index-2 问题带 event 时会明确拒绝。
该路径不支持自动一致性投影、自适应阶次/步长、一般高指数 DAE、质量矩阵专用格式、
superdense-time 固定点、rollback 或轨迹/完整事件表输出。

## C ABI 显式多模式 hybrid 服务

`smave_hybrid_problem_desc` 直接接收连续状态、初始 mode、`smave_hybrid_mode_desc[]`
和 `smave_hybrid_transition_desc[]`。每个 mode 提供独立 RHS；每个 transition 提供
source/target mode、方向、优先级、guard、reset 和独立 `user_data`。服务 id 为
`smave.verified-explicit-hybrid-solve.v1`，equation family 为
`explicit-hybrid-multimode`。`smave_result_get_hybrid_info` 返回时间、local-error、步数、
event count、last event time 和 final mode。

mode 索引在服务内部编码为导数恒为零的离散增广状态，因此同一 problem/solver 不持有
跨求解共享的可变 mode。只有当前 source mode 的 transition 才暴露真实 guard；模式专属
RHS、root localization、priority 排序、事务 reset、RHS/guard gate 和 retry fallback
均复用 verified ODE 服务。纯 C probe 执行 `mode0 → mode1 → mode0` 两次真实切换，
验证最终模式 0、两个 event 和终态约 `0.25`。

冲突 probe 在同一 root 激活两个 source-mode transition：高优先级 transition 先形成临时
post-state/mode，第二个 transition 因 source mode 已不匹配而失败。整个批次必须回滚至
事件前最后提交的 state/time/mode，且 `event_count` 保持零。该测试发现旧 ODE event
实现会在批次尚未成功时逐个累加 event count，现已改为整批 reset 与 post-RHS 全部通过
后一次提交审计计数。

`smave_hybrid_transition_desc` 尾部可选 `stable_reset` 与 `write_mask`。扩展 callback 同时读取
整个事件时刻固定不变的 root pre-state 和当前微步 state；同一 source mode 的 enabled
transition 先各自产生 proposed state，再按 write mask 合并。所有 transition 必须声明相同
target mode，写集合必须互不重叠；满足条件时整轮一次提交，否则返回
`SMAVE_DIAGNOSTIC_EVENT_RESET_CONFLICT` 并回滚。后续微步仍读取同一稳定 pre-state。

旧 transition prefix 的 `struct_size` 继续走原 priority 顺序 reset 语义，库不会读取新增尾部。
纯 C probe 故意在旧 prefix 后放置不完整扩展字段，验证仍按旧语义成功；扩展 probe 则覆盖
同轮两个不相交写集合并、下一微步稳定 pre-state、重叠写集合拒绝和 target mode 冲突拒绝。
显式 hybrid 最多执行 64 个跨 mode 微步，成功后整体提交，循环或 callback 失败则回滚整个
事件时刻。该合同仍只覆盖单实例同步执行和标量 guard；不支持 DAE stable-pre/write-set、
跨组件固定点、动态 transition、标准 rollback 协商、异步、Scheduled Execution 或完整
Modelica/FMI simultaneous-equation/superdense-time 语义。

## C ABI fully implicit 多模式 DAE 服务

`smave_hybrid_dae_problem_desc` 直接接收公共 differential mask、初始 state/derivative/mode、
`smave_hybrid_dae_mode_desc[]` 与 `smave_hybrid_dae_transition_desc[]`。每个 mode 提供独立
`F_m(t,y,ydot)=0` residual 和可选组合 Jacobian；transition 提供 source/target、方向、
优先级、读取 state/derivative 的 guard，以及同时输出 post-state/post-derivative 的 reinit。
服务 id 为 `smave.verified-fully-implicit-hybrid-dae-solve.v1`，equation family 为
`dae-fully-implicit-hybrid-multimode`。

DAE transition 尾部可选 `stable_reset`、`state_write_mask` 与
`derivative_write_mask`。扩展 callback 同时读取事件时刻固定的 root pre-state/
pre-derivative 和当前微步 state/derivative。同一 source mode 的 enabled transition 必须
具有相同 target mode；各 proposed state/derivative 仅按对应 write mask 合并，两类写集
分别要求互不重叠。合并完成后只执行一次目标 mode 一致性投影。state 写重叠、derivative
写重叠或 target mode 分歧都返回 `SMAVE_DIAGNOSTIC_EVENT_RESET_CONFLICT` 并回滚完整事件
时刻。旧 transition prefix 不读取新增尾部，继续走原顺序 reinit 语义。

mode 索引编码为导数恒为零的增广代数状态，因此共享 problem/solver 不保存可变 mode。
连续推进和 root localization 复用 fully implicit backward-Euler/Newton 路径。事件时刻最多
执行 64 个跨模式微步。若 reinit 未通过目标 mode residual，服务保持 differential state，
以 differential derivative 和 algebraic state 为未知量执行有限差分 damped-Newton 一致性
投影，代数导数固定为零，投影结果仍须通过目标 mode 原 residual。任何 callback、不可投影
目标模式或微步上限失败都会将 state、derivative、time、mode、event count 和 projection
count 回滚至事件前最后提交点。`smave_result_get_hybrid_dae_info` 返回这些计数。

纯 C probe 使用两个物理变量和三个 DAE mode，在约 `t=0.5` 执行 `0→1→2` 同刻级联；
两个 transition 都故意提交不一致 algebraic state，投影后第二步切换到约束 `z=3x`，最终
得到 `x=0.25,z=0.75`，且 committed projection count 为 2。负向 fixture 在第一微步投影
成功后进入不可满足的目标 mode，必须返回稳定一致性拒绝码，并保持 event/projection count
均为零。同一 solver 由 8 个线程各重复 16 次，终态、投影计数和事件时刻必须与串行基准
逐位一致；安装后公共头和动态库重新编译运行相同 probe。扩展 probe 还覆盖 state 与
derivative 不相交写集合并、跨微步 stable pre、三类冲突拒绝、旧 prefix 兼容及新路径
8×16 共享 solver 逐位确定性。该能力只覆盖固定 differential mask 的受限 index-1 事件后
投影与单实例写集仲裁，不等同于一般初始化投影、跨 mask 模式切换、高指数 index reduction、
跨组件/FMU 固定点或完整 Modelica/FMI rollback 语义。

## 生命周期

```text
smave_library_create
  → smave_linear_problem_create / smave_nonlinear_problem_create /
    smave_ode_problem_create / smave_event_ode_problem_create /
    smave_dae_problem_create / smave_event_dae_problem_create /
    smave_hybrid_problem_create / smave_hybrid_dae_problem_create
  → smave_problem_finalize
  → smave_solver_create
  → smave_solver_solve / smave_solver_solve_cancellable /
    smave_solver_solve_with_timeout /
    smave_solver_solve_linear_with_fallback /
    smave_solver_solve_nonlinear_with_fallback /
    smave_solver_solve_ode_with_fallback /
    smave_solver_solve_dae_with_fallback
  → smave_result_get_info / smave_result_get_ode_info /
    smave_result_get_dae_info / smave_result_get_hybrid_info /
    smave_result_get_hybrid_dae_info /
    smave_result_copy_solution
  → smave_result_destroy
  → smave_solver_destroy
  → smave_problem_destroy
  → smave_library_destroy
```

父对象存在活动子对象时，销毁返回 `SMAVE_STATUS_INVALID_STATE`。`smave_solver_solve` 在数值失败时可返回 `SMAVE_STATUS_SOLVE_FAILED` 和非空 result；调用方应读取 diagnostic，并仍然销毁 result。

## library 错误栈

`SMAVE_CAPABILITY_ERROR_STACK` 表示 library 级状态错误记录接口可用。对于已经能够确定
所属 library、但调用在创建 result 前失败的已覆盖路径，可依次调用
`smave_library_get_error_count` 和 `smave_library_get_error` 读取最近最多 8 条记录；索引
`0` 始终是最新记录。每条 `smave_error_info` 包含该 library 内单调递增且非零的
`trace_id`、稳定 `smave_status`、操作名和供人排障的消息。当前覆盖结构化 nonlinear/ODE/DAE
problem 创建、problem finalize/destroy、solver 创建、caller fallback 合同、cancel-token
生命周期、library live-child 销毁以及 solve result 分配/异常失败；有非空 result 的数值失败仍以
result diagnostic 为权威，不重复压入错误栈。调用在 library 尚不可知时（例如空 library handle）
只返回 status，也不会生成记录。

错误栈按“当前线程 + library”隔离：同一 library 在工作线程产生的记录不会出现在调用线程；
trace id 仍由 library 跨线程统一分配，因此不同线程间可观察到间隔，但不得假设连续。栈满后
丢弃最旧记录。`operation` 和 `message` 指针由当前线程的栈持有，仅在该线程上针对同一
library 的下一次错误栈修改（记录新错误、`smave_library_clear_errors` 或成功销毁 library）前
有效；需要长期保存时必须立即复制。查询本身不新增错误记录，ABI/size/index 失败只返回 status，
避免诊断读取递归污染。C++20 `Library::errors()` 返回已复制的 value records，
`Library::clear_errors()` 清空当前线程对应栈。

`smave_cancel_token_create` 创建由同一 library allocator 管理的一次性 sticky token；任意线程可调用
`smave_cancel_token_request`。`smave_solver_solve_cancellable` 只接受与 solver 属于同一 library
的 token。取消被观察后返回 `SMAVE_STATUS_CANCELLED` 和非空 result，`success=0`，稳定诊断为
`SMAVE_DIAGNOSTIC_CANCELLED`；动态状态结果保留最后完整提交的 state/time/审计计数，不暴露
当前未提交 candidate。token 在 request 后保持取消状态，只有无活动 solve 时
`smave_cancel_token_reset` 才可复用；活动 solve 期间 reset/destroy 返回
`SMAVE_STATUS_INVALID_STATE`。token 必须在所有使用它的调用完成前保持存活。

`smave_solver_solve_with_timeout` 接收相对纳秒预算。`0` 表示在首个 cooperative
checkpoint 立即过期，`SMAVE_TIMEOUT_INFINITE` 与既有无超时入口等价。预算被观察为过期后返回
`SMAVE_STATUS_DEADLINE_EXCEEDED` 和非空 result，稳定诊断为
`SMAVE_DIAGNOSTIC_DEADLINE_EXCEEDED`。timeout 不修改可选 token 的 sticky 状态，因此超时后可直接
以同一未 request token 重试；若同一 checkpoint 已同时观察到 token request 和 timeout，显式取消
优先并返回 cancelled。动态结果同样只包含最后完整提交状态。

该合同是 cooperative cancellation：verified service 在候选尝试、Newton/PGS/fixed-point
迭代、ODE/DAE 步进、root localization 和事件事务提交边界检查 token。用户 callback 或第三方
稀疏 backend 正在执行时不会被强制抢占，请求或预算过期会在其返回后的下一个检查点生效。当前没有
独立 watchdog/超时线程、绝对 wall-clock deadline、callback 参数中的取消 token、进程终止、SLA
调度或硬件 kernel 抢占语义。

## 稳定诊断码

`smave_result_get_diagnostic_code` 返回版本化数值枚举，宿主不得解析自由文本
`diagnostic` 来决定控制流。当前粗粒度分类包括 success、invalid contract、callback
failure、numerical failure、original gate rejected、iteration limit、event reinit callback
failure、event reinit consistency rejected、event guard not released、event reset conflict、
cancelled 和 deadline exceeded。枚举数值是 C ABI
合同；后续版本只能追加新值，不能改变既有值的含义。自由文本仍用于人类排障，可以增加
上下文或调整措辞，不属于机器稳定格式。

DAE event 负向 probe 明确验证：不一致 state/derivative 与非零代数导数返回
`SMAVE_DIAGNOSTIC_EVENT_REINIT_CONSISTENCY_REJECTED`，reset callback 失败返回
`SMAVE_DIAGNOSTIC_EVENT_REINIT_CALLBACK_FAILURE`，未离开 guard 面返回
`SMAVE_DIAGNOSTIC_EVENT_GUARD_NOT_RELEASED`；成功路径返回
`SMAVE_DIAGNOSTIC_SUCCESS`。该查询是新增函数，不改变冻结的 v1 result struct 布局。
显式 hybrid 扩展 transition 的 write-mask 重叠或 target-mode 分歧返回
`SMAVE_DIAGNOSTIC_EVENT_RESET_CONFLICT`；旧 transition 前缀的诊断分类保持不变。
公共 C 宿主还要求 dense/CSR 线性、非线性 fallback、平滑 ODE、显式事件和 DAE
成功结果全部返回 `SMAVE_DIAGNOSTIC_SUCCESS`，奇异线性系统返回
`SMAVE_DIAGNOSTIC_ORIGINAL_GATE_REJECTED`。服务枚举到 C ABI 枚举使用显式 switch，
不依赖两个枚举碰巧具有相同的底层数值。

所有 descriptor 输入在 create 时复制，以下内容除外：

- 非线性 residual/Jacobian callback；
- ODE RHS callback；
- DAE residual/组合 Jacobian callback；
- ODE/DAE event guard 与 reset/reinit callback；
- callback 的 `user_data`；
- 自定义 allocator 函数及其 `user_data`。

这些回调和上下文必须至少存活至对应 problem、solver 和 result 全部销毁。

## 线程安全

- finalize 后的问题数据不可变；
- 同一 solver 可以被多个线程并发调用；每次调用创建独立 result 和局部求解状态；
- 每个并发 solve 应使用独立 token；request 可从其它线程调用；
- 同一 result 不允许被并发修改；查询函数只读；
- 用户 callback 和自定义 allocator 是否线程安全由调用方负责；
- problem/solver/library 的销毁不得与其上的求解或查询并发执行。

## 正确性与 fallback

线性路径按结构化直接法、可用工业稀疏后端、内置稀疏直接法和 dense classic fallback 尝试。任何候选只有通过独立原矩阵 backward-residual gate 后才返回成功。

非线性 C API 将 callback 适配为 `smave.verified-nonlinear-solve.v1` 公共服务输入。该服务由正式非线性 plan 决定候选顺序，首先使用调用方 Jacobian 执行 damped Newton；该路径失败时从未修改的原始初值重跑有限差分 Jacobian damped Newton。最终结果再次调用原 residual callback 独立验收。callback Jacobian 不被当作最终正确性权威。

ODE C API 将 RHS callback 适配为 `smave.verified-explicit-ode-solve.v1`，主/备积分器都由正式 ODE plan 驱动并受 embedded error 与原 RHS 有限性门禁约束。DAE C API 将结构化 callback 适配为 `smave.verified-fully-implicit-dae-solve.v1`，由 fully implicit DAE assessment/plan、原 residual gate 和 finite-difference fallback 约束；可用组合 Jacobian 还会触发受限 index-2 hidden-rank/hidden-residual 门禁。显式 ODE event C API 复用 event assessment/plan、root/reset 与 RHS/guard gate；DAE event C API 复用隐式子步、原 residual gate 与一致 reinit 门禁；显式和 DAE hybrid C API 均以离散 mode 增广复用对应事件路径，并在同一物理时刻按 priority/source index 执行最多 64 个跨模式微步。hybrid DAE 还支持固定 differential mask 的受限 index-1 事件后自动一致性投影；循环、callback 或不可投影目标模式失败时回滚整个事件时刻及审计计数。线性、强单调线性互补、含 same-rate scalar feedback fixed point 的受限多速率块图、非线性、ODE、无事件且一致初始化的 Jacobian-backed Hessenberg-like index-2 DAE、受限显式/DAE 事件与受限多模式 hybrid 已进入公共 verified solve service；一般非线性/摩擦接触、连续/向量多物理网络、跨速率/非收缩反馈与跨组件事件迭代、自动高指数初始化/一般 index reduction、跨 mask 投影、完整 `pre(...)`、同轮冲突仲裁、高指数事件和标准 rollback 协商仍未完成，因此不代表 P0-G4/P2-G8 已全部完成。

## ABI 规则

- `SMAVE_ABI_VERSION` 当前为 1；
- 所有输入/输出 struct 均携带 `struct_size` 和 `abi_version`；
- opaque handle 隐藏 C++ 对象、STL、异常和编译器 ABI；
- C++ 异常不得越过动态库边界；
- 所有内存由创建 library 时选择的 allocator 管理；
- `backend` 和 `diagnostic` 字符串由 result 持有，仅在 result 存活期间有效；
- `smave_error_info` 字符串由当前线程的 library 错误栈持有，只在下一次同栈修改前有效；
- `smave_result_get_provenance` 返回 service id、plan id 与 equation family 的只读字符串；当前四类结果均必须返回非空 plan id 和对应 equation family；
- `smave_result_get_diagnostic_code` 是宿主错误分支的稳定接口，自由文本仅供人读；
- capability 返回 0 的问题类型不得创建，也不得静默转入文本解析或其他旁路。

`tests/abi/v1/smave/c_api.h` 是发布后不得改写的 v1 ABI 基线。兼容矩阵使用该
冻结头文件重新编译独立 C 宿主，并同时验证 build-tree 与安装后当前动态库；
还通过在 v1 struct 尾部添加哨兵字段，验证库只读取/写入已知前缀且不破坏未知尾部。
当前显式与 DAE hybrid transition 自身也以 `offsetof(stable_reset)` 作为旧前缀下限；只有
调用方 `struct_size` 覆盖完整新增尾部时，库才读取 stable-reset 与对应 write-mask 字段。
因为当前尚无 v1 之后的历史动态库，“新头/旧库”反向兼容只能从下一 ABI/SDK
版本发布后开始验证，现阶段不得声称该方向已有历史证据。

## 已验证边界

本机机器证据覆盖 dense/CSR 线性、dense/CSR 强单调 LCP、互补三重原问题 gate 与 terminal fallback、互补安装后 C/C++ 宿主、标量多速率块图、zero-order hold、稳定 commit order、callback 独立原输出 gate/local fallback、块图输入复制与安装后 C/C++ 宿主、跨线程 cooperative cancellation、最后完整 tick 原子提交、sticky token reset/reuse、活动 token 生命周期拒绝、跨 library token 拒绝与安装后 C/C++ cancellation 宿主、非线性错误 Jacobian fallback、平滑显式 ODE、显式 ODE 上升/下降方向事件、同刻优先级 atomic reset、stuck reset 拒绝、旧 ODE result 前缀兼容、含代数变量的 index-1 fully implicit DAE、DAE 错误 Jacobian fallback、DAE 初值一致性拒绝、仿射 Hessenberg-like index-2 解析解、hidden consistency/rank 拒绝、index-2 安装后 C/C++ 宿主与 DAE result 两级前缀/扩展尾兼容、DAE 隐式事件 root、同刻优先级事务 reinit、四类 reinit 负向拒绝、DAE event 8 线程共享 solver 逐位确定性与 allocator 平衡、显式 hybrid 两次模式切换、显式与 DAE 旧 transition prefix 兼容、跨微步稳定 pre-state、同轮不相交 state/derivative write-set 原子 merge、重叠 write-set/target-mode 冲突拒绝、显式与 DAE 受限跨模式 superdense 级联、目标 DAE residual 一致性拒绝、64 微步循环拒绝、事务 rollback 与审计原子性、hybrid DAE 新旧路径 8 线程共享 solver 逐位确定性、ABI mismatch、未 finalize 调用、奇异线性系统拒绝、父子生命周期、8 线程共享线性 solver、安装后宿主重编译，以及公共服务验证。

尚未覆盖：新头/旧历史库反向矩阵、Linux CI 实际结果、Windows、一般非线性/摩擦互补、连续/向量/bus 多物理网络、跨速率/非收缩/事件驱动的跨组件/FMU 反馈与写集仲裁、自动高指数初始化与一般 index reduction、高指数事件/index-3 多体、一般初始化/跨 differential-mask 一致性投影、标准 rollback 协商，以及 ODE/DAE/hybrid 轨迹与完整事件记录。
