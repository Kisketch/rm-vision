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


if __name__ == "__main__":
    hit_rates()
    field_conditions()
    latency()
    nis_adapt()
    sigma_coverage()
    error_budget()
