# 完整性审计（2026-11-10，单仓合并后终审）

目的：证明 rm-vision 包含合并前两仓与全部相关交付材料的**每一份内容**，
无遗失、无缺失；每份源文件都有明确去向。本审计在用户提出"上传内容必须
完整、标注分类、再次审查"后执行。

## 方法

1. 以两旧仓的**最终远端树**为准（归档后克隆：rm-autoaim @ `c53910f`
   129 个文件、rm-sentry-integration @ `5380f91` 85 个文件；两头均为迁移
   指针提交，其父提交即内容头 `c57343c` / `6fa921a`）。
2. 对每个源文件计算 SHA-256，先查其内容是否逐字节存在于 rm-vision；
   不存在者逐个 diff 对照其单仓对应文件，按下表分类，不留未解释项。
3. 再盘点工作区侧的全部 RM 交付材料（各轮交付目录、报告、清单、台架），
   逐项决定入仓位置或写明排除理由。

## 结果总览

| 来源 | 文件数 | 逐字节存在 | 经分类变换（见下） | 未解释缺口 |
|---|---|---|---|---|
| rm-autoaim | 129 | 98 | 31 | **0** |
| rm-sentry-integration | 85 | 53 | 32 | **0** |
| 工作区交付材料 | 25 项新增入仓 | — | — | **0** |

## "变换"分类明细（63 项，全部可复核）

- **合并取并集/超集（代码与配置，约 30 项）**：config、tracker、aimer、
  detector、types、pose、ballistics 等同名文件的单仓版本是两仓内容的并集
  或超集（合并决策见 `monorepo_migration.md`）。抽查证据：哨兵
  `ballistics.hpp` 的独有行数为 0（单仓版为其超集，多出弹丸分级 API）；
  哨兵 `evaluation.cpp` 的哨兵策略已逐字抽到 `sentry/src/evaluation_sentry.cpp`；
  哨兵 `tests/test_main.cpp` 的全部测试函数在单仓三套测试中均有对应
  （独有测试 0 个）。
- **路径/注释更新（约 15 项）**：tools 脚本、BASELINE.md、哨兵 apps、
  test_sentry 等的 diff 仅含单仓布局路径（`build/apps/…`、`core/config/…`、
  `SENTRY_ROOT` 字面量）与旧仓名注释的改写，无逻辑变化。
- **根级文件被新等价物取代（8 项）**：两仓 README → 根 README +
  `docs/project/pipeline_and_tuning.md`（叙事恢复）；两仓 CMakeLists → 顶层统一
  CMake；两仓 .gitignore → 合并版；两仓 ci.yml → 统一 CI（已在 GitHub
  Actions 实跑通过）；rm-autoaim 的 `.gitattributes` 已补回根目录。
- **编码规范化（1 项）**：`reports/sim_report.md` 由 UTF-16LE+CRLF 转为
  UTF-8+LF，解码后文本逐行一致。
- **退役机制归档（5 项）**：`tools/check_shared_core.sh`（两仓字节一致，
  存一份）与两份 `shared-core.yml` 已移入 `archive/shared-core/` 并标注
  退役；其清单文档 `docs/project/shared_core.md` 保留并标注历史。
- **本次审计补回的真遗失（2 项）**：`docs/detections/4_overlay_01050.png`（合并上传时
  漏传的二进制图）与 `.gitattributes`，此前不在仓内，本次已恢复原位。

## 工作区材料入仓清单（25 项）

- 文档类 → `docs/`：上车试机清单（on_robot_trial_checklist）、软件盘点与
  待补数据清单、新旧代码对比（legacy_comparison）、首轮代码审查改进清单
  （code_review_improvements）。
- 报告类 → `reports/real_video_regression/`：真视频回归 9 份分项报告。
- 过程存档 → `archive/patches/`：P0-1 与 DeviceId/CRC 两份补丁原件及
  P0-1 说明 2 份。
- 台架类 → `tools/offline_eval/`：evaldriver、evaldriver_vision、tg_smoke
  源码（冻结基线数字的复现工具）+ 说明。
- 说明类：`archive/README.md`、`docs/INDEX.md`（全仓目录索引）、本审计。

## 明确排除（非遗失，理由在案）

- 作业仓 `rm-armor-detection` 文件：按用户要求不触碰、不复制（其自身完整
  保存在原仓）。
- 视频与检测缓存：私有数据，存于私有仓 `rm-videos`，不进公开仓。
- 各轮交付的代码快照目录：代码已逐项合入本仓，与旧仓 git 历史重复，
  存入会产生第三份过期副本；各轮文档已分别入 `docs/`（清单见
  `archive/README.md`）。
- 两仓的 git 提交历史：文件级合并无法携带，历史完整保留在已归档的原仓中。
- Claude Code 配置包的 zip / install.sh / 全局配置：分发工具；其仓库内容
  （CLAUDE.md + .claude/）已入仓。

## 结论

两仓 214 个源文件：151 个逐字节存在，63 个有据可查的分类变换，0 个未解释
缺口；审计中另发现并补回 2 个真遗失。加上工作区材料 25 项与原有 legacy
全树、Claude 配置，本仓共 269 个文件，目录标注见 `docs/INDEX.md`。
