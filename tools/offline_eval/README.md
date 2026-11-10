# tools/offline_eval/ — 离线回归台架（非 CMake 目标）

这三个小程序是真视频回归与 TG ROI 冒烟所用的**离线台架源码**，不属于产品
构建，手动编译运行。BASELINE.md 与 `reports/real_video_regression/` 中的
冻结基线数字（EKF 67.5%/68.6%、预测中位 14.3/13.3 px、TG 890=890）就是
由它们复现的，存入本仓是为了让基线证据可复核。

| 文件 | 用途 |
|---|---|
| `evaldriver.cpp` | 以 ffmpeg 管道喂帧，驱动 core 评测（含哨兵策略）跑真视频检测缓存 |
| `evaldriver_vision.cpp` | 同上，单仓合并后版本（接入 `appendSentryStrategy`） |
| `tg_smoke.cpp` | SentryVision 跟踪预测 ROI 的在线冒烟（ROI 开/关观测一致性） |

输入数据（视频与检测缓存）属私有数据仓 `rm-videos`，不在本仓内；台架在本仓
构建产物（`build/core` 的 `autoaim_core`）之上链接，具体编译命令见各源文件
头部注释与 `docs/monorepo_migration.md` 的回归证据一节。
