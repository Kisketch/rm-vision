---
name: regression
description: RM 回归验证（单仓 rm-vision）。用户说跑回归、验证没退化、改完检查、regression 时使用。执行构建、全部测试、仿真，并按需做真实视频回归判定，给出与基线的量化对比。
allowed-tools: Read, Bash(cmake *), Bash(cmake --build *), Bash(ctest *), Bash(./build/*), Bash(git diff *), Bash(git status *)
---

# 回归流程（rm-vision）

1. 干净构建：`cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)`，记录错误/警告数（期望 0 警告）。
2. 测试：`ctest --test-dir build --output-on-failure`，期望 3 套全过（autoaim_tests / sentry_tests / serial_port_tests）。只建步兵侧时用 `-DBUILD_SENTRY=OFF`，此时只有 core 测试，属预期。
3. 仿真：`./build/apps/autoaim_sim`，两套参数各跑一次（`--config core/config/default.yaml` 与 `--config sentry/config/sentry.yaml`，CI 同款命令加 `--seconds 5` 做冒烟），输出表与 BASELINE.md 冻结表逐格对比，差异格必须标出并解释。注意：哨兵参数跑 autoaim_sim 时个别自旋格有 ±0.2/−0.7 的已知抖动（仿真驱动的是步兵瞄准器，生产哨兵路径不受影响，见 docs/project/monorepo_migration.md），不要误判为退化。
4. 真实视频回归（仅当改动涉及 detector/pose/tracker/ballistics/aimer 时）：
   - 视频只从本地私有来源读取，**不得把视频或逐帧数据写进仓库**，只报汇总：命中率、预测中位误差（px）、tracking 比例。
   - 与冻结基线对比：整车 EKF 67.5% / 68.6%、中位 14.3 / 13.3 px；哨兵策略（哨兵参数）67.6% / 71.9%。命中率波动 ≤0.1pp 且中位误差波动 ≤0.1px 判为行为保持；超出必须定位到具体改动并解释是物理预期（如弹速回写）还是退化。
5. 输出结论表：构建 | 测试 | sim（两套参数） | 视频回归 | 判定（保持/预期变化/退化）。任何一项没跑，明确写“未跑”，禁止推断。
