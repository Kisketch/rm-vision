---
name: monorepo
description: rm-vision 单仓模块边界检查。用户说模块归属、这段代码放哪、core 还是 sentry、合并后结构、monorepo 时使用。判定改动应属 core / sentry / apps / tools 哪一层，禁止核心代码多份拷贝。
allowed-tools: Read, Grep, Glob, Bash(git diff *), Bash(git log *), Bash(git status *)
---

# 单仓模块边界检查

## 背景（不要再按两仓思考）
2026-11-10 起，rm-autoaim 与 rm-sentry-integration 已合并为单仓 rm-vision 并归档只读。
共用核心只有 `core/` 一份拷贝。**不存在“跨仓移植”**：旧的 /port-sync 流程已退役，
不要再做“把 A 仓改动搬到 B 仓”的操作；`docs/project/shared_core.md` 是合并前的历史清单，
已退役，其配套校验脚本在新仓不存在，不要引用、不要恢复。

## 归属判定
- `core/`：检测、PnP、EKF 跟踪、弹道、瞄准、PoseBuffer、DelayCalibrator、相机源、评测。步兵与哨兵共用，改这里影响两条线，必须跑完整 /regression。
- `sentry/`：多目标管理、串口、TG ROI 接线、sentry_robot/sentry_offline、哨兵配置。只影响哨兵线。
- `apps/`：步兵/英雄离线程序（autoaim_video/sim/lights/detector_bench）。只影响离线评测与仿真。
- `tools/`：回归/调参/标定脚本。`docs/`：文档与基线说明。
- `legacy/`：旧代码整树存档，**只读，禁止修改**，包括“顺手修一下 legacy 的 bug”。
- `tests/`：三套测试与 ctest 一一对应。

## 步骤
1. 列出改动文件，逐项判定归属；跨模块的改动要说明为什么必须跨（例如 core 接口变了，sentry 调用点必须跟）。
2. 符号/约定核对：pitch 约定 core 与 sentry 模块相反；配置分工 core/config/default.yaml（步兵）vs sentry/config/sentry.yaml（哨兵上车）vs sentry/config/default.yaml（哨兵离线）。
3. 检查有意分叉是否仍与 docs/project/monorepo_migration.md 的分类一致；新增分叉必须写清原因，不得静默复制核心文件到另一模块。
4. 改完跑 /regression；涉及 core 的改动必须同时跑两套 sim 参数（步兵 + 哨兵）。
