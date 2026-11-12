# 更新日志

> 范围说明：本文件早期条目继承自 rm-sentry-integration 的哨兵变更史，
> 合并后也记录全仓级变更，但不追求完整——全仓变更以 git 提交历史与
> docs/ 下各轮审查文档（如 docs/engineering_review_r6.md）为准。

## 2026-11-11 — 检测前端：灯条断裂修复与横向色块过滤

逐帧排查两段实拍视频的漏检段，修了两个检测前端缺陷（[详细分析](docs/studies/split_light_merge.md)）：

- **过曝灯条被切成两段**：近距离时灯条中心三通道饱和，R−B 塌到阈值以下，细红边又被
  3×3 腐蚀吃掉，一根灯条变两截，配对因长度比失败、整帧漏检。新增
  `merge_split_lights`：上下堆叠、宽度相近、间隙不大且**间隙比碎片更亮**（过曝芯的
  物理特征；地面倒影的缝是暗的）的两块，其并集作为**额外**候选加入，碎片保留，
  配对按长度一致性选优。第一版"替换"会误伤 6 块正确的板（灯条上方的杂光），
  改为"追加"后全部恢复。
- **横向色块被当成灯条**：`makeLight` 对横躺矩形取到的"长度"是厚度、倾角恒为 0°，
  倾角过滤失效，车顶/车底灯带混进灯条候选，可经 `reject_contained_lights` 否决真板。
  新增 `reject_horizontal_lights`：长度 < 宽度即拒收。
- **结果**：`default.yaml` 3.avi 条件命中 57.3% → 58.1%（hits/s 8.8 → 9.1），
  哨兵 `sentry.yaml` 68.3% → 68.9%；4.avi 检测输出逐字节不变。N0 冻结口径不受影响
  （结构体默认关）。单帧耗时不变（3.3–3.5 ms）。BASELINE §12 记录重新冻结。
- 测试：`testSplitLightMerge`、`testHorizontalBlobRejected`（都同时断言旧行为会失败）；
  `autoaim_video` 缓存签名纳入三个新参数，`--sweep` 拒收（会改变检测）。

## 2026-11-11 — 赛场条件压力测试与评测器帧率解耦

新增 `autoaim_video --stress`（`dropout` / `framedrop` / `decimate` / `jitter_ms` /
`seed`），在检测缓存之后扰动，一次缓存即可复现"遮挡 / 掉帧 / 慢相机 / 时间戳抖动"。
详细数据与结论见 [赛场条件压力测试](docs/FIELD_CONDITIONS_2026-11-11.md)。

- **修复评测器对固定帧率的隐含假设**：`evaluate()` 的仿真云台步长原来是常数
  `1/fps`，`ShotJudge` 又用 `T*fps` 当下标取前后帧。任何丢帧都会让云台少走一半
  路程、judge 插错帧对。修复前"丢 20% 帧"的命中率是 22.5%，修复后是 68.3%——
  之前的掉帧类结论全部不可信。改法：`dt` 取相邻帧戳差（与标称值相差 <1 ns 时
  仍用标称值）、`platesAt` 按时间戳二分、预测误差按时间采样。25 fps 连续序列上
  输出与修复前**逐字节相同**，§10 基准继续可比。回归测试
  `testShotJudgeNonUniform`。
- **实测结论**：3 号目标（ω≈2.7 rad/s）在丢帧/降采样下稳定在 64–68%；
  4 号（ω≈8.4 rad/s）在 12.5 fps 掉到 55.2%、6.25 fps 掉到 38.5%。用
  `aim_err_px`/`fire_err_px` 拆开确认是**预测**（17.9→26.3 px）而不是执行
  （8.6→7.7 px）。`gimbal_ff=1` 在这种条件下更差（55.2→41.6），保持默认 0。
- **遮挡**：30%/50% 板漏检时命中率只掉 1.5/3 pp，但 hits/s 掉 55%/80%，原因是
  `lost_grace_frames=0` 让每一帧漏检都降级 TRACKING。`grace=1` 在干净素材上
  +4% hits/s（命中 −0.5 pp），30% 漏检下 +28% hits/s。**默认仍为 0**：干净素材
  上的收益在噪声内，遮挡统计是合成扰动而非实测标定，换默认值应由赛场判断决定。
- **时间戳抖动**：10 ms 抖动让 4.avi 从 68.6% 掉到 49.0%（预测中位 13.3→25.5 px），
  3.avi 只掉 3.6 pp；影响与 ω 成正比，曝光时间戳与传输延迟必须当真。
- **延迟**：离线回放上 30 ms 优于 60 ms（+4–5 pp），但离线没有采集链路，这不能
  作为改小默认值的理由，上机用 `--calib-delay` 标定。
- 两次完整重跑（含重新检测）的输出逐字节一致，缓存复用不改变任何结果。

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

## 2026-11-11 — Stage K：yaw 过程噪声拆分与 ω 机动检测

把"预测误差"从"执行误差"里拆出来之后（新增 `aim_err_px` / `fire_err_px`
诊断列），两条真实视频上的瓶颈定位到**瞄准点的预测**，而不是云台跟随：
`aim_err` 中位 16.8 px、`fire_err` 中位 8.1 px；`aim_err < 10 px` 时命中率 91.8%，
`25–60 px` 掉到 10.3%。进一步用"模型相位 vs 实测相位"离线拟合测出角速度误差
σ(ω)=0.69 rad/s，对应 0.39 s 预测时域上 15° 的相位误差——这才是命中率的主因。

### 算法改动

- **yaw 过程噪声拆成两项**（`tracker.q_yaw` + 新增 `tracker.q_yaw_walk`）。
  原来是 `addWhiteNoiseAccel(Q, YAW, q_yaw, dt)`，一个参数同时决定角速率噪声和
  相位噪声：想让 ω 刚性就必须把相位也冻住。拆开后 `q_yaw` 只管角速率扩散，
  `q_yaw_walk` 给相位一条独立随机游走。因为 `q_yaw_walk` 是加性项，`q_yaw`
  **保持冻结值 3 不动**，只新增 `q_yaw_walk: 0.05`。
  **这不是白拿的**：在无噪声的匀速旋转合成目标上，0.4 s 外推误差反而从
  20.1 mm 变差到 34.7 mm（`tests/test_main.cpp::testYawProcessNoiseSplit` 记录
  了这个数字）。它赢在真实视频上——真实相位量测的相关误差需要滤波器吸收，
  而干净数据上"注入方差"只是在污染物理模型。
- **ω 专用机动检测**（`tracker.q_yaw_adapt`，默认 1）。原有 `q_scale_` 由位置
  新息驱动，而位置新息对 ω 误差不敏感（XC/YC/R 会把相位误差吸收掉），所以对
  ω 完全免疫——实测打开后数字一位不变。新增由偏航新息驱动的 `q_yaw_scale_`，
  在角速率真的变化的场景放宽 ω。
- **`aimer.phase_lead_deg`**：绕目标中心给瞄准点加一个相位导前。默认 0；
  实验显示 ±20° 扫描关于 −5..0° 对称（峰值 ~57%），说明这两条录像上**没有**
  系统性相位偏置，误差是随机的，不需要修。注意它只作用于装甲板；中心模式的
  瞄准线不是板，不能被旋转（`testAimerPhaseLead` 覆盖）。
- **诊断列**：`_track.csv` 增加 `aim_err_px`（瞄准点 vs 弹着时刻真实板心的距离，
  纯预测误差）、`fire_err_px`（瞄准点到实际弹着点，执行误差）、`sigma_lat_px`
  （滤波器自报的横向 1σ）。三者对比可直接判断该修预测还是修云台。

上述结论与仓库自己的 `docs/miss_analysis.md` 一致（"误差是角速度估计抖动主导"），
只是这次把"抖动"量化成 σ(ω) 并拆开了过程噪声。

### 缺陷修复

- `sentry/src/serial_protocol.cpp` 的 `wrapPi` 仍是 while 循环，±inf 会死循环，
  违反 CLAUDE.md 的 `wrapAngle` 约定 → 改为 `std::remainder`
  （`tests/test_sentry.cpp` 加了大角度回绕断言）。
- `OutpostTracker` 缺少 `match_dt_scale_`：丢帧后关联门限不放宽，与 `RobotTracker`
  不一致 → 补齐（并在 `predictTo` 之前计算，否则 `stamp_` 已被改写）。
- `RobotAimer::aim(const TargetState&, double)`（哨兵重载）从不调用 `rate_.update`，
  所以 `AimCommand::yaw_rate/pitch_rate`（B-01）一直是 0；目标切换/丢失时也不
  `rate_.reset()`，一旦填值就会给出跨目标的假角速度 → 补齐，与步兵重载一致。
- `OutpostTracker::transition` 同步支持 `q_yaw_walk`；`Config::validate()` 拒绝负的
  `q_yaw_walk`。

### 新增回归测试

`testTrackerDropoutGrace`（单帧漏检不得降级，`lost_grace_frames` 0/1 两种行为）、
`testYawProcessNoiseSplit`（相位方差不降、ω/半径仍收敛、默认 0 保持旧模型）、
`testAimerPhaseLead`（导前 18°、保半径、中心模式忽略导前）、外加 `validate()` 与
`wrapPi` 各一条。

### 结果（两套口径分开看，见 BASELINE.md §11）

| 口径 | 3.avi robot EKF | 4.avi robot EKF |
|---|---|---|
| `default.yaml`（现役，已改为 `walk=0.05`） | 56.8% / 8.65 → **57.3% / 8.8** | 64.4% / 8.40 → **66.9% / 8.8** |
| `video_reference.yaml`（N0，未改，`walk=0`） | 67.5% / 10.60 → 67.5% / 10.6 | 68.6% / 10.06 → 68.6% / 10.1 |
| `video_reference.yaml` + `--set q_yaw_walk=0.05` | 67.5% → **68.4%** / 10.67 | 68.6% → **71.1%** / 10.54 |

现役配置是净提升；N0 冻结口径零变化，历史数字继续可比，同一个旋钮在 N0 上还能
再拿 +0.9 / +2.5 pp（属口径决策）。0.32 s 预测中位：default 15.1→15.3 / 13.2→12.4 px，
N0 14.3→14.3 / 13.3→12.4 px。仿真均值 74.5% → 73.0%（−1.5 pp，容差内）。

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
