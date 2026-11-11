# 部署地图：上车代码在哪

> 给第一次打开本仓的队友：先看这一页，就知道什么代码上车、在哪构建、
> 用哪份配置，以及哪些目录只是存档、不要拿错。

## 上车部署

| 部署对象 | 入口与构成 | 构建产物 | 配置 |
|---|---|---|---|
| **哨兵整车程序** | 入口 `sentry/apps/sentry_robot.cpp`；模块库 `autoaim_sentry`（`sentry/src/` + `sentry/include/`：多目标管理、SentryVision、串口协议、跟踪引导 ROI）链接共享核心库 `autoaim_core`（`core/`） | `sentry_robot`（顶层 CMake 构建，Linux） | `sentry/config/sentry.yaml`（哨兵参数）+ `core/config/default.yaml`（实测标定口径） |
| **步兵 / 英雄** | 本仓**没有**独立的实车 main：算法本体是 `core/`，以静态库 `autoaim_core` 形式链接进整车程序；`apps/` 下是离线评测与仿真工具，不上车 | 库 `autoaim_core` | 同上 `core/config/default.yaml` |

构建（仓库根目录）：

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build        # 三套单元/集成测试
```

注意区分两份 core 配置：`core/config/default.yaml` 是**实测口径的上车配置**；
`core/config/video_reference.yaml` 是**冻结的离线评测参考口径**（假设针孔、
25 m/s），只用于回归对比，**不要部署到车上**。哨兵另有
`sentry/config/default.yaml`（哨兵风味默认）与 `sentry/config/sentry.yaml`。

定位提醒：比赛整车工程（步兵/英雄/哨兵的整车集成）以队内主工程为准，
本仓是算法库与验证实验室——哨兵的可执行程序 `sentry_robot` 是本仓
唯一直接产出的实车程序。

## 全仓目录（2026-11-11 校验，331 个跟踪文件）

| 目录 | 文件数 | 内容 |
|---|---|---|
| `core/` | 30 | 共享算法库：检测、PnP、整车 EKF、弹道、开火控制、相机源、姿态缓冲、延迟标定 |
| `sentry/` | 23 | 哨兵模块：库、主程序、串口、配置、旧代码对比台架（`sentry/tools/`，非产品构建） |
| `apps/` | 5 | 离线程序：`autoaim_video`（评测）、`autoaim_sim`（仿真）、`autoaim_lights`、`detector_bench` |
| `tests/` | 7 | 三套 CTest：core / 哨兵 / 串口，另有回归门契约测试 |
| `docs/` | 85 | 68 篇设计/审查/研究文档（见 `docs/INDEX.md` 分类表）+ 设计档案页 + 样例媒体 + 数据图表 6 |
| `data/` | 39 | 离线评测缓存（双视频检测缓存与分项结果） |
| `tools/` | 39 | 回归门禁与离线评测脚本（含图表生成 make_charts.py） |
| `reports/` | 12 | 汇总报告 + 真视频回归分项报告 |
| `legacy/` | 65 | 上一代上车程序（auto-aim-master）原样存档，**不参与构建、不要部署** |
| `archive/` | 8 | 历史快照存档 |
| 根目录与 `.claude/` | 18 | README、BASELINE（冻结基线）、CHANGELOG、SECURITY、THIRD_PARTY、顶层 CMake、CI、Claude Code 配置包 |

分类原则：代码按子系统分（core / sentry / apps / tests），知识按形态分
（docs / reports / data），历史单独隔离（legacy / archive），入口文件在根目录。
每个文件的归属都能在 `docs/INDEX.md` 的分类表中查到。
