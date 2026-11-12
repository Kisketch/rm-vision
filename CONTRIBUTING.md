# 贡献指南

欢迎。本仓是 RoboMaster 视觉自瞄的单仓：共用核心（core/）+ 哨兵模块
（sentry/）+ 离线应用（apps/、sentry/apps/）。动手前先读三样东西：
[README](README.md)（项目全貌）、[docs/INDEX.md](docs/INDEX.md)（文档
分类索引）、[docs/decisions/](docs/decisions/)（架构决策记录 ADR-001～
003：为什么是单核心、为什么整车 EKF、为什么 CameraSource 抽象）。本
指南只写硬规则；设计理由在 ADR 和各专题文档里。

## 目录与依赖方向（硬约束）

- 依赖只能单向：`apps → sentry → core`。core 永远不包含 sentry 的头
  文件，sentry 永远不反向被 core 依赖（ADR-001）。哨兵专属逻辑（多目
  标管理、串口、前哨站、ROI）放 sentry/，不要塞进 core 凑合共用。
- 头文件路径 = 命名空间 = 模块归属，三者必须表达同一件事：
  core 的头在 `core/include/autoaim/`（namespace autoaim），哨兵的头在
  `sentry/include/sentry/`（namespace sentry），串口在
  `sentry/include/sentry/io/`（namespace sentry::io）。
- 不建 `common/`、`utils/`、`manager/` 这类大杂烩目录。新文件先问一句：
  它属于哪个模块、被谁依赖？答不上来就别建。
- 相机/串口等硬件后端：接口进公共头，SDK 头文件和句柄只许出现在实现
  文件里（PImpl，见 camera_source.cpp 中三个 Source 的写法）。

## 命名

- 文件：`snake_case.hpp/.cpp`；类：`PascalCase`；函数/方法：
  `camelCase`；成员变量：尾下划线 `name_`；常量：`kName`。
- 命名空间全小写：`autoaim`、`sentry`、`sentry::io`。
- 名字要带语义代价：只做一部分事的函数不能起全称的名字。本仓的教训是
  `Config::save()` 实际只写三个小节，已在第六轮改名为
  `saveTuningOverrides()`——新接口命名时先想清楚它到底承诺什么。

## 代码规则

- **公共头文件必须自包含**：每个 `.hpp` 要能独立编译，不许靠传递包含。
  用到的标准库类型自己 include（`std::array` → `<array>`，`std::string`
  → `<string>`）。CTest 的 `header_self_containment` 用例
  （tools/check_headers.sh）会逐个头文件验证，CI 里挂了会直接红。
- **有状态的操作不许标 const**：方法会改对象运行状态（滞回、选择记
  忆、变化率等）就去掉 const，更不许用 `mutable` 把状态藏起来。无状态
  的方法保持 const（如 DirectAimer::aim、PoseSolver::solve）。
- **资源用 RAII**：后端句柄、文件、串口 fd 一律由对象持有并在析构释
  放；裸 `void*` + 手工 new/delete 只许出现在第三方 C SDK 的接口边界
  （如 MVS 句柄），不许进我们自己的类成员。
- **配置在构造时固定**：对象构造后不许从外部改它的配置引用；运行时调
  整走显式 setter（如 SentryVision::setEnemyColor/setRoiEnabled），每
  个 setter 只改它名字说的那一件事。
- 格式基线是根目录 `.clang-format`（4 空格、100 列、指针贴类型），
  `.clang-tidy` 是建议性检查集；提交前至少保证你改的文件不引入新的
  格式漂移。

## 构建与测试

```bash
cmake -S . -B build            # 开关：BUILD_SENTRY / BUILD_APPS /
                               # BUILD_TESTING（CTest 标准，默认全开）
cmake --build build -j
(cd build && ctest --output-on-failure)
```

CTest 五项，提交前必须全绿：`autoaim_tests`（core 套件，按模块拆在
tests/test_*.cpp）、`sentry_tests`、`serial_port_tests`、
`regression_gate_contract`（回归门的 PASS/FAIL 契约）、
`header_self_containment`。
新测试放进对应模块的测试文件；没有对应文件就新建一个并登记进
tests/CMakeLists.txt 和 test_main.cpp 的调用表——只写文件不登记，等于
没测。

## 评测口径纪律（本仓最容易踩的坑）

- 两套配置口径永不混用：`core/config/video_reference.yaml` 是**冻结的
  N0 参考口径，一个字节都不许改**；`core/config/default.yaml` 是实测
  口径。两套口径在同一视频上差约 10 个百分点是来源差异，不是回归。
- 改动算法后，按 BASELINE.md 的规矩复跑冻结数字（条件/可验证/保守三
  口径一起报），并说明与冻结值的差异；复现不了就先查口径，别改基线。
- 占位值必须标注为占位（如 42 mm 弹速 16 m/s 是占位、待上车实测），
  不许把估计值写成实测值。
- 结论要有 A/B 或消融支撑：本仓有多个"仿真里有效、真实视频退化"的
  否决案例（sigma 门控、NIS 自适应 q），新机制先在真实数据上测分布，
  再谈默认值。

## 文档纪律

- 新增/删除/移动文件，必须同步更新 docs/INDEX.md 与
  docs/deploy/deployment_map.md 里的文件计数，并给新文档在 INDEX 分类表里
  登记。计数对不上就是未完成的提交。
- 新文档按用途放进 docs/ 的子目录（design / studies / calibration /
  deploy / reviews / project，判断标准见 docs/README.md），不要再平铺到
  docs/ 根目录；根目录只放入口页。移动文档时全仓链接要一起改，提交前
  确认没有失效的相对链接。
- README.md 与 README.zh-CN.md 是同源双语版：改一版，必须同步另一版
  （数字、表格、代码块保持逐项一致，锚点按各自语言本地化）。
- 新的设计决策写一篇 ADR 进 docs/decisions/（背景/决策/后果/被否决
  的备选，四段齐全），不要只把理由留在提交信息里。
- 历史文档是当时状态的记录，不回头改数字；结论变了就新增一篇并在
  INDEX 里标注关系。

## 提交

- 一个提交一件事；标题说清做了什么，正文写为什么和验证证据（跑了哪
  些测试、冻结数字是否复现）。
- 推送到 main 前，先确认本地树与远端无未同步差异（本仓有过"从旧本地
  副本整文件推送、覆盖掉队友新提交"的事发记录，教训在
  docs/reviews/engineering_review_r6.md 与历轮审查文档里）。
- 涉及电机/发射机构的改动：发射默认禁用，先确认安全再谈功能。
