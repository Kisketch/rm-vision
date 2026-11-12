# rm-vision — RoboMaster 视觉自瞄（单仓）

RoboMaster 视觉自瞄统一代码仓：步兵/英雄与哨兵共用同一套自瞄核心
（检测 → PnP 位姿 → 整车 EKF 跟踪 → 弹道解算 → 瞄准/火控），哨兵专属的
多目标管理、串口协议与跟踪预测 ROI 作为独立模块挂在核心之上。
C++17 / CMake / OpenCV 4。规则以 2026 赛季为准，2027 仅作前瞻。

当前代码以 `main` 最新提交为准；冻结指标口径见 `BASELINE.md` 与
`docs/monorepo_migration.md`，离线 N0 复现用 `core/config/video_reference.yaml`。
公开仓 https://github.com/Kisketch/rm-vision ，CI 见 GitHub Actions。
旧两仓 rm-autoaim / rm-sentry-integration 已归档只读，**不要再往旧仓推代码**；
作业仓 rm-armor-detection 不在本仓范围内，**一律不动**；回归视频在本仓 Release `data-v1-2026-11-09` 附件中。

## 构建与测试（改任何代码后必须跑，Linux）

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure   # 5 项: autoaim_tests / sentry_tests / serial_port_tests / regression_gate_contract / header_self_containment
./build/apps/autoaim_sim                     # 仿真表必须与 BASELINE.md 一致/可解释
./build/sentry/sentry_robot --help           # 哨兵上车程序冒烟
```

- 只构建步兵侧：`-DBUILD_SENTRY=OFF`（只出步兵四个程序和 core 测试）。
- 在 CPU 配额小于核数的容器里，OpenMP 后端的 OpenCV（如 conda-forge）会因超额线程
  被节流拖慢约 50 倍：先 `export OMP_NUM_THREADS=<配额>`（看 `/sys/fs/cgroup/cpu.max`）。
- 改检测器必须 `--redetect` 全新检测：`tools/regression.py` 复用检测缓存。
- 上车（MVS 相机）：`-DAUTOAIM_WITH_MVS=ON -DMVS_ROOT=/opt/MVS`。
  该分支只用桩 SDK 做过编译验证，真实 SDK 版本/ABI 对齐仍是上车待验项，
  未经真机编译验证前不要断言可用。

## 目录（单仓布局，旧两仓路径已失效）

- `core/` — 共用核心，只存一份：`core/include/autoaim/`、`core/src/`、
  `core/config/default.yaml`（步兵/英雄默认参数）、`core/model/`（mlp.onnx）
- `sentry/` — 哨兵模块，链接 core：`sentry/include/`（TargetManager、
  SentryVision、ROI、串口协议）、`sentry/src/`、`sentry/apps/`（sentry_robot /
  sentry_offline）、`sentry/config/sentry.yaml`（哨兵上车参数）、
  `sentry/config/default.yaml`（哨兵口味离线默认，`carry_velocity` 关）
- `apps/` — 步兵/英雄离线程序：autoaim_video / autoaim_sim / autoaim_lights /
  detector_bench（构建产物在 `build/apps/`）
- `tests/` — 三套测试；`docs/` — 设计与审计文档；`tools/` — 回归/调参/标定脚本
- `legacy/` — 旧上车代码 auto-aim-master 整树存档，**只读参考，禁止修改**
- 关键文档：`docs/monorepo_migration.md`（合并决策与回归证据）、
  `docs/pipeline_and_tuning.md`（原两仓项目 README 合并版：结果、评测方法、调参日志）、
  `BASELINE.md`（冻结回归基线）

## 硬约定（违反就是 bug）

- **pitch 约定按模块分**：sentry 模块旋转 Rz(yaw)·Ry(−pitch)·Rx(roll)、
  正 pitch 向下，与 core 侧相反。凡涉及姿态/角度先确认在哪个模块，禁止混用符号。
- 时间戳对齐：观测按**曝光中心时刻**查 PoseBuffer 姿态。恒定传输延迟 D
  不能只折进 aimer.latency——姿态查询同样要扣除（DelayCalibrator 标定，
  MVS 填 `MvsConfig::stamp_offset_us`，哨兵填 `--exposure-offset-ms`）。
  声称“延迟无害”是已证伪的旧结论。
- `wrapAngle` 必须用 `std::remainder` 实现，禁止手写循环归一化（±inf 会死循环）。
- Aimer 按值持有 Ballistics，禁止用引用成员持有（临时对象会悬垂，ASan 可复现）。
- PoseBuffer::push 必须拒收非有限样本（NaN 会破坏时间排序并污染后续插值）。
- 弹道：`Ballistics::solve` 有热启动加速；改数值方法必须对 golden 结果，
  pitch 偏差 ≤ 1e-4 rad 量级并报告实测偏差。
- 目标身份用 DeviceId（颜色+数字+板型），禁止退回只按数字关联；同一 DeviceId
  不得生成重复轨迹。
- AimCommand 含 yaw/pitch rate 前馈（B-01），改火控不要丢字段。
- Detector 入口有 normalizeFrame 守卫（空帧/灰度/非 8UC3），禁止绕过直接 split 通道。
- 灯条断裂合并（`merge_split_lights`）必须是**追加式**：并集作额外候选、碎片保留。
  "用并集替换碎片"已被实测证伪（灯条上方杂光也有亮缝，误伤 6 块正确的板），禁止改回。
  亮缝判据（缝灰度 ≥ 碎片灰度）是区分过曝芯与地面倒影的依据，不要删。
- 装甲几何：本仓用板面 0.225×0.055 / 0.135×0.055 m；旧代码的灯条间距
  0.230×0.050 不可混用。
- 串口协议：MCU 无时间戳，17B 姿态帧只有角度/颜色/buff；位姿时刻 = 主机接收
  时刻 − 估计传输延迟（约 1.5 ms），这是估计不是保证，不要写成精确对齐。
- 串口 termios 必须保持 raw：必须清 ICRNL/INLCR/IGNCR（不清则帧内 0x0D 被读成
  0x0A，角度按字节位置最高错约 20°）。串口 fd 生命周期必须有锁：reader 线程
  与 send() 不得无锁 close/reopen 同一 fd。
- CRC-8 在姿态帧 byte 2，`--require-crc` **默认关且必须保持关**，直到 MCU 固件
  真正填充 byte 2 才能开。
- 中心模式瞄准必须按 t0+t_fly 取中心（计飞行时间平移提前量），禁止用 centerAt(t0)。
- 选靶滞回必须复检资格（require_tracking / max_distance），不合格的旧目标
  不得被滞回选回。
- PoseBuffer 查询用有界外推（`--max-extrap-ms` 默认 10），不要退回严格包围查询
  ——默认参数下会系统性丢帧。
- ROI：`--roi` 默认关。窗口用 TG（跟踪引导）：检测前由 TargetManager 快照预测
  全部板位、60px 边距。110px 边距已被实验证伪（窗口重叠裁灯条→假装甲），禁止改回。
  ROI 开时新进入视野的目标最长约 1.2 s 才被发现（30 帧全扫周期），这是已知提醒。
- **单仓之后没有“跨仓移植”**：core 只有一份，改核心直接改 `core/`，不要复制
  文件到 sentry/ 或反向复制。`docs/shared_core.md` 是合并前的历史清单，**已退役**，
  其配套校验脚本在新仓不存在，不要引用它、不要“恢复”它。模块间有意分叉
  （前哨站、姿态架构、弹种 API 等）见 `docs/monorepo_migration.md`。

## 参数真实性

- 相机标定（fx 2399.58, fy 2398.60, cx 688.70, cy 533.42）来自旧代码实测；
  畸变 5 数存储顺序待标定脚本确认，不要擅自重排。
- 17mm 弹速实测 22 m/s；**42mm 的 16 m/s 是占位值**，未上车实测前，任何文档/
  注释不得写成实测。
- 相机→云台杠杆臂在配置中默认注释/旧行为，未经上车核对坐标系约定不得启用。
- aimer.latency、stamp_delay/exposure_offset 是估计值，须上车标定回写。
  上车第一项：先跑 `./build/sentry/sentry_robot --calib-delay`（静止板 + 扫云台
  约 10 s），按输出填 `--exposure-offset-ms`；云台不动时标定器应拒绝给结果，
  这是正确行为。

## 回归基线

- 真实视频回归基线 N0：整车 EKF 命中率 67.5% / 68.6%、预测中位 14.3 / 13.3 px；
  哨兵策略（哨兵参数）67.6% / 71.9%。改核心后波动 >0.1pp / >0.1px 必须解释原因。
- 回归视频在本仓 Release `data-v1-2026-11-09` 附件中（公开）：**视频文件本身
  不得提交进 git 历史**（单文件超限），只在本地跑。
- 只能上车验、离线不许下结论：attitude misses ≈ 0、reconnects = 0、USB 拔插重连、
  真实 SDK 的 ABI、云台运动下 TG 投影精度。

## 工作方式

- 先 Plan 后改：大改先出方案和影响清单，确认后再动手。
- 改完必须给证据：构建结果、ctest 结果、sim 表对比、（涉及核心时）回归数字。
  没跑不许说“完成”。
- 提交信息写清改了什么、为什么、验证数字；不要提交 `.bak`、构建产物、大文件。
- 回答与提交说明用中文，技术名词保留英文原名。
