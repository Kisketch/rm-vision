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

## 相机配置核对

硬件事实（配置表已与 perservance 代码逐项核对）见 [camera_hardware.md](camera_hardware.md)；本节只对照本仓采集实现的差异与复测要求。实际型号、当前运行模式和到达主机的帧率仍待实机核对。

| 相机 | 旧系统记录 | 仓库采集实现 |
|---|---|---|
| 海康 USB 3.0 主相机（USB VID:PID `2bdf:0001`） | MVS SDK，165 fps，手动曝光 2500 或 2000 µs；连续自动增益、连续自动白平衡；Bayer8 彩色输入 | MVS 默认请求 165 fps、手动曝光 2500 µs（2026-11-11 由 1500 µs 对齐实机自瞄设置），设置后回读曝光。初始化未显式设置自动增益和白平衡。 |
| 右侧 USB 辅助相机 | 截图设备节点 `/dev/video0`，1280×720、MJPG、120 fps，曝光控制值 150 | V4L2 单设备入口请求宽高，未显式请求 MJPG 和 120 fps。 |
| 左侧 USB 辅助相机 | 截图设备节点 `/dev/video2`，其余设置同右侧 | 与右侧相同。左右节点映射需在当次启动时确认。 |

复测记录应随录像保存启动命令、相机型号和设备标识。海康相机分别记录曝光回读值（µs）、增益模式和白平衡模式；两台辅助相机分别记录实际协商的分辨率、像素格式、曝光控制名称及单位。截图中的 V4L2 控制值 150 缺少具体控件信息，单位待核。统计实际收帧率时使用同一时钟的逐帧到达时间。

源码对照：[MVS 默认配置](../core/include/autoaim/camera_source.hpp)、[MVS 与 V4L2 采集](../core/src/camera_source.cpp)、[旧版海康初始化](../legacy/io/src/hikrobot.cpp)。

## 全仓目录（2026-11-11 校验，343 个跟踪文件）

| 目录 | 文件数 | 内容 |
|---|---|---|
| `core/` | 30 | 共享算法库：检测、PnP、整车 EKF、弹道、开火控制、相机源、姿态缓冲、延迟标定 |
| `sentry/` | 23 | 哨兵模块：库、主程序、串口、配置、旧代码对比台架（`sentry/tools/`，非产品构建） |
| `apps/` | 5 | 离线程序：`autoaim_video`（评测）、`autoaim_sim`（仿真）、`autoaim_lights`、`detector_bench` |
| `tests/` | 7 | 三套 CTest：core / 哨兵 / 串口，另有回归门契约测试 |
| `docs/` | 97 | 69 篇设计/审查/研究文档（见 `docs/INDEX.md` 分类表）+ 设计档案页 + 样例媒体 + 数据图表 17 |
| `data/` | 39 | 离线评测缓存（双视频检测缓存与分项结果） |
| `tools/` | 39 | 回归门禁与离线评测脚本（含图表生成 make_charts.py） |
| `reports/` | 12 | 汇总报告 + 真视频回归分项报告 |
| `legacy/` | 65 | 上一代上车程序（auto-aim-master）原样存档，**不参与构建、不要部署** |
| `archive/` | 8 | 历史快照存档 |
| 根目录与 `.claude/` | 18 | README、BASELINE（冻结基线）、CHANGELOG、SECURITY、THIRD_PARTY、顶层 CMake、CI、Claude Code 配置包 |

分类原则：代码按子系统分（core / sentry / apps / tests），知识按形态分
（docs / reports / data），历史单独隔离（legacy / archive），入口文件在根目录。
每个文件的归属都能在 `docs/INDEX.md` 的分类表中查到。
