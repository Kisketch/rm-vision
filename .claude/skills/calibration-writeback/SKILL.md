---
name: calibration-writeback
description: 上车实测数据回写配置（单仓 rm-vision）。用户说回写、标定结果、实测数据、试机数据带回时使用。只接受带测量方法与原始数据的实测值，估计/占位值不得回写为实测，写后跑回归并建新基线说明。
allowed-tools: Read, Grep, Edit, Bash(cmake *), Bash(cmake --build *), Bash(ctest *), Bash(./build/*), Bash(git diff *)
---

# 实测回写流程（rm-vision）

## 准入（缺一不写）
每项数据必须有：数值 + 单位 + 测量方法 + 原始记录（日志/计数/录像文件名）。只有结论数字、没有方法的，退回并列出缺什么。

## 可回写项与落点
- 传输/曝光时延：DelayCalibrator 输出 → 哨兵填 `--exposure-offset-ms`，MVS 填 `MvsConfig::stamp_offset_us`，并同步 aimer.latency 复核
- 弹速：17mm / 42mm 分开，实测均值与散布 → `core/config/default.yaml` 的 bullet_speed 相关字段；42mm 在实测前保持占位标注
- 静态偏差：yaw_offset_deg / pitch_offset_deg（注意 core 与 sentry 模块的 pitch 符号相反，写前复述一遍方向让用户确认）
- 标定确认项：畸变存储顺序、装甲板尺寸定版、杠杆臂坐标系——确认后才启用对应配置（杠杆臂默认注释状态）
- 注意配置分工：步兵/英雄默认在 `core/config/default.yaml`；哨兵上车在 `sentry/config/sentry.yaml`；哨兵离线默认在 `sentry/config/default.yaml`（含 carry_velocity 关）。不要把哨兵参数写进 core 配置，反之亦然。

## 步骤
1. 先 `git diff` 预览将改的每个字段：旧值→新值→证据来源，展示给用户确认后才落盘。
2. 落盘后跑 /regression；若基线因此改变（如弹速 25→22 曾使命中率 -5.4pp，属物理预期），在 docs/ 写新基线说明，区分“代码退化”与“参数变真实”。
3. 提交信息包含数据来源日期与方法一句话。禁止把估计值、仿真值写进“实测”字段或提交信息。
