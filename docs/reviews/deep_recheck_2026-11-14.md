# 深度复检：硬件集成前的软件端验证（2026-11-14）

> 目的：在硬件端（GEEKOM IT13 机载主机 + 海康 MVS 相机 + MCU 串口链路）跟进
> 之前，对软件端做一轮超出回归门禁的复检：消毒器全链路、压力测试、硬件边界
> 逐文件审查。环境：Ubuntu 26.04.1 LTS 虚拟机（16 vCPU / 7.3 GiB），
> GCC 15.2.0，OpenCV 4.10.0。提交基点 `03cbf94`。
>
> 结论：**离线可验证部分全部通过，无阻塞性缺陷**。发现 2 项中等集成风险
> （M1 已在本轮修复，M2 为硬件端契约项）与 3 项低危观察。

## 1. 动态验证

### 1.1 ASan + UBSan 全链路（RelWithDebInfo）

| 项目 | 结果 |
|---|---|
| 构建 | 0 警告 |
| ctest 5/5 | 通过，0 报告 |
| 3.avi / 4.avi 全片重检（实机配置，--redetect） | 通过，0 报告 |
| autoaim_sim 全 8 场景 | 通过，0 报告 |
| sentry_robot 冒烟（视频回放 300 帧） | 通过，0 报告 |

AddressSanitizer（含泄漏检测）+ UndefinedBehaviorSanitizer 合计 **0 发现**。
独立复现了提交史宣称的 ASan+UBSan 验证，覆盖面另增仿真与哨兵冒烟。

### 1.2 压力测试（Release，N0 口径，7 组扰动，种子与文档冻结值不同）

| 扰动 | 条件命中率 | 保守命中率 |
|---|---|---|
| 3.avi 遮挡丢检 20% | 63.0% | 46.9% |
| 4.avi 遮挡丢检 20% | 67.1% | 49.9% |
| 4.avi 遮挡丢检 40% | 54.4% | 26.6% |
| 4.avi 时间戳抖动 10 ms | 49.5% | 48.4% |
| 3.avi 半率降采样 | 67.8% | 65.1% |
| 4.avi 半率降采样 | 55.2% | 54.0% |
| 4.avi 丢检 20% + 抖动 5 ms | 61.7% | 46.1% |

无崩溃、无挂起。方向性结论与 [FIELD_CONDITIONS](../studies/FIELD_CONDITIONS_2026-11-11.md)
一致（慢目标抗丢帧、快目标退化明显、10 ms 抖动比 40% 丢检更致命、半率不塌方）。
绝对值与文档不同属预期：压力矩阵依赖扰动种子，本次种子不同，不构成回归。

## 2. 硬件边界审查（逐文件）

范围：serial_protocol、serial_port、sentry_robot、sentry、target、aimer、
camera_source、pose_buffer、delay_calibration、pose、tracker（844 行全读）、
ballistics、config、roi、detector、serial_port_test。

CLAUDE.md 的 18 条硬约定守卫逐条核验在位（pitch 符号、曝光时刻查姿态、
wrapAngle=std::remainder、Ballistics 按值持有、PoseBuffer 拒非有限、
DeviceId 单轨迹、rate 前馈、normalizeFrame、追加式合并、串口 raw 模式、
fd 锁、CRC 默认关、中心模式飞行时间平移、滞回复检资格、有界外推、
ROI 默认关 TG 60px 等）。

发射安全链五层核验：火控门（tracking ∧ warmup ∧ confident ∧ 朝向窗 ∧
converged，非收敛不发射）→ 保持段 0.5 s 强制 fire=0 → 标定模式每次 send
前无条件 fire=0 → 正常退出发 found=0/fire=0 失效帧 → 无姿态整帧跳过。

数值与并发：EKF Joseph 形式 + Cholesky（S 恒正定）+ 状态钳制 + 陈旧帧拒收 +
双通道 NIS 按帧聚合；PoseBuffer 全锁、乱序插入、同戳替换；串口计数器全
atomic；solveAll 并行无共享写；弹道迭代上限 + 退化斜率回退；配置加载即
validate（armors_num 限 2–4、外参必须真旋转矩阵）。

## 3. 发现项与逐条处置

### M1 · MVS 像素格式未固定，不支持格式终止进程 —— 已修复（本轮）

`MvsSource::open()` 原先不设置 PixelFormat，依赖相机持久化配置；`grab()`
遇到转换列表外的格式返回 false，主循环 `while(grab)` 视为流结束——一帧
不支持的格式就让整个程序退出（相机被 MVS 客户端改过格式即触发）。

**处置**：`MvsConfig` 新增 `pixel_format`（默认空 = 保持相机持久格式）；
open() 对配置值校验并 `MV_CC_SetEnumValueByString` 固定，随后回读
`MV_CC_GetEnumValue`，不在转换列表（Mono8 / BayerRG8 / RGB8_Packed /
BGR8_Packed / YUV422_Packed）内即**启动期报错**，与曝光回读同等
"load-bearing" 处理；grab() 的 default 分支补错误日志（正常运行不可达）。
验证：非 MVS 构建 0 警告 + ctest 5/5；MVS 分支以桩 SDK 编译验证通过
（桩仅编译用，真实 SDK 的 ABI 核对仍是上车待验项）。

### M2 · MCU 超时失效保护 —— 硬件端契约项（不改软件）

视觉端互锁不覆盖进程崩溃、断电、读帧阻塞（REVIEW_UPDATE_2026-11-10 已
明示）。**MCU 固件必须实现：一段时间收不到有效控制帧即停射**。硬件端
跟进时第一优先级落实；串口联调时以"拔视觉端进程"用例验收。

### L1 · setParam 绕过 validate —— 记录，暂不改

`--set` 可注入越界值（armors_num=0、负噪声等），仅影响离线调参工具；
机器人程序不接受 --set。建议后续在 setParam 后复跑 validate()。

### L2 · 标定互锁无永久回归测试 —— 记录，建议补

互锁是单行结构性保证（本轮代码级核验 + 历史 PTY 注入验证：旧版 76 帧
fire 非零 → 新版 0），但不在 ctest 套件内。建议把 REVIEW_UPDATE_2026-11-10
的 PTY 注入场景固化为测试。

### L3 · DeviceSource 不校验实际分辨率 —— 记录，暂不改

请求分辨率后不回读；相机给 720p 时内参与图像不匹配且无警告。UVC 路径
本就是 hw_stamp=false 的过渡方案，建议回读并在不匹配时告警。

### M3 · 门禁契约测试用 Python 3.11+ API，IT13 目标机必挂 —— 已修复（本轮）

22.04 部署验证（GCC 11.4 / OpenCV 4.5.4 / Python 3.10 容器）首轮 ctest
4/5：`tests/test_regression_gate.py` 用了 `contextlib.chdir`（3.11+），
目标机的 3.10 没有该 API。**处置**：增加 3.10 兼容回退（3.11+ 仍走原生
实现）；修复后 22.04 / 3.10 与 26.04 / 3.14 均 5/5。同轮验证：12 项冻结
指标零偏差、N0 检测缓存与发布版 MD5 逐字节一致（4.5.4 的 dnn 与 MJPEG
解码两条风险路径解除），详见 [ubuntu2204_compat](../deploy/ubuntu2204_compat.md)。

### 信息项

- `--camera` 等参数用 std::stoi 无异常包裹，非法输入以未捕获异常终止
  （离线工具可接受，上车脚本注意传参）。
- 无 CRC 时姿态帧存在理论伪同步可能（载荷恰现 AA BB）；17 字节定长解析
  使其难以持续危害，开 CRC 后消除。
- ClockAligner 假设设备时钟频率恒定（中位数偏移）；长时漂移由上车试机
  清单覆盖。

## 4. 未验证项（诚实声明）

- MVS 真实 SDK 编译与运行：本轮为桩编译验证；真实 SDK 版本/ABI 核对、
  相机持久格式确认仍在上车清单。
- 一切实机行为（云台动力学、弹丸散布、USB 拔插、长时漂移）：按
  [on_robot_trial_checklist](../calibration/on_robot_trial_checklist.md) 执行。
- MVS 运行中拔插重连：grab 硬错误即退出（与串口自动重连不同），本轮未改；
  上车需确认相机供电/线缆可靠性，或接受"掉线即停 + systemd 重启"行为。

## 5. 复现方法

```bash
# ASan+UBSan
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-asan -j8
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=0 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=0
(cd build-asan && ctest --output-on-failure)
./build-asan/apps/autoaim_video video/3.avi --config core/config/default.yaml --out runs/asan_live --redetect
./build-asan/apps/autoaim_video video/4.avi --config core/config/default.yaml --out runs/asan_live --redetect
./build-asan/apps/autoaim_sim
./build-asan/sentry/sentry_robot --video video/3.avi --serial "" --max-frames 300
# 压力（Release，复用检测缓存）
./build/apps/autoaim_video video/4.avi --config core/config/video_reference.yaml \
  --out runs/eval_n0 --stress dropout=0.2,seed=42
```
