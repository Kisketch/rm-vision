# P0-1 — 曝光时间戳 + 云台姿态对齐：实现说明

Prepared 2026-11-10 (Mon) ~19:10 PST, after the user said "可以，继续" to
starting P0 item 1 (MVS capture + pose alignment per the G-02 design).
Goal: RM 视觉自瞄方案落地与代码实现 (`goal_76dccf5b0e86`).
Status: **draft prepared, not applied to any repo, not built on the robot.**
Files in this folder: `timestamp_aligner.hpp` (+ passing test),
`mvs_camera.hpp` (interface + SDK checklist), `sentry_robot-patch.md`.

## 1. Why this matters now

The code review (2026-11-10, `rm-autoaim-code-review-improvements.md`)
ranked this P0: everything downstream — PnP, the 11-state whole-robot EKF,
ballistics — is evaluated at a timestamp, and today that timestamp is wrong.
The EKF is the strongest part of the user's codebase; feeding it frames
stamped milliseconds late is what caps its real-car accuracy, not the filter.

## 2. Diagnosis — with one correction to the review

I re-read the reviewed copy of `rm-sentry-integration` before writing code,
and the review's summary was imprecise on one point. The precise defects:

| # | Defect | Evidence (reviewed copy, 2026-11-10) |
|---|---|---|
| 1 | **Frame stamped at processing time, not exposure.** `sentry_robot.cpp`: `cap.read(frame)` then `const double stamp = nowSeconds();` — the stamp is taken *after* USB transfer + driver queueing, milliseconds after exposure, with jitter. | `rm-sentry-integration__apps_sentry_robot.cpp` |
| 2 | **Attitude stamped at receive time.** `SerialPort::readerLoop()`: `a.stamp = nowSeconds()` when a frame is *parsed*, not when the MCU sent it. The 17-byte MCU protocol has no timestamp field (`AA BB [yaw][pitch][roll] color buff BB`), so no true send-time exists on this link at all. | `rm-sentry-integration__src_io_serial_port.cpp`, `rm-sentry-integration__src_serial_protocol.cpp` |
| 3 | **No MVS adapter.** The robot loop uses `cv::VideoCapture` (V4L2 / file). The sentry README says "swap cv::VideoCapture for the HikRobot source on the IT13" and lists "MVS adapter" as a remaining deployment gap. OpenCV capture exposes no exposure timestamp. | `rm-sentry-integration__README.md` |
| 4 | **Correction — interpolation DOES exist.** `SerialPort::query(stamp, …)` already keeps a queue (max 64) and linearly interpolates yaw (with `std::remainder` wrap handling), pitch and roll between bracketing samples. The review's shorthand "没有环形缓冲插值" is not accurate for this copy: the ring buffer + interpolation are there; what is missing is *correct stamps to interpolate at*, plus any refusal to extrapolate / bridge gaps. The new `AttitudeBuffer` keeps the interpolation, adds the missing guards, and is unit-tested. | `rm-sentry-integration__src_io_serial_port.cpp`, `query()` |

## 3. The error budget, in numbers (why 0.75 ms is hard)

- The user's G-02 design doc is cited in the review as putting the budget at
  **0.75 ms**. I could **not** locate the G-02 doc in local files, so I have
  not re-read it — delivery should confirm the number and its derivation
  from the repo before treating it as the acceptance line.
- At the config's `gimbal_max_rate: 12 rad/s` (`config/sentry.yaml`), each
  1 ms of timestamp error is 0.012 rad ≈ 0.69° of gimbal-angle error.
  A 0.75 ms budget therefore allows ≈ 0.009 rad ≈ 0.52°. For reference,
  Tongji reports < 0.01 rad steady-state single-plate error — the budget is
  the same order as the whole remaining error allowance.
- The serial link alone: 17 bytes × 10 bits ÷ 115200 baud ≈ **1.48 ms** just
  to transmit one attitude frame, before USB-serial latency. That already
  exceeds a 0.75 ms budget if attitude is stamped at receive time and used
  uncorrected. Conclusion: **receive-stamped attitude can never meet the
  G-02 budget; the latency must be measured and subtracted** (interim), or
  the protocol must gain an MCU timestamp (proper fix, needs electro
  agreement — overlaps P1-2, do not bundle it silently into this patch).

## 4. Design (what the draft does)

- **Camera thread → (image, exposure_stamp).** `MvsCamera::grab()` returns
  the exposure *midpoint* on the host monotonic clock:
  `exposure_stamp = device_stamp + exposure/2`, mapped through a
  `ClockOffset` EMA (see `timestamp_aligner.hpp`). The MVS SDK mapping is
  deliberately left as a checklist in `mvs_camera.hpp` — the timestamp field
  name/units and the exposure-start-vs-readout semantics must be read from
  the `MvCameraControl.h` installed on the IT13, not written from memory.
- **Attitude → `AttitudeBuffer`.** Ring buffer (default 128 samples),
  angle-aware linear interpolation, and three explicit refusals instead of
  silent fallbacks: `kNotYet` (stamp newer than newest sample — wait or skip
  the frame), `kTooOld`, `kGapTooLarge` (bracketing samples > 50 ms apart —
  the serial stream stalled). No extrapolation by default.
- **Robot loop → sample at the exposure stamp, skip on failure.**
  `sentry_robot-patch.md` gives the before/after for `apps/sentry_robot.cpp`.
  Rule: no attitude at the exposure instant ⇒ skip the frame, never process
  it with the newest attitude. Skips are counted and logged, so a laggy link
  shows up as a number instead of silent inaccuracy.
- **Logging.** Per-frame CSV: `frame_id, exposure_stamp, receive_stamp,
  attitude_stamp, attitude_age_at_use, serial_latency_estimate,
  skipped_reason`. This is the Tongji sp_vision_24 pattern (timestamp +
  video + quaternion recording) and is what makes the budget checkable.

## 5. What was verified here — and what was NOT

- ✅ `timestamp_aligner.hpp` compiles clean (`-Wall -Wextra`, C++17) and its
  standalone test passes: interpolation, yaw wrap through ±π, refusal modes
  (empty / too-old / not-yet / gap), out-of-order push, exposure midpoint
  (1500 µs → +0.75 ms), clock-offset EMA with wild-sample rejection.
  Run: `g++ -std=c++17 -Wall -Wextra -O2 timestamp_aligner_test.cpp -o /tmp/aligner_test && /tmp/aligner_test` → "all timestamp_aligner tests passed".
- ❌ NOT compiled against the real repos (no checkout here), NOT built on the
  IT13, NOT run against a real MVS camera or MCU. The MVS SDK calls do not
  exist yet — that is the one deliberate gap, flagged in `mvs_camera.hpp`.
- ❌ G-02 design doc not re-read (not found locally); 0.75 ms budget is
  second-hand from the review. Confirm from the repo.
- ❌ Camera model / SDK version / exposure time / MCU attitude rate are all
  unknown from local sources. The note in the review references Tongji's
  MV-CS016-10UC and SMBU's 165 Hz / BayerRG8 / 1500 µs only as *reference*
  hardware — do not assume they are the user's.

## 6. Decisions the user still needs to make

1. **Where to land it:** implement in `rm-sentry-integration` (where the
   robot loop lives) and merge to mainline later, or merge the sentry fork
   back into `rm-autoaim` first (P1-1) and implement once? The review
   recommends `rm-autoaim` as the single mainline; implementing in the fork
   first is faster but adds merge debt.
2. **Attitude latency:** accept the interim fix (measure serial latency on
   the bench, subtract a constant) or ask electro for an MCU timestamp field
   in the protocol now (proper fix, but a protocol change + P1-2 CRC work)?
3. **Confirm on the IT13:** camera model, MVS SDK version, exposure time,
   MCU attitude send rate — needed before `mvs_camera.cpp` can be written.

## 7. Acceptance tests (for the bench, after the patch lands)

- A1 (unit): the shipped `timestamp_aligner_test.cpp` passes.
- A2 (static vs slewing): same target, gimbal static vs uniform slew —
  PnP world-position jitter while slewing must be no worse than static
  (the review's P0-1 acceptance line).
- A3 (latency): log `receive_stamp - exposure_stamp` and attitude age over a
  bench run; report mean/p95. If attitude age p95 > 0.75 ms after latency
  compensation, the G-02 budget is not met and the protocol-timestamp
  question (decision 2) is forced.
- A4 (regression): offline videos 3/4 still pass the `BASELINE.md` §10 gates
  (hit-rate degradation ≤ 1.5 pp, 0.32 s prediction error +≤ 1.5 px,
  frame time +≤ 10%, unit tests green) — the offline path must be untouched
  by the robot-loop change.
