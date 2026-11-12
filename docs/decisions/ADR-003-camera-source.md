# ADR-003：CameraSource 抽象 + 曝光时刻戳，而非到处直用 VideoCapture

- 状态：已接受
- 背景：旧代码在串口收帧时刻给图像打戳并用相邻姿态均值对齐；在云台
  3 rad/s 转速下，每 1 ms 戳误差就是 0.17° 瞄准误差，且接收时刻戳的
  偏差恒为处理+传输延迟、方向固定。
- 决策：所有帧源（视频文件/UVC 设备/MVS 工业相机）统一走 CameraSource
  接口，出接口时携带曝光中心时刻戳与 hw_stamp 标志（软件戳不得冒充
  硬件对齐戳）；姿态侧用 PoseBuffer 在曝光时刻做 SLERP 插值。后端句柄
  一律 PImpl（VideoCapture、MVS 句柄都不进公共头文件；Video/Device 两源
  于第六轮审查后对齐 MvsSource 的既有写法）。
- 后果：离线评测与上车采集共用同一条管线，时间对齐误差有明确预算与
  标定手段（DelayCalibrator）；代价是新增相机后端要实现一个 Impl，
  且戳的可信度需要靠 hw_stamp 在数据层显式传递、不能默认相信。
- 备选与否决：各处直用 cv::VideoCapture + 接收时刻戳——旧实现，已实测
  为最大误差源，否决；只抽象 MVS、离线另走一套——评测与实车行为会
  分叉，否决。
- 证据：docs/design/timestamp_design.md、docs/design/attitude_timestamp_alignment.md、
  docs/calibration/camera_hardware.md。
