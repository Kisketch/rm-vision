# Camera calibration record (template)

A calibration is an archive, not five numbers. Whoever recalibrates a
camera fills in one copy of this file (kept next to the config that uses
it) and stores the raw material it points to. The distortion storage
order question in `distortion_order.md` exists precisely because an
earlier calibration kept only the final vector — do not repeat that.

## Record

| Field | Value |
|---|---|
| Camera model / serial | |
| Lens (focal length, mount) | |
| Date | |
| Operator | |
| Calibration target | type (checkerboard / circles), squares, **square size (mm, measured)** |
| Images | count, resolution, storage location (repo path or release) |
| OpenCV version | |
| Flags / model | e.g. standard 5-param, rational, fisheye |
| Camera matrix K | fx, fy, cx, cy (full precision) |
| Distortion vector | **in the order OpenCV emitted it**, with the order named (k1 k2 p1 p2 k3 / k1 k2 k3 p1 p2 / …) |
| Reprojection error | RMS (px), plus per-image max if available |
| Raw tool output | file name / pasted block below |
| Entered into config | file + the values as entered (must be traceable to the row above) |

## Raw calibration output

```
(paste the calibration tool's own output here, unedited)
```

## Verification

- [ ] Static target at a measured distance: PnP distance error recorded
      (value: ___ m at ___ m).
- [ ] Config values diffed against the raw output above by a second
      reader (or a script), not retyped from memory.
