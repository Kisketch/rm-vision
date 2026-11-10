# P0-1 patch sketch: `apps/sentry_robot.cpp` (rm-sentry-integration)

Target file: `rm-sentry-integration/apps/sentry_robot.cpp`, robot-side loop.
All "current code" quotations below are from the copy reviewed on 2026-11-10
(`workspace/goals/rm/files/rm-autoaim-code-review-improvements/.src/repos/
rm-sentry-integration__apps_sentry_robot.cpp`). Verify against the live repo
before applying — the mainline has moved since the 2026-11-06 sentry snapshot.

## What changes, and what does NOT change

- `SentryVision::process(frame, stamp, yaw, pitch, roll)` keeps its signature:
  it already takes a stamp and an attitude. Only the *values* passed in change.
- `SerialPort::query()` keeps its interpolation. (See the note: it already
  interpolates — the bug is upstream of it.)
- `SerialPort::readerLoop()` keeps stamping attitude at receive time for now,
  because the 17-byte MCU protocol carries no timestamp field (see §4 of the
  note). The latency-compensation step below is the honest interim fix; a
  protocol timestamp is a P1-2 follow-up, not smuggled into this patch.

## Current code (the defect, in two lines)

```cpp
while (cap.read(frame)) {
    if (max_frames > 0 && processed >= max_frames) break;
    const double stamp = nowSeconds();          // <-- (A) processing time, not exposure

    sentry::AttitudeFrame attitude;
    attitude.stamp = stamp;
    bool have_attitude = false;
    if (serial && serial->query(stamp, attitude)) {   // <-- (B) interpolates, but TO the wrong stamp
        have_attitude = true;
        ...
    }
    const auto out = vision.process(frame, stamp, ...);
```

(A) `cap.read()` returns after USB transfer + driver queueing, so `stamp` is
milliseconds after the photons hit the sensor, with jitter. (B) `query()`
faithfully interpolates the attitude *at that late stamp* — correct
interpolation of the wrong instant.

## Proposed loop

```cpp
io::MvsCamera camera;                       // replaces cv::VideoCapture (see mvs_camera.hpp)
if (!camera.open(/* serial, if known */)) { std::cerr << "cannot open MVS camera\n"; return 1; }

autoaim::AttitudeBuffer attitude_buf;       // replaces SerialPort::query() as the sampler
                                            // (feed it from the serial reader; see note §3)

io::MvsFrame mvs;
while (camera.grab(mvs)) {
    if (max_frames > 0 && processed >= max_frames) break;
    const double stamp = mvs.exposure_stamp;   // exposure midpoint, host clock

    // Sample the attitude AT the exposure instant. Distinguish the three
    // failure modes instead of falling back to "current attitude":
    autoaim::AttitudeSample att;
    const auto st = attitude_buf.sample(stamp, att);
    if (st == autoaim::SampleStatus::kNotYet) {
        // Attitude stream lags the camera (serial latency). Options in order:
        //  1. wait briefly for the next attitude frame (bounded, e.g. 2 ms),
        //     then re-sample — preferred while the lag is < 1 frame period;
        //  2. if it still has not arrived, SKIP the frame (do not process it
        //     with a stale attitude). Log and count skips.
        // Never silently substitute the newest attitude: at 12 rad/s slew
        // that is a 0.012 rad error per millisecond of lag.
        ++skipped_waiting_attitude;
        continue;
    }
    if (st != autoaim::SampleStatus::kOk) {   // kTooOld / kGapTooLarge / kEmpty
        ++skipped_no_attitude;
        continue;                              // same rule: no attitude, no shot
    }
    attitude_buf.pruneOlderThan(stamp);

    const auto out = vision.process(mvs.image, stamp, att.yaw, att.pitch, att.roll);
    ... // fire-control / serial send unchanged, except:
        // yaw-rate feed-forward (P1-3) should be computed from att.yaw deltas
        // at exposure stamps, not from command deltas at processing stamps.
    ++processed;
}
```

## Feeding `attitude_buf` from the serial thread

`SerialPort::readerLoop()` currently pushes parsed frames into its own
`queue_` stamped with `nowSeconds()` at parse time. Two options, in
preference order:

1. **Minimal change (recommended for this step):** keep `SerialPort` as is,
   add a `drainInto(AttitudeBuffer&)` / callback that converts each queued
   `AttitudeFrame` into an `AttitudeSample` and pushes it. The receive-time
   stamp is then corrected by a measured serial latency (see note §4):
   `sample.stamp -= latency_estimate_s`, where the estimate is calibrated
   once on the bench (see acceptance test A3) and logged per run.
2. **Cleaner, slightly bigger:** move the ring buffer into `SerialPort`
   (i.e. replace its `queue_` + `query()` with `AttitudeBuffer`), so there is
   one attitude store instead of two. Do this only if the team is willing to
   touch `io/serial_port.*` in the same change; otherwise option 1.

## Latency instrumentation to add (cheap, high value)

Per frame, log to a CSV (same style as `sentry_offline`'s per-frame CSV):
`frame_id, exposure_stamp, receive_stamp, attitude_stamp, attitude_age_at_use,
serial_latency_estimate, skipped_reason`. This is what makes the 0.75 ms
budget *checkable* instead of asserted — and it is the recording Tongji's
sp_vision_24 added for exactly this reason ("时间戳＋视频＋四元数" recording).

## Out of scope for this patch (do not bundle in)

- MVS SDK calls themselves (blocked on the IT13 header check — see mvs_camera.hpp).
- Protocol timestamp / CRC (P1-2; needs electro agreement).
- Real calibration values (P0-2 in the review's numbering is detection; the
  calibration item is the other P0 — `fx=2400`, zero distortion, empty
  `R_cam2gimbal` stay as they are until a real calibration run lands).
- `rm-autoaim` mainline: this loop lives only in `rm-sentry-integration`.
  The fork question (merge sentry back into mainline first, or implement
  here and merge after) is a decision for the user — see the note §5.
