---
name: serial-check
description: 哨兵串口链路专项检查（robot-vision-aim 的 sentry/ 模块）。用户说串口、serial、姿态流、MCU 联调、CRC 时使用。离线查协议与代码，上车项给出可勾选的验证步骤与判据，不把离线结论冒充上车结论。
allowed-tools: Read, Grep, Bash(./build/tests/serial_port_tests *), Bash(./build/sentry/sentry_robot *), Bash(ctest *)
---

# 串口专项检查（sentry/ 模块）

## 离线可查
- termios raw 设置完整性（ICRNL/INLCR/IGNCR 等），用含 0x0D/0x0A 字节的构造帧跑 serial_port_tests 验证无字节改写
- fd 竞争：grep 所有 close/reopen 路径，确认同一把锁保护
- 协议帧长/字段偏移与 17B 进、21B 出定义逐字节核对；CRC-8 计算与 byte 2 位置
- `--require-crc` 默认关的断言测试
- 串口代码位置：`sentry/include/io/serial_port.hpp`、`sentry/src/io/`；不要按旧两仓路径找文件

## 上车验证清单（输出给用户逐项勾，离线不要代勾）
- [ ] 姿态流实际频率 (Hz) 记录
- [ ] 默认参数下 attitude misses ≈ 0（计数器日志）
- [ ] reconnects = 0（长稳运行计数）
- [ ] USB 拔插后自动重连并恢复姿态流
- [ ] MCU 固件是否已填充 byte 2 —— 未填充前禁止开 --require-crc
- [ ] 先跑 `--calib-delay` 标定帧戳—姿态时间差，再复测命中率

## 输出
离线结论与上车清单分开两段；上车数据带回后走 /calibration-writeback 或更新试机清单状态。
