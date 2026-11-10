#ifndef AUTO_BUFF__SOLVER_HPP
#define AUTO_BUFF__SOLVER_HPP

#include <yaml-cpp/yaml.h>

#include <Eigen/Dense>  // 必须在opencv2/core/eigen.hpp上面
#include <opencv2/core/eigen.hpp>
#include <optional>

#include "BuffType.h"
#include "math_tools.hpp"
#include "Base.h"
namespace auto_buff
{
// 旋转角度
const double THETA = 2.0 * CV_PI / 5.0;  // 2/5π

class Solver
{
public:
    explicit Solver();
    // 返回旋转矩阵
    Eigen::Matrix3d R_gimbal2world() const;
    // 更新角度状态
    void UpdateStatus(Data_Save ds);
    // 模块主接口
    void Solve(std::optional<PowerRune> & ps) const;
    // 调试用
    cv::Point2f point_buff2pixel(cv::Point3f x);
    // 重投影函数
    std::vector<cv::Point2f> reproject_buff(const Eigen::Vector3d & xyz_in_world, double yaw, double row) const;

private:
    cv::Mat camera_matrix_;
    cv::Mat distort_coeffs_;
    Eigen::Matrix3d R_gimbal2imubody_;
    Eigen::Matrix3d R_camera2gimbal_;
    Eigen::Vector3d t_camera2gimbal_;
    Eigen::Matrix3d R_gimbal2world_;

    cv::Vec3d rvec_, tvec_;

    // 第一个扇叶
    const std::vector<cv::Point3f> OBJECT_POINTS = {
        cv::Point3f(0, 0, 835e-3), cv::Point3f(0, 135e-3, 700e-3),
        cv::Point3f(0, 0, 565e-3), cv::Point3f(0, -135e-3, 700e-3),
        cv::Point3f(0, 0, 700e-3), cv::Point3f(0, 0, 344e-3),
        cv::Point3f(0, 0, 0)
    };  // 单位：米
    // 相邻扇叶
    const std::vector<cv::Point3f> ADJACENT_POINTS = {
        cv::Point3f(0, 0.794132, 0.258029), cv::Point3f(0, 0.707457, 0.0879193),
        cv::Point3f(0, 0.537347, 0.174595), cv::Point3f(0, 0.624022, 0.344705),
        cv::Point3f(0, 0.66574,  0.216312), cv::Point3f(0, 0.327163, 0.106302)
    };  // 单位：米
    // 间隔扇叶
    const std::vector<cv::Point3f> INTERVAL_POINTS = {
        cv::Point3f(0, 0.490801, -0.675529), cv::Point3f(0,  0.302232, -0.645663),
        cv::Point3f(0, 0.332099, -0.457095), cv::Point3f(0,  0.520667, -0.486961),
        cv::Point3f(0, 0.41145,  -0.566312), cv::Point3f(0,  0.202198, -0.278302)
    };  // 单位：米
    
    // 函数：生成绕x轴旋转的旋转矩阵
    cv::Matx33f rotation_matrix(double angle) const;

    // 函数：旋转点并填充到 OBJECT_POINTS 中
    void compute_rotated_points(std::vector<std::vector<cv::Point3f>> & object_points);
};
}  // namespace auto_buff
#endif  // AUTO_AIM__SOLVER_HPP