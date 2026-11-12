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

The current `apps/autoaim_video` writer emits one text row per frame,
starting with frame 0. There is no header row. The first three fields
are the frame index, timestamp in seconds, and number of detections.
Each detection then contributes 13 fields: number, confidence, type,
and the top x/y, bottom x/y and width for each of the two light bars.
There is no separate colour field in this payload. See
`saveDetections` and `loadDetections` in `core/src/evaluation.cpp`.

The current video app uses a separate `.meta.yml` file for cache
validation. On a cache hit it rebuilds the timeline from the frame
index and evaluation FPS. The timestamps used by this path cannot
establish the original intervals between hardware captures.

> **Detector version note (2026-11-11).** These caches are the detector
> output *before* the split-bar merge (`docs/studies/split_light_merge.md`). The
> frozen N0 config (`core/config/video_reference.yaml`) leaves the merge off,
> so they still reproduce BASELINE §10 exactly. The shipped configs turn it
> on: fresh detection then adds 14 frames on 3.avi and is byte-identical on
> 4.avi.

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
