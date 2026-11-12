# 用户视频完整试跑：2026-11-10

本轮基于 `fix/review-reliability` 工作树（起点 b51b688，第一轮可靠性修复后），从用户提供目录 `/home/lgnatius/测试视频/` 直接重检。没有在本轮修改检测/跟踪参数或模型权重。

## 输入与边界

| 输入 | SHA-256 | 分辨率 / 帧率 | 完整帧数 |
|---|---|---|---:|
| 3(1).avi | 87c61f917059a5e39343afc28358a2d7fafb87a81acca2431b3695292b001277 | 1440×1080 / 25 FPS | 2209 |
| 4(1).avi | 7478973c072a9efaacf193f7998c4998de1a05388c9cf1a37b20ec02ed7423a8 | 1440×1080 / 25 FPS | 3416 |

两文件与原工作区 3.avi/4.avi 字节级一致，也与仓库历史回归素材的 SHA 一致。这是重复基准，不是新场景的泛化测试。两段都为室内暗场红色 3 号目标，无逐帧人工真值，无实弹数据。测试使用传统灯条+MLP 后端，不覆盖可选 ONNX 检测。

## 控制变量

- current：`core/config/default.yaml`，当前导入的内参/非零畸变、22 m/s。
- reference：只把 current 相机改成历史离线假设 fx=fy=2400、cx=720、cy=540、畸变为零，仍用 22 m/s。其它参数完全相同。配置快照 `runs/user-video-test/reference_config.yaml`。
- n0：在 reference 的基础上把 17 mm 弹速设为 25 m/s，复现历史 N0；新增固定入口 `core/config/video_reference.yaml`。

current 全片使用 `--redetect`。后两组共享同一检测缓存，只比较位姿、跟踪、弹道；缓存身份检查正常，三个配置下检测文本相同。reference 和 n0 的相机及弹速都是离线复现假设，不是此次测量值，不能自动用于实车。

## 实测

| 指标 | 3(1).avi | 4(1).avi |
|---|---:|---:|
| 有检测帧 | 2132 / 2209（96.5%） | 3343 / 3416（97.9%） |
| 无检测帧 | 77 | 73 |
| current TRACKING 状态占比 | 87.1% | 83.7% |
| reference / n0 TRACKING 状态占比 | 93.3% | 93.2% |
| current 有检测但没有关联成功的帧 | 199 | 468 |
| reference / n0 有检测但没有关联成功的帧 | 69 | 152 |
| current 虚拟命中率 | 56.8%（764 / 1345） | 64.4%（1154 / 1793） |
| reference 虚拟命中率 | 62.1%（870 / 1400） | 63.2%（1258 / 1989） |
| n0 虚拟命中率 | 67.5%（937 / 1388） | 68.6%（1374 / 2004） |
| current 虚拟命中 / 秒 | 8.6 | 8.4 |
| reference 虚拟命中 / 秒 | 9.8 | 9.2 |
| n0 虚拟命中 / 秒 | 10.6 | 10.1 |
| current 0.32s 预测中位误差 | 15.1 px | 13.2 px |
| reference / n0 0.32s 预测中位误差 | 14.3 px | 13.3 px |

命中率分母是可判定的虚拟射击次数。有检测帧比例不是 precision/recall。关联失败帧包括初始化、重初始化和正常门控拒绝，不能全部称为算法错误。两组相机参数改变了关联和开火样本，命中率和预测中位数应结合跟踪率、hits/s 及样本数解读。

N0 复现仓库当前迁移记录的 67.5%/68.6%。这说明旧基线与当前默认值的差异主要来自配置口径，不能把改变配置后数字更高称为本轮算法改进。reference 的第 4 段命中率还低于 current，亦说明单一指标不足以选参数。

当前环境 Linux / GCC 15.2 / OpenCV 4.10 / Release。此次原始完整重检 detector 平均约 2.58 ms/帧；pose 约 0.037/0.039 ms/帧；tracker+aimer+judge 约 0.057/0.048 ms/帧。后两项是离线批处理均摊，不包括解码、采集、渲染、串口和排队，不能换算为实车延迟或实车 FPS。

## 抽查的失败案例

- 3(1).avi 937–968 帧：反复进入 TEMP_LOST。947–956 连续 10 帧未关联，其中 950 帧没有完整装甲板检测。画面可见遮挡/侧向灯条，完整灯条配对不足；945 帧虽有 2 个检测，仍被关联拒绝。这里同时有观测缺失与位姿/关联问题，不能只放宽门限。
- 4(1).avi 1472–1492 帧：连续 21 帧未关联。1475 帧前景脚部经过目标区域；1486 帧已进入 LOST，1492 帧出现检测后仍在 DETECTING。对照叠加画面保留了这一完整过程。
- 相同检测下 current 的关联成功率明显低于 reference，下一步应优先确认相机畸变顺序和坐标约定，而不是用两段旧视频反复调 Q/R。

## 交付与复现

本地原始数据：`runs/user-video-test/`。

- `current/`、`reference/`、`n0/`：每组完整 report、检测缓存、track/yawdiag/pairdiag CSV。
- `current3.log`、`current4.log`：此次完整重检的检测耗时和输出。
- `preview/tracking_comparison.mp4`：3 号 850–1000、4 号 1425–1525 帧，左=current，右=仅相机改成 pinhole / 弹速同为 22。视频明确不是 current 对 N0。
- `preview/comparison.jpg`：950 和 1475 帧的局部对照；绿=观测，青=模型板，红圈=预测瞄准点。图中预测点并非实际弹着点。

```bash
./build/apps/autoaim_video '/home/lgnatius/测试视频/3(1).avi' --out runs/user-video-test/current --redetect
./build/apps/autoaim_video '/home/lgnatius/测试视频/4(1).avi' --out runs/user-video-test/current --redetect
./build/apps/autoaim_video '/home/lgnatius/测试视频/3(1).avi' --config core/config/video_reference.yaml --out runs/user-video-test/n0
./build/apps/autoaim_video '/home/lgnatius/测试视频/4(1).avi' --config core/config/video_reference.yaml --out runs/user-video-test/n0
```

本轮新增离线参考配置与此报告，保留实车默认配置。相机标定来源、真实友敌混合/蓝色场景、快速平移、多目标及真实通信延迟仍需独立数据验证。
