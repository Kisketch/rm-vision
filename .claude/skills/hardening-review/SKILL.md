---
name: hardening-review
description: RM 自瞄全量硬化审查（单仓 rm-vision）。用户说审查、复核、找缺陷、hardening、code review、通读代码时使用。按固定清单逐项核验并用构建/测试/复现实验给证据，不凭阅读印象下结论。含串口与单仓模块边界清单。
allowed-tools: Read, Grep, Glob, Bash(cmake *), Bash(cmake --build *), Bash(ctest *), Bash(git diff *), Bash(git log *), Bash(git status *)
---

# 硬化审查流程（rm-vision 单仓）

## 步骤
1. 先进入 Plan 模式通读：列出将查的文件与清单，确认后再逐项核验。
2. 每项缺陷必须给出：文件:行号定位 + 代码证据 + 可复现验证（最小复现/测试/实验数字）。复现不了的标“未证实”，不得写成缺陷。
3. 区分三类结论：已证实缺陷 / 已证伪（声称不成立，附证据）/ 离线无法验证（只能上车）。

## 必查清单（两轮真实审查 + 全量审计命中过的问题）
- 角度工具：wrapAngle 等归一化对 ±inf/NaN 是否安全（禁止循环减 2π，手写实现要用极端值实测）
- 退化输入：空帧/单通道/异常尺寸进 Detector 是否有守卫；PnP 角点退化/NaN；tracker 喂 NaN 观测；PoseBuffer 乱序 push 与非有限样本；缺失配置项的报错是否清晰
- 时间戳与姿态：观测时刻是否为曝光中心；恒定延迟是否同时修正预测时域和姿态查询（只修一处 = 缺陷）；PoseBuffer 查询失败/外推策略在默认参数下是否系统性丢帧
- 火控：中心模式是否按 t0+t_fly 取目标位置；ballistics ok=false 是否被检查；弹道求解与 golden 的偏差；Aimer 是否按值持有 Ballistics（引用成员 = 悬垂风险）
- 跟踪/选靶：目标身份键是否完整（DeviceId=颜色+数字+板型）；同一身份是否会生成重复轨迹；选靶滞回是否复检资格（tracking 状态、距离门）
- 数值与性能声称：任何“优化”必须给出改前/改后实测（ms 或 µs、样本量），等价性优化必须给像素级/数值级等价证据
- 仓库卫生：.bak/构建产物/大文件误提交；docs 描述的功能在本仓是否真实存在；默认值声称与代码是否一致；`legacy/` 是否被改动（只读存档，禁止改）

## 串口专项（sentry/ 模块，历史上全是真缺陷）
- termios：ICRNL/INLCR/IGNCR 是否清零（构造含 0x0D 的帧验证字节不被改写）
- fd 生命周期：reader 线程与 send() 的 close/reopen 是否有锁保护
- 帧解析：长度/帧头错位后的重同步；CRC 开/关两条路径（--require-crc 默认必须关）
- PoseBuffer：有界外推上限（--max-extrap-ms 默认 10）是否生效；默认参数下 attitude miss 率

## 单仓模块边界（合并后新增）
- core/ 是否只有一份核心：禁止在 sentry/ 或 apps/ 里复制核心文件；模块间分叉必须与 docs/monorepo_migration.md 的分类一致
- 不要把 docs/shared_core.md 当现行清单——它是已退役的历史文档，其校验脚本在新仓不存在
- pitch 符号按模块核对：sentry 模块 Rz(yaw)·Ry(−pitch)·Rx(roll)、正 pitch 向下，与 core 侧相反

## 收尾
跑 /regression 的完整流程。输出表格：声称 | 核验结果 | 证据。修复只在用户确认清单后进行，逐项修复逐项复验。
