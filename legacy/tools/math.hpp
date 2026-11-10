#ifndef TOOLS_MATH_HPP
#define TOOLS_MATH_HPP

#include <eigen3/Eigen/Dense>
#include <iostream>
#include <cmath>
#include "Base.h"

namespace tools
{
    inline Eigen::Vector3d xyz2ypd(const Eigen::Vector3d& xyz)
    {
        auto x = xyz.x(), y = xyz.y(), z = xyz.z();
        auto yaw = std::atan2(y, x);
        auto pitch = std::atan2(z, std::sqrt(x * x + y * y));
        auto distance = std::sqrt(x * x + y * y + z * z);
        return {yaw, pitch, distance};
    }

    inline Eigen::Matrix4d xyz2ypd_jacobian(const Eigen::Vector3d& xyz)
    {
        auto x = xyz.x(), y = xyz.y(), z = xyz.z();

        auto dyaw_dx = -y / (x * x + y * y);
        auto dyaw_dy = x / (x * x + y * y);
        auto dyaw_dz = 0.0;

        auto dpitch_dx = -(x * z) / ((z * z / (x * x + y * y) + 1) * std::pow((x * x + y * y), 1.5));
        auto dpitch_dy = -(y * z) / ((z * z / (x * x + y * y) + 1) * std::pow((x * x + y * y), 1.5));
        auto dpitch_dz = 1 / ((z * z / (x * x + y * y) + 1) * std::pow((x * x + y * y), 0.5));

        auto ddistance_dx = x / std::sqrt(x * x + y * y + z * z);
        auto ddistance_dy = y / std::sqrt(x * x + y * y + z * z);
        auto ddistance_dz = z / std::sqrt(x * x + y * y + z * z);

        // clang-format off
        Eigen::Matrix4d J{
            {dyaw_dx, dyaw_dy, dyaw_dz, 0},
            {dpitch_dx, dpitch_dy, dpitch_dz, 0},
            {ddistance_dx, ddistance_dy, ddistance_dz, 0},
            {0, 0, 0, 1}
        };
        // clang-format on

        return J;
    }

    inline double R2D(double rad)
    {
        return rad * 180 / PI;
    }

    inline double D2R(double degree)
    {
        return degree * PI / 180;
    }
}

#endif // TOOLS_MATH_HPP