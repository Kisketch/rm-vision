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


void testAssociationDiagnostics() {
    CameraModel cam(CameraConfig{});
    TrackerConfig cfg;
    cfg.max_match_yaw_diff = 0.1;
    auto observation = [] {
        ArmorObservation o;
        o.position = {4.75, 0, 0}; o.distance = 4.75; o.yaw = 0;
        o.det.number = "3"; o.det.left.length = o.det.right.length = 20;
        return o;
    };
    for (int scenario = 0; scenario < 4; ++scenario) {
        RobotTracker tr(cfg, cam), plain(cfg, cam);
        tr.setDiagnosticsEnabled(true);
        FrameObservations f; f.stamp = 0; f.armors = {observation()};
        tr.update(f); plain.update(f);
        CHECK(std::string(tr.association_diagnostics[0].reason) == "initialized", "init reason");
        f.stamp = 0.04;
        if (scenario == 1) f.armors[0].det.number = "4";
        if (scenario == 2) f.armors[0].position[2] = 5;
        if (scenario == 3) f.armors[0].yaw = 0.6;
        tr.update(f); plain.update(f);
        const char* expected[] = {"matched", "other_identity", "distance_gate", "yaw_gate"};
        CHECK(std::string(tr.association_diagnostics[0].reason) == expected[scenario], "scenario %d reason %s", scenario, tr.association_diagnostics[0].reason);
        CHECK(tr.state() == plain.state() && tr.matched_count == plain.matched_count && cv::norm(tr.x() - plain.x()) == 0, "diagnostic parity");
        CHECK(plain.association_diagnostics.empty(), "disabled diagnostics allocate no records");
        f.stamp = 0.08; f.armors.clear(); tr.update(f);
        CHECK(tr.association_diagnostics.empty(), "no stale diagnostic entries on empty frame");
    }
    RobotTracker settled(cfg, cam);
    settled.setDiagnosticsEnabled(true);
    FrameObservations f;
    for (int i = 0; i < 100; ++i) {
        f.stamp = i * 0.04; f.armors = {observation()}; settled.update(f);
    }
    f.stamp = 4.0; f.armors = {observation(), observation()}; settled.update(f);
    int conflicts = 0, matches = 0;
    for (const auto& d : settled.association_diagnostics) {
        conflicts += std::string(d.reason) == "assignment_conflict";
        matches += std::string(d.reason) == "matched";
    }
    CHECK(conflicts == 1 && matches == 1, "one plate cannot consume two observations");
    f.stamp = 4.04; f.armors = {observation()}; f.armors[0].position[1] = 0.25;
    settled.update(f);
    CHECK(std::string(settled.association_diagnostics[0].reason) == "score_gate",
          "covariance gate reason %s score %.2f", settled.association_diagnostics[0].reason,
          settled.association_diagnostics[0].best_score);
}

void testTracker() {
    CameraConfig cc;
    CameraModel cam(cc);
    TrackerConfig tc;
    RobotTracker tr(tc, cam);
    const double w = 3.0, r = 0.24, fps = 50;
    const cv::Vec3d c(5, 0.2, -0.1);
    for (int n = 0; n < 200; ++n) {
        const double t = n / fps;
        FrameObservations f;
        f.frame = n, f.stamp = t;
        const double los = std::atan2(c[1], c[0]);
        for (int k = 0; k < 4; ++k) {
            const double yaw = 0.2 + w * t + kPi / 2 * k;
            if (std::abs(wrapAngle(yaw - los)) > 1.0) continue;
            ArmorObservation o;
            o.position = {c[0] - r * std::cos(yaw), c[1] - r * std::sin(yaw), c[2]};
            o.distance = cv::norm(o.position);
            o.yaw = wrapAngle(yaw);
            o.det.number = "3";
            o.det.left.length = o.det.right.length = 20;
            o.image_center_dist = 100;
            f.armors.push_back(o);
        }
        tr.update(f);
    }
    const auto& x = tr.x();
    CHECK(tr.state() == TrackState::TRACKING, "tracking state %s", toString(tr.state()));
    CHECK(std::abs(x[RobotTracker::W] - w) < 0.2, "omega %f", x[RobotTracker::W]);
    CHECK(std::hypot(x[RobotTracker::XC] - c[0], x[RobotTracker::YC] - c[1]) < 0.05, "centre %f %f",
          x[RobotTracker::XC], x[RobotTracker::YC]);
    CHECK(std::abs(x[RobotTracker::R1] - r) < 0.03, "radius %f", x[RobotTracker::R1]);
    std::printf("tracker: w %.3f (true %.1f), r1 %.3f r2 %.3f (true %.2f)\n", x[RobotTracker::W], w,
                x[RobotTracker::R1], x[RobotTracker::R2], r);
}

void testTrackerCaModel() {
    // The CA (constant-acceleration) branch is off by default and was previously
    // exercised by no test at all. Spin the target up (yaw = w0 t + a t^2 / 2)
    // with ca_model on: the branch must run, stay finite and reach TRACKING.
    // This asserts the path is alive, not that CA beats the CV model (on the
    // pure-translation simulator scenario it does not; see BASELINE.md).
    CameraConfig cc;
    CameraModel cam(cc);
    TrackerConfig tc;
    tc.ca_model = 1;
    RobotTracker tr(tc, cam);
    const double w0 = 1.0, acc = 1.2, r = 0.24, fps = 50;
    const cv::Vec3d c(5, 0.2, -0.1);
    for (int n = 0; n < 250; ++n) {
        const double t = n / fps;
        FrameObservations f;
        f.frame = n, f.stamp = t;
        const double los = std::atan2(c[1], c[0]);
        for (int k = 0; k < 4; ++k) {
            const double yaw = 0.2 + w0 * t + 0.5 * acc * t * t + kPi / 2 * k;
            if (std::abs(wrapAngle(yaw - los)) > 1.0) continue;
            ArmorObservation o;
            o.position = {c[0] - r * std::cos(yaw), c[1] - r * std::sin(yaw), c[2]};
            o.distance = cv::norm(o.position);
            o.yaw = wrapAngle(yaw);
            o.det.number = "3";
            o.det.left.length = o.det.right.length = 20;
            o.image_center_dist = 100;
            f.armors.push_back(o);
        }
        tr.update(f);
    }
    const auto& x = tr.x();
    bool finite = true;
    for (int i = 0; i < RobotTracker::N; ++i) finite &= std::isfinite(x[i]);
    CHECK(finite, "CA state must stay finite");
    CHECK(tr.state() == TrackState::TRACKING, "CA tracking state %s", toString(tr.state()));
    const double w_true = w0 + acc * (249 / fps);
    CHECK(std::abs(x[RobotTracker::W] - w_true) < 1.0, "CA omega %.3f vs true %.3f", x[RobotTracker::W], w_true);
    CHECK(std::hypot(x[RobotTracker::XC] - c[0], x[RobotTracker::YC] - c[1]) < 0.1, "CA centre %.3f %.3f",
          x[RobotTracker::XC], x[RobotTracker::YC]);
    std::printf("tracker CA: w %.3f (true %.2f), axc %.3f ayc %.3f\n", x[RobotTracker::W], w_true,
                x[RobotTracker::AXC], x[RobotTracker::AY]);
}

void testTrackerDropoutGrace() {
    // One frame without a detection is a dropout, not a lost target. With
    // lost_grace_frames = 0 the track is demoted (the historical rule); with 1
    // it must stay TRACKING, because demoting it also stops firing and resets
    // every warm-up counter.
    const Spinner sp;
    for (int grace : {0, 1}) {
        TrackerConfig tc;
        tc.lost_grace_frames = grace;
        CameraModel cam{CameraConfig{}};
        RobotTracker tr(tc, cam);
        for (int n = 0; n < 80; ++n) tr.update(sp.frame(n / 25.0, n));
        CHECK(tr.state() == TrackState::TRACKING, "grace %d: state before drop %s", grace,
              toString(tr.state()));
        FrameObservations empty;  // no plates at all
        empty.frame = 80, empty.stamp = 80 / 25.0;
        tr.update(empty);
        if (grace == 0)
            CHECK(tr.state() == TrackState::TEMP_LOST, "grace 0 must demote, got %s", toString(tr.state()));
        else
            CHECK(tr.state() == TrackState::TRACKING, "grace 1 must hold, got %s", toString(tr.state()));
        // A second consecutive dropout exhausts the grace.
        FrameObservations empty2;
        empty2.frame = 81, empty2.stamp = 81 / 25.0;
        tr.update(empty2);
        CHECK(tr.state() != TrackState::TRACKING, "grace %d: two drops must demote, got %s", grace,
              toString(tr.state()));
    }
}

void testYawProcessNoiseSplit() {
    // What q_yaw_walk actually guarantees (measured 2026-11-11):
    //   1. it is wired: the phase variance over a prediction grows with it;
    //   2. it does not disturb convergence: omega and the radius still settle.
    // It is NOT a free lunch. On a noise-free constant-rate target the phase
    // random walk makes a 0.4 s extrapolation worse (34.7 mm vs 20.1 mm for
    // q_yaw 1 + walk 0.12 vs q_yaw 3 + walk 0), because injected variance beats
    // physics when the measurements are clean. It wins on the real videos
    // (58.0% / 70.4% vs 56.8% / 64.4%), where the phase measurements carry
    // correlated errors the filter has to absorb. See BASELINE.md section 11.
    const Spinner sp;
    CameraModel cam{CameraConfig{}};
    auto settle = [&](const TrackerConfig& tc) {
        RobotTracker tr(tc, cam);
        for (int n = 0; n < 200; ++n) tr.update(sp.frame(n / 25.0, n));
        return tr;
    };
    TrackerConfig base;
    base.q_yaw_walk = 0.0;
    TrackerConfig walk = base;
    walk.q_yaw_walk = 0.12;

    const RobotTracker a = settle(base);
    const RobotTracker b = settle(walk);
    const double t = 199 / 25.0;
    const double va = a.predictCovariance(t + 0.4, true)(RobotTracker::YAW, RobotTracker::YAW);
    const double vb = b.predictCovariance(t + 0.4, true)(RobotTracker::YAW, RobotTracker::YAW);
    // The walk must add variance at the rate it is configured for; the exact
    // difference is not q*horizon because the inflated angle also changes the
    // gains during the run, so only half of it is required.
    CHECK(vb > va, "q_yaw_walk must widen the phase variance: %.3g vs %.3g", vb, va);
    CHECK(vb - va > 0.5 * walk.q_yaw_walk * 0.4, "walk added %.3f rad^2, want > %.3f", vb - va,
          0.5 * walk.q_yaw_walk * 0.4);
    for (const auto* tr : {&a, &b}) {
        const auto& x = tr->x();
        CHECK(tr->state() == TrackState::TRACKING, "state %s", toString(tr->state()));
        CHECK(std::abs(x[RobotTracker::W] - sp.w) < 0.2, "omega %f", x[RobotTracker::W]);
        CHECK(std::abs(x[RobotTracker::R1] - sp.r) < 0.03, "radius %f", x[RobotTracker::R1]);
    }
    // Shipping default must keep the historical single-q behaviour.
    TrackerConfig shipped;
    CHECK(shipped.q_yaw_walk == 0.0, "q_yaw_walk must default to 0");
    std::printf("yaw q split: phase var %.3g -> %.3g, omega %.3f, radius %.3f\n", va, vb,
                b.x()[RobotTracker::W], b.x()[RobotTracker::R1]);
}

void testStaleFrameRejected() {
    // A duplicate or out-of-order frame must not touch the filter state:
    // predictTo() no-ops for dt <= 0, so without an explicit guard the old
    // measurement would be applied to the current state as-is.
    CameraConfig cc;
    CameraModel cam(cc);
    TrackerConfig tc;
    RobotTracker tr(tc, cam);
    Spinner sp;
    double t_last = 0;
    for (int n = 0; n < 60; ++n) {
        t_last = n / 50.0;
        tr.update(sp.frame(t_last, n));
    }
    CHECK(tr.state() == TrackState::TRACKING, "precondition: tracking");
    const RobotTracker::Vec before = tr.predict(t_last + 0.3);
    FrameObservations dup = sp.frame(t_last, 900);          // same stamp
    tr.update(dup);
    CHECK(std::string(tr.diagnostic_event) == "stale_frame", "duplicate stamp rejected (%s)", tr.diagnostic_event);
    FrameObservations old = sp.frame(t_last - 0.5, 901);    // older stamp
    tr.update(old);
    CHECK(std::string(tr.diagnostic_event) == "stale_frame", "old stamp rejected (%s)", tr.diagnostic_event);
    const RobotTracker::Vec after = tr.predict(t_last + 0.3);
    double drift = 0;
    for (int i = 0; i < RobotTracker::N; ++i) drift = std::max(drift, std::abs(after[i] - before[i]));
    CHECK(drift == 0.0, "state untouched by stale frames (drift %.3g)", drift);
    std::printf("stale frames: rejected, state bit-identical\n");
}

void testYawNisOrderRobustness() {
    // The yaw-NIS adaptation aggregates ALL of a frame's matches (mean).
    // Feeding the same multi-plate frames in reverse observation order
    // must land the adaptation in nearly the same place; with the old
    // last-match-only rule the two runs diverged at first order.
    CameraConfig cc;
    CameraModel cam(cc);
    TrackerConfig tc;
    tc.r_yaw = 0.01;         // the tuned YAML values - the struct defaults
    tc.r_yaw_head_on = 0.02; // (0.06/0.25) damp the yaw NIS below engagement
    RobotTracker ab(tc, cam), ba(tc, cam);
    // Custom frames (not Spinner): a wide visibility gate so two plates are
    // present nearly every frame, and a per-plate yaw bias keyed to the
    // plate index - even plates +0.30 rad, odd plates +0.05 rad.  The two
    // plates' yaw NIS then differ ~25x, which is exactly the case where
    // last-match-only adaptation becomes order-dependent; the frame mean
    // is not.  (A single sporadic biased frame would not engage the EMA
    // at all - it answers sustained phase error, by design.)
    const cv::Vec3d c{5, 0.2, -0.1};
    const double w = 3.0, r = 0.24, los = std::atan2(c[1], c[0]);
    for (int n = 0; n < 200; ++n) {
        const double t = n / 50.0;
        FrameObservations f;
        f.frame = n, f.stamp = t;
        for (int k = 0; k < 4; ++k) {
            const double yaw = 0.2 + w * t + kPi / 2 * k;
            if (std::abs(wrapAngle(yaw - los)) > 1.45) continue;
            ArmorObservation o;
            o.position = {c[0] - r * std::cos(yaw), c[1] - r * std::sin(yaw), c[2]};
            o.distance = cv::norm(o.position);
            o.yaw = wrapAngle(yaw + (k % 2 == 0 ? 0.30 : 0.05));
            o.det.number = "3";
            o.det.left.length = o.det.right.length = 20;
            o.image_center_dist = 100;
            f.armors.push_back(o);
        }
        if (f.armors.size() < 2) continue;
        ab.update(f);
        std::reverse(f.armors.begin(), f.armors.end());
        ba.update(f);
    }
    CHECK(ab.state() == TrackState::TRACKING && ba.state() == TrackState::TRACKING, "both tracking");
    // Engaged = above the 1.0 floor both runs sit at on clean frames.
    // (At a much larger corruption, 0.5 rad, the two runs' STATES diverge
    // into different association basins - sequential updates plus greedy
    // association are order-sensitive at that scale, a separate effect
    // from the NIS aggregation tested here.)
    CHECK(ab.qYawScale() > 1.02 && ba.qYawScale() > 1.02, "adaptation engaged (ab %.3f ba %.3f)", ab.qYawScale(),
          ba.qYawScale());
    const double d = std::abs(ab.qYawScale() - ba.qYawScale());
    CHECK(d < 0.5, "yaw adaptation order gap %.3f (ab %.3f ba %.3f)", d, ab.qYawScale(), ba.qYawScale());
    std::printf("yaw NIS order: q_yaw_scale ab %.3f ba %.3f (gap %.3f)\n", ab.qYawScale(), ba.qYawScale(), d);
}
