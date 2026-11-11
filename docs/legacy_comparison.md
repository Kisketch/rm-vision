> **出处（rm-vision 归档说明）：** 本报告原为最终算法交付包的 `COMPARISON.md`，对比旧代码 `auto-aim-master` 与合并前的新算法（rm-autoaim @ `0c9874e` / rm-sentry-integration @ `b96f5c1`）；两仓现已合并为本仓，报告结论与补齐清单仍适用。

当前相机采集实现、旧系统配置和待实测项目见[相机配置核对](deployment_map.md#相机配置核对)。

# 新旧代码对比报告（auto-aim-master ↔ 最终算法）

对比对象：旧代码 `auto-aim-master`（用户 2026-11-10 提供，即压缩包原件）与本包交付的新算法（`rm-autoaim` @ `0c9874e`、`rm-sentry-integration` @ `b96f5c1`）。
结论先行：**两套代码不是同一条演进线**。旧代码是一台步兵/英雄车的完整实车程序（相机+串口+自瞄+打符，单可执行 `action`）；新代码是离线流水线导向的重构（检测→PnP→EKF→弹道→评测，带回归测试），并在审查后补了时间戳对齐、弹速可配、唯一 ID、CRC 等旧代码没有的正确性改进。旧代码里新代码缺的关键部分已全部补进本包（见第三节）。

---

## 一、新代码改了哪些、可能有什么用（对照旧代码逐项）

| 环节 | 旧代码做法 | 新代码做法 | 用途/收益 |
|---|---|---|---|
| 时间戳对齐 | 相机帧用采集线程的接收时刻；姿态在 `SerialPort::Query` 里把前后两帧**简单平均**（不是按时间比例插值），帧时刻用处理时刻 | PoseBuffer 四元数 SLERP 按**曝光时刻**对齐姿态，查不到姿态的帧整帧丢弃；ClockAligner 做时钟偏移估计 | 云台高速转动时（3 rad/s）1 ms 时间误差≈0.17° 瞄准误差；旧做法在转速高时系统性偏，新做法把对齐误差压到微秒量级（合成验证 3e-8 rad） |
| 目标身份 | `ChooseProcess` 只按数字 `armor.number` 锁定单个目标 | deviceId = 颜色+数字+板型，全程关联 | 旧做法遇到同号异色/异板型（如两队都有 3 号、英雄大板 1 号）会把别的车融进同一条轨迹 |
| 弹速 | `#define bullet_speed 22` 硬编码（Base.h） | 配置分档（17/42mm）+ `setMeasuredBulletSpeed()` 运行时注入实测值 | 2026 规则限速 25 m/s 且超速锁枪，换枪/换弹/裁判系统读数变化时不用改代码重编译 |
| 检测 | 灯条 + MLP 数字分类（传统路线） | 传统路线（默认，同一 mlp.onnx，md5 一致）+ 可选 YOLO 四关键点 ONNX 后端（加载失败自动回退） | 关键点后端是升级通道：换模型即可，不动下游；传统路线保底 |
| 位姿解算 | IPPE PnP + yaw 暴力扫描 ±70° 压重投影误差（ArmorSolver） | IPPE 双解择优 + yaw 搜索优化（同一思路）+ 畸变实测值已回填 | 思路一致；新代码把 yaw 优化做成了可关的配置项并纳入回归 |
| 预测/跟踪 | ArmorEKF（Eigen）+ Predictor 整车模型，Aim 单目标 | 11 状态整车 EKF（Singer 阻尼、NIS 自适应 Q、各向异性 R）+ 多目标/前哨站（哨兵仓） | 新 EKF 在仓库自带虚拟射击评测里有回归门禁（BASELINE），改参数先过评测 |
| 火控 | **Planner 五次多项式轨迹规划**（ArmorPlanner）：按 max_yaw_acc 规划云台运动，输出 manage_yaw + yaw_vel | Aimer 直接解算 + 输出 yaw_rate/pitch_rate 指令角速度（B-01）供下位机前馈 | 两条不同路线：旧的是"规划出一条云台能跟上的轨迹"，新的是"把角速度给电控让它自己跟"。旧 Planner 是新火控缺的一课，已随旧代码完整保留（见第三节），是否移植进新 Aimer 需要上车对比数据再定 |
| 串口协议 | 17B 入（AA BB+yaw/pitch/roll+color+armor_or_buff）/ 21B 出，与哨兵仓同族 | 哨兵仓同协议 + 姿态帧可选 CRC-8（第 2 字节，默认关） | 旧协议无任何校验，串口误码会直接变成错误姿态角；CRC 是协议层补强，需 MCU 配合填字节 |
| 标定 | text.yaml 里的**实测值**（fx 2399.58 等，见第四节） | 原为假设值（fx=2400、零畸变）；**本次已把旧代码实测值回填** | 见第四节，这是本次对比最有价值的发现之一 |
| 工程化 | 单 CMake、全局 include、无测试目标（Test/ 是仿真函数，未被 main 调用） | 每仓带完整测试套件（ctest）、配置与代码分离、文档齐全 | 新代码的改动有回归保护；旧代码改参数只能上车试 |

## 二、旧代码有、新代码缺的关键部分（缺失清单）

1. **打符（能量机关）全流水线**：`Buff/`（BuffDetect/BuffTarget/BuffSlove/BuffPlan）+ `AutoBuff` + 专属模型 `mlp_bh4.onnx`（代码实际引用；bh/bh1 为旧迭代）+ `plot_buff.py`。新代码完全没有打符。
2. **实车硬件层**：`io/hikrobot`（海康 MVS 相机真实驱动：USB 复位、守护线程、曝光/增益设置、165 fps）与 `io/SerialPort`（/dev/ttyACM0、21B 协议收发、断线重连），以及 `Backbone/main.cpp` 的实车主循环（AimStateMachine 0.5 s 保持）。新主仓只有 CameraSource 的 MVS 骨架（未编译验证），没有这台车的串口发送。
3. **轨迹规划火控 Planner**（见上表）。
4. **实测标定与参数**（text.yaml + Base.h，已回填，见第四节）。
5. **额外模型**：`mlp5_9.onnx`、`outpost.onnx`（与 mlp.onnx 不同文件，前哨站专用）、`mlp_bh.onnx`、`mlp_bh1.onnx`——均未被当前旧代码引用，属迭代备件；新仓只带 mlp.onnx。
6. **实测素材**：`armor.webm`（注意：它是调试过程的**录屏**，不是相机原始流，见第五节）、`circle_with_sector_labels.jpg`（符的扇叶标号参考图）。

## 三、补充方式（本包 v2 的处理）

- **旧代码整树保留**：本包新增 `auto-aim-legacy/`，含上述全部模块、6 个模型、text.yaml、armor.webm、Test 仿真与 tools，未删改任何算法代码（配置/代码共两处补齐：text.yaml 键名，见第五节第 1 条；BuffDetect 的 ROI 越界钳制，见第五节第 2 条——均为「按原样跑不起来」级别的修复，算法逻辑未动）。它在队伍机器上按原 CMake 构建出 `action` 即可直接上车，依赖：OpenCV4、Eigen3、yaml-cpp、fmt、spdlog（代码用了但 CMake 没写，靠系统装着）、libusb、MVS SDK（/opt/MVS）、Ceres（CMake 写了 REQUIRED，但**代码里零调用**，属闲置依赖）。
- **实测参数回填新主线**：`rm-autoaim/config/default.yaml` 已更新并推送 GitHub（提交 `0c9874e`）：内参/畸变换实测值、17mm 弹速 25→22。回填后主仓测试仍全绿、仿真正常出表。
- **未强行移植的部分**（说明原因）：Planner 与 Buff 没有硬塞进新架构——两者都深度绑旧的类型系统（Armor/KinematicModel/PowerRune）和 Eigen/yaml 依赖，移植等于重写，必须配合上车数据验证，不是"补文件"能保证正确的。它们以可编译的完整形态保留在 legacy 树里，新架构需要时按模块搬（Buff 的几何点表、Planner 的五次多项式与加速度约束是最值得搬的两块）。

## 四、从旧代码找回的实测数据（已回填）

| 参数 | 旧代码值 | 原新代码值 |
|---|---|---|
| fx / fy | 2399.5837 / 2398.5972 | 2400 / 2400（假设） |
| cx / cy | 688.7026 / 533.4234 | 720 / 540（图心假设） |
| 畸变（5 个） | 0.002593, 0.048045, 0.293334, 0.000164, 0.000193 | 全 0（错误假设） |
| 相机→云台外参 | R 为轴置换矩阵，t = [0.10, −0.05, 0] m | 新 CameraModel 尚不能表示平移杠杆臂（待办） |
| 17mm 弹速 | 22 m/s（Base.h 硬编码） | 配置默认 25 → 已改为 22 |
| Planner | predict_time 0.075 s，max_yaw_acc 20 | aimer.latency 假设 0.06 s（不同量纲，仅记录参考） |
| 补偿角 | yaw +0.5°，pitch −0.5°；打符 pitch 9°，yaw −0.8° | 新代码无对应补偿项（待办） |

两个必须上车确认的点：
- **畸变参数顺序**：旧 yaml 的存法大概率是 (k1, k2, k3, p1, p2)（第三个 0.293 是 k3 才合理），但旧代码把整段向量原样喂给 solvePnP（按 OpenCV 的 (k1,k2,p1,p2,k3) 解释）。本次回填也是原样复制以保持与旧机器行为一致；请对照你们的标定脚本输出顺序确认——若确为 (k1,k2,k3,p1,p2)，新旧代码都应把第 3/5 位对调才是物理正确值。
- **装甲板几何不一致**：旧 Solver 的灯条端点间距为大板 0.230×0.050 m、小板 0.130×0.050 m；新代码是 0.225×0.055 / 0.135×0.055 m。两套检测器输出的角点定义不同（灯条端点 vs 板面角点），各自自洽，但**不可混用**：新代码上车前要用新检测器的角点实测一次板面尺寸核对。

## 五、旧代码里发现的问题（顺带指出）

1. **text.yaml 与 AutoBuff 对不上（已在本包补齐）**：`AutoBuff::readYAML` 读 `Buff.compensate_pitch/compensate_yaw`，而 shipped 的 text.yaml 里只有 `Buff.pitch/yaw`，按原样构造 AutoBuff 会抛 YAML bad conversion——打符模式根本起不来（这也解释了 main.cpp 里模式切换代码为何是注释状态）。本包 legacy 树的 text.yaml 已补上两个键（值取原 pitch/yaw），代码未动。
2. **BuffDetect 硬编码帧尺寸（包内已修复）**：`BuffDetect.cpp:150` 写死裁剪 `Rect(180, 0, 1080, 1080)`，只适用于 1440×1080 输入；喂其他分辨率会直接抛 OpenCV 异常终止进程（本次验证实测复现）。本包 legacy 副本已改为把 ROI 钳制到实际帧范围（1440×1080 时逐像素不变），复验三种尺寸下打符全链均跑通；即便如此，换相机/分辨率时符的几何标定仍按原尺寸假设，需另行核对。
3. **armor.webm 是录屏**：画面里是旧程序自己的调试窗口（带它当时的检测叠加：4 号板、X:2.60 Yaw:−24.0）和 Motion State 窗口。用它离线复跑检测器检出为 0（画面暗、压缩、带叠加），不能当回归素材；回归请录相机原始流。
4. **Ceres 闲置**：CMake `find_package(Ceres REQUIRED)` 但全仓零引用，徒增部署依赖，可删。
5. **spdlog 未声明**：`tools/logger.hpp` 用 spdlog 但 CMake 没 find，靠环境里恰好装着才编得过，换机器会断——本包构建说明里已注明。
6. **YamlPath 写死** `/home/perseverance/auto-aim/text.yaml`，换机器/换用户名即哑火，建议改相对路径或参数传入（新主线已用相对 config 解决）。

## 六、本次验证记录（2026-11-10）

| 对象 | 验证 | 结果 |
|---|---|---|
| rm-autoaim @0c9874e（回填后） | CMake 全量构建 + autoaim_tests + autoaim_sim | 构建通过；**ALL TESTS PASSED**；仿真正常出表 |
| rm-sentry-integration @b96f5c1 | CMake 全量构建 + ctest | 构建通过；**3/3 passed** |
| auto-aim-legacy（旧代码整树） | 按原 CMake 全量构建（本机用 .deb 解压依赖前缀 + MVS 桩头/桩库 + Ceres 空配置占位，仅因本机无 SDK/系统包，队伍机器不需要） | **全部编译单元通过，`action` 链接成功** |
| 旧装甲流水线运行 | ArmorDetector 对 armor.webm 抽帧（44 帧，含裁剪版）+ Slover 解算 | 无崩溃跑完；检出 0（素材为录屏，原因见第五节第 3 条） |
| 旧打符流水线运行 | BuffDetector→Solver→BigTarget→Planner 分级探针 + AutoBuff::AutoAim 整链（1440×1080 帧与空帧） | 全部跑通，输出 21B 控制帧；无符时优雅返回 nullopt |

