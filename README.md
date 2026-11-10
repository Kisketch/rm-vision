# rm-vision — RoboMaster 视觉自瞄（单仓）

RoboMaster 视觉自瞄的统一代码仓：步兵/英雄与哨兵共用同一套自瞄核心
（检测 → PnP 位姿 → 整车 EKF 跟踪 → 弹道解算 → 瞄准/火控），哨兵专属的
多目标管理、串口协议与跟踪预测 ROI 作为独立模块挂在核心之上。

本仓由 `rm-autoaim`（main @ `c57343c`）与 `rm-sentry-integration`
（master @ `6fa921a`）于 2026-11-10 合并而成，合并决策与回归证据见
[`docs/monorepo_migration.md`](docs/monorepo_migration.md)；旧两仓已归档
只读，历史留在原仓。作业仓 `rm-armor-detection` 与数据仓 `rm-videos`
不在本仓范围内。项目叙事（结果、评测方法、调参日志，原主仓 README）见
[`docs/pipeline_and_tuning.md`](docs/pipeline_and_tuning.md)。全仓逐目录
索引见 [`docs/INDEX.md`](docs/INDEX.md)，完整性审计（每个源文件的去向
对账）见 [`docs/completeness_audit_2026-11.md`](docs/completeness_audit_2026-11.md)。

## 目录表

```
rm-vision/
├── CMakeLists.txt # 顶层：BUILD_SENTRY 开关，统一构建入口
├── README.md # 本文件（目录总表）
├── BASELINE.md # 冻结回归基线（实拍视频指标，勿随意改）
├── CHANGELOG.md
├── LICENSE / THIRD_PARTY.md
├── reports/ # 历次评测报告
│
├── core/ #
│ ├── CMakeLists.txt # autoaim_core 库；AUTOAIM_WITH_MVS 选项
│ ├── include/autoaim/ # config/detector/pose/tracker/ballistics/
│ │ # aimer/evaluation/camera_source/pose_buffer…
│ ├── src/ # 对应实现（含 MVS 相机源、海康 SDK 桩外接口）
│ ├── config/default.yaml # 步兵/英雄默认参数（队伍相机实测标定已回填）
│ └── model/ # mlp.onnx + label.txt（灯条分类模型）
│
├── sentry/ # 链接 core
│ ├── CMakeLists.txt # autoaim_sentry 库 + sentry_robot/sentry_offline
│ ├── include/
│ │ ├── autoaim/target.hpp # 多目标管理器
│ │ ├── sentry/ # SentryVision、ROI、串口协议、策略评测
│ │ └── io/serial_port.hpp # 串口（termios，ICRNL 已清——见文档）
│ ├── src/ # target/sentry/roi/serial_protocol/io…
│ ├── apps/ # sentry_robot（上车主程序）、sentry_offline
│ ├── config/
│ │ ├── sentry.yaml # 哨兵上车参数
│ │ └── default.yaml # 哨兵口味的离线默认（carry_velocity 关）
│ └── tools/ # 旧检测器对比台架（legacy bench）
│
├── apps/ # 步兵/英雄离线程序（链接 core）
│ ├── autoaim_video.cpp # 视频全流程评测（含哨兵策略行，模块在时）
│ ├── autoaim_sim.cpp # 合成场景仿真对比表
│ ├── autoaim_lights.cpp # 灯条检测调试
│ └── detector_bench.cpp # 检测器基准
│
├── tests/ # ctest 三套：core / sentry / serial(PTY)
├── docs/ # 设计与审计文档（基线、对齐、弹道、ROI、审计…）
├── tools/ # 回归/调参/标定脚本（regression.py 等）
└── legacy/ # 旧上车代码 auto-aim-master 整树存档（只读参考）
```

## 构建与测试

```bash
cmake -S. -B build
cmake --build build -j
ctest --test-dir build
```

- 只构建步兵侧：`-DBUILD_SENTRY=OFF`。
- 上车（MVS 相机）：`-DAUTOAIM_WITH_MVS=ON -DMVS_ROOT=/opt/MVS`。
- 哨兵上车程序：`build/sentry/sentry_robot`（参数见 `--help`；帧戳—姿态
时间差先跑 `--calib-delay` 标定，方法见 `docs/review_2026-11_round2.md`）。

## 回归红线

改动核心后必须复跑：`ctest` 全过；`autoaim_sim` 两套参数表；
实拍视频评测对冻结基线（`BASELINE.md`）：整车 EKF 67.5 % / 68.6 %、
预测中位 14.3 / 13.3 px；哨兵策略（哨兵参数）67.6 % / 71.9 %。
口径细节见 `docs/monorepo_migration.md`。

## 离线视频配置口径

历史 3.avi / 4.avi 的 N0 基线使用假设内参、零畸变和 25 m/s；当前
`core/config/default.yaml` 使用导入的标定值和 22 m/s，不能直接与 N0 数字比较。
复现旧基线请显式传 `--config core/config/video_reference.yaml`。
该配置只供离线回归，不是机器人标定。完整试跑、失败帧和指标限制见
[2026-11-10 视频测试](docs/VIDEO_TEST_2026-11-10.md)。

PnP/关联排查可加 `--diagnostics`，生成逐检测位姿质量和逐观测关联原因 CSV。
可选 `--set max_pose_reproj=像素值` 对最终姿态平均角点残差门控（默认 0，关闭经验残差门限）。
诊断字段、实测统计及标定溯源见
[位姿与关联诊断](docs/POSE_ASSOCIATION_DIAGNOSTICS_2026-11-10.md)。
