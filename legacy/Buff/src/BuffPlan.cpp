#include "BuffPlan.h"
#include "logger.hpp"
#include "math_tools.hpp"
#include "trajectory.hpp"

namespace auto_buff
{
Planner::Planner()
{
    auto yaml = YAML::LoadFile(YamlPath);
    predict_time_ = yaml["Planer"]["predict_time"].as<double>();
}

Gimbal_Angle Planner::Plan(Target & target, time_point timestamp)
{
    Gimbal_Angle gimbal_angle; gimbal_angle.manage_yaw = 0;   
    if (target.is_unsolve()) return gimbal_angle;
    auto now = std::chrono::steady_clock::now();
    auto detect_now_gap = tools::delta_time(now, timestamp);
    auto future = detect_now_gap + predict_time_;
    double yaw, pitch;
    if (GetSendAngle(target, future, yaw, pitch)) {
        gimbal_angle.target_yaw = yaw;
        gimbal_angle.target_pitch = pitch;  
        if (mistake_count_ > 3) {
            switch_fanblade_ = true;
            mistake_count_ = 0;
        } else if (std::abs(last_yaw_ - yaw) > 5 / 57.3 || std::abs(last_pitch_ - pitch) > 5 / 57.3) {
            switch_fanblade_ = true;
            mistake_count_++;
        } else {
            switch_fanblade_ = false;
            mistake_count_ = 0;
        }
        last_yaw_ = yaw;
        last_pitch_ = pitch;
    }
    if (!switch_fanblade_) gimbal_angle.manage_yaw = 1;
    
    return gimbal_angle;
}

bool Planner::GetSendAngle(Target & target, const double predict_time, double & yaw, double & pitch)
{
    // 考虑detecor所消耗的时间，此外假设aimer的用时可忽略不计
    target.predict(predict_time);
    // 计算目标点的空间坐标
    auto aim_in_world = target.fire_buff2world(Eigen::Vector3d(0.0, 0.0, 0.7));
    double d = std::sqrt(aim_in_world[0] * aim_in_world[0] + aim_in_world[1] * aim_in_world[1]);
    double h = aim_in_world[2];
     
    // 创建弹道对象
    tools::Trajectory trajectory0(bullet_speed, d, h);
    if (trajectory0.unsolvable) {  // 如果弹道无法解算，返回未命中结果
        tools::logger()->debug(
        "[Planner] Unsolvable trajectory0: {:.2f} {:.2f} {:.2f}", bullet_speed, d, h);
        return false;
    }

    // 根据第一个弹道飞行时间预测目标位置
    target.predict(trajectory0.fly_time);

    // 计算新的目标点的空间坐标
    aim_in_world = target.fire_buff2world(Eigen::Vector3d(0.0, 0.0, 0.7));
    // aim_in_world = target.point_R2world();//调试用
    d = fsqrt(aim_in_world[0] * aim_in_world[0] + aim_in_world[1] * aim_in_world[1]);
    h = aim_in_world[2];
    tools::Trajectory trajectory1(bullet_speed, d, h);
    if (trajectory1.unsolvable) {  // 如果弹道无法解算，返回未命中结果
        tools::logger()->debug(
        "[Planner] Unsolvable trajectory1: {:.2f} {:.2f} {:.2f}", bullet_speed, d, h);
        return false;
    }

    // 计算时间误差
    auto time_error = trajectory1.fly_time - trajectory0.fly_time;
    if (std::abs(time_error) > 0.01) {  // 如果时间误差过大，返回未命中结果
        tools::logger()->debug("[Planner] Large time error: {:.3f}", time_error);
        return false;
    }

    // 计算偏航角和俯仰角，并返回命中结果
    yaw = std::atan2(aim_in_world[1], aim_in_world[0]);
    pitch = trajectory1.pitch;

    return true;
};

}  // namespace auto_buff