# PnP 质量检查与关联诊断（2026-11-10）

## 本轮修改

- `PoseSolver::solve` 增加可选 `PoseDiagnostic*` 参数，兼容现有两参数调用。
- 在调用 IPPE 前拒绝 NaN/Inf、非凸/交叉/退化四边形；筛掉非有限或任一角点非正深度的候选；最终状态再检查有限性、正深度及原有 0.2–30 m 距离范围。失败清空输出，避免保留上一帧结果。
- `reproj_error` 改为最终 yaw_gain 修正后的平均四角点像素残差。另存 `fit_reproj_px`（修正前）与 `final_reproj_px`，避免将经验修正前的拟合残差当作最终姿态误差。
- 新增 `pose.max_reproj_error_px`，CLI `--set max_pose_reproj=...`。默认 **0：关闭经验重投影门限**；几何、数值和正深度检查始终生效。设正数后对最终平均像素残差门控。需要带标注数据确定阈值，尤其要考虑 yaw_gain 自身改变残差。
- `RobotTracker` 增加可选逐观测诊断，默认关闭。区分初始化、身份不符、位置门控、朝向门控、组合评分门控、非有限评分、匹配冲突与匹配成功；记录重初始化事件及其观测索引。门限未放宽，非有限评分会被拒绝。
- `autoaim_video --diagnostics` 导出独立的 PnP 和关联 CSV。诊断用同一批观测按原始顺序额外回放，逐帧检查状态/匹配数与主评测一致；不计入评测打印的计时。

关联诊断目前覆盖离线整车 RobotTracker 策略，不涵盖哨兵 OutpostTracker 或所有策略。PnP 仍为无状态解算器，本轮没有添加多解的时序选择。

## 复现

```bash
./build/apps/autoaim_video '/home/lgnatius/测试视频/3(1).avi' --out runs/pose-quality/current --diagnostics
./build/apps/autoaim_video '/home/lgnatius/测试视频/4(1).avi' --out runs/pose-quality/current --diagnostics
./build/apps/autoaim_video '/home/lgnatius/测试视频/3(1).avi' --config core/config/video_reference.yaml --out runs/pose-quality/n0 --diagnostics
./build/apps/autoaim_video '/home/lgnatius/测试视频/4(1).avi' --config core/config/video_reference.yaml --out runs/pose-quality/n0 --diagnostics
```

输出：`*_pose_diagnostics.csv`、`*_association.csv`、原有 track/report/诊断文件。全片回放使用已验证的视频/检测配置缓存；此次变更不影响像素检测，所以未重复检测。

- pose CSV 的 detection 是原始检测索引，observation 是过滤后观测索引，失败为 -1。无检测帧也输出一行 `no_detection`。
- association CSV 按过滤后的 observation 索引对应。空帧输出 `no_observation`；记录 state_before/state_after、事件及重新初始化索引。初始化不等于一次 EKF 匹配。
- nearest_distance 和 nearest_yaw 分别是所有预测板中的最小值，可能属于不同板；distance_pass 是通过距离门控的候选数，geometry_pass 是同时通过距离和朝向的候选数。
- best_score 是通过几何门控候选的最小 `position Mahalanobis + 4*dyaw²`，现有门限为 40。不是单独的 NIS，不能当作纯卡方统计量。未计算时为 inf/nan。
- 一个观测可能经过多个检查；reason 表示拒绝的阶段。有候选通过评分但被一对一贪心分配占用时才记 assignment_conflict。

## 两段视频的证据

两视频总计 5625 帧；current / n0 各完整运行一次。所有检测均通过本轮新增 PnP 检查（2380 / 3885 个）。当前素材上没有新删除的观测。两种配置的 track.csv 均与上一轮逐字节一致，虚拟命中与预测指标不变：current 56.8%/64.4%，N0 67.5%/68.6%。yawdiag 的 reproj 列随误差定义修复而改变。

下面按**装甲板观测数**统计，不能直接当成失败帧数。同一帧可有一个观测被拒绝、另一个被匹配。

| 原因 | current 3 | current 4 | N0 3 | N0 4 |
|---|---:|---:|---:|---:|
| matched | 1944 | 2890 | 2149 | 3400 |
| distance_gate | 263 | 543 | 88 | 193 |
| yaw_gate | 171 | 441 | 141 | 281 |
| score_gate | 1 | 3 | 1 | 4 |
| initialized | 1 | 6 | 1 | 5 |
| not_selected | 0 | 2 | 0 | 2 |

无观测帧分别为 77 / 73，两种配置相同。实际视频没有出现身份不符、匹配冲突或非有限评分事件；身份与匹配冲突路径由合成测试验证，非有限评分保护未单独进行故障注入。

当前配置未匹配的整帧数是 276 / 541（含初始化）；N0 为 146 / 225。第 4 段，前面已统计的“有检测但未关联”468 帧，加上无检测 73 帧，恰为 541。

修复后的平均角点误差：

| 当前配置 | 修正前 P50/P90/P99（px） | 最终 P50/P90/P99（px） |
|---|---|---|
| 3(1).avi | 1.50 / 2.03 / 3.51 | 1.79 / 2.86 / 6.26 |
| 4(1).avi | 1.36 / 1.81 / 3.76 | 1.71 / 3.19 / 6.39 |

**结论：**当前拒绝主要出现在评分前的几何门控。优先核实标定、角点和朝向解算偏差，再判断门限/测量噪声是否合理。不能仅靠上述计数宣布每次拒绝都是错误，也不能通过放宽门限追求更高跟踪率。零畸变离线假设与样本的兼容性较好，不等于它是相机真值。

本地证据：`runs/pose-quality/`（日志、全部逐帧 CSV、summary.json）。

## 标定溯源

`legacy/text.yaml` 存五个非零系数。`legacy/Armor/src/ArmorSolver.cpp:26–31` 直接读取并转换到 OpenCV；第 63 行 solvePnP、第 106 行 projectPoints 都原样使用。没有重排系数的代码。

仓库有现用 `tools/calibrate_camera.py` 与 `tools/calibrate_intrinsics.py`，但没有找到能将旧系数追溯到这些工具的原始标定结果。第三项 0.29333 引人怀疑，仍不足以确认应如何重排。更新了 `docs/distortion_order.md` 中“当前默认为零畸变”的过时叙述，保留参数待原标定图或原始输出确认。

## 验证

- Release 构建通过；CTest 3/3；缓存/CSV 集成测试通过。
- 新增独立合成验证：最终姿态残差、启用残差门控、异常/交叉/重复角点、距离门控、输出清空、初始化/身份/位置/朝向/评分/冲突诊断、诊断开关数值一致性、空帧不保留旧记录。
- 两套完整仿真表与上一轮输出逐字节一致。
- 两视频 × 两配置回归均完成，四个完整 track.csv 与上一轮逐字节一致，诊断回放一致性检查全部通过。

本轮目标是建立可靠诊断和输入保护，没有宣称命中率提升。
