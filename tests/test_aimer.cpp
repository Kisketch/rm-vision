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
#include "fixtures.hpp"

using namespace autoaim;

void testAimerOffsets() {
    // Fixed aim offsets must shift the final command by exactly the
    // configured amounts and change nothing else.
    CameraConfig cc;
    cc.fx = cc.fy = 2400;
    cc.cx = 720;
    cc.cy = 540;
    cc.dist = {0, 0, 0, 0, 0};
    CameraModel cam(cc);
    BallisticConfig bc;
    auto run = [&](double yo, double po) {
        AimerConfig ac;
        ac.latency = 0.0;
        ac.yaw_offset_deg = yo;
        ac.pitch_offset_deg = po;
        RobotTracker tracker(TrackerConfig{}, cam);
        RobotAimer aimer(ac, Ballistics(bc));
        FrameObservations f;
        AimCommand cmd;
        for (int i = 0; i < 30; ++i) {
            f.stamp = i / 120.0;
            f.armors.clear();
            ArmorObservation o;
            o.det.number = "4";
            o.det.type = ArmorType::SMALL;
            o.position = {5.0, 0.8, 0.15};
            o.yaw = 0.4;
            o.distance = 5.06;
            f.armors = {o};
            tracker.update(f);
            cmd = aimer.aim(tracker, f.stamp);
        }
        return cmd;
    };
    AimCommand base = run(0.0, 0.0), shifted = run(2.0, -1.0);
    CHECK(base.valid && shifted.valid, "offset test commands valid");
    CHECK(std::abs(wrapAngle(shifted.yaw - base.yaw) - 2.0 * kPi / 180.0) < 1e-9,
          "yaw offset exact: %.12f", wrapAngle(shifted.yaw - base.yaw));
    CHECK(std::abs((shifted.pitch - base.pitch) + 1.0 * kPi / 180.0) < 1e-9,
          "pitch offset exact: %.12f", shifted.pitch - base.pitch);
    CHECK(shifted.fire == base.fire, "offset does not change fire decision");
}

void testAimerPhaseLead() {
    // phase_lead_deg rotates the aimed plate about the robot centre and must not
    // touch the centre-mode aim ray (that ray is not a plate).
    CameraModel cam{CameraConfig{}};
    TrackerConfig tc;
    RobotTracker tr(tc, cam);
    Spinner sp;
    sp.w = 0.0;  // static: no side-angle switch lead, identical flight time
    for (int n = 0; n < 60; ++n) tr.update(sp.frame(n / 25.0, n));
    CHECK(tr.state() == TrackState::TRACKING, "state %s", toString(tr.state()));

    BallisticConfig bc;
    Ballistics ball(bc);
    AimerConfig ac;
    ac.center_mode_omega = 1e9;  // force armour mode
    AimerConfig lead = ac;
    lead.phase_lead_deg = 18.0;

    const double now = 59 / 25.0;
    const AimCommand a0 = RobotAimer(ac, ball).aim(tr, now);
    const AimCommand a1 = RobotAimer(lead, ball).aim(tr, now);
    CHECK(a0.valid && a1.valid, "both commands valid");
    const auto& x = tr.predict(now + ac.latency + a0.t_fly);
    const double cx = x[RobotTracker::XC], cy = x[RobotTracker::YC];
    const double ang0 = std::atan2(a0.aim_point[1] - cy, a0.aim_point[0] - cx);
    const double ang1 = std::atan2(a1.aim_point[1] - cy, a1.aim_point[0] - cx);
    const double delta = wrapAngle(ang1 - ang0);
    CHECK(std::abs(delta - 18.0 * kPi / 180.0) < 0.5 * kPi / 180.0, "lead rotated by %.2f deg, want 18",
          delta * 180 / kPi);
    CHECK(std::abs(cv::norm(a0.aim_point - cv::Vec3d(cx, cy, a0.aim_point[2])) -
                   cv::norm(a1.aim_point - cv::Vec3d(cx, cy, a1.aim_point[2]))) < 1e-6,
          "lead must preserve the plate radius");

    // Centre mode: the lead must be a no-op, otherwise the ray leaves the centre.
    // A spinning target is needed because centre mode triggers on |omega|.
    Spinner spinning;
    spinning.w = 1.0;
    RobotTracker tr_spin(tc, cam);
    for (int n = 0; n < 60; ++n) tr_spin.update(spinning.frame(n / 25.0, n));
    AimerConfig centre = ac;
    centre.center_mode_omega = 0.5;
    AimerConfig centre_lead = centre;
    centre_lead.phase_lead_deg = 18.0;
    const AimCommand c0 = RobotAimer(centre, ball).aim(tr_spin, now);
    const AimCommand c1 = RobotAimer(centre_lead, ball).aim(tr_spin, now);
    CHECK(c0.valid && c1.valid && c0.center_mode && c1.center_mode, "centre mode active");
    CHECK(std::abs(wrapAngle(c1.yaw - c0.yaw)) < 1e-9, "centre-mode yaw must ignore the phase lead (%g)",
          wrapAngle(c1.yaw - c0.yaw));
}

void testAimerRates() {
    // B-01: a target circling at a known rate must produce aim commands whose
    // yaw_rate matches it; pitch_rate stays ~0 for level motion.
    CameraConfig cc;
    CameraModel cam(cc);
    TrackerConfig tc;
    PointTracker pt(tc, cam);
    Ballistics ball{ BallisticConfig{} };
    AimerConfig ac;
    PointAimer aimer(ac, ball);
    const double w = 0.6, radius = 3.0, fps = 50;
    double sum_rate = 0, sum_pitch = 0;
    int samples = 0;
    double max_abs_rate = 0;
    for (int n = 0; n < 200; ++n) {
        const double t = n / fps;
        FrameObservations f;
        f.frame = n, f.stamp = t;
        ArmorObservation o;
        o.position = {radius * std::cos(w * t), radius * std::sin(w * t), 0.0};
        o.distance = cv::norm(o.position);
        o.yaw = w * t;
        o.det.number = "3";
        f.armors.push_back(o);
        pt.update(f);
        const auto cmd = aimer.aim(pt, t);
        CHECK(cmd.valid, "aim valid at %d", n);
        max_abs_rate = std::max(max_abs_rate, std::abs(cmd.yaw_rate));
        if (t > 2.0) {  // KF settled, rate state warm
            sum_rate += cmd.yaw_rate;
            sum_pitch += cmd.pitch_rate;
            ++samples;
        }
    }
    CHECK(samples > 50, "samples %d", samples);
    CHECK(std::abs(sum_rate / samples - w) < 0.1, "yaw_rate %.3f vs %.1f", sum_rate / samples, w);
    CHECK(std::abs(sum_pitch / samples) < 0.05, "pitch_rate %.3f", sum_pitch / samples);
    CHECK(max_abs_rate <= ac.gimbal_max_rate + 1e-9, "rate clamp %.3f", max_abs_rate);
    std::printf("aimer rates: yaw_rate %.3f (true %.1f), pitch_rate %.3f\n", sum_rate / samples, w,
                sum_pitch / samples);
}
