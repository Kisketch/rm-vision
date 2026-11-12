// Shared test fixtures (split out of the former single-file suite, round 6).
#pragma once

#include "autoaim/types.hpp"

namespace autoaim {
struct Spinner {
    double w = 3.0, r = 0.24, yaw0 = 0.2;
    cv::Vec3d c{5, 0.2, -0.1};
    FrameObservations frame(double t, int n) const {
        FrameObservations f;
        f.frame = n, f.stamp = t;
        const double los = std::atan2(c[1], c[0]);
        for (int k = 0; k < 4; ++k) {
            const double yaw = yaw0 + w * t + kPi / 2 * k;
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
        return f;
    }
};
}  // namespace autoaim
