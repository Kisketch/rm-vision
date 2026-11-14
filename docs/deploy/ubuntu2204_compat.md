# Ubuntu 22.04 / OpenCV 4.5.4 兼容性静态审计（B-05）

目标机：GEEKOM IT13，Ubuntu 22.04（GCC 11、CMake 3.22、OpenCV 4.5.4）。
本仓库在 Windows + OpenCV 4.14、Ubuntu 26.04 + OpenCV 4.10 上验证过；22.04 未实机跑。

## 静态检查结果

- `cmake_minimum_required`：3.16（22.04 自带 3.22，OK）
- C++ 标准：C++17（GCC 11 支持 C++17/20 子集，OK）
- 用到的 OpenCV 模块：calib3d, core, highgui, imgproc, opencv, videoio
- 各模块在 4.5.4 中的可用性：core, imgproc, highgui, videoio, dnn, imgcodecs, calib3d 全部存在

## 需要留意但静态查不出的点

1. **DNN 后端**：4.5.4 的 `dnn` 对 ONNX 算子支持比 4.10 少；`mlp.onnx` 只是全连接，风险低。
2. **`solvePnPGeneric` + IPPE**：4.5.4 有，但返回解的顺序可能与新版本不同——我们的代码取的是重投影误差最小解，不依赖顺序。
3. **`cv::VideoCapture` 的 MJPG 解码**：22.04 的 FFmpeg 版本较旧，若打不开 3.avi/4.avi，装 `libopencv-videoio-dev` 或改用图像序列。
4. **CMake 4.x 语法**：本仓库只用 3.16 起就有的命令，22.04 的 3.22 可以配置。

## 结论

- 静态层面没有发现必然不兼容的用法；风险集中在 DNN 算子与视频解码两条运行时路径。
- 交付前必须在 22.04 上跑一遍：`cmake -S . -B build && cmake --build build -j` + `autoaim_video` 两条视频 + `autoaim_tests`。
- 若 4.5.4 的 DNN 读不了 ONNX，退路是把分类头改成 OpenCV MLP 的 XML 格式，或在 22.04 上装 4.8 的官方包。

*扫描了 11 个源文件；本审计是静态的，不能替代实机编译。*

## 实测验证（2026-11-14，容器环境）

静态审计要求的"交付前必须在 22.04 上跑一遍"已完成。环境：Ubuntu 22.04
容器（Docker，4 vCPU 配额），GCC 11.4.0，CMake 3.22.1，OpenCV 4.5.4
（发行版包），Python 3.10.12——与 IT13 目标机的发行版工具链一致
（容器非 IT13 硬件，性能数字不代表目标机）。

- **构建**：Release 0 警告。
- **ctest**：首轮 4/5，`regression_gate_contract` 失败——测试用了
  `contextlib.chdir`（Python 3.11+ API），22.04 的 3.10 没有。**已修复**：
  tests/test_regression_gate.py 增加 3.10 兼容回退（3.11+ 仍走原生实现）。
  修复后 22.04 / Python 3.10 与 26.04 / Python 3.14 均 5/5 通过。
- **全量重检（--redetect，两套配置 × 两视频）**：12 项冻结指标逐项偏差
  0.0（N0：67.5% / 68.6%、14.3 / 13.3 px、10.6 / 10.1 hits/s；实机：
  58.1% / 66.9%、15.2 / 12.4 px、9.1 / 8.8 hits/s），两套门禁均 PASS
  （仿真均值 73.6%，按机器相关口径 74.5±3 通过；急横移专项 11.4% 逐位一致）。
- **检测缓存**：N0 口径新检测与发布缓存 **MD5 逐字节一致**
  （3.avi `fc8c9beaea1b873f6204705d2f25d185`、4.avi `cae8f2d5361fe853ae5b7272136c99bb`）。
  静态审计担心的两条运行时路径全部解除：4.5.4 的 dnn 正常读取 mlp.onnx，
  FFmpeg 正常解码 MJPEG 视频，且检测输出与 4.13 参考环境完全一致。
- **检测耗时**：2.1–2.2 ms/帧（1440×1080，容器 4 vCPU 配额，单线程口径；
  快于参考环境的 3.3–3.5 ms，属 CPU 差异，非优化）。

结论：22.04 / OpenCV 4.5.4 / GCC 11 / Python 3.10 目标环境**实测可用**，
无需升级 OpenCV 或 Python；唯一的部署阻塞（测试的 3.11 API）已修复。
