# 更新日志

## 2026-11-10 — 评测可靠性与位姿/关联诊断（PR #1）

- 修复检测缓存身份、无效检测参数 sweep、ONNX 后端配置同步与异常关键点过滤。
- 延迟标定模式禁止开火，正常退出发送失效帧，补齐 `sentry_robot --help`。
- 增加 PnP 几何/数值/正深度检查；修正最终姿态重投影误差，经验残差门限默认关闭。
- 增加 `--diagnostics` 导出逐检测和逐观测原因；补齐 N0 离线参考配置及 CI 集成测试。
- 两视频 × 两配置的完整报告/跟踪输出保持一致；详细证据见
  [位姿与关联诊断](docs/POSE_ASSOCIATION_DIAGNOSTICS_2026-11-10.md)、
  [视频复测](docs/VIDEO_TEST_2026-11-10.md)及
  [首轮修复](docs/REVIEW_UPDATE_2026-11-10.md)。
- 更新目录索引与配置口径说明，保留单份核心实现及历史材料。

## 2026-11-10 — 审查修复与性能优化（分支 `optimize/core-hardening`）

详细分析、数据与合并前检查清单见 [`docs/review_2026-11.md`](docs/review_2026-11.md)。

### 缺陷修复（均附回归测试，旧代码上会失败）

- **串口未处于 raw 模式**：驱动默认的 `ICRNL` 把姿态帧中每个 `0x0D` 字节读成 `0x0A`，
  误差随字节位置从 3e-4° 到约 20°。现为 cfmakeraw 等价设置。
- **串口描述符竞争**：读线程独占打开/关闭/重连，`send()` 在互斥锁下写、出错只请求重连；
  `EAGAIN/EINTR` 不再触发重连；同一次 read 中的多帧按字节回推各自时间戳；新增 `reconnects()` 计数。
- **默认参数上车每帧都被跳过**：帧戳总比最新姿态样本新。`PoseBuffer` 增加有界外推，
  `sentry_robot --max-extrap-ms`（默认 10，设 0 恢复严格包围）。
- **多目标管理**：改为一身份一轨迹（`deviceId` / 前哨站），不再产生同 id 的重复轨迹；
  切换滞回只保留合格目标（不再选回 TEMP_LOST 或超距目标）。`target.association_gate` 不再使用。
- **中心模式提前量**：按弹着时刻的中心瞄准，计入飞行期间的平移。
- **数值健壮性**：`wrapAngle` 改为 O(1)，±inf 不再死循环；Singer 外推同步衰减速度；
  装甲板高度随中心竖直外推。
- **ROI**：前哨站窗口使用前哨站俯仰；"身后"判断改用相机深度。
- **其他**：`sentry_robot` 前馈角速度在目标切换/跳帧时清零；`SentryVision` 禁止拷贝。

### 性能

- 弹道：RK4 10 ms 步长 + 三次 Hermite 求交（相对 10 µs 参考解误差 < 0.05 µm），割线法求俯仰。
  `Ballistics::solve` 87 → 8.6 µs，`RobotAimer::aim` 259 → 28 µs，仿真全量 5.9 → 1.2 s。
- ROI 窗口检测前先合并（`sentry::mergeWindows`，`src/roi.cpp`）。

### 工程化

- `Config::validate()`：加载时检查参数合法性并一次列出所有问题。
- 新增 `aimer.yaw_offset_deg / pitch_offset_deg`（`docs/hardening.md` 有描述但此前缺失）。
- `serial_protocol.hpp` 不再依赖 OpenCV/检测器头文件。
- `sentry_offline --roi`；删除误提交的 `.bak` 文件；修正 README 链接。

### 验证

- `autoaim_tests`、`sentry_tests`（新增 10 组）、`serial_port_tests`（新增 0x0D 原样到达、单次三帧解析）全部通过。
- 仿真 A/B（`config/sentry.yaml`，3 种子均值）：整车 EKF 命中率 77.7% → 78.0%，
  哨兵策略 77.8% → 78.0%，无场景退步；中心模式生效时 6 rad/s + 平移场景 36.1% → 51.2%。

### 合并前待办（审查环境无法完成）

1. `python3 tools/regression.py`（3.avi / 4.avi 实拍门禁）。
2. `sentry_offline <video> --roi` 与不带 `--roi` 对照。
3. 实车：稳态下 `attitude misses` ≈ 0、`reconnects` = 0；拔插 USB 验证重连。
