# 第六轮审查处置：工程化视角（2026-11-11）

本轮审查的评价标准与前五轮不同：不评算法是否有效、不评数据是否足够，
而是把本仓当作一个准备长期维护、多人协作、持续扩展的 C++ 工程来审查，
参照对象为 Ceres Solver（可复用 C++ 库工程）与 PX4（模块化机器人系统）
的工程原则。处置原则与历轮一致：每条主张先在源码里核验，属实才改，
不属实或代价不对的写明理由缓办；不为了"看起来像顶尖仓库"增加无用目录。

## 核验结论（主张 → 判定 → 证据）

| # | 主张 | 判定 | 证据 |
|---|---|---|---|
| 1 | TargetManager 在 `sentry/include/autoaim/target.hpp` 且用 `namespace autoaim`，归属不一致 | 属实 | 原文件 15 行 namespace autoaim、17 行 class TargetManager |
| 2 | `RobotAimer::aim()` 与 `TargetManager::select()` 为 const 却经 mutable 成员改状态 | 属实 | aimer.hpp 原 86–88 行三个 mutable 成员；target.hpp 原 59 行 mutable last_selected_id_（PointAimer 的 rate_ 同病） |
| 3 | `SentryVision::mutableConfig()` 暴露内部配置、与已构造子对象不同步 | 属实 | sentry.hpp 原 60 行；且全仓无任何调用点，子对象各持构造时副本 |
| 4 | `Config::save()` 名为保存实为部分字段 | 属实 | 原实现只写 camera（5 字段）/tracker（6）/aimer（3），load 却读八大类；且全仓无调用点 |
| 5 | CameraSource 用 `void* cap_` 手工管理 VideoCapture | 属实（限 Video/Device 两源） | camera_source.hpp 原 52/73 行；MvsSource 本就已是 PImpl，是仓内现成范式 |
| 6 | 根 CMake 只有 BUILD_SENTRY 一个开关，apps/tests 无条件构建 | 属实 | 原根 CMakeLists 全文 27 行，仅一个 option |
| 7 | autoaim_core 把编译选项与宏设为 PUBLIC | 部分属实 | -Wall/-Wextra 确为 PUBLIC（应改）；但 AUTOAIM_ROOT 被 apps 与 tests 直接使用（autoaim_video.cpp:275、test_sentry.cpp:544），必须 PUBLIC；AUTOAIM_WITH_MVS 定义被 sentry_robot.cpp 与 camera_source.hpp 的 #ifdef 依赖，亦须 PUBLIC |
| 8 | test_main.cpp 超千行、单文件 | 属实 | 1096 行、25 个测试函数 |
| 9 | CI 未跑 test_regression_gate.py | 属实 | ci.yml 只有 test_cache.py 一步 Python 测试 |
| 10 | 无 .clang-format/.clang-tidy/.editorconfig | 属实 | 根目录清单核对 |
| 11 | pose.hpp 用 std::array 无 <array>、aimer.hpp 用 std::string 无 <string> | 属实 | 两文件原 include 列表核对，靠传递包含才编得过 |
| 12 | sentry.hpp 关于 aimer_ 持有 ballistics_ 引用的注释过时 | 部分属实 | aimer_ 子句过时（Ballistics 已按值持有，见 v9 审计的悬垂引用修复）；pose_/manager_ 持有 camera_ 引用子句仍属实 |
| 13 | core 还混有 IO/评测/绘图，install/export 缺失，docs 平铺无 ADR | 属实（方向性） | 见缓办节 |

## 已修复（本轮提交）

**API 语义（P1）**
- `RobotAimer::aim()` 两个重载、`PointAimer::aim()` 去 const，三个 mutable
  状态成员去 mutable 化（aimer.hpp/aimer.cpp）。DirectAimer 无状态，其
  const 属实，保留。全部调用点原本就在非 const 上下文（评测器局部对象、
  SentryVision 成员、测试里的临时对象），无调用点需要改。
- `TargetManager::select()` 去 const、last_selected_id_ 去 mutable。
- 删除 `SentryVision::mutableConfig()`（零调用点）；配置在构造时固定，
  运行时调整走已有的显式接口 setEnemyColor()/setRoiEnabled()。顺手修正
  sentry.hpp 的过时注释（只改 aimer_ 子句）。
- `Config::save()` 改名为 `saveTuningOverrides()` 并在声明处写明只持久化
  camera/tracker/aimer 三节；新增契约测试 testConfigTuningRoundTrip 把
  "写出值往返一致 + 其余节不写"钉死（tests/test_config.cpp）。

**模块归属（P1）**
- `sentry/include/autoaim/target.hpp` → `sentry/include/sentry/target_manager.hpp`，
  `TargetManager` 入 `namespace sentry`（其依赖的跟踪器/类型仍是
  autoaim::，在头文件中显式限定）。哨兵目录下不再有 autoaim/ 头文件目录。
- 串口同理：`sentry/include/io/serial_port.hpp` →
  `sentry/include/sentry/io/serial_port.hpp`，`namespace io` →
  `namespace sentry::io`。受影响文件：target.cpp、evaluation_sentry.cpp、
  sentry.hpp、serial_port.cpp、sentry_robot.cpp、两套测试。

**资源管理（P1）**
- VideoFileSource / DeviceSource 改 PImpl（与 MvsSource 同款式）：
  `struct Impl { cv::VideoCapture cap; }` + unique_ptr，删掉 void* 与
  capOf() 强制转换、手工 new/delete。注：camera_source.cpp 里 MVS 段的
  `void* handle` 是 MVS SDK C 接口自身的句柄类型，不在此列。

**CMake（P1/P2）**
- 根工程改用 `include(CTest)` 提供标准 BUILD_TESTING，并新增
  BUILD_APPS；apps/tests 子目录按开关加入（默认值不变，CI 无感）。
- 新增别名目标 `autoaim::core`、`autoaim::sentry`（Ceres 式命名空间目标）。
- autoaim_core 的警告/编码选项 PUBLIC → PRIVATE；MVS 的头文件目录与
  链接库 PUBLIC → PRIVATE（只在 camera_source.cpp 内使用）。
  AUTOAIM_ROOT 与 AUTOAIM_WITH_MVS 保持 PUBLIC，理由已写进
  core/CMakeLists.txt 注释（主张 7 的证据）。

**测试与规范（P1/P2）**
- tests/test_main.cpp（1096 行）按模块拆为 test_detector / test_pose /
  test_tracker / test_aimer / test_ballistics / test_evaluation /
  test_timing / test_config 八个文件 + check.hpp 共享 harness +
  fixtures.hpp 共享夹具（Spinner 原为文件作用域结构体，拆分时一并归位）；
  test_main.cpp 只保留 main 与调用顺序，25 个原测试一个不少、顺序不变。
- test_regression_gate.py 注册为 CTest 用例 regression_gate_contract
  （CI 的 ctest 步骤自此自动覆盖它，两个 job 都跑）。
- 头文件自包含：补 pose.hpp 的 <array>、aimer.hpp 的 <string>，并新增
  tools/check_headers.sh 注册为 CTest 用例 header_self_containment：
  19 个公共头文件逐个独立编译，今后靠传递包含的头文件会在 CI 报错。
- 新增 .clang-format（按现有代码风格定基线）、.clang-tidy（建议性
  检查集）、.editorconfig。format 暂不进 CI 强制：代码从未按
  clang-format 全量排过，先立基线、增量收敛，避免一次性重排淹没真实改动。

**文档（P2）**
- CHANGELOG.md 头部加范围说明：早期条目为哨兵继承史，全仓变更以 git
  历史与各轮审查文档为准。
- 新增 ADR 决策记录三篇（docs/decisions/）：ADR-001 单一共享核心、
  ADR-002 整车 EKF 而非逐板点跟踪、ADR-003 CameraSource 抽象与曝光
  时刻戳。docs/ 物理目录暂不搬迁（见缓办）。

## 缓办（附理由）

- **install/export（find_package(rm-vision)）**：中期目标，本轮不做半成品。
  前置问题是 AUTOAIM_ROOT 把源码目录路径编进编译定义、config.cpp 依此
  找模型与默认配置；安装部署前须先改为安装相对路径或显式运行时资源
  路径，否则装出来的库在本机外不可用。
- **IO/评测/可视化拆为独立 CMake target**：方向认可，逐步做。本轮先把
  能收紧的依赖收紧（MVS 依赖 PRIVATE 化）；camera_source / evaluation /
  draw 的 target 拆分等下一次动 core 构建时连同 install 一起做，避免为
  拆而拆产生两轮构建改动。
- **docs/ 物理分层（architecture/algorithms/...）**：不搬。69 篇文档彼此
  以相对路径互链，README、CLAUDE.md、空间与交付包都引用现路径；搬迁的
  链接腐烂成本高于收益。分类继续由 docs/INDEX.md 承担，本轮先补 ADR 层。
- **AimerState 显式状态拆分**：本轮先恢复 const 语义（API 不再说谎）；
  把滞回/身份/变化率状态拆成独立结构体，等出现"多目标并行 aim"的真实
  需求再做，现在拆是为假设的需求付接口复杂度。
- **legacy/archive 长期策略、工具脚本目录统一**：维持现状（完整性优先），
  与审查方 P3 判断一致。

## 验证证据

- ctest 5/5 通过：autoaim_tests（26 个用例，含新增契约测试）、
  sentry_tests、serial_port_tests、regression_gate_contract、
  header_self_containment（19/19 头文件独立编译通过）。
- 冻结基线逐格复现（重构后用评测 runner 对冻结检测缓存重跑 N0 口径）：
  3.avi 937 命中、条件 67.5%、保守 65.1%、10.60 hits/s、预测中位
  14.3 px；4.avi 1374 命中、68.6%、67.1%、10.06 hits/s、13.3 px——
  与冻结数字完全一致，本轮全部改动行为等价。
- 构建开关验证：BUILD_APPS=OFF + BUILD_TESTING=OFF + BUILD_SENTRY=OFF
  配置下只产出 libautoaim_core.a，无 apps/tests 目标，0 错误。
- 本 VM 已知边界：apps 与 sentry_offline 在本机因 videoio 缺
  GStreamer/FFmpeg 系统库不能链接；已用基线树（改动前 b1e290b）做 A/B，
  其 sentry_offline 同样失败，证明与本轮改动无关，CI 环境为准。
