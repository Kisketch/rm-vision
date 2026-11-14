# docs — 文档导航 / Documentation map

按**用途**分目录：先看入口页，再按问题去对应目录。完整的逐文件说明见
[`INDEX.md`](INDEX.md)；冻结数字以根目录 [`BASELINE.md`](../BASELINE.md) 为准。

Docs are grouped by purpose. Start from the entry pages; the file-by-file
catalogue is [`INDEX.md`](INDEX.md), and the frozen numbers live in
[`BASELINE.md`](../BASELINE.md).

## 入口页 / Entry pages

| 文件 | 用途 |
|---|---|
| [`INDEX.md`](INDEX.md) | 全仓目录索引：每个目录、每篇文档的一句话标注 |
| [`api_overview.md`](api_overview.md) | 公共 API 一页速览：两条流水线入口与易误用语义 |
| [`parameters.md`](parameters.md) | 参数清单：哪些可 `--set` 扫描、哪些要改配置文件 |
| [`performance_charts.md`](performance_charts.md) | 20 张数据图表，每张配数据表与解读 |

## 目录 / Folders

| 目录 | 文件数 | 回答什么问题 | 先读 |
|---|---|---|---|
| [`design/`](design/) | 9 | 为什么这样设计：时间戳、身份、ROI、多目标、关键点后端 | [`timestamp_design.md`](design/timestamp_design.md) |
| [`studies/`](studies/) | 28 | 每个参数/方案怎么选的：A/B、消融、扫描、压力测试、被否决的方案 | [`split_light_merge.md`](studies/split_light_merge.md)、[`miss_analysis.md`](studies/miss_analysis.md) |
| [`calibration/`](calibration/) | 5 | 相机与实测：内参、畸变顺序、相机硬件、上车试机清单 | [`on_robot_trial_checklist.md`](calibration/on_robot_trial_checklist.md) |
| [`deploy/`](deploy/) | 3 | 上车跑什么、怎么装：部署地图、目标机、Ubuntu 兼容 | [`deployment_map.md`](deploy/deployment_map.md) |
| [`reviews/`](reviews/) | 13 | 历轮代码审查、加固与审计的逐条处置 | [`engineering_review_r6.md`](reviews/engineering_review_r6.md) |
| [`project/`](project/) | 13 | 项目史与盘点：单仓合并、相对旧代码的改动、待办、进度 | [`changes_vs_legacy.md`](project/changes_vs_legacy.md) |
| [`decisions/`](decisions/) | 3 | 架构决策记录（ADR） | — |
| [`charts/`](charts/) | 20 | 数据图表 SVG（`tools/make_charts.py` 生成） | — |
| [`diagrams/`](diagrams/) | 3 | 原理示意图 SVG（`tools/make_diagrams.py` 生成） | — |
| [`figures/`](figures/) | 2 | 真实画面图：README 动图、检测四步（`tools/make_figures.py` 生成） | — |
| [`detections/`](detections/) | 11 | 检测效果样例图/视频 | — |
| [`assets/`](assets/) | 1 | 仓库标志 | — |

## 写新文档放哪 / Where a new doc goes

- 一次实验或参数选择（有数据、有结论、可能被否决）→ `studies/`
- 一个机制为什么这样设计 → `design/`；跨模块的大决策另写一篇 ADR 到 `decisions/`
- 实测数据、标定档案 → `calibration/`；上车/部署步骤 → `deploy/`
- 审查意见与逐条处置 → `reviews/`；进度、待办、迁移记录 → `project/`

新增文档后同步 [`INDEX.md`](INDEX.md) 的分类表与文件计数（见
[`CONTRIBUTING.md`](../CONTRIBUTING.md) 的文档纪律）。
