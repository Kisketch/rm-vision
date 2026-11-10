# archive/ — 历史存档（只读性质，不参与构建）

这里存放**已被单仓合并取代、但作为历史记录必须保留**的材料。不要把这里的
脚本/工作流接入现行构建与 CI；它们的适用背景在各自条目里写明。

## patches/ — 分轮交付补丁（合并前的工作记录）

| 文件 | 内容 |
|---|---|
| `rm-autoaim-timestamp-alignment.patch` | P0-1 时间戳对齐（PoseBuffer/CameraSource）合入 rm-autoaim 前的补丁原件 |
| `sentry-device-id-crc.patch` | 哨兵 DeviceId + CRC-8 工作合入前的补丁原件（465 行） |
| `sentry_robot-patch.md` | P0-1 哨兵侧接入的补丁说明 |
| `P0-1-note.md` | P0-1 设计与交付说明 |

以上工作的代码均已合入 `core/`、`sentry/` 现行代码（见 `docs/timestamp_design.md`、
`docs/device_id_and_crc.md`），补丁仅作过程存档。

## shared-core/ — 已退役的双仓共享核心守卫

双仓时代用来校验两仓同名文件一致性的机制，单仓合并后其职能由"只存一份"
的结构本身取代，**已退役**，其清单文档保留在 `docs/shared_core.md`（历史）。

| 文件 | 内容 |
|---|---|
| `check_shared_core.sh` | 一致性校验脚本（两仓版本字节一致，只存一份） |
| `shared-core.main.yml` | rm-autoaim 侧的 CI 工作流原件 |
| `shared-core.sentry.yml` | rm-sentry-integration 侧的 CI 工作流原件 |

## 未存入本仓的同类材料（及原因）

- 各轮交付的**代码快照目录**（bullet-speed、timestamp-alignment、
  sentry-attitude-alignment、software-completion 等）：其代码已逐项合入本仓
  现行代码，快照与旧仓 git 历史重复，存入只会产生第三份过期副本，故不存；
  每轮的文档部分已分别进入 `docs/`（如 `bullet_speed.md`、
  `timestamp_alignment.md`、`software_completion.md`）。
- 作业仓 `rm-armor-detection` 的文件：属于该仓自身，按用户要求不触碰、不复制。
- 视频与检测缓存数据：私有数据（`rm-videos` 仓），不进入公开仓。
