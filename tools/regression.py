#!/usr/bin/env python3
"""Regression gate over the two real videos and the simulator.

Reads the numbers BASELINE.md section 10 froze, re-runs `autoaim_video` on both
recordings (reusing the detection cache) and `autoaim_sim`, and fails if any
metric drops past its tolerance. This is the manual gate that CI cannot run,
because the videos are not in the repository.

Section 10 was frozen with core/config/video_reference.yaml (the N0 reference:
zero distortion, assumed pinhole intrinsics), NOT with the shipped default.yaml.
Running the gate without --config therefore compared the live configuration
against N0 numbers and could never pass. --config now defaults to the N0 file.

    python3 tools/regression.py                 # uses build/Release on Windows
    python3 tools/regression.py --exe build/apps/autoaim_video
    python3 tools/regression.py --config core/config/default.yaml   # live config
    python3 tools/regression.py --sim-ref 74.5                      # other machine

Exit code 0 = no regression, 1 = regression or missing input.
"""

from __future__ import annotations

import argparse
import math
import re
import subprocess
import sys
from pathlib import Path

# Frozen references (BASELINE.md sections 10 and 11). Each configuration has its
# own reference set: the N0 reference config and the shipped live config differ
# by ~10 pp on the same build, so comparing across them is meaningless.
# 2026-11-11: default.yaml refrozen for the split-bar merge (BASELINE.md
# section 12; video 3 gains 14 close-range frames, video 4 is byte-identical).
# video_reference.yaml is frozen and does not enable the merge.
REFERENCE_BY_CONFIG = {
    "video_reference.yaml": {
        "3": {"hit": 67.5, "pred_032": 14.3, "hits_per_s": 10.6},
        "4": {"hit": 68.6, "pred_032": 13.3, "hits_per_s": 10.1},
    },
    "default.yaml": {
        "3": {"hit": 58.1, "pred_032": 15.2, "hits_per_s": 9.1},
        "4": {"hit": 66.9, "pred_032": 12.4, "hits_per_s": 8.8},
    },
}
DEFAULT_CONFIG = "core/config/video_reference.yaml"
# The simulator mean is machine dependent (BASELINE.md section 8: compare only
# same machine, same binary, +-3 pp). 78.1 is the reference machine; this one
# measures 74.5 with unmodified code, so pass --sim-ref 74.5 here.
SIM_MEAN_REF = 78.1
TOL_HIT_PP = 1.5
TOL_PRED_PX = 1.5
TOL_HPS = 0.5
TOL_SIM_PP = 3.0


def default_exe() -> Path:
    for p in ("build/apps/Release/autoaim_video", "build/apps/autoaim_video"):
        if Path(p + ".exe").is_file():
            return Path(p + ".exe")
        if Path(p).is_file():
            return Path(p)
    return Path("build/apps/Release/autoaim_video.exe")


def default_video_dir() -> Path:
    for p in ("../template/template/video", "../video", "video"):
        if Path(p).is_dir():
            return Path(p)
    return Path("video")


def parse_report(path: Path) -> dict:
    text = path.read_text(encoding="utf-8", errors="replace")
    out: dict[str, float] = {}
    m = re.search(r"^\| robot EKF \(whole-robot model\) \|.*?\| ([\d.]+)% \| ([\d.]+) \|",
                  text, re.MULTILINE)
    if m:
        out["hit"] = float(m.group(1))
        out["hits_per_s"] = float(m.group(2))
    m = re.search(r"^\| robot EKF \| 0\.32 s \|.*?\| ([\d.]+) \| ([\d.]+) \|",
                  text, re.MULTILINE)
    if m:
        out["pred_032"] = float(m.group(2))
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description="regression gate against BASELINE section 10")
    ap.add_argument("--exe", default=str(default_exe()))
    ap.add_argument("--videos", default="", help="逗号分隔；默认 3.avi,4.avi")
    ap.add_argument("--out", default="runs/regress")
    ap.add_argument("--sim-exe", default="", help="默认取 video exe 同目录的 autoaim_sim")
    ap.add_argument("--config", default=DEFAULT_CONFIG,
                    help="传给 autoaim_video 的配置（默认 N0 参考口径，见 BASELINE.md §10）")
    ap.add_argument("--sim-ref", type=float, default=SIM_MEAN_REF,
                    help="仿真均值基准（机器相关，见 BASELINE.md §8）")
    args = ap.parse_args()
    if not math.isfinite(args.sim_ref):
        ap.error("--sim-ref must be finite")

    exe = Path(args.exe)
    if not exe.is_file():
        print(f"找不到 autoaim_video: {exe}", file=sys.stderr)
        return 1
    reference = REFERENCE_BY_CONFIG.get(Path(args.config).name)
    if reference is None:
        print(f"没有为 {args.config} 冻结基准；已知：{sorted(REFERENCE_BY_CONFIG)}", file=sys.stderr)
        return 1

    vdir = default_video_dir()
    stems = [s.strip() for s in args.videos.split(",") if s.strip()] or ["3", "4"]
    videos = []
    for stem in stems:
        if stem not in reference:
            print(f"{stem}: 没有冻结基准", file=sys.stderr)
            return 1
        v = vdir / f"{stem}.avi"
        if not v.is_file():
            print(f"找不到视频: {v}", file=sys.stderr)
            return 1
        videos.append((stem, v))

    failures = 0
    print(f"{'video':10s} {'metric':12s} {'reference':>9s} {'measured':>9s} {'delta':>8s}  verdict")
    for stem, video in videos:
        report_path = Path(args.out) / f"{stem}_report.md"
        before = report_path.stat() if report_path.is_file() else None
        proc = subprocess.run([str(exe), str(video), "--out", args.out, "--config", args.config],
                              capture_output=True, text=True)
        if proc.returncode != 0:
            print(f"{stem}: 运行失败\n{proc.stderr[-500:]}", file=sys.stderr)
            return 1
        try:
            after = report_path.stat()
            if before and (before.st_mtime_ns, before.st_size, before.st_ino) == (
                    after.st_mtime_ns, after.st_size, after.st_ino):
                print(f"{report_path}: 本次运行未能确认报告已更新", file=sys.stderr)
                return 1
            got = parse_report(report_path)
        except (OSError, ValueError) as error:
            print(f"{report_path}: 报告读取失败: {error}", file=sys.stderr)
            return 1
        ref = reference.get(stem)
        if len(got) < 3 or not all(map(math.isfinite, got.values())):
            print(f"{stem}: 无法解析报告", file=sys.stderr)
            return 1
        for key, tol, unit in (("hit", TOL_HIT_PP, "%"), ("pred_032", TOL_PRED_PX, "px"),
                               ("hits_per_s", TOL_HPS, "/s")):
            delta = got[key] - ref[key]
            ok = delta >= -tol if key != "pred_032" else delta <= tol
            failures += 0 if ok else 1
            print(f"{stem + '.avi':10s} {key:12s} {ref[key]:9.1f} {got[key]:9.1f} "
                  f"{delta:+8.1f}  {'PASS' if ok else 'FAIL'}")

    sim_exe = Path(args.sim_exe) if args.sim_exe else exe.with_name(
        "autoaim_sim.exe" if exe.name.endswith(".exe") else "autoaim_sim")
    if sim_exe.is_file():
        proc = subprocess.run([str(sim_exe)], capture_output=True, text=True)
        if proc.returncode:
            print(f"sim: 退出码 {proc.returncode}\n{proc.stderr[-500:]}", file=sys.stderr)
            return 1
        hits = [float(m) for m in re.findall(r"^\| .*? \| [\d.]+ \| [\d.]+ \| ([\d.]+) \|",
                                             proc.stdout, re.MULTILINE)]
        if len(hits) == 8 and all(map(math.isfinite, hits)):
            mean = sum(hits) / 8
            ok = mean >= args.sim_ref - TOL_SIM_PP
            failures += 0 if ok else 1
            print(f"{'sim':10s} {'mean_hit':12s} {args.sim_ref:9.1f} {mean:9.1f} "
                  f"{mean - args.sim_ref:+8.1f}  {'PASS' if ok else 'FAIL'}")
            # The mean masks the weakest scenario: pure translation (no spin,
            # strafe) is where the whole-robot EKF trails even the direct
            # strategy (11.4% vs 14.2% on two independent machines; the sim is
            # seed-deterministic). Gate that row on its own so a further
            # collapse cannot hide inside a healthy mean. The 11.4 reference
            # is for the sim's default 20 s run, which is what this gate uses.
            rows = re.findall(r"^\| (no spin, strafe[^|]*)\| [\d.]+ \| [\d.]+ \| ([\d.]+) \|",
                              proc.stdout, re.MULTILINE)
            if len(rows) == 1:
                strafe = float(rows[0][1])
                ok = strafe >= 11.4 - TOL_SIM_PP
                failures += 0 if ok else 1
                print(f"{'sim':10s} {'strafe_hit':12s} {11.4:9.1f} {strafe:9.1f} "
                      f"{strafe - 11.4:+8.1f}  {'PASS' if ok else 'FAIL'}")
            else:
                print(f"sim: 专项检查行数为 {len(rows)}，应为 1", file=sys.stderr)
                return 1
        else:
            print(f"sim: 解析到 {len(hits)} 个场景（应为 8）", file=sys.stderr)
            return 1
    else:
        print(f"无法完成仿真检查：找不到 {sim_exe}", file=sys.stderr)
        return 1

    print("\nRESULT: PASS" if failures == 0 else f"\nRESULT: FAIL ({failures} 项)")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        print(f"检查失败: {error}", file=sys.stderr)
        sys.exit(1)
