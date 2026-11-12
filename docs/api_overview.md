# 公共 API 速览

给第一次在本仓写代码的人：一页看清两条流水线的入口、关键类型和语义
约定。细节看各头文件的注释与专题文档；设计理由见 docs/decisions/。

## 两条流水线

**核心链（步兵/英雄，离线与实车共用）**

```
Config::load → Detector::detect → PoseSolver::solve → FrameObservations
→ RobotTracker::update → RobotAimer::aim → AimCommand
```

**哨兵链（整机封装）**

```
SentryVision::process(bgr, stamp, gimbal_yaw/pitch/roll) → SentryOutput
（内部：Detector → PoseSolver → TargetManager（多目标）→ RobotAimer）
```

离线评测走第三条等价路径：`loadDetections → computeObservations →
evaluate`，与核心链共用跟踪/瞄准/判分代码，所以离线数字可与实车对照。

## core（namespace autoaim）

| 头文件 | 关键入口 | 说明 |
|---|---|---|
| config.hpp | `Config::load(path)`；`saveTuningOverrides(path)` | 全量配置加载。保存只写 camera/tracker/aimer 三节调参子集（契约由 test_config.cpp 钉死），不是完整序列化 |
| detector.hpp | `Detector::detect(bgr)`；`detect(bgr, windows)`；`setEnemyColor(c)` | 传统灯条管线；windows 重载是 ROI 窗口检测；可选 YOLO 关键点后端（默认关） |
| pose.hpp | `CameraModel`；`PoseSolver::solve(det, obs)`（const） | 内参/畸变/外参（支持杠杆臂）；IPPE_SQUARE 解算 + 质量诊断 |
| pose_buffer.hpp | `PoseBuffer`；`ClockAligner` | 姿态按曝光时刻 SLERP 查询；设备时钟与主机时钟对齐 |
| delay_calibration.hpp | `DelayCalibrator` | 恒定传输延迟 D 的上车标定（静态装甲板 + 云台扫动） |
| camera_source.hpp | `CameraSource::grab(SourceFrame&)`；`VideoFileSource` / `DeviceSource` / `MvsSource` | 统一帧源接口，后端全是 PImpl。`SourceFrame.hw_stamp=false` 的戳是软件估计，不满足 G-02 对齐预算，不许当硬件戳用 |
| tracker.hpp | `RobotTracker::update(frame)`；`OutpostTracker`；`PointTracker` | 整车 EKF（生产路径）；PointTracker 是对照基线 |
| ballistics.hpp | `Ballistics::solve(s, h)`；`simulate(...)`；`setMeasuredBulletSpeed(v)` | 二次阻力弹道；实测弹速可运行时注入覆盖配置 |
| aimer.hpp | `RobotAimer::aim(tracker/target, now)`；`PointAimer::aim`；`DirectAimer::aim`（const） | Robot/Point 的 aim **有状态**（换板滞回、指令变化率），故不标 const；Direct 无状态，保持 const |
| evaluation.hpp | `evaluate(cfg, obs, cam)`；`ShotJudge`；`loadDetections`；`computeObservations` | 三口径命中率（条件/可验证/保守）与预测误差统计；评测器与帧率无关（用戳差分） |
| draw.hpp | Overlay 绘图函数 | 只服务可视化，不进算法路径 |
| types.hpp | `ArmorDetection`、`FrameObservations`、`TargetState`、`AimCommand`、`deviceId()` | 跨模块数据契约；目标身份 = 颜色+编号+板型（DeviceId） |

## sentry（namespace sentry / sentry::io）

| 头文件 | 关键入口 | 说明 |
|---|---|---|
| target_manager.hpp | `TargetManager::update(frame)`；`snapshots(t)`；`select(states)` | 一身份一跟踪器；select 有选择记忆（滞回），不标 const |
| sentry.hpp | `SentryVision::process(...)`；`setEnemyColor`；`setRoiEnabled`；`config()`（只读） | 配置构造时固定，无 mutableConfig；运行时只许用显式 setter |
| roi.hpp | `mergeWindows(windows, slack)`；跟踪引导窗口构建 | TG ROI：按各跟踪的预测板位开窗（60 px 边距），全帧扫描兜底周期 30 帧 |
| serial_protocol.hpp | 17B 入 / 21B 出帧编解码；可选 CRC-8 | 协议无时间戳字段；byte 2 的 CRC 默认关，MCU 固件填充前不许开 |
| io/serial_port.hpp | `sentry::io::SerialPort` | 串口全生命周期（打开/重连/读线程）在类内；姿态查询失败返回 false = 本帧无姿态，调用方必须跳帧，不许沿用旧姿态 |
| evaluation_sentry.hpp | 哨兵策略的离线评测入口 | 与核心策略共用 ShotJudge，数字可直接同表对照 |

## 语义约定（容易误用的四条）

1. **戳的语义**：全仓时间一律是"曝光中心时刻"。姿态在曝光时刻插值，
   预测时域从曝光时刻起算；把接收时刻当曝光时刻用，是本仓历史上最大
   的系统误差源（ADR-003）。
2. **配置不可变**：Config 在构造时分发给各子对象并各自持有副本；构造
   之后改配置对象不会同步到已构造的对象——要改行为，用对象自己的
   setter 或重建对象。
3. **aim 的状态性**：同一个 Aimer 实例服务一条瞄准会话；多目标并行时
   一会话一个实例，不要跨会话共用（滞回状态会串扰）。
4. **AUTOAIM_ROOT**：apps 与 tests 靠它定位默认配置与模型路径，它是
   PUBLIC 编译定义且指向源码树 core/——这正是 install/export 暂缓的
   原因（见 docs/engineering_review_r6.md 缓办节）。
