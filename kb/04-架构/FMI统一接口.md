---
tags: [fmi, modelica, simulink]
status: partial-cpp-baseline
updated: 2026-07-16
---

# FMI 统一接口

> **定位更新：** FMI 不再是核心知识表示。它适用于跨工具执行、最终部署和与原工具做差分测试；当源方程或块图可得时，核心流程使用 [[04-架构/源方程IR与可验证求解]]。仅有 FMU 时必须明确无法进行完整方程级验证。

## 为什么仍保留 FMI

- 避免直接解析 Modelica 方程展开、Simulink 私有 `.slx` 语义；
- 同一执行器可消费不同工具导出的模型；
- 保留变量元数据、参数、输入输出、事件和初始化生命周期；
- 最终代理也能封装为 FMU，回到原工程系统中。

## Model Exchange 与 Co-Simulation

| 类型 | 特点 | 对 AI 管线的价值 |
|---|---|---|
| Model Exchange | FMU 提供方程，宿主提供积分器 | 可控制采样与求解器；研究向量场/导数更合适 |
| Co-Simulation | FMU 自带求解器，通过通信步长推进 | 工具兼容通常更简单；内部状态/导数可见性较弱 |
| Scheduled Execution (FMI 3.0) | 时钟和分区调度 | 离散控制与嵌入式场景后续重点 |

第一版同时接受 ME/CS，但优先选择能暴露所需状态/导数的 ME；无法获得内部状态时训练输入—输出 latent state 模型。

## 导入检查

1. 校验 FMI 版本、平台二进制和 `modelDescription.xml`；
2. 列出可调参数、输入、输出、单位、默认实验；
3. 检查状态保存/恢复、方向导数、事件指示器等能力；
4. 运行 smoke simulation 和确定性复跑；
5. 记录 FMU hash 与导出工具版本；
6. 对不支持能力给出降级策略，而非静默忽略。

## 当前 C++20 基线

仓库现已实现元数据级 `blackbox-degraded` 导入：`smave import-fmu` 读取 FMI 2.0/3.0 ZIP 或解包目录，生成 `SMAVE_FMI_BLACKBOX_4`。v4 在 v3 的 derivative 与数组维度描述符上新增 Scheduled Execution input Clock priority；SE input Clock 缺失 priority 或超出 UInt32 范围时硬拒绝。旧 `SMAVE_FMI_BLACKBOX_1/2/3` 可只读升级，旧 SE Clock 以兼容 priority 0 读取，但不构成 source-declared priority 证据。

`smave smoke-fmu-se` 现提供受限 Scheduled Execution runtime：接受一个或多个标量周期 input Clock，要求 CLI interval 等于最小声明周期，并按 interval/shift 生成 activation 队列。同刻 activation 先按更小数值代表更高优先级的静态 priority 排序，再按 value reference/name 稳定 tie-break；报告记录每次 activation 的 priority 与 Clock interval/shift/priority 汇总。缺失 priority、周期不符或回调不平衡均拒绝。该证据不代表 Clock 依赖图、并发抢占、动态 priority/周期重配置、aperiodic Clock 或多 FMU 调度。

权限边界不可扩张：仅允许轨迹代理与差分测试，禁止方程级 residual 验证和 Direct expert。当前另有显式 opt-in 的 FMI 2.0/3.0 smoke。FMI 2 Co-Simulation 执行标量 Real 同步固定通信步 lifecycle，兼容 host tuple 与 legacy platform binary directory；声明 `canGetAndSetFMUstate=true` 时强制首步保存、恢复和确定性重放。FMI 2 Model Exchange 从 derivative state index 推导连续状态，以宿主 RK4 驱动最多 1024 个 event indicator，支持有限 horizon 内的 root、time event、连续状态 reset 和完整首步 replay。FMI 3 Co-Simulation 执行标量 Float64 固定通信步 lifecycle；声明 `hasEventMode=true` 时，还允许在已完成通信点执行 `EnterEventMode→UpdateDiscreteStates` 有界固定点`→EnterStepMode`。Model Exchange 对最多 1024 个 event indicator 的显式 ODE 由宿主 RK4 调用 continuous-state/derivative API；所有横截候选分别二分，按时间和 indicator index 选择最早 root，事件固定点可重置连续状态后重算剩余区间，因此同一宏步可顺序处理多个不同 root。初始化及后续事件固定点还可通过绝对 `nextEventTime` 调度 horizon 内的时间事件；宿主在计划时间和横截 root 中选择最早者并切分 RK4 区间。FMI 3 路径声明 state save/restore 时同样强制首步 replay。CMake 自有 fixtures 已证明当前 host 的 FMI 2 CS/ME 与 FMI 3 CS/ME instantiate、initialization、step/integrator、get-set、state、受限 CS/ME event mode 和 terminate ABI 路径。

该 smoke 同进程加载 native code，不是生产沙箱。FMI 2 CS 若声明 `canHandleVariableCommunicationStepSize=true`，可在 `DoStep` 返回 `Discard` 后读取有限且严格内部前进的 `lastSuccessfulTime`，按最多 1024 个子步继续到原通信点；缺失 status query、零进展、越界时间和未声明可变通信步均拒绝。若声明规范属性 `canRunAsynchronuously=true`，`Pending` 路径要求同时存在 `fmi2GetStatus`、`fmi2CancelStep` 和 `stepFinished` 回调；当前宿主以 1 ms 轮询，并使用默认 100 ms、可配置为 1–60000 ms 的期限等待，并要求查询完成状态与回调一致；自有 fixture 由可取消且析构前 join 的 worker thread 发出回调，报告必须证明回调来自非调用线程，超时取消后失败。独立 `simulate-ssp` 已覆盖 SSP 1.0 中至少两个 FMI 2/3 CS 的固定宏通信网格、有向无环 feed-forward 组合，可在同一系统中混用 FMI 2 scalar Real 和 FMI 3 scalar Float64；连接先按经 SSD/FMU 双重 BaseUnit 定义核验的 affine unit conversion 转换，再执行系数和结果均有限的 `target=factor*source+offset`。FMI 3 实例在完整通信点请求且声明 `hasEventMode` 时，可执行最多 1024 轮本地离散固定点；固定点产生的 future `nextEventTime` 会在全体组件声明可变通信步时成为内部全局通信点，所有实例同步推进、到点处理、传播信号并继续原宏步。未声明 event mode、过去/越界时间、任一组件缺失可变步能力、continuous-state change、termination、early return 和 incomplete step 均拒绝。该多 FMU 切片仍不提供 FMI 2 Discard/Pending 的跨实例协调、跨实例 superdense 固定点、rollback、并发 stepping、参数绑定、其他 mapping transformation 或一般 SSP hierarchy。FMI 3 单实例 smoke 另支持 early return；该能力尚未进入 SSP master。ME grazing/nominal change、数值/Boolean/String/Binary 标量 I/O，以及固定 extent 与冻结 unsigned structuralParameter extent 的 FMI 3 数值、Enumeration、Boolean、String 和 Binary 数组 I/O 已由自有 C++ fixture 覆盖；数组按 XML 维度顺序扁平化。运行期结构参数重配置、FMI 2 数组、Scheduled Execution 多 Clock/多分区优先级与抢占及广泛第三方 FMU 兼容性仍未覆盖。

FMI 3 Clock 按标准只允许标量，导入器显式拒绝 Clock 的 `Dimension`。ME smoke 在初始化离散固定点之后通过 `GetIntervalDecimal/GetShiftDecimal` 查询 output Clock 的 decimal interval、shift 和 qualifier，验证 interval 有限且为正、shift 有限且满足 `0 <= shift < interval`，并写入 `CLOCK_INTERVALS`、`CLOCK_SHIFTS` 和 `CLOCK_INTERVAL_QUALIFIERS`。这只是时钟元数据查询，不执行模型分区激活，也不构成 Scheduled Execution master。

FMI 3 基础 ME/CS serialized state 由 capability 显式启用，并绑定现有首步 replay：宿主取得 state handle 后查询有界字节数、序列化、释放原 handle、反序列化出新 handle，再从新 handle 恢复并完整重放。报告中的 `STATE_SERIALIZATION_ATTEMPTED`、`STATE_SERIALIZATION_PASSED` 和 `SERIALIZED_STATE_BYTES` 是权威证据；损坏 magic 与仅声明序列化但不声明基础 state 能力的 FMU 均有负向门禁。当前只证明自有 C++ fixture 的同版本、同平台、同进程字节往返，不把 serialized state 宣称为跨版本或跨平台稳定格式。

FMI 2 基础 ME/CS 现通过对应的 `SerializedFMUstateSize/SerializeFMUstate/DeSerializeFMUstate` ABI 执行同一事务；ME 的 35-byte fixture payload 保存时间、连续状态、参数和事件标志，CS 使用 32-byte payload。两版基础 ME/CS 都从反序列化的新 state handle 恢复。该证据不覆盖 event-mode、early-return、Pending 等事件或异步 CS，也不能概括为所有 FMI 接口。

## Simulink 注意事项

Simulink 的 FMU 导出/导入能力会受到 MATLAB 版本、附加产品或第三方工具影响。项目文档和 CLI 应区分：

- 仅有 FMU：降级管线直接处理，但不得声明达到方程级验证；
- 有 MATLAB 工具链：调用适配器自动导出；
- 无导出能力：允许用户通过 CSV/Parquet 轨迹接入，但会失去自动主动采样能力。

## 模型组

模型组可先由 `simulate-ssp` 读取受限 SSP/System Structure，按子 FMU 建立固定步 feed-forward 执行图；更一般的反馈迭代、事件/rollback、多 rate 和复杂 hierarchy 仍可由 OMSimulator 或现有 master 作为单一黑盒生成数据，后续再扩展整体代理、局部代理或混合执行。
