"""Exercise the gate's PASS/FAIL contract using synthetic tool output."""
import contextlib
import io
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import regression


class RegressionGateTest(unittest.TestCase):
    report = ("| robot EKF (whole-robot model) | sample | 67.5% | 10.6 |\n"
              "| robot EKF | 0.32 s | sample | 10 | 14.3 |\n")

    def test_outcomes(self):
        cases = ("complete", "missing sim", "failed sim", "partial output", "missing row",
                 "duplicate row", "missing report", "stale report", "malformed report",
                 "failed video", "metric regression")
        for case in cases:
            with self.subTest(case=case), tempfile.TemporaryDirectory() as tmp, contextlib.chdir(tmp):
                video = Path("video/3.avi")
                video.parent.mkdir()
                video.touch()
                Path("reader").touch()
                if case != "missing sim":
                    Path("sim").touch()
                report = Path("3_report.md")
                if case == "stale report":
                    report.write_text(self.report)

                def run(command, **kwargs):
                    if Path(command[0]).name == "reader":
                        if case not in {"missing report", "stale report"}:
                            report.write_text("broken" if case == "malformed report" else
                                              self.report.replace("67.5%", "0%") if case == "metric regression"
                                              else self.report)
                        return subprocess.CompletedProcess(command, int(case == "failed video"), "", "")
                    rows = ["scenario"] * 7 + ["no spin, strafe"]
                    if case == "partial output":
                        rows.pop()
                    elif case == "missing row":
                        rows[-1] = "other"
                    elif case == "duplicate row":
                        rows[0] = rows[-1]
                    output = "\n".join(f"| {name} | 1 | 2 | 80 |" for name in rows)
                    return subprocess.CompletedProcess(command, int(case == "failed sim"), output, "")

                args = ["gate", "--videos", "3", "--out", ".", "--exe", "reader", "--sim-exe", "sim"]
                stdout = io.StringIO()
                with patch.object(sys, "argv", args), patch.object(subprocess, "run", run), \
                        contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(io.StringIO()), \
                        patch.object(regression, "default_video_dir", return_value=video.parent):
                    self.assertEqual(regression.main(), 0 if case == "complete" else 1)
                    self.assertEqual("RESULT: PASS" in stdout.getvalue(), case == "complete")


if __name__ == "__main__":
    unittest.main()
