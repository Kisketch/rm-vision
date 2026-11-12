# Code-level review fixes, round 3 (2026-11-10)

A third external review read the actual sources (aimer / tracker / pose /
camera_source / sentry_robot) and raised nine points. Every one was
checked against the code; eight were real and are fixed here, one
(greedy vs Hungarian association) is deferred *by the review's own
criterion* — switch only on a measured association error rate, which
needs logged multi-plate footage we do not have yet.

## Fire-path correctness

1. **Forecast covariance omitted process noise.** `RobotAimer` computed
   `sigma_lat` from `predictCovariance(t_impact, false)` — F P F' only.
   Now the full F P F' + Q(dt) form everywhere a forecast uncertainty is
   published (aimer + both `targetState()` snapshots). Two qualifications
   stay on record: the shipped configs keep the fire gate itself OFF
   (`fire_sigma_scale: 0`, "Q is tuned for response, not calibrated"), so
   no shipped behaviour depended on the understated value; and with Q
   included the magnitude is still a *relative* measure, not a calibrated
   forecast distribution. Gate-on A/B (fire_sigma_scale = 1.0,
   reference config, robot EKF):

   | estimator | video | shots | conditional | hits/s |
   |---|---|---:|---:|---:|
   | F P F' (old) | 3.avi | 1201 | 69.8% | 9.1 |
   | F P F' (old) | 4.avi | 1442 | 71.4% | 7.4 |
   | F P F' + Q (new) | 3.avi | 12 | 100.0% | 0.1 |
   | F P F' + Q (new) | 4.avi | 0 | — | 0.0 |

   Both ends of the problem are now measured: the old estimator made
   the gate nearly decorative (it passed ~83% of the gate-off shots at
   a hit rate barely above the ungated 67.5/68.6%), and the full
   covariance at scale 1.0 blocks essentially everything — the
   response-tuned Q implies forecast sigmas of decimetres, far above
   the half-panel threshold. Conclusion: sigma_lat is now the honest
   quantity, and the gate stays OFF until Q (or the scale) is
   calibrated as a forecast distribution against ground truth — a
   coverage study (1σ/2σ empirical coverage in the simulator, then on
   robot logs), not a number to be fitted on these two videos.
2. **Flight-time fixed point could exit unconverged and still fire.**
   All three aimers now track convergence explicitly (`AimCommand::
   converged`), iterate 6 times instead of 4, and never fire on an
   unconverged command (the command stays valid for gimbal tracking).
   Census over range 3–11 m × bullet 15/22/25 m/s × spin 0/3/8 rad/s ×
   radial ±2 m/s (32,076 valid commands): non-convergence was 6.8% at
   4 iterations, 0.46% at 6 — the residue is plate-selection oscillation
   at switch boundaries, which no iteration count fixes and which is
   precisely where firing is least justified.

## Tracker robustness

3. **Yaw NIS adaptation used only the last match of a frame** while the
   position NIS used the frame mean — order-dependent adaptation. Now
   both aggregate as the mean over the frame's matches. Unit test:
   two-plate frames with per-plate yaw biases (+0.30 / +0.05 rad) fed in
   both orders land `q_yaw_scale` at 1.082 in both runs (gap 0.000).
   Recorded boundary: at a much larger corruption (0.5 rad) the two
   runs' *states* diverge into different association basins — sequential
   updates plus greedy association are order-sensitive at that scale, a
   separate effect from the aggregation fixed here.
4. **Stale/duplicate observations were applied to the current state**
   (`predictTo` no-ops at dt ≤ 0, then the update ran anyway). All three
   trackers (robot / outpost / point) now reject `frame.stamp <= stamp_`
   outright; the robot tracker reports `diagnostic_event = "stale_frame"`.
   Out-of-sequence data would need a fixed-lag smoother, not a direct
   update. Unit test: state bit-identical after duplicate and rewound
   frames.

## PnP defensive path

5. **A missing per-candidate reprojection error was treated as 0.0**,
   silently dropping the reprojection term from the IPPE selection cost.
   Now recomputed as the RMS reprojection error via `cv::projectPoints`.
   The selection's pitch term is documented in code as what it is: a
   soft prior; the hard gates are finiteness and points in front of the
   camera.

## MVS runtime hardening

6. **Timestamp tick frequency silently defaulted to 1 GHz.** A failed
   query now aborts startup unless `MvsConfig::tick_freq_override`
   carries a model-verified value — a wrong frequency is a time-*scale*
   error that grows unbounded, unlike an offset. The chosen frequency
   is printed at startup.
7. **Camera parameter sets were unchecked.** TriggerMode / TriggerSource
   / ExposureAuto / ExposureTime / frame-rate enable+value now throw on
   failure (the packet-size optimisation stays best-effort), and the
   exposure is **read back** from the camera; the read-back value is
   what the exposure-centre stamp math uses. Startup logs both the tick
   frequency and the exposure read-back vs configured.
8. **One grab timeout ended the sentry main loop.** `MV_E_NODATA` is
   now retried inside `grab()` (streak of 10, logged at 1 and 10); only
   hard errors — or a 10 s unbroken stall — end the stream. Video-file
   end-of-stream semantics are unchanged (that source is separate).

## Deferred

9. **Greedy association vs Hungarian.** Kept greedy, per the review's
   own condition: decide on a measured association error rate from
   logged four-plate footage (goes on the on-robot data list), not on a
   constructed example.
