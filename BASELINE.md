# BASELINE — 2026-11-04 基线快照

> **数据获取**：本文件用到的两段视频（3.avi / 4.avi，含 SHA-256）发布在本仓自己的
> Release [`data-v1-2026-11-09`](https://github.com/Kisketch/rm-vision/releases/tag/data-v1-2026-11-09)
> 附件中（4.avi 因上传上限分两片，拼接方法与分片哈希见 Release 说明）。
> 下载到本仓 `video/` 目录后即可运行 `tools/regression.py` 复现 §10/§11 的全部数字：
> `gh release download data-v1-2026-11-09 --repo Kisketch/rm-vision -D ./video`

> Stage 0 的产物：锁定"改进前"的数字，之后每一次改动都拿它对比。
> 所有数字都来自本目录 `reports/` 下的原始输出，环境与命令见下。

> **2026-11-04 更新**：§4/§5 仍是 `edfd512` 的基线快照（62.5% / 59.6%），保留作历史对比。
> 之后合入的 Singer 速度阻尼（`singer_tau=0.3`，见 README"Tuning log" F 行）、装甲板切换滞回
> （`switch_hysteresis_deg=3`，G 行）与 yaw 过程噪声调整（`q_yaw` 30→8，H 行）
> 把两条视频提到 **67.6% / 68.6%**，0.32 s 中位预测误差 14.3 / 13.4 px；
> `reports/` 已按新配置重新生成。回归判断请用 §10 的新基准，§7 的数字只用于判断"相对 Stage 0 是否退步"。

## 1. 环境

| 项 | 值 |
| --- | --- |
| 系统 | Windows 11 10.0.26300（Home China） |
| CPU | Intel Core Ultra 9 275HX |
| 编译器 | MSVC（Visual Studio 18 2026，x64） |
| 构建 | CMake 4.3.1，生成器 `Visual Studio 18 2026`，Release |
| OpenCV | 4.14.0（`C:/opencv/opencv/build`，vc16） |
| 代码版本 | `edfd512`（工作区干净） |
| 二进制时间 | 2026-11-04 03:26:51（`build/Release/autoaim_video.exe`） |
| 配置 | `core/config/default.yaml`（未改动） |

## 2. 复现命令

Windows（PowerShell）：

```powershell
$env:Path = "C:\opencv\opencv\build\x64\vc16\bin;" + $env:Path
cd autoaim
.\build\Release\autoaim_tests.exe
.\build\Release\autoaim_video.exe ..\3.avi --out out
.\build\Release\autoaim_video.exe ..\4.avi --out out
.\build\Release\autoaim_sim.exe
```

Linux：

```bash
cmake -S . -B build && cmake --build build -j
./build/tests/autoaim_tests
./build/apps/autoaim_video ../3.avi --out out
./build/apps/autoaim_video ../4.avi --out out
./build/apps/autoaim_sim
```

## 3. 单元测试

```
pose: max rel. position error 2.25%, max yaw error 1.76 deg (0.3 px noise)
tracker: w 3.000 (true 3.0), r1 0.240 r2 0.240 (true 0.24)
ALL TESTS PASSED
```

## 4. 真实视频评测

原始输出：`reports/video3_report.md`、`reports/video4_report.md`。

### 4.1 虚拟射击命中率

| 视频 | 策略 | shots | 可验证 | 命中 | 命中率 | hits/s |
| --- | --- | --- | --- | --- | --- | --- |
| 3.avi | direct | 1943 | 1856 | 658 | 35.5% | 7.4 |
| 3.avi | point KF | 1743 | 1653 | 489 | 29.6% | 5.5 |
| 3.avi | **robot EKF** | 1331 | 1279 | 799 | **62.5%** | 9.0 |
| 3.avi | oracle | 2010 | 1990 | 1977 | 99.3% | 22.4 |
| 4.avi | direct | 2927 | 2840 | 1611 | 56.7% | 11.8 |
| 4.avi | point KF | 1138 | 1096 | 472 | 43.1% | 3.5 |
| 4.avi | **robot EKF** | 1914 | 1877 | 1119 | **59.6%** | 8.2 |
| 4.avi | oracle | 3229 | 3198 | 3001 | 93.8% | 22.0 |

### 4.2 预测误差（预测点 vs 观测点，像素）

| 视频 | 提前量 | no prediction | point KF | robot EKF |
| --- | --- | --- | --- | --- |
| 3.avi | 0.12 s | 27.4 | 9.7 | 5.1 |
| 3.avi | 0.20 s | 44.1 | 29.4 | 9.1 |
| 3.avi | 0.32 s | 39.0 | 66.5 | 16.2 |
| 4.avi | 0.12 s | 50.9 | 73.6 | 8.1 |
| 4.avi | 0.20 s | 23.1 | 91.2 | 12.8 |
| 4.avi | 0.32 s | 44.5 | 85.2 | 19.3 |

### 4.3 跟踪质量与几何

| 视频 | tracking ratio | frames with armor | median \|ω\| | r1 | r2 | dz |
| --- | --- | --- | --- | --- | --- | --- |
| 3.avi | 0.932 | 0.965 | 2.570 rad/s | 0.243 m | 0.208 m | 0.050 m |
| 4.avi | 0.918 | 0.979 | 8.267 rad/s | 0.239 m | 0.213 m | 0.046 m |

### 4.4 耗时（单线程）

| 视频 | detector | pose | tracker+aimer+judge | 合计 |
| --- | --- | --- | --- | --- |
| 3.avi | 3.749 ms | 0.845 ms | 0.379 ms | **4.97 ms/帧** |
| 4.avi | 3.690 ms | 0.899 ms | 0.377 ms | **4.97 ms/帧** |

## 5. 仿真评测

原始输出：`reports/sim_report.md`。

| 场景 | direct | point KF | **robot EKF** | hits/s | 中心误差 | ω 误差 | 半径误差 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 2.1 rad/s @5 m（同视频） | 42.3 | 97.5 | **99.4** | 16.95 | 4.5 cm | 0.62 | 0.9 cm |
| 6 rad/s + 平移 0.5 m | 53.6 | 14.6 | **45.0** | 5.45 | 3.5 cm | 0.80 | 0.3 cm |
| 10 rad/s @4 m | 43.2 | 37.0 | **98.7** | 14.65 | 2.4 cm | 0.41 | 0.2 cm |
| 无旋转 + 硬平移 0.8 m/2 s | 14.0 | 24.5 | **17.1** | 2.50 | 6.9 cm | 1.31 | 0.8 cm |
| 4 rad/s，7→3 m | 51.8 | 44.4 | **98.9** | 13.40 | 3.9 cm | 0.54 | 0.5 cm |
| 变速 0.5–4.5 rad/s | 69.9 | 73.2 | **97.4** | 13.00 | 5.2 cm | 0.59 | 0.5 cm |
| 往返摆动 ±3 rad/s | 64.4 | 88.7 | **98.2** | 16.60 | 5.1 cm | 0.61 | 0.4 cm |
| 3 rad/s + 轻平移 0.3 m | 74.9 | 67.0 | **85.2** | 11.80 | 4.9 cm | 0.60 | 0.5 cm |

## 6. 关键发现（写进回归规则）

1. **真实视频评测可精确复现**：本次重跑 3.avi 的 62.5% / 9.0，与仓库原报告逐项相同；4.avi 的 59.6% / 8.2 同样一致。视频评测是确定性的（检测结果缓存 + 固定判定），**作为回归主指标**。
2. **仿真评测有跨环境波动**：本次 `autoaim_sim` 得 99.4 / 98.7 / 45.0 / 17.1，而仓库原 `out/sim_report.md` 记录的是 100.0 / 99.6 / 41.8 / 19.1，差 0.6–3.2 个百分点。同一二进制连跑两次结果完全一致，说明波动不是"随机种子"，而更可能来自报告生成时的平台/OpenCV 版本/参数快照不同（该项目在 Linux 与 Windows 都跑过）。
   → **规则：仿真只在"同一台机器 + 同一二进制 + 同一配置"之间比较，阈值放宽到 ±3 个百分点。**
3. 单帧总耗时 ≈ 5 ms（1440×1080、单线程），其中检测占 75%；换 YOLO 后要把这部分单独计时对比。
4. 硬平移（5–8 m/s²）是当前模型的结构性短板（17–45%），远低于旋转场景（98%+）——这也是阶段四 4A 的唯一目标。

## 7. 回归阈值

| 指标 | 允许劣化 | 说明 |
| --- | --- | --- |
| 3.avi 命中率 | ≤ -2 pp | 与 62.5% 比 |
| 4.avi 命中率 | ≤ -2 pp | 与 59.6% 比 |
| 仿真 10 rad/s | ≤ -3 pp | 同机同二进制，与 98.7% 比 |
| 0.32 s 预测误差（median） | ≤ +2 px | 3.avi 16.2 / 4.avi 19.3 |
| 单帧耗时 | ≤ +10% | 4.97 ms |
| 单元测试 | 必须全过 | `autoaim_tests` |

## 8. 已知不可比项

- 更换 OpenCV 主版本、更换 OS、重编译后，仿真数字可能漂移 ±3 pp；
- 修改 `core/config/default.yaml` 后，旧基线作废，必须重新生成并提交新的 `reports/`；
- 25 fps 视频本身是限制条件，实车相机换 100+ fps 后本基线的"预测误差"不再适用，需要新基线。

## 9. 下一步

- 阶段一：真实相机标定 → 替换 `core/config/default.yaml` 里的假设内参（fx=2400、dist=0）；
- 阶段二：相机采集 + 串口 + 延迟实测；
- 每次改动后按 §7 跑一遍，结果贴到 PR 描述里。

## 10. 当前回归基准（2026-11-05，`singer_tau=0.3`、`q_yaw=8`、滞回 3°）

| 指标 | 当前值 | 允许劣化 |
| --- | --- | --- |
| 3.avi robot-EKF 命中率 | 67.6%（10.6 hits/s） | ≤ -1.5 pp |
| 4.avi robot-EKF 命中率 | 68.6%（10.0 hits/s） | ≤ -1.5 pp |
| 0.32 s 预测误差（median） | 3.avi 14.3 px / 4.avi 13.4 px | ≤ +1.5 px |
| 仿真均值（8 场景，seed 42） | 78.0% | 只做同机同二进制比较，±3 pp |
| 单帧耗时 | ≈ 5.0 ms（未变） | ≤ +10% |
| 单元测试 | `ALL TESTS PASSED` | 必须全过 |

仿真均值下降是已知取舍：合成场景的平移是光滑正弦，阻尼模型在真实机器人
（频繁换向）上更准。判断"能不能合入"以真实视频为准，仿真只用于发现结构性退化。

自动门禁：`python3 tools/regression.py` 会跑两条视频（复用检测缓存）与仿真，
逐项对比上表，任何一项超出容差即返回非零。当前实测与上表完全一致（+0.0）。

> **统计口径补充（2026-11-10，第二轮外部审查后）**：上表命中率是条件口径
> （命中 / 可验证发数）。评测报告现同时给出三口径——条件命中率、可验证率
> （可验证 / 总发数）、保守命中率（命中 / 总发数，未验证按未命中计）。
> 冻结值（robot EKF）：参考口径 3.avi 发 1440 / 可验证 1388（96.4%）/ 保守
> 65.1% / 条件 67.5%；4.avi 发 2049 / 可验证 2004（97.8%）/ 保守 67.1% /
> 条件 68.6%。现行口径 3.avi 保守 55.3%（条件 57.3%），4.avi 保守 65.3%
> （条件 66.9%）。本素材上条件口径高估约 1.5–2.4 pp；遮挡重的素材上该差
> 值会放大，三口径并列就是为了让它可见。门禁仍按条件口径（历史可比）。

## 11. 两套口径必须分开：default.yaml 与 video_reference.yaml

§10 的数字是用 `core/config/video_reference.yaml`（N0 参考口径：零畸变、
假定针孔内参）测的，**不是** `core/config/default.yaml`（现用内参 + 非零畸变）。
两者差 10 pp 左右，用哪套口径必须写在报告里：

| 3.avi / 4.avi robot-EKF | default.yaml | video_reference.yaml (N0) |
| --- | --- | --- |
| Stage J（`12409a7`） | 56.8% / 64.4% | **67.5% / 68.6%** |
| 0.32 s 预测中位 | 15.1 / 13.2 px | 14.3 / 13.3 px |
| tracking ratio | 0.871 / 0.837 | 0.933 / 0.932 |

两条口径在 Windows / MSVC 14.51 / OpenCV 4.14.0 上都能逐位复现上表，因此
"换机器导致 10 pp 漂移"的说法不成立——之前的差异全部来自配置口径。

`tools/regression.py` 原来既按 §10 的 N0 数字判定、又用 `default.yaml` 跑，
自相矛盾，在任何机器上都通不过。现在 `--config` 默认指向
`core/config/video_reference.yaml`；要与现用配置比较，显式传
`--config core/config/default.yaml`。

### Stage K 的实测（两套口径都测，不能混读）

`video_reference.yaml` 是冻结口径，本轮**一个字没有改**，所以 N0 的
`q_yaw_walk` 仍是 0：

| 口径 | 3.avi robot-EKF | 4.avi robot-EKF | 0.32 s 预测中位 |
| --- | --- | --- | --- |
| N0 冻结（未改，`walk=0`） | 67.5%（10.6） | 68.6%（10.1） | 14.3 / 13.3 px |
| N0 + `--set q_yaw_walk=0.05` | **67.5% → 68.4%**（10.67） | **68.6% → 71.1%**（10.54） | 14.3 / 12.4 px |
| default.yaml（已改为 `walk=0.05`） | **56.8% → 57.3%**（8.8） | **64.4% → 66.9%**（8.8） | 15.1→15.3 / 13.2→12.4 px |

也就是说：现役 `default.yaml` 是净提升，N0 冻结口径零变化（历史数字继续可比），
而同一个旋钮在 N0 上还能再拿 +0.9 / +2.5 pp，要不要把 `q_yaw_walk` 写进 N0
属于口径决策，不是技术决策。更大的 `q_yaw_walk` 能把 4.avi 推到 72.3%，但
N0 的 3.avi 掉 2.5 pp，所以取 0.05 作为跨口径折中。

### 仿真均值是机器相关的

本机未改动的代码量到 74.5%，§10 的 78.1% 在参考机器上；§8 已经写明"只做同机
同二进制比较，±3 pp"。本机跑门禁需要 `--sim-ref 74.5`。Stage K 让仿真均值从
74.5% 降到 73.0%（−1.5 pp，容差内）：`walk=0.05` 单独用会到 71.1%，`q_yaw_adapt`
把"variable spin"和"swing"两个角速度阶跃场景拉回 6.4 / 13.0 pp，补回 1.9 pp。
`q_yaw_walk` 注入的相位方差在干净合成数据上就是噪声，这一点由
`tests/test_main.cpp::testYawProcessNoiseSplit` 显式记录。
