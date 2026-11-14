# 独立复现验证（2026-11-14）

> 问题：冻结基线能否在**另一台机器、另一套工具链**上复现？本仓的全部历史
> 数字都产自同一参考环境（Linux / GCC 15.3 / OpenCV 4.13.0），跨环境可复现性
> 此前未被验证。本次在一台不同构的机器上全量重新检测（`--redetect`，不使用
> 发布缓存），逐项对照 BASELINE §10/§12 的冻结值。
>
> 结论：**12 项冻结指标逐项偏差 0.0**；过曝灯条修复的行为断言（3.avi +14 帧、
> 4.avi 逐字节不变）精确成立；检测缓存与发布版的差异仅为数字分类器 softmax
> 输出的 float 末位（≤6×10⁻⁷，OpenCV 4.10 与 4.13 的实现差异），几何零差异。
> 仿真均值按 BASELINE §8 声明的机器相关口径复核通过。

## 1. 环境

| 项 | 本次复现机 | 仓库参考环境 |
|---|---|---|
| 系统 | Ubuntu 26.04.1 LTS（内核 7.0.0-43-generic，VMware 虚拟机，16 vCPU / 7.3 GiB） | Linux（容器） |
| 编译器 | GCC 15.2.0 | GCC 15.3 |
| OpenCV | 4.10.0（发行版包） | 4.13.0 |
| CMake | 4.2.3 | ≥3.16 |
| Python | 3.14.4 | 3.x |

提交：`eca3eadee688be9c5d272abf08c46b3ad4d2cb19`（main，2026-11-12）。
视频为 Release `data-v1-2026-11-09` 资产，SHA-256 与 Release 说明逐项一致
（3.avi `87c61f91…01277`；4.avi 两片拼接后 `7478973c…7423a8`）。

## 2. 构建与测试

- Release 干净构建：**0 警告**。
- ctest：**5/5 通过**（core / sentry / serial PTY 端到端 / 回归门契约 / 头文件自包含）。

## 3. 全量重新检测与冻结基线对照

每段视频在两套配置下各独立重检一遍（共 4 次全片，1440×1080）：

| 视频 | 配置 | 检测耗时（本机） | 参考环境 |
|---|---|---|---|
| 3.avi | N0 参考 | 2.54 ms/帧 | 3.31–3.52 ms |
| 4.avi | N0 参考 | 2.60 ms/帧 | 同上 |
| 3.avi | 实机 default | 2.60 ms/帧 | — |
| 4.avi | 实机 default | 2.55 ms/帧 | — |

`tools/regression.py` 门禁输出（实测 − 冻结）：

### N0 参考口径（core/config/video_reference.yaml）

| 视频 | 指标 | 冻结值 | 实测 | 偏差 | 判定 |
|---|---|---|---|---|---|
| 3.avi | 命中率 | 67.5% | 67.5% | +0.0 | PASS |
| 3.avi | 0.32 s 预测误差 | 14.3 px | 14.3 px | +0.0 | PASS |
| 3.avi | hits/s | 10.6 | 10.6 | +0.0 | PASS |
| 4.avi | 命中率 | 68.6% | 68.6% | +0.0 | PASS |
| 4.avi | 0.32 s 预测误差 | 13.3 px | 13.3 px | +0.0 | PASS |
| 4.avi | hits/s | 10.1 | 10.1 | +0.0 | PASS |

命中数核对：3.avi 937/1388、4.avi 1374/2004（verifiable），与
`reports/real_video_regression/` 的冻结报告一致。

### 实机口径（core/config/default.yaml）

| 视频 | 指标 | 冻结值 | 实测 | 偏差 | 判定 |
|---|---|---|---|---|---|
| 3.avi | 命中率 | 58.1% | 58.1% | +0.0 | PASS |
| 3.avi | 0.32 s 预测误差 | 15.2 px | 15.2 px | +0.0 | PASS |
| 3.avi | hits/s | 9.1 | 9.1 | +0.0 | PASS |
| 4.avi | 命中率 | 66.9% | 66.9% | +0.0 | PASS |
| 4.avi | 0.32 s 预测误差 | 12.4 px | 12.4 px | +0.0 | PASS |
| 4.avi | hits/s | 8.8 | 8.8 | +0.0 | PASS |

命中数核对：3.avi 803/1383 = 58.1%（保守口径 803/1420 = 56.5%），与 README
"由表格推断恰为 803 次命中"一致。

### 仿真门禁（机器相关项）

- 8 场景均值：本机 **73.6%**。对参考机 78.1% 偏差 −4.5 pp，按默认基准 FAIL；
  按脚本注释与 BASELINE §8 声明的机器相关口径（74.5 ± 3 pp）复核：−0.9 pp，**PASS**。
- 急横移专项行：**11.4%**，与两台参考机逐位一致（脚本注释记载该行为种子确定、
  跨机器一致；本次为第三台机器复现）。

## 4. 检测缓存比对

### 新检测（N0）vs 发布缓存（data/eval_cache/）

逐行解析比对（帧号、编号、类型、灯条端点/宽度逐项对比，置信度单独统计）：

| 视频 | 检测总数 | 置信度差异数 | 最大置信度差 | 受影响帧 | 几何/结构差异 |
|---|---|---|---|---|---|
| 3.avi | 2380 | 58 | 4.8×10⁻⁷ | 56 | **0** |
| 4.avi | 3885 | 114 | 6.0×10⁻⁷ | 111 | **0** |

差异全部位于数字分类器（cv::dnn MLP）softmax 输出的 float 末位，量级 1e-7，
归因于 OpenCV 4.10 与 4.13 的 dnn 实现差异；灯条端点、宽度、类型、编号完全一致。
不影响任何下游判定——12 项指标偏差 0.0 即为证据。发布缓存的 MD5 与
`data/eval_cache/README.md` 记载一致（3.avi `fc8c9bea…`、4.avi `cae8f2d5…`）。

### 修复行为断言（N0 新检测 vs 实机配置新检测）

- 3.avi：**恰好 14 行**由 0 装甲变为 1 块 3 号板，置信度均 ≥ 0.9999，新增帧位于
  1966–2050 近距离过曝段（1966、1967、1974、1987、2021–2025、2028 等），
  无丢板、无改板——与 [split_light_merge](split_light_merge.md) 的宣称逐帧吻合。
- 4.avi：**0 行差异**；两次独立全量重检的输出逐字节一致
  （MD5 均为 `1a890a11b6e1c88f1d0f937d95f5c883`）。

## 5. 结论与边界

- 冻结基线跨环境（GCC 15.2 / OpenCV 4.10 / Ubuntu 26.04）逐项零偏差复现，
  "证据可复现"的宣称成立；检测器几何输出对 OpenCV 小版本差异稳健。
- 仿真均值的机器相关性（±3 pp 口径）再次得到验证；急横移短板（11.4%）为
  结构性问题而非环境噪声，与 README Limitations 一致。
- 本次仍为**离线**复现：不涉及真实云台动力学与实弹；上车试机清单
  （[on_robot_trial_checklist](../calibration/on_robot_trial_checklist.md)）
  各项依旧待执行，N1 基线（本队相机 footage）仍缺。

## 6. 复现方法

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
(cd build && ctest --output-on-failure)
# 全量重检 + 评测（两套配置，各自独立输出目录）
./build/apps/autoaim_video video/3.avi --config core/config/video_reference.yaml --out runs/eval_n0 --redetect
./build/apps/autoaim_video video/4.avi --config core/config/video_reference.yaml --out runs/eval_n0 --redetect
./build/apps/autoaim_video video/3.avi --config core/config/default.yaml --out runs/eval_live --redetect
./build/apps/autoaim_video video/4.avi --config core/config/default.yaml --out runs/eval_live --redetect
# 门禁（仿真基准按机器口径，见 BASELINE §8）
python3 tools/regression.py --exe build/apps/autoaim_video --sim-ref 74.5
python3 tools/regression.py --exe build/apps/autoaim_video --config core/config/default.yaml --sim-ref 74.5
```

本次运行产物存于复现机 `~/code/robot-vision-aim/runs/`（eval_n0、eval_live、
sim_output.txt），未入仓；入仓的权威缓存仍是 `data/eval_cache/`（N0 口径，
几何与本次新检测零差异）。
