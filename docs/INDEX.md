# rm-vision 全仓目录索引（INDEX）

本文件是全仓的**完整目录表**：每个目录一行，标注用途、内容与维护状态。
文件总数与分类汇总见文末；完整性审计（每个源文件去向的逐项对账）见
[`completeness_audit_2026-11.md`](completeness_audit_2026-11.md)。
根 README 是入口与构建说明；项目叙事（结果/评测/调参）见
[`pipeline_and_tuning.md`](pipeline_and_tuning.md)。

## 顶层

| 路径 | 标注 |
|---|---|
| `README.md` | 仓库入口：定位、构建、目录表摘要、状态（居中标志 + 语言选择头） |
| `README.zh-CN.md` | README 的简体中文版（与英文版同源同数据，顶部可互跳） |
| `CLAUDE.md` | Claude Code 的仓库级指令（基线、约定、红线） |
| `BASELINE.md` | 冻结回归基线与复现命令（回归门禁以此为准） |
| `CHANGELOG.md` | 哨兵侧变更史（自 rm-sentry-integration 继承） |
| `LICENSE` / `THIRD_PARTY.md` | 许可证与第三方声明 |
| `.gitignore` / `.gitattributes` | 忽略规则（构建产物/视频）与行尾/二进制标注（两仓合并版） |
| `.github/workflows/ci.yml` | 统一 CI：装 OpenCV → 构建 → ctest → 缓存/CSV 集成 → 双参数仿真冒烟 |

## 代码

| 目录 | 文件数 | 标注 |
|---|---|---|
| `core/include/autoaim/` | 13 | 共用核心头文件：检测/位姿/PnP/跟踪/弹道/瞄准/配置/评测/相机源/位姿缓冲/时延标定 |
| `core/src/` | 12 | 共用核心实现（只存一份，两车共用） |
| `core/config/` | 2 | `default.yaml` 为步兵/通用默认；`video_reference.yaml` 为历史 N0 离线复现，不能替代实车标定 |
| `core/model/` | 2 | 装甲数字 MLP 模型（onnx + 说明） |
| `sentry/include/autoaim/` | 1 | 哨兵目标定义（`target.hpp`：TargetManager/前哨站） |
| `sentry/include/sentry/` | 4 | 哨兵模块头：视觉管线、串口协议、评测策略、配置 |
| `sentry/include/io/` | 1 | 串口端口头文件 |
| `sentry/src/`（含 `io/`） | 6 | 哨兵实现：TargetManager、SentryVision、串口、哨兵评测策略 |
| `sentry/apps/` | 2 | `sentry_robot`（上车主程序）、`sentry_offline`（离线调试） |
| `sentry/config/` | 2 | `sentry.yaml`（哨兵参数）、`default.yaml`（哨兵风味默认） |
| `sentry/tools/`（含 `legacy_include/`） | 6 | 旧代码对比台架（与 legacy/ 对照用，非产品构建） |
| `apps/` | 5 | 步兵侧程序：`autoaim_video`（离线评测）、`autoaim_sim`（仿真）、`autoaim_lights`、`detector_bench` + CMake |
| `tests/` | 7 | 三套 CTest：core/哨兵/串口，另有缓存集成、视频对齐测试、回归门契约测试（test_regression_gate.py）与 CMake |

## 文档与报告

| 目录 | 文件数 | 标注 |
|---|---|---|
| `docs/` | 69 | 全部设计/审查/调参文档（见下方 docs 分类表） |
| `docs/charts/` | 17 | 数据图表（SVG，tools/make_charts.py 生成，见 performance_charts） |
| `docs/assets/` | 1 | 仓库标志 logo.svg（README 顶部居中展示） |
| `docs/detections/` | 9 | 检测效果样例图/视频（说明用） |
| `reports/` | 3 | 仿真与真视频汇总报告（sim_report 由 UTF-16 转码为 UTF-8，文本不变） |
| `reports/real_video_regression/` | 9 | 真视频回归分项报告（N0/N1/弹速/标定四组 × 两视频 + 总报告） |

### docs/ 分类表

- **设计**：timestamp_design、attitude_timestamp_alignment、timestamp_alignment、
  device_id_and_crc、bullet_speed、yolo_keypoints_plan、multi_target、roi_detection
- **审查与加固**：full_audit_2026-11、review_2026-11、review_2026-11_round2、
  review_response、core_hardening_port、hardening、code_review_improvements、
  REVIEW_UPDATE_2026-11-10、POSE_ASSOCIATION_DIAGNOSTICS_2026-11-10、
  code_review_fixes_r3（第三轮源码级审查的九项处置：预测协方差补过程噪声并附
  门控 A/B、迭代收敛判定与普查、Yaw NIS 帧聚合、过期观测拒绝、PnP 误差回退、
  MVS 频率/读回/超时三修、贪心关联按实测标准暂缓）、
  sigma_coverage_study（P2-9 门控标定研究：仿真覆盖率标定出 FSS* 1.97/2.22，但分布不可
  单一 scale 标定且真实视频迁移失败、hits/s 净损失——结论门控保持关闭，附重开条件）、
  nis_adapt_study（P2-10 子实验：NIS 驱动自适应 q——仿真平移 +3.5/+7.0 pp 成立，但真实
  视频阈值锁存、占用 92–100% 帧，四格守门全退化，否决出厂；τ 联动仿真即灾难性否决；
  补丁留实验副本未合入，附重开条件）、
  detector_tuning_study（检测器参数 A/B：阈值 60/80、自适应阈值、分类阈值 0.6 均未达
  合入标准，出厂参数保持；角点精化经消融证实值 +3–4 pp，原始数据存目标工作区）
- **标定与实测**：distortion_order、MEASUREMENTS、on_robot_trial_checklist（上车试机清单）、
  calibration_record_template（标定档案模板：重标定必须存原始图像/工具输出/重投影误差，不只留五个数字）
- **调参研究**：parameter_sensitivity、parameter_interaction、parameters、
  adaptive_threshold、carry_velocity、warmup_gate、fire_window、exposure_robustness、
  fps_ablation、fx_sensitivity、threshold_vs_range、window_by_range、hit_by_range、
  hit_by_omega、impact_margin、dps_sweep、occlusion_reassociation、miss_analysis、
  FIELD_CONDITIONS_2026-11-11（遮挡/掉帧/降采样/抖动/延迟压力矩阵）
- **性能**：optimization、optimization_round2
- **交付与盘点**：deployment_map（部署地图：上车代码位置、构建产物与配置清单、目录总表——新人先看这页）、
  changes_vs_legacy（相对老代码的改动总览：11 类改动逐项说明作用与证据，含已否决方案清单）、
  performance_charts（数据图表页：十七张深色主题 SVG 图表呈现命中率/压力/耗时/误差预算、两项否决研究，以及误差与强度的时间线、预测时域曲线、sigma 校准、检测器 A/B、模拟器场景、策略阶梯、ROI 窗口、弹道曲线与相机带宽，生成脚本 tools/make_charts.py）、
  camera_hardware（相机硬件与采集参数：队伍实机主相机/全向感知相机配置表，已与 perservance 代码逐项对照，含本仓默认值修正与 USB 带宽提醒）、
  design_archive.html（设计档案总览页，浏览器直接打开；数字以 BASELINE.md 为准）、
  monorepo_migration（合并决策与回归证据）、
  completeness_audit_2026-11（迁移时完整性审计）、VIDEO_TEST_2026-11-10（用户视频复测）、software_completion、
  software-only-inventory-and-data-needed（软件盘点与待补数据清单）、
  legacy_comparison（新旧代码对比）、pipeline_and_tuning（项目叙事/原主仓 README）、
  BACKLOG、SENTRY_BACKLOG、PROGRESS_2026-11-04、EXTERNAL_COMPARISON、
  SENTRY_INTEGRATION、DEPLOY_IT13、automation、ubuntu2204_compat、
  gimbal_ff（已否决方案记录）、failure_modes、shared_core（已退役机制，历史）

## 历史与旧代码

| 目录 | 文件数 | 标注 |
|---|---|---|
| `legacy/` | 65 | 旧实车代码 `auto-aim-master` 全树原样保存（含 Buff 打符管线与模型），仅作对照与素材来源，不参与构建 |
| `archive/` | 8 | 已退役/已合入工作的过程存档：`patches/`（4，分轮补丁原件）、`shared-core/`（3，退役守卫）+ 本目录说明 |

## 工具

| 目录 | 文件数 | 标注 |
|---|---|---|
| `tools/` | 14 | 回归/调参/标定脚本（regression、tune_sim、auto_improve、calibrate_intrinsics、check_extrinsics、make_charts 等，路径已按单仓布局更新） |
| `tools/offline_eval/` | 4 | 离线回归台架源码 + 说明（非 CMake 目标，基线证据复现用） |
| `data/eval_cache/` | 39 | 真视频评测原始数据：两视频的检测缓存（每视频一份去重后的权威副本）+ 全部跟踪 CSV/报告/日志，按视频分 3/、4/ 两组，分类说明见其 README |
| `tools/tasks/` | 21 | 任务/数据处理脚本集 |
| `.claude/` | 7 | Claude Code 配置：settings + rm-reviewer 代理 + 5 个技能（hardening-review / regression / calibration-writeback / serial-check / monorepo） |

## 不在本仓内的材料（明确排除，非遗失）

- 作业仓 `rm-armor-detection` 的任何文件——按用户要求不触碰、不复制。
- 视频数据——在本仓 Release `data-v1-2026-11-09` 的附件中（3.avi 整文件；4.avi 分两片，拼接后按 Release 说明的 SHA-256 校验）；不进 git 历史（单文件超 GitHub 100 MB 上限）。检测缓存与评测原始输出已入仓，见 `data/eval_cache/`。
- 各轮交付的代码快照目录——代码已合入本仓，理由与清单见 `archive/README.md`。
- Claude Code 配置包的 zip/install.sh/全局配置——分发工具，内容已入仓
  （CLAUDE.md + .claude/），包本体留在交付渠道。

## 本轮合并完整性复核（2026-11-10）

当前跟踪文件 345 个（`git ls-files`）；迁移审计的 269 个是历史快照。
其后新增：离线参考配置与三份审查/视频/诊断报告（4 个）、英文 README 与
CI 改版等同数替换，以及 2026-11-10 数据整合轮新增的 `data/eval_cache/`
（39 个：检测缓存去重后每视频一份 + 全部评测原始输出）。
没有删除或移动历史文件，`legacy/`、`archive/`、模型和冻结基线保持原样。
共用实现仍仅在 `core/`，哨兵模块继续链接它；默认全模块和
`BUILD_SENTRY=OFF` 两种构建入口都纳入本轮验证。
