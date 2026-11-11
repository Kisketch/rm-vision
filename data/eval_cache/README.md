# Evaluation caches — real-video regression data (in git)

The raw material behind every real-video number in `BASELINE.md` and
`reports/real_video_regression/`: per-frame detection caches, tracker
output CSVs, evaluation reports and run logs for the two reference
recordings. The recordings themselves are release assets (too large for
git) — see the `data-v1-2026-11-09` release of this repository.

## Layout

```
data/eval_cache/
├── README.md          # this file
├── 3/                 # everything for 3.avi (2209 frames)
│   ├── detections.txt # canonical detection cache (see Deduplication)
│   ├── v3_track.csv · v3_report.md            # headline run
│   ├── v3_n0_* · v3_n1_*                      # config-calibre ablation (BASELINE §11)
│   ├── v3_bullet_* · v3_camonly_*             # bullet-speed / camera-only ablations
│   ├── v3new_*                                # post-review code, same cache
│   └── 3_cur_* · 3_opt_* · 3_ported_*         # porting-comparison runs (sentry lineage)
└── 4/                 # everything for 4.avi (3416 frames), same structure
```

File kinds: `*_track.csv` is the per-frame tracker/aimer output of a run,
`*_report.md` its formatted evaluation report, `*.log` the raw evaluator
log. Reports that were written up as findings live in
`reports/real_video_regression/`; the files here are the raw outputs they
cite.

## The detection cache format

`detections.txt` is the cache protocol consumed by
`tools/offline_eval/evaldriver_vision.cpp` and produced by
`apps/autoaim_video`: one header line (format version, frame count,
image size, fps) followed by one line per frame with the detected plates
(corners, colour, number, confidence). Evaluating from the cache makes
the tracker/aimer half of the pipeline bit-reproducible without
re-running the detector; `tests/test_cache.py` round-trips the format in
CI.

## Deduplication (why there is one detections.txt per video, not six)

The working set this directory was imported from held six detection
files per video (`v*_detections.txt`, `v*_detections_old.txt`,
`v*new_detections.txt`, `*_cur_`, `*_opt_`, `*_ported_detections.txt`).
All six are **byte-identical** — the detector output never changed
across those runs; only the tracker/aimer side did:

- 3.avi: md5 `fc8c9beaea1b873f6204705d2f25d185`
- 4.avi: md5 `cae8f2d5361fe853ae5b7272136c99bb`

One canonical copy per video is kept as `detections.txt`; nothing is
lost. The per-run *outputs* (track CSVs, reports, logs) genuinely differ
and are all preserved above.
