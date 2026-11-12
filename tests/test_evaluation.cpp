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

void testShotJudgeNonUniform() {
    // platesAt() used to index by T*fps, which silently interpolates the wrong
    // plane pair as soon as the stream is not gapless (dropped frames, a camera
    // that is not eval.fps). Bracket by timestamp instead.
    CameraModel cam{CameraConfig{}};
    Ballistics ball{BallisticConfig{}};
    std::vector<FrameObservations> obs(3);
    // Deliberately non-uniform: 0 s, 40 ms, 120 ms (two frames lost in between).
    const double stamps[3] = {0.0, 0.04, 0.12};
    for (int i = 0; i < 3; ++i) {
        obs[i].frame = i;
        obs[i].stamp = stamps[i];
        ArmorObservation a;
        a.position = {5.0 + 0.4 * i, 0.0, 0.0};  // 0.4 m per sample
        a.yaw = 0.0;
        a.distance = 5.0;
        a.det.number = "3";
        a.det.left.length = a.det.right.length = 20;
        a.det.center = cv::Point2f(100.f * i, 50.f);
        for (int k = 0; k < 4; ++k) a.det.corners[k] = a.det.center;
        obs[i].armors.push_back(a);
    }
    ShotJudge judge(obs, cam, ball, 25.0);
    std::vector<ArmorObservation> out;
    // Midway through the 80 ms gap: the old index arithmetic picked frames 0/1
    // and returned x = 5.2 m here; the correct answer is 5.8 m.
    CHECK(judge.platesAt(0.08, "", out), "platesAt must cover the gap");
    CHECK(out.size() == 1, "one plate, got %zu", out.size());
    CHECK(std::abs(out[0].position[0] - 5.8) < 1e-6, "gap interpolation x=%.3f, want 5.8", out[0].position[0]);
    // Exactly on a sample must return that sample, not a blend.
    CHECK(judge.platesAt(0.04, "", out) && std::abs(out[0].position[0] - 5.4) < 1e-6, "exact sample");
    // Outside the recording must fail rather than extrapolate.
    CHECK(!judge.platesAt(0.2, "", out), "beyond the last sample must fail");
    CHECK(!judge.platesAt(-0.01, "", out), "before the first sample must fail");
}

void testDeviceId() {
    ArmorDetection d;
    d.number = "3";
    CHECK(deviceId(d) == "R3:small", "base id %s", deviceId(d).c_str());
    ArmorDetection blue = d;
    blue.color = Color::BLUE;
    CHECK(deviceId(blue) == "B3:small", "blue id %s", deviceId(blue).c_str());
    ArmorDetection large = d;
    large.type = ArmorType::LARGE;
    CHECK(deviceId(large) == "R3:large", "large id %s", deviceId(large).c_str());
    ArmorDetection other = d;
    other.number = "4";
    CHECK(deviceId(other) != deviceId(d), "number matters");

    // A tracker must not fuse a same-number plate of another colour or size:
    // both teams field a "3", and the wrong fusion would teleport the track.
    CameraConfig cc;
    CameraModel cam(cc);
    TrackerConfig tc;
    PointTracker pt(tc, cam);
    auto mk = [](Color col, ArmorType ty, const cv::Vec3d& p, double stamp) {
        FrameObservations f;
        f.stamp = stamp;
        ArmorObservation o;
        o.position = p;
        o.distance = cv::norm(p);
        o.yaw = 0;
        o.det.number = "3";
        o.det.color = col;
        o.det.type = ty;
        f.armors.push_back(o);
        return f;
    };
    pt.update(mk(Color::RED, ArmorType::SMALL, {4, 0, 0}, 0.0));
    CHECK(pt.active() && pt.id() == "R3:small", "init id %s", pt.id().c_str());
    pt.update(mk(Color::BLUE, ArmorType::SMALL, {-4, 3, 0}, 0.1));  // foreign "3"
    CHECK(pt.id() == "R3:small", "id after foreign obs %s", pt.id().c_str());
    const cv::Vec3d predicted = pt.position(pt.predict(0.2));
    pt.update(mk(Color::RED, ArmorType::LARGE, {-4, -3, 0}, 0.2));  // right colour, wrong size
    CHECK(pt.id() == "R3:small", "id after large obs %s", pt.id().c_str());
    CHECK(cv::norm(pt.position(pt.predict(0.2)) - predicted) < 1e-6, "a foreign plate was fused");
    std::printf("device id: same-number foreign plates are not fused\n");
}
