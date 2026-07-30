---
tags: [pinn, residual, constraints, training]
status: proposed
updated: 2026-07-13
---

# PINN 可借鉴机制

## 定位

本项目借鉴 PINN 的核心思想是：**让原始物理方程参与训练，而不是仅从求解器轨迹拟合输入—输出关系。** 采用范围限定为方程块候选解、数值求解器 warm start、未知闭合项和反例采样；不采用“一个 PINN 直接替代任意完整 Modelica/Simulink 混合 DAE”的假设。

## Block-PINN 形式

对结构分析得到的方程块

$$R_b(v_b;c_b)=0,$$

其中 $v_b$ 是块内待求变量，$c_b$ 是参数、上游变量、前一收敛解、步长和当前模式等已知上下文。网络输出候选解

$$\hat v_b=N_\theta(c_b).$$

训练不需要每个配点都有求解器标签，可以直接计算原方程：

$$L_{eq}=\frac{1}{|C|}\sum_{c_b\in C}\left\|W_bR_b(N_\theta(c_b);c_b)\right\|_2^2.$$

$W_b$ 必须根据方程量纲、变量 nominal、绝对/相对容差或局部 Jacobian 做缩放，防止大数值方程支配训练。

## 混合训练目标

纯 residual loss 容易收敛到错误根或病态区域，因此默认使用：

$$
L=\lambda_sL_{solver}
+\lambda_rL_{eq}
+\lambda_cL_{constraint}
+\lambda_bL_{branch}
+\lambda_jL_{jacobian}.
$$

| 损失 | 作用 |
|---|---|
| $L_{solver}$ | 少量原求解器收敛解监督，锚定正确解支 |
| $L_{eq}$ | 大量无标签上下文上的原方程残差 |
| $L_{constraint}$ | 守恒、不等式、非负性、互补和代数约束 |
| $L_{branch}$ | 与前一解、模式和 continuation 参数保持分支连续 |
| $L_{jacobian}$ | 避免奇异区，或使候选落入 Newton 收敛域 |

损失权重不能长期固定。应借鉴自适应 PINN 的梯度平衡、增广拉格朗日或按方程残差统计动态调整，避免某些方程“看似训练完成、实际未被优化”。

## 约束编码

优先级如下：

1. **硬参数化：** 能通过输出变换严格满足的边界、正值或简单守恒约束，直接编码进网络输出；
2. **变量消元：** 编译器可消去的代数约束不交给神经网络学习；
3. **增广拉格朗日：** 对一般等式约束动态更新乘子；
4. **惩罚项：** 仅用于无法硬编码的软约束；
5. **部署投影：** 候选解投影回可行流形后，仍重新计算原 residual。

## Collocation 不等于枚举

PINN 配点应在局部上下文空间 $c_b$ 中生成，而不是枚举完整输入函数。配点来源包括：

- 原求解器实际访问的 block context；
- 相邻上下文插值和局部扰动；
- 方程 residual 最大的自适应配点；
- Jacobian 近奇异、多根边界和事件邻域；
- 闭环 rollout 到达的状态；
- 区间细分中尚未验证的 cell。

每轮训练后重新评估候选池，按 residual、约束违例和 ensemble disagreement 追加配点。最终选择由原 residual 决定，不由网络不确定性单独决定。

## 正确根与分支

方程残差接近零只说明找到了一个根。为保持与原模型相同的解支，需要联合使用：

- 前一时间步/continuation 步的收敛解；
- 当前离散模式与事件历史；
- 与预测器 extrapolation 的距离；
- 物理势能、熵条件或稳定性判据；
- 原求解器的 branch ID 或初始化规则；
- 必要时让网络输出多个候选，再由 residual 和分支规则选择。

## 训练残差与验收残差隔离

必须生成两个独立函数：

- `training_residual`：可自动微分、可批处理，供训练使用；
- `runtime_residual`：由原方程代码生成器产生，使用求解器一致的精度和容差，供部署验收。

二者要做 golden test，但运行时 gate 不应依赖训练框架实现，以避免共同缺陷使验证失效。

## 适用性门槛

| 方程块特征 | 使用方式 |
|---|---|
| 平滑、低中维、单根 | 可训练 candidate solver，达标后直接接受 |
| 平滑、多根 | 多候选 + 分支约束；默认保留 Newton 修正 |
| 刚性或近奇异 | 只做 warm start/预条件器 |
| 高指数 DAE | 先 index reduction 和一致初始化，不直接 PINN 化 |
| 事件/不连续块 | 按模式分别训练，事件边界由原逻辑处理 |
| 安全关键块 | 默认 warm-start-only，除非有更强形式证书 |

## 实施原则

- PINN loss 是训练信号，不是发布证书；
- 原方程始终可执行并保留 block fallback；
- 优先证明减少 Newton/Krylov 迭代，再尝试跳过原求解；
- 训练失败时拆块、缩放或改变数值参数化，不盲目增大网络；
- 任何直接接受策略都必须经过独立 `runtime_residual` gate。

