# 数据图表：代码的强项与弱项

> 本页图表由 `tools/make_charts.py` 生成（数据全部抄自仓库内已核验
> 来源，逐图标注；基线变动时改脚本数据重跑即可）。读图原则：
> 强项和弱项都画出来——命中率的天花板在哪、哪种工况最脆弱，
> 比只报最好数字更有用。

## 1. 命中率：整车 EKF 相对点 KF 的代差

![Hit rate by configuration](charts/hit_rates.svg)

- **强项**：整车 EKF（条件率 67.5/68.6）相对点 KF（29.6/43.1）是
  代差级优势，哨兵配置在 4.avi 达 71.9。
- **注意**：实测口径（57.3/66.9）低于 N0 参考口径不是退化——测试
  视频非队伍相机所摄，口径差异见 changes_vs_legacy.md 第 9 节。
- 出处：BASELINE.md、docs/MEASUREMENTS.md。

## 2. 赛场压力：慢目标稳、快目标怕丢帧、最怕时间戳抖动

![Field stress](charts/field_conditions.svg)

- **强项**：慢速目标（3.avi，ω≈2.7 rad/s）丢帧 40% 仍保 66.3。
- **弱项**：快速目标（4.avi，ω≈8.4 rad/s）对丢帧与降采样敏感
  （40% 丢帧 55.3、12.5 fps 55.2）；最脆弱的是时间戳质量——
  10 ms 抖动直接掉到 49.0。这就是时间对齐与延迟标定列为
  上车第一优先级的原因。
- 出处：docs/FIELD_CONDITIONS_2026-11-11.md（合成注入，非实测）。

## 3. 计算耗时：解算与检测都已降一个量级

![Latency](charts/latency.svg)

- **强项**：位姿解算 1302→72 µs、弹道 109→40 µs、检测
  ~7.0→~1.6 ms/帧，整条管线有充足的帧率余量。
- 出处：docs/hardening.md、docs/roi_detection.md。

## 4. 否决案例一：NIS 自适应 q——仿真有效、真实全退化

![NIS adaptive q study](charts/nis_adapt.svg)

- **弱项（方法论）**：同一机制在仿真 +3.5/+7.0 pp、真实视频
  四格 −0.6~−4.5 pp。滤波器自报统计量的尺度在仿真与真实间
  漂移，固定阈值机制不可直接迁移——已否决出厂，补丁留档。
- 出处：docs/nis_adapt_study.md。

## 5. 否决案例二：sigma 门控——不确定度抓不住最弱模式

![Sigma coverage](charts/sigma_coverage.svg)

- **弱项**：预测误差是高峰度混合分布，1σ 过覆盖（83.0 vs 名义
  68.3）、2σ 欠覆盖（89.3 vs 95.4），无单一标度可校准；而最弱
  的纯平移模式 1σ 覆盖仅约 16%，且其 sigma 与良好模式重叠，
  门控原理上抓不住。sigma 只可作序数风险分数，门控保持关闭。
- 出处：docs/sigma_coverage_study.md。

## 6. 剩余误差在哪：预测，不是执行

![Error budget](charts/error_budget.svg)

- **强项**：云台执行误差中位仅 8.1 px，执行链是可靠的。
- **弱项**：瞄准预测误差中位 16.8 px，且命中率随它陡降
  （<10 px 时 91.8%，25–60 px 时 10.3%）；角速度估计误差
  σ(ω)=0.69 rad/s 是当前第一瓶颈，后续优化应继续投向
  运动建模与时间对齐，而非执行端。
- 出处：CHANGELOG.md（Stage K 诊断）。
