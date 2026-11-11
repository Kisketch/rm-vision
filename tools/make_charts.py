#!/usr/bin/env python3
"""Generate the repository's data charts (docs/charts/*.svg).

Every number below is copied from a verified in-repo source (named in
each chart's subtitle and in docs/performance_charts.md).  If a baseline
moves, update the data here and re-run:  python3 tools/make_charts.py
Pure stdlib; the SVGs are text so they diff and render on GitHub.
"""
import os

OUT = os.path.join(os.path.dirname(__file__), "..", "docs", "charts")

BLUE, TEAL, AMBER, RED, GRAY, GRID, INK = (
    "#2563eb", "#0d9488", "#d97706", "#dc2626", "#9ca3af", "#e5e7eb", "#111827")
FONT = "font-family=\"'Segoe UI', Helvetica, Arial, sans-serif\""


def open_svg(w, h, title, subtitle):
    return [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" '
        f'viewBox="0 0 {w} {h}" {FONT}>',
        f'<rect width="{w}" height="{h}" fill="white"/>',
        f'<text x="24" y="30" font-size="17" font-weight="bold" fill="{INK}">{title}</text>',
        f'<text x="24" y="50" font-size="11.5" fill="#4b5563">{subtitle}</text>',
    ]


def close_svg(parts, path):
    parts.append("</svg>")
    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, path), "w", encoding="utf-8") as f:
        f.write("\n".join(parts) + "\n")
    print("wrote", path)


def y_axis(p, x0, y0, y1, vmax, ticks, unit=""):
    for t in ticks:
        y = y1 - (t / vmax) * (y1 - y0)
        p.append(f'<line x1="{x0}" y1="{y:.0f}" x2="{x0 + 820}" y2="{y:.0f}" stroke="{GRID}"/>')
        p.append(f'<text x="{x0 - 6}" y="{y + 4:.0f}" font-size="10.5" text-anchor="end" fill="#4b5563">{t}{unit}</text>')


# ---------------------------------------------------------------- hit rates
def hit_rates():
    series = [  # (label, colour, 3.avi, 4.avi) — BASELINE.md / MEASUREMENTS.md
        ("N0 reference, conditional", BLUE, 67.5, 68.6),
        ("N0 reference, conservative", "#93c5fd", 65.1, 67.1),
        ("Live calibre, conditional", TEAL, 57.3, 66.9),
        ("Live calibre, conservative", "#2dd4bf", 55.3, 65.3),
        ("Sentry config, conditional", AMBER, 67.6, 71.9),
        ("Point KF, conditional", RED, 29.6, 43.1),
    ]
    W, H, x0, y0, y1, vmax = 920, 540, 70, 76, 428, 80.0
    p = open_svg(W, H, "Hit rate by configuration and calibre (%)",
                 "Offline video evaluation; conditional = hits / verifiable shots, conservative = hits / all shots. Live-calibre videos were not shot with the team camera.")
    y_axis(p, x0, y0, y1, vmax, [0, 20, 40, 60, 80], "%")
    bw, gap = 34, 6
    for gi, video in enumerate(["3.avi", "4.avi"]):
        gx = x0 + 90 + gi * 400
        for si, (label, colour, v3, v4) in enumerate(series):
            v = v3 if gi == 0 else v4
            x = gx + si * (bw + gap)
            y = y1 - (v / vmax) * (y1 - y0)
            p.append(f'<rect x="{x}" y="{y:.0f}" width="{bw}" height="{y1 - y:.0f}" fill="{colour}"/>')
            p.append(f'<text x="{x + bw / 2}" y="{y - 4:.0f}" font-size="9.5" text-anchor="middle" fill="{INK}">{v}</text>')
        p.append(f'<text x="{gx + 3 * (bw + gap):.0f}" y="{y1 + 20}" font-size="13" text-anchor="middle" fill="{INK}">{video}</text>')
    lx = x0 + 10
    for si, (label, colour, _, _) in enumerate(series):
        x = lx + (si % 3) * 300
        y = y1 + 48 + (si // 3) * 20
        p.append(f'<rect x="{x}" y="{y - 10}" width="12" height="12" fill="{colour}"/>')
        p.append(f'<text x="{x + 18}" y="{y}" font-size="11" fill="{INK}">{label}</text>')
    close_svg(p, "hit_rates.svg")


# ------------------------------------------------------- field conditions
def field_conditions():
    # FIELD_CONDITIONS_2026-11-11.md, conditional hit rate, synthetic stress
    lines = [("3.avi (slow target)", TEAL, [67.5, 68.3, 66.3]),
             ("4.avi (fast target)", BLUE, [68.6, 62.1, 55.3])]
    W, H, x0, y0, y1 = 920, 500, 70, 76, 400
    vmin, vmax = 40.0, 75.0
    p = open_svg(W, H, "Robustness under field stress (conditional hit rate, %)",
                 "Synthetic stress injected into cached detections (not on-robot data). Vertical axis starts at 40%. Source: docs/FIELD_CONDITIONS_2026-11-11.md")
    for t in [40, 50, 60, 70]:
        y = y1 - (t - vmin) / (vmax - vmin) * (y1 - y0)
        p.append(f'<line x1="{x0}" y1="{y:.0f}" x2="{x0 + 820}" y2="{y:.0f}" stroke="{GRID}"/>')
        p.append(f'<text x="{x0 - 6}" y="{y + 4:.0f}" font-size="10.5" text-anchor="end" fill="#4b5563">{t}%</text>')
    xs = [x0 + 110, x0 + 420, x0 + 730]
    for x, lab in zip(xs, ["0% dropped", "20% dropped", "40% dropped"]):
        p.append(f'<text x="{x}" y="{y1 + 20}" font-size="12" text-anchor="middle" fill="{INK}">{lab}</text>')
    for label, colour, vals in lines:
        pts = " ".join(f"{x},{y1 - (v - vmin) / (vmax - vmin) * (y1 - y0):.0f}" for x, v in zip(xs, vals))
        p.append(f'<polyline points="{pts}" fill="none" stroke="{colour}" stroke-width="2.5"/>')
        for x, v in zip(xs, vals):
            y = y1 - (v - vmin) / (vmax - vmin) * (y1 - y0)
            p.append(f'<circle cx="{x}" cy="{y:.0f}" r="4.5" fill="{colour}"/>')
            p.append(f'<text x="{x}" y="{y - 10:.0f}" font-size="11" text-anchor="middle" fill="{INK}">{v}</text>')
        p.append(f'<text x="{xs[-1] + 16}" y="{y1 - (vals[-1] - vmin) / (vmax - vmin) * (y1 - y0) + 4:.0f}" font-size="11.5" fill="{colour}">{label}</text>')
    for x, v, lab in [(xs[1], 49.0, "4.avi + 10 ms timestamp jitter: 49.0"),
                      (xs[2], 55.2, "4.avi at 12.5 fps: 55.2")]:
        y = y1 - (v - vmin) / (vmax - vmin) * (y1 - y0)
        p.append(f'<rect x="{x - 6}" y="{y - 6:.0f}" width="12" height="12" transform="rotate(45 {x} {y:.0f})" fill="{RED}"/>')
        p.append(f'<text x="{x}" y="{y + 24:.0f}" font-size="11" text-anchor="middle" fill="{RED}">{lab}</text>')
    close_svg(p, "field_conditions.svg")


# ---------------------------------------------------------------- latency
def latency():
    panels = [  # (title, unit, legacy, current, note)
        ("Pose solve", "us", 1302, 72, "18x faster"),
        ("Ballistics solve", "us", 109, 40, "2.7x faster"),
        ("Detection / frame", "ms", 7.0, 1.6, "full frame -> tracking-guided ROI"),
    ]
    W, H = 920, 430
    p = open_svg(W, H, "Compute cost, legacy vs current",
                 "Detection figures are approximate (full frame 6.5-8 ms, ROI 1.3-1.9 ms). Sources: docs/hardening.md, docs/roi_detection.md")
    for i, (title, unit, old, new, note) in enumerate(panels):
        cx = 40 + i * 300
        vmax = old * 1.18
        y1, y0 = 330, 110
        p.append(f'<text x="{cx + 130}" y="92" font-size="13.5" font-weight="bold" text-anchor="middle" fill="{INK}">{title} ({unit})</text>')
        for j, (v, colour, lab) in enumerate([(old, GRAY, "legacy"), (new, BLUE, "current")]):
            x = cx + 40 + j * 120
            y = y1 - (v / vmax) * (y1 - y0)
            p.append(f'<rect x="{x}" y="{y:.0f}" width="76" height="{y1 - y:.0f}" fill="{colour}"/>')
            p.append(f'<text x="{x + 38}" y="{y - 6:.0f}" font-size="12" text-anchor="middle" fill="{INK}">{v:g}</text>')
            p.append(f'<text x="{x + 38}" y="{y1 + 18}" font-size="11" text-anchor="middle" fill="#4b5563">{lab}</text>')
        p.append(f'<text x="{cx + 130}" y="{y1 + 42}" font-size="11.5" text-anchor="middle" fill="{TEAL}">{note}</text>')
    close_svg(p, "latency.svg")




# ------------------------------------------------------------ NIS study
def nis_adapt():
    rows = [  # (label, delta pp) — docs/nis_adapt_study.md
        ("Simulation, pure strafe", 3.5),
        ("Simulation, spin + strafe", 7.0),
        ("Video N0, 3.avi", -2.0),
        ("Video N0, 4.avi", -4.5),
        ("Video live calibre, 3.avi", -0.6),
        ("Video live calibre, 4.avi", -2.3),
    ]
    W, H = 920, 470
    p = open_svg(W, H, "P2-10 NIS-adaptive q: change in hit rate (pp)",
                 "Helps in simulation, hurts on every real-video cell -> rejected for shipping. Source: docs/nis_adapt_study.md")
    zx, scale, y = 470, 34.0, 108
    p.append(f'<line x1="{zx}" y1="86" x2="{zx}" y2="392" stroke="{INK}" stroke-width="1.4"/>')
    for t in [-6, -3, 0, 3, 6]:
        x = zx + t * scale
        p.append(f'<text x="{x}" y="410" font-size="10.5" text-anchor="middle" fill="#4b5563">{t:+d}</text>')
    for label, v in rows:
        colour = TEAL if v > 0 else RED
        x = zx if v > 0 else zx + v * scale
        w = abs(v) * scale
        p.append(f'<rect x="{x:.0f}" y="{y}" width="{w:.0f}" height="30" fill="{colour}"/>')
        p.append(f'<text x="{zx - 14}" y="{y + 20}" font-size="12" text-anchor="end" fill="{INK}">{label}</text>')
        tx = zx + v * scale + (10 if v > 0 else -10)
        p.append(f'<text x="{tx:.0f}" y="{y + 20}" font-size="12" text-anchor="{"start" if v > 0 else "end"}" fill="{INK}">{v:+.1f}</text>')
        y += 46
    close_svg(p, "nis_adapt.svg")


# --------------------------------------------------------- sigma coverage
def sigma_coverage():
    # docs/sigma_coverage_study.md, reference calibre, ~57k simulated shots
    bars = [("1-sigma, all modes", 83.0, TEAL, "nominal 68.3"),
            ("2-sigma, all modes", 89.3, BLUE, "nominal 95.4"),
            ("1-sigma, pure strafe only", 16.0, RED, "range 15-17")]
    W, H, x0, y0, y1, vmax = 920, 470, 70, 84, 372, 100.0
    p = open_svg(W, H, "Sigma coverage: predicted vs actual (%)",
                 "The error distribution is a heavy-tailed mixture: no single scale calibrates 1-sigma and 2-sigma, and the weak mode hides inside the same sigma range -> fire gate stays OFF. Source: docs/sigma_coverage_study.md")
    y_axis(p, x0, y0, y1, vmax, [0, 25, 50, 75, 100], "%")
    for i, (label, v, colour, note) in enumerate(bars):
        x = x0 + 120 + i * 250
        y = y1 - (v / vmax) * (y1 - y0)
        p.append(f'<rect x="{x}" y="{y:.0f}" width="110" height="{y1 - y:.0f}" fill="{colour}"/>')
        p.append(f'<text x="{x + 55}" y="{y - 6:.0f}" font-size="13" text-anchor="middle" fill="{INK}">{v:g}</text>')
        p.append(f'<text x="{x + 55}" y="{y1 + 20}" font-size="11.5" text-anchor="middle" fill="{INK}">{label}</text>')
        p.append(f'<text x="{x + 55}" y="{y1 + 38}" font-size="10.5" text-anchor="middle" fill="#4b5563">{note}</text>')
    for nv, lab in [(68.3, "nominal 1-sigma 68.3"), (95.4, "nominal 2-sigma 95.4")]:
        y = y1 - (nv / vmax) * (y1 - y0)
        p.append(f'<line x1="{x0}" y1="{y:.0f}" x2="{x0 + 820}" y2="{y:.0f}" stroke="{AMBER}" stroke-dasharray="6 4"/>')
        p.append(f'<text x="{x0 + 824}" y="{y + 4:.0f}" font-size="10" fill="{AMBER}">{lab}</text>')
    close_svg(p, "sigma_coverage.svg")


# ----------------------------------------------------------- error budget
def error_budget():
    W, H = 920, 450
    p = open_svg(W, H, "Where the remaining error lives (live diagnosis)",
                 "Prediction, not gimbal execution, is the bottleneck. Source: CHANGELOG.md, Stage K diagnosis on both videos.")
    # panel A: median errors
    y1, y0, vmax = 320, 120, 20.0
    p.append(f'<text x="230" y="96" font-size="13.5" font-weight="bold" text-anchor="middle" fill="{INK}">Median error (px)</text>')
    for j, (v, colour, lab) in enumerate([(16.8, RED, "aim (prediction)"), (8.1, TEAL, "fire (execution)")]):
        x = 100 + j * 150
        y = y1 - (v / vmax) * (y1 - y0)
        p.append(f'<rect x="{x}" y="{y:.0f}" width="86" height="{y1 - y:.0f}" fill="{colour}"/>')
        p.append(f'<text x="{x + 43}" y="{y - 6:.0f}" font-size="12.5" text-anchor="middle" fill="{INK}">{v}</text>')
        p.append(f'<text x="{x + 43}" y="{y1 + 18}" font-size="11" text-anchor="middle" fill="#4b5563">{lab}</text>')
    # panel B: hit rate by aim-error bucket
    p.append(f'<text x="660" y="96" font-size="13.5" font-weight="bold" text-anchor="middle" fill="{INK}">Hit rate by aim error (%)</text>')
    vmax2 = 100.0
    for j, (v, colour, lab) in enumerate([(91.8, TEAL, "aim error &lt; 10 px"), (10.3, RED, "aim error 25-60 px")]):
        x = 530 + j * 150
        y = y1 - (v / vmax2) * (y1 - y0)
        p.append(f'<rect x="{x}" y="{y:.0f}" width="86" height="{y1 - y:.0f}" fill="{colour}"/>')
        p.append(f'<text x="{x + 43}" y="{y - 6:.0f}" font-size="12.5" text-anchor="middle" fill="{INK}">{v}</text>')
        p.append(f'<text x="{x + 43}" y="{y1 + 18}" font-size="11" text-anchor="middle" fill="#4b5563">{lab}</text>')
    close_svg(p, "error_budget.svg")



# ------------------------------------------- per-frame series data
# Bucketed (25 frames) medians/shares from evaluate() per-frame logs on
# the frozen detections (N0), generated 2026-11-11; CAL = (sigma bin
# centre, median aim error, frame count) for bins with n >= 8 shown.
V3_AIM = [33.0, 19.5, 12.4, 15.6, 36.1, 28.1, 23.4, 21.0, 12.0, 28.2, 31.5, 44.7, 27.1, 25.5, 21.0, 40.8, 26.1, 42.9, 44.3, 34.9, 55.9, 23.6, 21.7, 14.2, 13.2, 15.4, 16.0, 40.5, 30.1, 21.7, 21.7, 25.0, 18.2, 16.9, 30.4, 27.2, 29.7, 16.1, 11.9, 44.9, 19.4, 22.8, 18.4, 22.4, 26.9, 15.2, 53.9, 41.9, 53.1, 52.9, 26.8, 17.0, 74.7, 40.2, 31.5, 33.3, 14.6, 11.5, 34.8, 18.6, 9.0, 31.4, 12.3, 15.3, 14.8, 20.6, 14.6, 27.3, 1.6, 0.4, 38.4, 4.7, 11.7, 6.7, 10.3, 5.6, 16.1, 7.0, 11.6, 18.3, 9.0, 4.1, 4.8, 4.0, 0.6, 4.7, 0.6, 0.5, 0.0]
V3_FIRE = [6.0, 10.2, 7.9, 6.6, 6.9, 7.5, 7.6, 6.8, 5.8, 8.2, 7.0, 5.7, 9.6, 6.7, 11.2, 12.9, 9.9, 7.7, 8.2, 7.8, 10.9, 14.0, 13.1, 11.1, 7.4, 7.6, 9.0, 12.0, 8.2, 8.2, 9.2, 7.3, 7.1, 7.1, 6.6, 7.1, 9.8, 5.4, 22.0, 6.9, 7.4, 7.0, 7.4, 6.4, 7.3, 7.9, 9.8, 8.7, 4.7, 7.3, 6.0, 7.1, 6.6, 8.0, 9.2, 8.7, 7.7, 7.2, 10.5, 7.2, 7.7, 9.0, 6.8, 6.6, 6.3, 7.3, 9.0, 7.5, 0.2, 0.1, 2.5, 1.0, 5.6, 2.3, 3.4, 4.6, 4.6, 7.0, 8.3, 3.5, 4.7, 4.0, 14.1, 1.0, 0.2, 3.3, 0.1, 0.2, 0.0]
V3_SIG = [52.0, 48.8, 49.0, 48.6, 49.3, 50.3, 49.5, 50.4, 50.3, 49.9, 49.1, 48.4, 49.6, 49.4, 48.3, 49.8, 51.1, 48.8, 48.9, 49.9, 50.4, 50.1, 48.5, 49.3, 49.0, 48.5, 49.8, 49.6, 49.5, 51.7, 50.7, 49.4, 50.4, 49.0, 49.5, 49.5, 49.0, 54.5, 60.0, 50.1, 49.0, 50.9, 49.4, 51.1, 49.7, 50.4, 50.0, 47.8, 48.4, 48.9, 48.3, 49.0, 48.1, 48.6, 49.9, 49.2, 50.6, 50.3, 51.2, 51.4, 49.0, 48.0, 49.9, 50.2, 50.1, 49.8, 50.0, 50.4, 47.7, 47.7, 47.5, 48.1, 48.0, 47.2, 47.2, 46.9, 47.2, 45.4, 44.6, 43.5, 43.5, 43.0, 43.7, 44.9, 44.9, 45.2, 45.6, 45.5, 45.5]
V3_TRK = [88, 88, 92, 96, 92, 96, 100, 88, 96, 100, 88, 100, 80, 92, 96, 88, 96, 92, 100, 88, 96, 100, 100, 92, 88, 92, 100, 100, 92, 84, 92, 92, 92, 88, 92, 92, 96, 48, 44, 76, 96, 96, 92, 84, 96, 96, 96, 92, 88, 100, 96, 100, 96, 96, 96, 88, 100, 92, 88, 100, 96, 96, 96, 100, 92, 96, 96, 92, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 88, 96, 84, 80, 96, 100, 100, 100, 100, 100, 100]
V3_CUM = [3, 7, 16, 29, 33, 42, 51, 63, 75, 80, 89, 95, 98, 104, 112, 112, 117, 125, 129, 134, 137, 146, 150, 159, 167, 178, 190, 196, 202, 210, 216, 228, 240, 251, 255, 263, 264, 267, 268, 270, 281, 291, 305, 317, 325, 339, 344, 345, 350, 350, 356, 369, 375, 376, 383, 386, 400, 410, 419, 432, 445, 452, 471, 484, 500, 510, 524, 530, 549, 574, 582, 606, 626, 648, 667, 692, 705, 730, 745, 758, 774, 789, 812, 837, 862, 887, 912, 937, 937]
V3_CAL = [(35, 0.0, 0), (45, 15.7, 1400), (55, 24.2, 677), (70, 31.1, 32), (90, 13.9, 9)]
V4_AIM = [58.8, 34.7, 20.5, 18.5, 22.6, 17.2, 16.9, 21.5, 42.1, 15.9, 24.2, 20.1, 12.6, 25.1, 22.4, 19.5, 19.4, 41.5, 12.4, 11.6, 19.0, 13.5, 10.1, 14.0, 12.1, 14.9, 16.3, 19.2, 11.2, 26.5, 55.0, 22.4, 13.8, 23.8, 7.9, 18.7, 11.9, 24.8, 16.3, 14.3, 21.7, 20.1, 11.1, 11.1, 27.9, 40.8, 14.8, 17.6, 14.1, 13.0, 11.4, 13.9, 18.4, 11.4, 13.3, 37.1, 17.9, 23.3, 13.1, 28.2, 11.3, 20.8, 26.0, 14.8, 19.9, 21.8, 36.0, 36.7, 33.0, 13.7, 21.3, 13.7, 16.9, 34.5, 14.4, 42.9, 18.9, 22.2, 23.6, 13.6, 15.6, 20.5, 15.2, 14.0, 11.2, 11.5, 28.0, 21.7, 18.8, 10.3, 17.5, 16.9, 42.8, 14.6, 13.5, 21.9, 18.8, 15.6, 10.9, 19.1, 11.3, 12.9, 23.3, 19.8, 14.2, 22.7, 13.7, 36.6, 17.6, 11.4, 26.6, 24.0, 18.6, 13.9, 10.1, 10.7, 18.7, 14.4, 24.4, 28.1, 25.0, 14.7, 34.3, 25.9, 22.6, 23.1, 12.6, 20.3, 14.9, 30.1, 5.5, 10.8, 11.6, 7.9, 18.8, 58.3, 5.4]
V4_FIRE = [8.7, 9.2, 9.8, 7.3, 8.7, 9.2, 7.8, 6.3, 6.6, 6.7, 6.6, 8.4, 13.3, 10.6, 11.0, 10.6, 6.5, 8.2, 7.9, 9.0, 8.5, 7.9, 7.4, 9.2, 10.3, 8.0, 8.3, 6.5, 7.7, 7.1, 8.7, 7.4, 8.5, 6.8, 5.6, 7.5, 6.6, 8.2, 8.2, 8.0, 10.0, 9.1, 8.5, 5.9, 9.2, 10.0, 6.0, 8.6, 8.4, 8.2, 9.6, 8.8, 9.6, 11.1, 7.3, 12.1, 8.6, 8.3, 10.3, 0.0, 7.4, 7.6, 9.3, 9.8, 10.4, 7.4, 11.5, 8.3, 8.2, 9.6, 7.7, 7.4, 7.3, 8.8, 7.0, 10.4, 8.9, 8.1, 8.8, 7.2, 8.6, 7.6, 7.9, 10.1, 9.3, 9.0, 7.9, 9.9, 8.4, 8.7, 7.8, 6.2, 9.0, 8.0, 10.4, 6.8, 7.9, 9.7, 8.8, 7.6, 9.6, 8.2, 8.2, 8.9, 9.2, 9.8, 10.9, 8.8, 9.1, 7.5, 9.4, 8.8, 7.9, 7.4, 8.7, 7.8, 7.5, 9.1, 7.8, 10.9, 8.6, 10.1, 9.3, 7.7, 9.0, 7.8, 9.2, 9.5, 10.1, 6.7, 8.7, 8.7, 9.5, 10.0, 7.9, 19.7, 1.1]
V4_SIG = [52.6, 50.6, 49.7, 52.0, 50.5, 50.0, 50.6, 50.3, 50.7, 50.1, 49.8, 50.3, 51.1, 49.3, 49.3, 50.2, 50.6, 51.5, 51.3, 50.1, 49.5, 50.2, 49.8, 48.9, 50.2, 49.3, 50.0, 50.4, 49.2, 49.8, 50.6, 51.7, 51.3, 49.9, 50.5, 49.9, 50.1, 49.5, 49.8, 49.5, 49.7, 49.1, 49.1, 49.6, 49.6, 49.8, 50.7, 50.5, 50.1, 50.8, 49.6, 50.3, 49.4, 50.2, 49.5, 49.7, 49.8, 49.5, 51.2, 74.0, 49.7, 49.4, 49.0, 49.3, 49.5, 49.5, 50.0, 50.2, 49.5, 49.9, 49.4, 49.4, 49.5, 49.7, 49.4, 49.3, 48.8, 49.4, 49.5, 49.8, 50.0, 49.5, 48.8, 49.7, 50.1, 49.5, 50.8, 65.0, 49.4, 50.4, 49.9, 49.6, 49.0, 49.4, 49.1, 49.1, 49.8, 49.1, 50.2, 49.5, 48.9, 49.8, 49.5, 49.9, 49.1, 50.1, 49.1, 49.2, 48.8, 49.5, 50.5, 50.2, 49.0, 50.0, 49.4, 49.8, 49.5, 49.9, 49.7, 50.8, 49.8, 50.4, 49.9, 49.0, 49.3, 49.4, 49.4, 49.5, 49.9, 49.3, 50.8, 50.0, 50.0, 50.8, 49.3, 49.5, 47.5]
V4_TRK = [68, 96, 100, 88, 88, 100, 96, 84, 100, 92, 100, 92, 96, 92, 96, 96, 92, 96, 96, 92, 92, 84, 92, 96, 96, 100, 100, 92, 96, 92, 96, 96, 88, 96, 88, 92, 92, 92, 92, 96, 92, 96, 96, 92, 92, 92, 92, 96, 88, 96, 100, 96, 100, 96, 100, 100, 88, 100, 76, 4, 88, 96, 96, 100, 96, 92, 88, 88, 96, 96, 96, 100, 96, 96, 96, 100, 100, 100, 96, 92, 100, 96, 100, 96, 96, 84, 76, 84, 88, 88, 96, 84, 88, 96, 100, 92, 96, 100, 100, 92, 92, 92, 96, 92, 100, 88, 96, 92, 88, 96, 92, 96, 96, 88, 96, 96, 92, 96, 88, 100, 100, 92, 92, 92, 100, 100, 96, 100, 100, 96, 88, 92, 96, 92, 92, 88, 100]
V4_CUM = [2, 6, 17, 27, 34, 45, 57, 65, 72, 82, 96, 105, 112, 116, 120, 130, 136, 146, 161, 175, 185, 198, 213, 226, 238, 251, 262, 270, 285, 295, 299, 311, 320, 329, 339, 347, 361, 370, 382, 397, 407, 415, 427, 438, 448, 455, 469, 480, 493, 508, 519, 531, 540, 550, 562, 568, 579, 586, 592, 592, 605, 613, 621, 631, 640, 651, 655, 664, 670, 682, 696, 706, 718, 727, 739, 745, 754, 764, 775, 785, 795, 807, 815, 825, 836, 845, 850, 856, 869, 877, 887, 898, 902, 916, 927, 937, 948, 959, 973, 981, 993, 1008, 1019, 1031, 1043, 1051, 1063, 1070, 1080, 1094, 1102, 1110, 1122, 1132, 1147, 1162, 1170, 1181, 1190, 1197, 1209, 1222, 1229, 1241, 1252, 1264, 1278, 1289, 1302, 1308, 1320, 1332, 1344, 1356, 1366, 1368, 1374]
V4_CAL = [(35, 0.0, 0), (45, 18.0, 1793), (55, 17.1, 1447), (70, 28.9, 67), (90, 63.6, 8)]


# ------------------------------------------- per-frame series (bucketed)
def _trace(p, x0, y0, y1, vmax, values, colour, width=2.2):
    n = len(values)
    pts = " ".join(
        f"{x0 + i * 820 / (n - 1):.0f},{y1 - min(v, vmax) / vmax * (y1 - y0):.0f}"
        for i, v in enumerate(values))
    p.append(f'<polyline points="{pts}" fill="none" stroke="{colour}" stroke-width="{width}"/>')


def _panel_grid(p, x0, y0, y1, vmax, ticks, unit=""):
    for t in ticks:
        y = y1 - t / vmax * (y1 - y0)
        p.append(f'<line x1="{x0}" y1="{y:.0f}" x2="{x0 + 820}" y2="{y:.0f}" stroke="{GRID}"/>')
        p.append(f'<text x="{x0 - 6}" y="{y + 4:.0f}" font-size="10.5" text-anchor="end" fill="#4b5563">{t}{unit}</text>')


def aim_error_trace():
    W, H, x0 = 920, 560, 70
    p = open_svg(W, H, "Aim vs fire error over time (median per 1 s bucket, px)",
                 "Per-frame logs of evaluate() on the frozen detections (N0). Aim = prediction error at impact, fire = execution error. Spikes are spin / plate-switch segments.")
    for cx, colour, lab in [(560, RED, "aim (prediction)"), (700, TEAL, "fire (execution)"), (830, AMBER, "filter sigma")]:
        p.append(f'<line x1="{cx - 130}" y1="66" x2="{cx - 104}" y2="66" stroke="{colour}" stroke-width="2.5"/>')
        p.append(f'<text x="{cx - 98}" y="70" font-size="11" fill="{INK}">{lab}</text>')
    for (aim, fire, sig, lab, y0, y1) in [
            (V3_AIM, V3_FIRE, V3_SIG, "3.avi", 96, 268), (V4_AIM, V4_FIRE, V4_SIG, "4.avi", 336, 508)]:
        _panel_grid(p, x0, y0, y1, 40.0, [0, 10, 20, 30, 40])
        p.append(f'<text x="{x0}" y="{y0 - 8}" font-size="12.5" font-weight="bold" fill="{INK}">{lab}</text>')
        _trace(p, x0, y0, y1, 40.0, sig, AMBER, 1.6)
        _trace(p, x0, y0, y1, 40.0, fire, TEAL)
        _trace(p, x0, y0, y1, 40.0, aim, RED)
    p.append(f'<text x="{x0 + 410}" y="548" font-size="11" text-anchor="middle" fill="#4b5563">time (1 bucket = 25 frames = 1 s)</text>')
    close_svg(p, "aim_error_trace.svg")


def tracking_intensity():
    W, H, x0 = 920, 560, 70
    p = open_svg(W, H, "Engagement intensity over time: tracking share and cumulative hits",
                 "Per-frame logs of evaluate() on the frozen detections (N0). Teal = share of frames in TRACKING per second (left axis), blue = cumulative robot-EKF hits (right axis); the slope of the blue line is the hit intensity.")
    for (trk, cum, lab, y0, y1, cmax) in [
            (V3_TRK, V3_CUM, "3.avi", 96, 268, 1000), (V4_TRK, V4_CUM, "4.avi", 336, 508, 1500)]:
        _panel_grid(p, x0, y0, y1, 100.0, [0, 50, 100], "%")
        for t in (0, cmax // 2, cmax):
            y = y1 - t / cmax * (y1 - y0)
            p.append(f'<text x="{x0 + 826}" y="{y + 4:.0f}" font-size="10.5" fill="{BLUE}">{t}</text>')
        p.append(f'<text x="{x0}" y="{y0 - 8}" font-size="12.5" font-weight="bold" fill="{INK}">{lab}</text>')
        _trace(p, x0, y0, y1, 100.0, trk, TEAL)
        n = len(cum)
        pts = " ".join(f"{x0 + i * 820 / (n - 1):.0f},{y1 - v / cmax * (y1 - y0):.0f}" for i, v in enumerate(cum))
        p.append(f'<polyline points="{pts}" fill="none" stroke="{BLUE}" stroke-width="2.2"/>')
        p.append(f'<text x="{x0 + 824}" y="{y1 - cum[-1] / cmax * (y1 - y0) - 8:.0f}" font-size="11" fill="{BLUE}">{cum[-1]} hits</text>')
    p.append(f'<text x="{x0 + 410}" y="548" font-size="11" text-anchor="middle" fill="#4b5563">time (1 bucket = 25 frames = 1 s)</text>')
    close_svg(p, "tracking_intensity.svg")


def pred_horizon():
    series = [  # evaluate() predictions on frozen detections, median px
        ("robot EKF, 3.avi", BLUE, [5.0, 8.6, 14.3]),
        ("robot EKF, 4.avi", TEAL, [6.8, 9.5, 13.3]),
        ("point KF, 3.avi", RED, [9.7, 29.7, 66.4]),
        ("point KF, 4.avi", AMBER, [73.6, 91.0, 84.9]),
    ]
    W, H, x0, y0, y1, vmax = 920, 470, 70, 76, 392, 100.0
    p = open_svg(W, H, "Prediction error grows with horizon (median px)",
                 "Whole-robot EKF vs point KF on the frozen detections (N0). The point model cannot follow spin: its error explodes with horizon, the robot model's grows gently.")
    _panel_grid(p, x0, y0, y1, vmax, [0, 25, 50, 75, 100])
    xs = [x0 + 120, x0 + 420, x0 + 720]
    for x, lab in zip(xs, ["0.12 s", "0.20 s", "0.32 s"]):
        p.append(f'<text x="{x}" y="{y1 + 20}" font-size="12" text-anchor="middle" fill="{INK}">{lab}</text>')
    for label, colour, vals in series:
        pts = " ".join(f"{x},{y1 - v / vmax * (y1 - y0):.0f}" for x, v in zip(xs, vals))
        p.append(f'<polyline points="{pts}" fill="none" stroke="{colour}" stroke-width="2.5"/>')
        for x, v in zip(xs, vals):
            y = y1 - v / vmax * (y1 - y0)
            p.append(f'<circle cx="{x}" cy="{y:.0f}" r="4.5" fill="{colour}"/>')
            p.append(f'<text x="{x}" y="{y - 10:.0f}" font-size="11" text-anchor="middle" fill="{INK}">{v}</text>')
        p.append(f'<text x="{xs[-1] + 14}" y="{y1 - vals[-1] / vmax * (y1 - y0) + 4:.0f}" font-size="11.5" fill="{colour}">{label}</text>')
    close_svg(p, "pred_horizon.svg")


def sigma_calibration():
    W, H, x0, y0, y1 = 920, 470, 70, 76, 392
    p = open_svg(W, H, "Filter sigma vs actual aim error (binned medians, px)",
                 "Per-frame logs of evaluate() on the frozen detections. Sigma concentrates at 43-54 px while the actual error is 16-24 px: heavily over-covered and only weakly ordered - the P2-9 verdict in one picture.")
    _panel_grid(p, x0, y0, y1, 80.0, [0, 20, 40, 60, 80])
    def X(s): return x0 + (s - 30) / 55 * 820
    def Y(e): return y1 - e / 80 * (y1 - y0)
    p.append(f'<line x1="{X(30):.0f}" y1="{Y(30):.0f}" x2="{X(80):.0f}" y2="{Y(80):.0f}" stroke="{GRAY}" stroke-dasharray="6 4"/>')
    p.append(f'<text x="{X(80) - 4:.0f}" y="{Y(80) - 8:.0f}" font-size="10.5" text-anchor="end" fill="#4b5563">calibrated (error = sigma)</text>')
    for cal, colour, lab in [(V3_CAL, BLUE, "3.avi"), (V4_CAL, TEAL, "4.avi")]:
        pts = [(c, e) for c, e, n in cal if n >= 30]
        line = " ".join(f"{X(c):.0f},{Y(e):.0f}" for c, e in pts)
        p.append(f'<polyline points="{line}" fill="none" stroke="{colour}" stroke-width="2.5"/>')
        for c, e in pts:
            p.append(f'<circle cx="{X(c):.0f}" cy="{Y(e):.0f}" r="4.5" fill="{colour}"/>')
            p.append(f'<text x="{X(c):.0f}" y="{Y(e) - 10:.0f}" font-size="11" text-anchor="middle" fill="{INK}">{e}</text>')
        p.append(f'<text x="{X(pts[-1][0]) + 12:.0f}" y="{Y(pts[-1][1]) + 4:.0f}" font-size="11.5" fill="{colour}">{lab}</text>')
    for s in (40, 50, 60, 70, 80):
        p.append(f'<text x="{X(s):.0f}" y="{y1 + 20}" font-size="11" text-anchor="middle" fill="{INK}">sigma {s}</text>')
    close_svg(p, "sigma_calibration.svg")


def detector_tuning():
    series = [  # (label, colour, 3.avi, 4.avi) - docs/detector_tuning_study.md
        ("Baseline (shipped)", BLUE, 67.5, 68.6),
        ("Adaptive thr ON", AMBER, 66.5, 67.8),
        ("Fixed thr 60", TEAL, 67.5, 67.2),
        ("Fixed thr 80", GRAY, 66.1, 68.8),
        ("Corner refine OFF", RED, 63.4, 65.6),
        ("Classify thr 0.6", "#93c5fd", 67.3, 68.5),
    ]
    W, H, x0, y0, y1 = 920, 540, 70, 76, 428
    vmin, vmax = 55.0, 72.0
    p = open_svg(W, H, "Detector tuning A/B: conditional hit rate (%)",
                 "Fresh detection + full downstream evaluation per variant (N0). Vertical axis starts at 55%. Nothing beat the shipped settings; removing corner refinement is the only big mover - downwards. Source: docs/detector_tuning_study.md")
    for t in [55, 60, 65, 70]:
        y = y1 - (t - vmin) / (vmax - vmin) * (y1 - y0)
        p.append(f'<line x1="{x0}" y1="{y:.0f}" x2="{x0 + 820}" y2="{y:.0f}" stroke="{GRID}"/>')
        p.append(f'<text x="{x0 - 6}" y="{y + 4:.0f}" font-size="10.5" text-anchor="end" fill="#4b5563">{t}%</text>')
    bw, gap = 34, 6
    for gi, video in enumerate(["3.avi", "4.avi"]):
        gx = x0 + 90 + gi * 400
        for si, (label, colour, v3, v4) in enumerate(series):
            v = v3 if gi == 0 else v4
            x = gx + si * (bw + gap)
            y = y1 - (v - vmin) / (vmax - vmin) * (y1 - y0)
            p.append(f'<rect x="{x}" y="{y:.0f}" width="{bw}" height="{y1 - y:.0f}" fill="{colour}"/>')
            p.append(f'<text x="{x + bw / 2}" y="{y - 4:.0f}" font-size="9.5" text-anchor="middle" fill="{INK}">{v}</text>')
        p.append(f'<text x="{gx + 3 * (bw + gap):.0f}" y="{y1 + 20}" font-size="13" text-anchor="middle" fill="{INK}">{video}</text>')
    lx = x0 + 10
    for si, (label, colour, _, _) in enumerate(series):
        x = lx + (si % 3) * 300
        y = y1 + 48 + (si // 3) * 20
        p.append(f'<rect x="{x}" y="{y - 10}" width="12" height="12" fill="{colour}"/>')
        p.append(f'<text x="{x + 18}" y="{y}" font-size="11" fill="{INK}">{label}</text>')
    close_svg(p, "detector_tuning.svg")



if __name__ == "__main__":
    hit_rates()
    field_conditions()
    latency()
    nis_adapt()
    sigma_coverage()
    error_budget()
    aim_error_trace()
    tracking_intensity()
    pred_horizon()
    sigma_calibration()
    detector_tuning()
