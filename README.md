# rm-vision — RoboMaster Vision Auto-Aim

A complete vision auto-aim system for RoboMaster robots, written in C++17.
One shared core serves the infantry/hero robots and the sentry: armor
detection → PnP pose estimation → whole-robot EKF tracking → ballistic
solution → aiming and fire control. The sentry adds its own module on top of
the same core: multi-target management (including the outpost), a serial
link to the MCU, and tracking-guided ROI detection.

The system is built around three concerns that competition code often gets
wrong: **time** (every observation is aligned to the camera's exposure
instant, not to when the frame happened to arrive), **identity** (a target
is a physical robot — colour + plate number + plate size — not "whatever
blob is nearest"), and **evidence** (every headline number in this README
can be reproduced from the repository plus its public test data).

## Highlights

- **Exposure-time alignment.** Camera frames and gimbal attitude live in
  separate clocks. A ring-buffer pose store (`PoseBuffer`) interpolates
  attitude at the exposure instant with SLERP, and a dedicated calibrator
  (`DelayCalibrator`) measures the constant transport delay between the two
  streams — at 6 rad/s of gimbal rate, an uncorrected 10 ms offset is a
  3.4° aiming error that no amount of prediction tuning can remove.
- **Whole-robot EKF.** A 13-state filter tracks the robot centre, its spin,
  and *both* armor radii plus the second pair's height offset as states, so
  plate switching needs no hand-written jump logic. Measurement noise is
  anisotropic (lateral vs. depth), the covariance update uses the Joseph
  form, and two separate innovation channels (position and yaw) drive the
  process-noise adaptation — a phase error is invisible in position
  innovation, so spin-rate changes get their own detector.
- **Rate-agnostic evaluation.** The offline evaluator steps the simulated
  gimbal by actual frame-stamp deltas and brackets impact frames by
  timestamp, so dropped frames, 12.5 fps cameras and timestamp jitter can
  be measured honestly. A `--stress` harness perturbs detection caches
  (dropout / frame drop / decimation / jitter) for field-condition testing.
- **Measured, not assumed.** Ballistic drag, bullet speed (17 mm and
  42 mm classes, with a runtime override for referee-system muzzle
  measurements), camera intrinsics and the camera-to-gimbal lever arm are
  configuration, and the values shipped here are the ones measured on the
  team's own hardware.
- **Sentry-grade target management.** One track per physical robot,
  hysteresis-guarded target selection, an outpost tracker, and ROI windows
  predicted from the tracker's own state (validated to match full-frame
  detection output exactly on the reference footage).

## Pipeline

```
frame ──► Detector ──► PoseSolver ──► Tracker (EKF) ──► Aimer ──► fire command
            │              │               │               │
     light bars +     IPPE multi-     13-state whole-   ballistic RK4 +
     MLP digits,      solution +      robot model,      Hermite crossing,
     or ONNX          physical        multi-armor       centre mode for
     keypoints        screening       association       fast spinners
```

| Stage | What it does |
|---|---|
| `Detector` | Colour-channel differencing with an adaptive threshold finds light bars, pairs them into armors, refines corners on the symmetry axis, and classifies the digit with a small MLP. An optional ONNX keypoint backend (four corners per plate, per-corner confidence gating) can replace the front end. |
| `PoseSolver` | `SOLVEPNP_IPPE` with per-solution physical screening (finite, positive depth), a pitch prior, and a coarse-to-fine yaw search. Rejects degenerate, crossed or non-finite quads before they can poison the filter; reports both the raw fit residual and the final-pose residual. |
| `RobotTracker` | The 13-state EKF described above, plus a point-mass KF baseline and a dedicated 3-plate outpost tracker. Association gates widen with the actual frame interval, so a dropped frame is not treated as a lost target. |
| `Ballistics` | RK4 integration with drag and a cubic-Hermite impact crossing (sub-0.05 µm against a 10 µs reference), warm-started secant solve (~40 µs). 17/42 mm classes; measured muzzle velocity overrides configuration at runtime. |
| `Aimer` | Predicts the target over flight time + system latency, picks the plate (or the robot centre at high spin rates, waiting for a plate to rotate into the window), and emits yaw/pitch plus their rates for gimbal feedforward. |

## Repository layout

```
rm-vision/
├── CMakeLists.txt        # top level; options BUILD_SENTRY, AUTOAIM_WITH_MVS
├── README.md             # this file
├── BASELINE.md           # frozen regression baselines and how to reproduce them
├── CHANGELOG.md
├── CLAUDE.md             # repository conventions for AI-assisted development
├── LICENSE / THIRD_PARTY.md
│
├── core/                 # the shared auto-aim core (one copy, used by all robots)
│   ├── include/autoaim/  # config, detector, pose, tracker, ballistics, aimer,
│   │                     # evaluation, camera sources, pose buffer, delay calibration
│   ├── src/
│   ├── config/           # default.yaml (live config) · video_reference.yaml (frozen reference)
│   └── model/            # digit-classifier MLP (onnx) + labels
│
├── sentry/               # sentry module, links against core
│   ├── include/          # target manager, SentryVision, serial protocol, serial port
│   ├── src/ · apps/      # sentry_robot (on-robot) · sentry_offline
│   ├── config/           # sentry.yaml · default.yaml (sentry-flavoured)
│   └── tools/            # legacy-comparison bench
│
├── apps/                 # autoaim_video (offline evaluation) · autoaim_sim (simulator)
│                         # autoaim_lights · detector_bench
├── tests/                # three ctest suites: core, sentry, serial (PTY end-to-end)
├── tools/                # regression gate, sim tuning, calibration helpers,
│                         # offline evaluation harnesses
├── docs/                 # design notes, reviews, tuning studies (indexed in docs/INDEX.md)
├── reports/              # simulation and real-video evaluation reports
├── legacy/               # the previous on-robot codebase, kept whole for reference
└── archive/              # retired tooling and historical patches, labelled as such
```

A complete, annotated index of every directory is in
[`docs/INDEX.md`](docs/INDEX.md).

## Build and run

Requirements: CMake ≥ 3.16, a C++17 compiler, OpenCV 4 (core, imgproc,
calib3d, features2d, dnn).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure     # 3 suites: core / sentry / serial
./build/apps/autoaim_sim                       # simulator table (see BASELINE.md)
./build/apps/autoaim_video <video.avi>         # offline evaluation on a recording
./build/sentry/sentry_robot --help             # sentry on-robot program
```

Options:

- `-DBUILD_SENTRY=OFF` — infantry/hero only (core, apps, core tests).
- `-DAUTOAIM_WITH_MVS=ON -DMVS_ROOT=<path>` — build against the HikRobot
  MVS camera SDK for the physical robot. This path is compile-verified
  against an SDK stub; verify the SDK version/ABI on the target machine
  before relying on it.

## Configuration

Three configuration files, three distinct jobs — do not mix their numbers:

| File | Role |
|---|---|
| `core/config/default.yaml` | **Live configuration.** Measured camera intrinsics and distortion, measured 17 mm bullet speed (22 m/s), current tracker tuning. This is what runs on the robot. |
| `sentry/config/sentry.yaml` | Sentry overrides (target management, outpost, tracker behaviour) on top of the same core. |
| `core/config/video_reference.yaml` | **Frozen evaluation reference** (assumed pinhole intrinsics, zero distortion, 25 m/s). Exists only so the historical video baselines stay reproducible; it is not a robot configuration. |

The two video configurations differ by roughly 10 percentage points on the
same footage. Any reported number is meaningless without naming its
configuration — see `BASELINE.md` §11.

Two shipped values are placeholders, not measurements: the 42 mm bullet
speed (16 m/s) and its drag coefficient must be measured on the hero robot
before use. The 17 mm speed limit of the current season's rules (25 m/s,
with over-speed lockout) is a referee constraint, not a tuning target.

## Evaluation and results

Headline metrics are *virtual shooting* scores: a simulated first-order
gimbal fires on recorded footage, and the impact point is tested against
the target plate's polygon at the impact instant. They are comparable
across code changes on the same data; they are not live-fire hit rates.

Frozen reference results (`core/config/video_reference.yaml`, detection
cache shared):

| Metric | 3.avi | 4.avi |
|---|---:|---:|
| Robot-EKF virtual hit rate | **67.5%** | **68.6%** |
| Hits per second | 10.6 | 10.1 |
| 0.32 s prediction error, median | 14.3 px | 13.3 px |

With the live configuration (`core/config/default.yaml`) the same footage
scores 57.3% / 66.9% — the gap is the configuration (measured calibration
vs. the reference assumptions), not the code; both configurations are
tracked in `BASELINE.md` and gated separately by `tools/regression.py`.

**Reproduce them yourself.** The two recordings (1440×1080, 25 fps,
indoor dark-field, red #3 infantry target), their SHA-256 sums and the
detection caches are published in the companion data repository
[Kisketch/rm-videos](https://github.com/Kisketch/rm-videos). Download the
release assets into `video/`, then:

```bash
python3 tools/regression.py            # video gate (frozen reference) + simulator gate
```

Field-condition robustness (occlusion, frame drops, half-rate cameras,
timestamp jitter, latency sweeps) is covered by the `--stress` harness;
the full matrix and its conclusions — including which historical results
the pre-fix evaluator had invalidated — are in
[`docs/FIELD_CONDITIONS_2026-11-11.md`](docs/FIELD_CONDITIONS_2026-11-11.md).
The detailed results, evaluation method and tuning history live in
[`docs/pipeline_and_tuning.md`](docs/pipeline_and_tuning.md).

## On the robot

- Work through [`docs/on_robot_trial_checklist.md`](docs/on_robot_trial_checklist.md)
  in order. The **first** item is delay calibration: park the gimbal on a
  static plate, sweep for ~10 s (`sentry_robot --calib-delay`), and write
  the measured offset back — every later measurement depends on it.
- The MCU attitude frame carries no timestamp; its stamp is estimated as
  receive-time minus the serial transmission delay. The serial link itself
  is covered by a PTY end-to-end test in the test suite.
- CRC-8 checking on attitude frames is implemented but **off by default**:
  it stays off until the MCU firmware writes the CRC byte, and the
  firmware should treat a vision-side timeout as a fail-safe regardless.
- Mind the sign conventions when moving code between modules: the sentry
  module's pitch axis points *down* (positive pitch = down), the core's
  does not. The conventions are written down in `CLAUDE.md`.

## Documentation

- [`docs/INDEX.md`](docs/INDEX.md) — annotated index of the whole tree.
- [`docs/pipeline_and_tuning.md`](docs/pipeline_and_tuning.md) — results,
  evaluation method, and the full tuning log.
- [`BASELINE.md`](BASELINE.md) — frozen baselines, machine-dependence
  notes, and the two-configuration rule.
- [`docs/FIELD_CONDITIONS_2026-11-11.md`](docs/FIELD_CONDITIONS_2026-11-11.md) —
  field-condition stress matrix.
- Design notes per subsystem (timestamp alignment, bullet speed, target
  identity and CRC, ROI detection, delay calibration) are in `docs/`,
  indexed by topic in the index file.

## License

MIT — see `LICENSE`. Third-party components and the ideas adapted from
other open-source RoboMaster projects are credited in `THIRD_PARTY.md`.
