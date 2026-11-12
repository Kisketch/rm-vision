# Monorepo consolidation — rm-autoaim + rm-sentry-integration → rm-vision

Date: 2026-11-10. Sources: `rm-autoaim` main @ `c57343c` and
`rm-sentry-integration` master @ `6fa921a` (both after the full-audit merge).
The two source repos are archived read-only after this migration; their git
histories stay there (the sentry repo began as a file snapshot, so the two
histories share no ancestor and could not be grafted — rm-vision starts from
the merged snapshot of the two HEADs).

`rm-armor-detection` (frozen coursework repo) and `rm-videos` (private data)
were NOT touched by the consolidation.

## Why

The two repos carried two copies of the same auto-aim core. Every core fix
had to be ported by hand and guarded by a manifest + cross-repo CI
(`docs/project/shared_core.md`, now historical). The consolidation makes the core a
single copy: `core/` is the only implementation; the sentry-specific code
lives in `sentry/` and links it.

## How forks were merged (one decision per formerly-diverged file)

| File | Decision |
|---|---|
| `types.hpp` | Sentry version (superset: `OUTPOST`, `TargetState`, `TrackState` lives here) |
| `pose.*` | Sentry version: its `CameraModel` adds live `setGimbalAttitude`; its no-attitude transform is identical to the main repo's (the mounting pitch folds into the same product), so offline behaviour is unchanged |
| `config.*` | Union: main's fields (adaptive threshold, keypoint backend, bullet classes, `carry_velocity`, warmup) + sentry's (`TargetConfig`, outpost fields, target section in the loader, sentry `validate()` checks, sentry override keys) |
| `detector.cpp` | Main version, except the type/number filter takes the sentry line: a LARGE plate numbered `outpost` survives (the sentry pipeline needs it; the infantry config can still drop it via `detector.ignore_classes`) |
| `detector.hpp` | Main + sentry's `setEnemyColor` / `setConfig` |
| `tracker.*` | Main version (carries `carry_velocity`, gate `match_dt_scale`, tracking-frame counter) + sentry's additions verbatim: `RobotTracker::targetState`, free `snapshot()` overloads, the whole `OutpostTracker` |
| `aimer.*` | Both RobotAimer implementations kept: the tracker-based one (infantry; warmup gating + `CommandRate`) and the `TargetState` one (sentry pipeline). Point/Direct aimers are main's |
| `ballistics.*` | Main version (bullet-class API; numeric path was already synced) |
| `evaluation.cpp` | Main version in `core/`; the sentry strategy moved to `sentry/src/evaluation_sentry.cpp` as `appendSentryStrategy()` (same judge, same gimbal start) and is appended by `apps/autoaim_video.cpp` when built with the sentry module |
| `pose_buffer.*`, `delay_calibration.*`, `draw.*` | Were byte-identical; single copy in `core/` |
| Configs | `core/config/default.yaml` (infantry; `carry_velocity: 1` now explicit), `sentry/config/default.yaml` (sentry-flavoured defaults; `carry_velocity: 0` explicit), `sentry/config/sentry.yaml` (robot config; `carry_velocity: 0` added) |

`AUTOAIM_ROOT` now points at `core/` so the historical
`"/config/default.yaml"` and `"/model/mlp.onnx"` suffixes resolve unchanged;
the sentry apps resolve their config via `SENTRY_ROOT` (`sentry/`).

## Regression evidence (this machine, 2026-11-10)

- Build clean (`-Wall -Wextra`, 0 warnings), MVS option build verified
  against the stub SDK; `ctest` 3/3 (core, sentry, serial PTY).
- Simulation, infantry config: **cell-identical** to rm-autoaim.
  Simulation, sentry config: two spin cells move (+0.2 / −0.7 pp) — the sim
  drives the tracker-based aimer, which in the sentry repo was a forwarding
  shim to the TargetState aimer; the production sentry path is unaffected
  (see below).
- Real videos (frozen detections, N0 config): robot EKF **67.5 % / 68.6 %**,
  prediction medians **14.3 / 13.3 px** — exactly the frozen baseline.
  Sentry strategy under sentry configs (`carry_velocity: 0`): **67.6 % /
  71.9 %** — exactly the sentry repo's numbers (shot counts within ±3,
  floating-point-level). Under the infantry N0 config the sentry row reads
  67.9 % / 71.8 %: the unified tracker carries velocity through LOST there,
  which the sentry pipeline never had; the sentry configs pin it off.
- Fresh detection with the merged detector: **byte-identical** detection
  files on both regression videos.
- `SentryVision` integration smoke (800 frames of 4.avi, tracking-guided ROI
  on vs off): 890 = 890 observations, 0 frames differ, 192 = 192 valid aim
  commands — identical to the pre-merge sentry repo.

Note: `reports/sim_report.md` was stored as UTF-16LE in rm-autoaim; it is
transcoded to UTF-8 here (same text) so every file in the repo is UTF-8.

## Retired

- `tools/check_shared_core.sh` and the `shared-core.yml` CI workflows: the
  divergence they guarded against is impossible with one core copy.
  `docs/project/shared_core.md` stays as the historical record of the fork audit.
