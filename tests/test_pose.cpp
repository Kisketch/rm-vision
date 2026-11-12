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

void testPose() {
    CameraConfig cc;
    cc.pitch_deg = 3.0;
    CameraModel cam(cc);
    PoseConfig pc;
    PoseSolver solver(cam, pc);
    std::mt19937 rng(1);
    std::normal_distribution<double> n(0, 0.3);
    double max_pos = 0, max_yaw = 0;
    for (double dist : {2.0, 4.0, 6.0})
        for (double yaw_off : {-40.0, -15.0, 10.0, 35.0}) {
            const cv::Vec3d p(dist, 0.3, -0.15);
            const double yaw = std::atan2(p[1], p[0]) + yaw_off * kPi / 180;
            const auto c3 = armorCorners(p, yaw, 15 * kPi / 180, ArmorType::SMALL);
            ArmorDetection d;
            d.type = ArmorType::SMALL;
            for (int i = 0; i < 4; ++i) {
                const cv::Point2d q = cam.project(c3[i]);
                d.corners[i] = cv::Point2f(float(q.x + n(rng)), float(q.y + n(rng)));
            }
            d.center = (d.corners[0] + d.corners[1] + d.corners[2] + d.corners[3]) * 0.25f;
            ArmorObservation o;
            CHECK(solver.solve(d, o), "pose solve");
            max_pos = std::max(max_pos, cv::norm(o.position - p) / dist);
            max_yaw = std::max(max_yaw, std::abs(wrapAngle(o.yaw - yaw)) * 180 / kPi);
        }
    CHECK(max_pos < 0.03, "relative position error %.3f", max_pos);
    CHECK(max_yaw < 8.0, "yaw error %.1f deg", max_yaw);
    std::printf("pose: max rel. position error %.2f%%, max yaw error %.2f deg (0.3 px noise)\n", 100 * max_pos,
                max_yaw);
}

void testPoseQuality() {
    CameraModel cam(CameraConfig{});
    PoseConfig pc;
    pc.yaw_gain = 1.5;
    PoseSolver solver(cam, pc);
    ArmorDetection d;
    d.type = ArmorType::SMALL;
    const auto corners = armorCorners({4, 0.2, 0}, 0.5, 15 * kPi / 180, d.type);
    for (int i = 0; i < 4; ++i) d.corners[i] = cv::Point2f(cam.project(corners[i]));
    d.corners[0].y += 0.3f;  // nonzero fit residual, plus yaw_gain correction
    d.center = (d.corners[0] + d.corners[1] + d.corners[2] + d.corners[3]) * 0.25f;
    ArmorObservation o;
    PoseDiagnostic diag;
    CHECK(solver.solve(d, o, &diag), "valid pose");
    CHECK(diag.physical_candidates > 0 && std::string(diag.status) == "accepted", "physical solution");
    double final_error = 0;
    const auto final = armorCorners(o.position, o.yaw, 15 * kPi / 180, d.type);
    for (int i = 0; i < 4; ++i) {
        CHECK(cam.worldToCamera(final[i])[2] > 0, "positive depth");
        final_error += cv::norm(cam.project(final[i]) - cv::Point2d(d.corners[i])) / 4;
    }
    CHECK(std::abs(o.reproj_error - final_error) < 1e-9, "final yaw residual %.6f != %.6f", o.reproj_error, final_error);
    CHECK(diag.final_reproj_px > diag.fit_reproj_px, "gain correction reflected in residual");
    pc.max_reproj_error_px = final_error * 0.5;
    CHECK(!PoseSolver(cam, pc).solve(d, o, &diag) && std::string(diag.status) == "reprojection_gate", "explicit residual gate");
    CHECK(o.distance == 0, "failure clears previous output");
    auto bad = d;
    bad.corners[0].x = std::numeric_limits<float>::quiet_NaN();
    CHECK(!solver.solve(bad, o, &diag) && std::string(diag.status) == "invalid_corners", "NaN input");
    bad = d; bad.corners[0].y = std::numeric_limits<float>::infinity();
    CHECK(!solver.solve(bad, o, &diag), "Inf input");
    bad = d; bad.corners[1] = bad.corners[0];
    CHECK(!solver.solve(bad, o, &diag) && std::string(diag.status) == "degenerate_quad", "duplicate corners");
    bad = d; std::swap(bad.corners[0], bad.corners[1]);
    CHECK(!solver.solve(bad, o, &diag), "self-intersecting quad");
    for (double distance : {0.1, 60.0}) {
        bad = d;
        const auto c = armorCorners({distance, 0.02, 0}, 0.4, 15 * kPi / 180, d.type);
        for (int i = 0; i < 4; ++i) bad.corners[i] = cv::Point2f(cam.project(c[i]));
        CHECK(!solver.solve(bad, o, &diag) && std::string(diag.status) == "distance_gate", "distance gate %.1f: %s", distance, diag.status);
    }
}

void testCameraExtrinsic() {
    // Lever-arm support: with a configured camera->gimbal extrinsic, the
    // transforms round-trip and a synthetic armor generated THROUGH the
    // extrinsic model is recovered by the solver; the default model (no
    // extrinsic) must stay measurably wrong on the same detection, which is
    // the error the lever arm exists to remove.
    CameraConfig cc;
    cc.fx = cc.fy = 2400;
    cc.cx = 720;
    cc.cy = 540;
    cc.dist = {0, 0, 0, 0, 0};
    CameraConfig cx = cc;
    cx.R_cam2gimbal = {0, 0, 1, -1, 0, 0, 0, -1, 0};
    cx.t_cam2gimbal = {0.10, -0.05, 0.0};
    CameraModel withX(cx), plain(cc);
    const cv::Vec3d p(3.0, -1.2, 0.4);
    CHECK(cv::norm(withX.cameraToWorld(withX.worldToCamera(p)) - p) < 1e-12,
          "extrinsic round trip");
    // Generate a detection through the extrinsic model.
    ArmorDetection det;
    det.type = ArmorType::SMALL;
    det.number = "4";
    const cv::Vec3d center(5.0, 0.8, 0.15);
    auto corners = armorCorners(center, 0.4, 15.0 * kPi / 180.0, det.type);
    for (int i = 0; i < 4; ++i) det.corners[i] = cv::Point2f(withX.project(corners[i]));
    det.center = (det.corners[0] + det.corners[2]) * 0.5f;
    PoseConfig pc;
    ArmorObservation o1, o2;
    CHECK(PoseSolver(withX, pc).solve(det, o1), "solve with extrinsic model");
    CHECK(cv::norm(o1.position - center) < 0.01, "extrinsic model recovers position: %.4f m",
          cv::norm(o1.position - center));
    CHECK(PoseSolver(plain, pc).solve(det, o2), "solve with default model");
    CHECK(cv::norm(o2.position - center) > 0.03,
          "default model keeps the lever-arm error: %.4f m", cv::norm(o2.position - center));
}
