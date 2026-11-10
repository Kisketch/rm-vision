# 2026-11-10 代码审查后第一轮修复

起点：rm-vision/main `b51b68865832e06d2d1514244cb64c212897392a`。
本地分支：`fix/review-reliability`。目标是让参数实验可信、修复标定模式火控、收紧可选关键点输入的质量检查。

## 参考与取舍

- [chenjunnn/rm_auto_aim PnP](https://github.com/chenjunnn/rm_auto_aim/blob/main/armor_detector/src/pnp_solver.cpp)：对照四点几何与解算接口，本轮保留既有跟踪及弹道数值路径。
- [Ultralytics pose 输出说明](https://github.com/ultralytics/ultralytics/blob/main/docs/en/tasks/pose.md)：关键点有独立置信度，不能以目标框置信度替代四角点质量。
- [OpenCV calib3d](https://docs.opencv.org/4.x/d9/d0c/group__calib3d.html)：畸变顺序为 k1,k2,p1,p2,k3。旧标定来源未确认，本轮不重新排列默认参数，也不凭视频调到更好看的数值。

未引入外部源码或新增运行库。

## 修改

1. `apps/autoaim_video.cpp`：检测缓存协议升至 v3，签名增加自适应阈值、后端、ONNX 模型内容与路径、类别和推理/关键点门限。旧 v2 缓存会自动失效。不存在的 ONNX 文件仍按原来的传统检测 fallback 处理；将来该路径出现模型时会触发重检。
2. 检测参数的 `--sweep` 全部拒绝：refine_corners、channel_diff_threshold、diff_adaptive、diff_k/min/max。原实现只扫第二阶段，不能代表重新检测。请使用逐组 `--set`；有效签名变化会自动重检，必要时使用 `--redetect`。跟踪/PnP/弹道参数扫描保持支持。
3. `keypoint_detector.*`：新增 `detector.keypoint_min_conf`（默认 0.25），在 NMS 前拒绝低置信度角点、NaN/Inf、图像外角点、非正尺寸框和非凸/退化四边形。模型输出契约仍为四关键点、每点 x/y/conf。
4. `Detector`：敌方颜色更新同步至 ONNX 子检测器；setConfig 重建两个检测后端，使模型、分类器门限与后端选择生效。该调用可能重新加载模型，不应逐帧调用。
5. `sentry_robot`：延迟标定模式强制 fire=0；正常退出尝试发送一次 found=0/fire=0；补齐 `--help`。这不覆盖进程崩溃、断电或相机读帧阻塞，MCU 仍需要超时失效机制。
6. 扩展缓存集成测试及关键点异常输入测试，并把缓存测试加入 GitHub Actions。

## 本地验证

环境：Linux / GCC 15.2.0 / OpenCV 4.10.0 / Release。默认 BUILD_SENTRY=ON，未启用 MVS SDK。

- 构建通过；CTest 3/3；`python3 tests/test_cache.py build/apps/autoaim_video` 通过；`sentry_robot --help` 与 `git diff --check` 通过。
- 缓存测试覆盖同名视频、损坏缓存、自适应阈值、模型内容、ONNX 配置变化，以及所有受支持检测参数的无效 sweep 拒绝。
- 两套完整默认仿真表（核心配置及 sentry.yaml）：修改前后输出逐字节一致。
- 本地 3.avi（2209 帧）和 4.avi（3416 帧）分别用原版和修改版完整重检。检测文本、track.csv、yawdiag.csv、pairdiag.csv 均逐字节一致。

| 当前 core/config/default.yaml | 修改前虚拟命中率 | 修改后虚拟命中率 | 0.32s 预测误差中位数（前=后） |
|---|---:|---:|---:|
| 3.avi | 56.8% | 56.8% | 15.1 px |
| 4.avi | 64.4% | 64.4% | 13.2 px |

这两组使用当前非零畸变的默认相机配置，不等同于 N0 冻结口径的 67.5%/68.6%。本轮没有提升命中率；这些是可判定虚拟射击指标，不能代表实弹命中率。并发重跑用于结果对比，耗时不能作为严格性能基准。

串口 PTY 端到端复现：对 3.avi 前 250 帧注入零姿态/红色敌方元数据流，并启用 --calib-delay。旧版收到 250 个控制帧，其中 247 个 found 非零、76 个 fire 非零；修改版收到 251 个控制帧，247 个 found 非零、fire 全部为零，最后一个控制帧 found/fire 均为零。串口模拟测试不能替代实际 MCU 联调。

原始本地结果在 `runs/review-before/`、`runs/review-after/`、`runs/sim-*-*.log` 及 `runs/calibration-*.log`（runs 为 git 忽略目录）。

## 新视频与仍需数据的项目

收到视频后，使用独立输出目录保留修改前后结果，检查颜色/类别、遮挡与切板失败帧、跟踪保持和预测误差，并保存叠加画面。需要目标真值或人工标注才能报告检测准确率；仅非空检测帧比例不足以说明正确性。

ONNX 的图像友敌颜色判断尚未补齐：当前颜色同步修复仅使配置标签一致，不证明模型能区分友方。真实模型权重、类别与关键点顺序需另行对齐。PnP 正深度/重投影门控、MVS 接入、标定顺序及外参、实测弹速与延迟仍需后续验证。
