#!/usr/bin/env python3
"""Generate the README's real-footage figures (docs/figures/).

Everything drawn on the frames is real pipeline output read from the
in-repo caches -- nothing is re-detected or hand-placed:

  data/eval_cache/<v>/detections.txt   per-frame armor detections
  data/eval_cache/<v>/v<v>_n0_track.csv per-frame tracker state (N0 run)

The detector-stages figure re-applies the detector's own preprocessing
(R-B difference, threshold 70, erode3 -> dilate7 -> erode5, as in
Detector::findLightsIn) only to show the intermediate images; its final
panel is the cached detection.  Frames are brightened for display (the
recordings are very dark); the caption says so.

Needs the release videos and opencv-python + numpy + pillow:
    python3 tools/make_figures.py --video3 video/3.avi --video4 video/4.avi
"""
import argparse
import csv
import os

import cv2
import numpy as np
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "docs", "figures")
CACHE = os.path.join(ROOT, "data", "eval_cache")

# BGR versions of the chart palette in tools/make_charts.py
SLATE = (42, 23, 15)          # #0f172a
INK = (240, 232, 226)         # #e2e8f0
MUTED = (184, 163, 148)       # #94a3b8
TEAL = (166, 184, 20)         # #14b8a6
AMBER = (11, 158, 245)        # #f59e0b
BLUE = (246, 130, 59)         # #3b82f6


def load_detections(v):
    dets = {}
    with open(os.path.join(CACHE, v, "detections.txt")) as f:
        for line in f:
            p = line.split()
            frame, n = int(p[0]), int(p[2])
            armors = []
            for i in range(n):
                a = p[3 + 13 * i: 16 + 13 * i]
                num, conf = a[0], float(a[1])
                lt, lb = (float(a[3]), float(a[4])), (float(a[5]), float(a[6]))
                rt, rb = (float(a[8]), float(a[9])), (float(a[10]), float(a[11]))
                armors.append({"num": num, "conf": conf, "quad": [lb, lt, rt, rb]})
            dets[frame] = armors
    return dets


def load_track(v):
    with open(os.path.join(CACHE, v, f"v{v}_n0_track.csv")) as f:
        return {int(r["frame"]): r for r in csv.DictReader(f)}


def read_frames(path, first, count):
    cap = cv2.VideoCapture(path)
    cap.set(cv2.CAP_PROP_POS_FRAMES, first)
    frames = []
    for _ in range(count):
        ok, fr = cap.read()
        if not ok:
            break
        frames.append(fr)
    return frames


def brighten(img, gamma=0.45, gain=1.0):
    lut = np.clip(((np.arange(256) / 255.0) ** gamma) * 255 * gain, 0, 255).astype(np.uint8)
    return cv2.LUT(img, lut)


def put(img, text, org, scale, colour, thick=1):
    cv2.putText(img, text, org, cv2.FONT_HERSHEY_SIMPLEX, scale, colour, thick, cv2.LINE_AA)


def draw_armor(img, armor, offset, scale, label=True):
    ox, oy = offset
    pts = [(int(round((x - ox) * scale)), int(round((y - oy) * scale))) for x, y in armor["quad"]]
    lb, lt, rt, rb = pts
    cv2.line(img, lb, lt, AMBER, max(2, int(scale)), cv2.LINE_AA)       # light bars
    cv2.line(img, rb, rt, AMBER, max(2, int(scale)), cv2.LINE_AA)
    cv2.polylines(img, [np.array(pts, np.int32)], True, TEAL, 3, cv2.LINE_8)
    for p in pts:
        cv2.circle(img, p, 3, INK, -1, cv2.LINE_AA)
    if label:
        text = f"No.{armor['num']}  {armor['conf']:.2f}"
        (tw, th), _ = cv2.getTextSize(text, cv2.FONT_HERSHEY_SIMPLEX, 0.5, 1)
        x = int(sum(p[0] for p in pts) / 4 - tw / 2)
        y = min(p[1] for p in pts) - 22
        cv2.rectangle(img, (x - 4, y - th - 4), (x + tw + 4, y + 5), SLATE, -1)
        put(img, text, (x, y), 0.5, TEAL, 1)


# ------------------------------------------------------------------ GIF
def demo_gif(video4):
    """4.avi, ~8 rad/s spin: cached detections + tracker state, 2 s."""
    dets, track = load_detections("4"), load_track("4")
    first, count, step = 1000, 50, 1
    cx, cy, cw, ch, s = 585, 612, 280, 140, 1.8
    ox, oy = cx - cw // 2, cy - ch // 2
    frames = read_frames(video4, first, count)
    out = []
    for k, fr in enumerate(frames[::step]):
        f = first + k * step
        crop = brighten(fr[oy:oy + ch, ox:ox + cw])
        img = cv2.resize(crop, None, fx=s, fy=s, interpolation=cv2.INTER_CUBIC)
        for a in dets.get(f, []):
            draw_armor(img, a, (ox, oy), s)
        bar = np.full((44, img.shape[1], 3), SLATE, np.uint8)
        r = track.get(f)
        state = r["state"] if r else "-"
        w = abs(float(r["w"])) if r else 0.0
        n = len(dets.get(f, []))
        put(bar, f"4.avi  frame {f}", (10, 18), 0.45, MUTED)
        put(bar, f"armors {n}   tracker {state}   spin {w:.1f} rad/s", (10, 36), 0.48, INK)
        out.append(np.vstack([img, bar]))
    # One shared palette: 56 colours fitted to the footage plus the exact
    # overlay colours, so thin overlay lines never get quantised away.
    pal_src = Image.fromarray(cv2.cvtColor(np.vstack(out[::10]), cv2.COLOR_BGR2RGB))
    base = pal_src.quantize(colors=56, method=Image.Quantize.MEDIANCUT).getpalette()[:56 * 3]
    for c in (TEAL, AMBER, INK, MUTED, SLATE, BLUE):
        base += [c[2], c[1], c[0]]
    pal = Image.new("P", (1, 1))
    pal.putpalette(base + [0] * (768 - len(base)))
    pil = [Image.fromarray(cv2.cvtColor(o, cv2.COLOR_BGR2RGB)).quantize(
        palette=pal, dither=Image.Dither.NONE) for o in out]
    path = os.path.join(OUT, "demo_spin_4avi.gif")
    pil[0].save(path, save_all=True, append_images=pil[1:], duration=int(40 * step), loop=0,
                optimize=True, disposal=1)
    print("wrote", path, f"{os.path.getsize(path) / 1e6:.2f} MB", len(pil), "frames")


# ------------------------------------------------------- detector stages
def detector_stages(video3):
    """One frame, four panels: input, R-B, mask + bars, cached detection."""
    dets = load_detections("3")
    f = 1600
    fr = read_frames(video3, f, 1)[0]
    armors = dets.get(f, [])
    xs = [x for a in armors for x, _ in a["quad"]]
    ys = [y for a in armors for _, y in a["quad"]]
    cx, cy = int(sum(xs) / len(xs)), int(sum(ys) / len(ys))
    cw, ch, s = 240, 150, 2.0
    ox, oy = cx - cw // 2, cy - ch // 2
    crop = fr[oy:oy + ch, ox:ox + cw]
    up = lambda im: cv2.resize(im, None, fx=s, fy=s, interpolation=cv2.INTER_NEAREST)

    p1 = up(brighten(crop))
    diff = cv2.subtract(crop[:, :, 2], crop[:, :, 0])            # red target: R - B
    p2 = up(cv2.applyColorMap(cv2.convertScaleAbs(diff, alpha=1.6), cv2.COLORMAP_INFERNO))
    mask = (diff > 70).astype(np.uint8) * 255                    # channel_diff_threshold
    k = lambda n: cv2.getStructuringElement(cv2.MORPH_RECT, (n, n))
    mask = cv2.erode(cv2.dilate(cv2.erode(mask, k(3)), k(7)), k(5))
    p3 = up(cv2.cvtColor(mask, cv2.COLOR_GRAY2BGR))
    p3[np.where((p3 == 255).all(axis=2))] = INK
    cnts, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    for c in cnts:
        if cv2.contourArea(c) < 12:
            continue
        box = cv2.boxPoints(cv2.minAreaRect(c)) * s
        cv2.polylines(p3, [box.astype(np.int32)], True, AMBER, 2, cv2.LINE_AA)
    p4 = up(brighten(crop))
    for a in armors:
        draw_armor(p4, a, (ox, oy), s)

    titles = ["1  input (brightened)", "2  R - B channel difference",
              "3  threshold 70 + morphology (before shape filters)", "4  paired armor, corners, digit"]
    panels = []
    for t, p in zip(titles, [p1, p2, p3, p4]):
        head = np.full((34, p.shape[1], 3), SLATE, np.uint8)
        put(head, t, (10, 23), 0.55, INK)
        panels.append(np.vstack([head, p]))
    gap = np.full((panels[0].shape[0], 8, 3), SLATE, np.uint8)
    row1 = np.hstack([panels[0], gap, panels[1]])
    row2 = np.hstack([panels[2], gap, panels[3]])
    sheet = np.vstack([row1, np.full((8, row1.shape[1], 3), SLATE, np.uint8), row2])
    sheet = cv2.copyMakeBorder(sheet, 8, 8, 8, 8, cv2.BORDER_CONSTANT, value=SLATE)
    path = os.path.join(OUT, "detector_stages_3avi.png")
    cv2.imwrite(path, sheet, [cv2.IMWRITE_PNG_COMPRESSION, 9])
    print("wrote", path, f"{os.path.getsize(path) / 1e3:.0f} kB", sheet.shape[1], "x", sheet.shape[0])


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--video3", default=os.path.join(ROOT, "video", "3.avi"))
    ap.add_argument("--video4", default=os.path.join(ROOT, "video", "4.avi"))
    args = ap.parse_args()
    os.makedirs(OUT, exist_ok=True)
    demo_gif(args.video4)
    detector_stages(args.video3)


if __name__ == "__main__":
    main()
