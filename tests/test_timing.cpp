// Split from the former single-file test_main.cpp (round 6): same tests,
// grouped by module so a failure points at its subsystem.  Harness: check.hpp.
#include <cmath>
#include <cstdio>
#include <limits>
#include <random>
#include <utility>

#include "autoaim/aimer.hpp"
#include "autoaim/ballistics.hpp"
#include "autoaim/config.hpp"
#include "autoaim/delay_calibration.hpp"
#include "autoaim/detector.hpp"
#include "autoaim/evaluation.hpp"
#include "autoaim/keypoint_detector.hpp"
#include "autoaim/pose.hpp"
#include "autoaim/pose_buffer.hpp"
#include "autoaim/tracker.hpp"

#include "check.hpp"

using namespace autoaim;

void testPoseBuffer() {
    // Gimbal spinning at 3 rad/s (the G-02 reference case), pose samples at
    // 200 Hz like a serial IMU stream, frames queried at in-between times.
    const double w = 3.0;
    PoseBuffer buf(512, 0.05);
    for (int n = 0; n <= 440; ++n) {  // cover the last queried frame stamp (2.063 s)
        const double t = n * 0.005;
        buf.push({t, quatFromYawPitchRoll(w * t, 0.0, 0.0)});
    }
    double max_aligned = 0, max_latest = 0;
    for (int n = 0; n < 50; ++n) {
        const double t = 0.1 + n * 0.04 + 0.003;  // 25 fps frames, off-sample phase
        Quat q;
        CHECK(buf.query(t, q), "pose query at %f", t);
        max_aligned = std::max(max_aligned, quatAngleBetween(q, quatFromYawPitchRoll(w * t, 0, 0)));
        // The pre-G-02 behaviour: newest sample not newer than the frame.
        const double t_latest = std::floor(t / 0.005) * 0.005;
        max_latest = std::max(max_latest, quatAngleBetween(quatFromYawPitchRoll(w * t_latest, 0, 0),
                                                           quatFromYawPitchRoll(w * t, 0, 0)));
    }
    // SLERP between exact samples of a constant-rate rotation is exact; the
    // G-02 alignment budget is 0.3 ms == 0.0009 rad at 3 rad/s.
    CHECK(max_aligned < 9e-4, "aligned error %g rad exceeds the G-02 budget", max_aligned);
    CHECK(max_latest > 10 * max_aligned, "latest-sample error should dwarf aligned: %g vs %g", max_latest,
          max_aligned);
    std::printf("pose buffer: aligned err %.2e rad, latest-sample err %.2e rad at 3 rad/s\n", max_aligned,
                max_latest);

    Quat q;
    CHECK(!buf.query(-0.1, q), "query before coverage must fail");
    CHECK(!buf.query(2.3, q), "query after coverage must fail");

    PoseBuffer gappy(16, 0.02);
    gappy.push({0.0, quatFromYawPitchRoll(0, 0, 0)});
    gappy.push({0.1, quatFromYawPitchRoll(0.3, 0, 0)});
    double gap = 0;
    CHECK(!gappy.query(0.05, q, gap), "query across a dropped-frame gap must fail");
    CHECK(std::abs(gap - 0.1) < 1e-12, "reported gap %f", gap);

    PoseBuffer ooo(16, 0.05);  // out-of-order arrival still queries correctly
    ooo.push({0.010, quatFromYawPitchRoll(0.03, 0, 0)});
    ooo.push({0.000, quatFromYawPitchRoll(0.00, 0, 0)});
    ooo.push({0.005, quatFromYawPitchRoll(0.015, 0, 0)});
    CHECK(ooo.query(0.0075, q), "out-of-order query");
    CHECK(quatAngleBetween(q, quatFromYawPitchRoll(0.0225, 0, 0)) < 1e-9, "out-of-order slerp");
}

void testPoseBufferExtrapolation() {
    PoseBuffer strict(64, 0.05), loose(64, 0.05, 0.010);
    for (int i = 0; i < 10; ++i) {
        const double t = i * 0.005;  // 200 Hz, yaw rate 2 rad/s
        strict.push({t, quatFromYawPitchRoll(2.0 * t, 0.1, 0.0)});
        loose.push({t, quatFromYawPitchRoll(2.0 * t, 0.1, 0.0)});
    }
    Quat q;
    const double newest = 9 * 0.005;
    CHECK(!strict.query(newest + 0.004, q), "strict buffer must not extrapolate");
    CHECK(loose.query(newest + 0.004, q), "extrapolation within the limit");
    CHECK(quatAngleBetween(q, quatFromYawPitchRoll(2.0 * (newest + 0.004), 0.1, 0.0)) < 1e-6,
          "extrapolated attitude");
    CHECK(!loose.query(newest + 0.011, q), "beyond max_extrapolation must fail");
}

void testPoseBufferNonFinite() {
    // A corrupt serial frame can decode to NaN floats; such samples must be
    // dropped at push(), never stored (full-audit 2026-11 finding).
    PoseBuffer buf(64, 0.05, 0.010);
    const double nan = std::nan("");
    buf.push({0.000, quatFromYawPitchRoll(0.0, 0, 0)});
    buf.push({0.005, quatFromYawPitchRoll(0.01, 0, 0)});
    buf.push({nan, quatFromYawPitchRoll(0.02, 0, 0)});
    buf.push({0.010, {nan, 0, 0, 0}});
    buf.push({0.010, quatFromYawPitchRoll(0.02, 0, 0)});
    CHECK(buf.size() == 3, "non-finite samples dropped: %zu", buf.size());
    Quat q;
    CHECK(buf.query(0.0075, q), "query across the dropped samples");
    CHECK(std::isfinite(q[0]) && std::isfinite(q[1]) && std::isfinite(q[2]) && std::isfinite(q[3]),
          "queried pose stays finite");
    CHECK(buf.query(0.012, q), "extrapolation past a dropped NaN sample");
    CHECK(std::isfinite(q[0]), "extrapolated pose stays finite");
}

void testClockAligner() {
    ClockAligner al(64);
    std::mt19937 rng(2);
    std::normal_distribution<double> jitter(0, 0.001);
    const double true_offset = 12.345;
    for (int n = 0; n < 64; ++n) {
        const double remote = n * 0.005;
        double local = remote + true_offset + jitter(rng);
        if (n % 17 == 5) local += 0.05;  // transfer outlier the median must reject
        al.addPair(remote, local);
    }
    CHECK(al.ready(), "aligner ready");
    CHECK(std::abs(al.offset() - true_offset) < 0.002, "offset %f vs %f", al.offset(), true_offset);
    CHECK(std::abs(al.toLocal(100.0) - (100.0 + true_offset)) < 0.002, "toLocal");
    std::printf("clock aligner: offset error %.2e s\n", std::abs(al.offset() - true_offset));
}

void testClockAlignerDrift() {
    // Sustained-run behaviour (review request): the remote clock runs
    // 200 ppm fast for 60 simulated seconds at 165 Hz.  The aligner does
    // not estimate skew explicitly; its 64-pair sliding median must still
    // track the drifting offset.  A frozen offset would be 12 ms wrong
    // after 60 s; the tracked error must stay two orders below that.
    ClockAligner al(64);
    std::mt19937 rng(7);
    std::normal_distribution<double> jitter(0, 0.0005);
    const double skew = 200e-6;   // remote seconds per local second, beyond 1
    const double base = 0.012;    // initial offset, s
    double worst = 0.0;
    const int total = 60 * 165;
    for (int n = 0; n < total; ++n) {
        const double local = n / 165.0;
        const double remote = local * (1.0 + skew);
        al.addPair(remote, local + base + jitter(rng));
        if (n > 128) {  // steady state: check every converted stamp
            const double err = std::abs(al.toLocal(remote) - (local + base));
            worst = std::max(worst, err);
        }
    }
    CHECK(worst < 0.0005, "drift tracking worst error %.2e s over 60 s at 200 ppm", worst);
    std::printf("clock aligner drift: worst steady-state error %.2e s (200 ppm, 60 s)\n", worst);
}

void testDelayCalibration() {
    const double true_delay = 0.012;
    const cv::Vec3d plate(5.0, 0.3, 0.1);
    for (double amp : {0.4, 0.0}) {
        auto yp = [amp](double t) {
            return std::pair<double, double>(amp * std::sin(2 * kPi * 0.8 * t),
                                             0.25 * amp * std::sin(2 * kPi * 0.5 * t));
        };
        PoseBuffer att(4096, 0.05);
        for (int i = 0; i < 2400; ++i) {
            const double t = i * 0.005;
            const auto [y, p] = yp(t);
            att.push({t, quatFromYawPitchRoll(y, -p, 0.0)});
        }
        const auto lookup = [&att](double t, cv::Matx33d& R) {
            Quat q;
            if (!att.query(t, q)) return false;
            R = quatToMat(q);
            return true;
        };
        DelayCalibrator cal(-0.01, 0.05, 0.0005);
        std::mt19937 rng(7);
        std::normal_distribution<double> noise(0.0, 0.01);
        for (int n = 0; n < 250; ++n) {
            const double t_exp = 0.5 + n * 0.04;
            const auto [y, p] = yp(t_exp);
            const cv::Vec3d pg = quatToMat(quatFromYawPitchRoll(y, -p, 0.0)).t() * plate;
            cal.add(t_exp + true_delay, pg + cv::Vec3d(noise(rng), noise(rng), noise(rng)));
            cal.resolve(t_exp - 0.1, lookup);
        }
        cal.resolve(1e9, lookup);
        const DelayEstimate e = cal.estimate();
        if (amp > 0) {
            CHECK(e.ok, "delay estimate: %s", e.reason);
            CHECK(std::abs(e.delay - true_delay) < 0.001, "delay %.2f ms, want 12", e.delay * 1e3);
            CHECK(e.spread < 0.5 * e.spread_at_zero, "scatter %.3f -> %.3f m", e.spread_at_zero, e.spread);
            std::printf("delay calibration: est %.2f ms (true 12), plate scatter %.1f -> %.1f cm\n",
                        e.delay * 1e3, e.spread_at_zero * 100, e.spread * 100);
        } else {
            CHECK(!e.ok, "no excitation must give no estimate");
        }
    }
}
