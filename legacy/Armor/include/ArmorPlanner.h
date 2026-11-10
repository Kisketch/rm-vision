#ifndef ARMOR_PLANNER_H
#define ARMOR_PLANNER_H

#include <Eigen/Dense>
#include <iostream>
#include "ArmorPredictor.h"
#include "trajectory.hpp"
#include "Base.h"

class Planner
{
public:
    Planner();
    Gimbal_Angle TrajectoryPlanning(std::shared_ptr<KinematicModel> model);
private:

    void GetTurn();
    // 获取轨迹
    void GetPath();
    // 获取多项式
    void GetFPolynomial();
    // 封装选择多项式和计算云台角度的逻辑
    Gimbal_Angle ComputeGimbalAngle();
    // 工具函数获取Yaw
    double GetGimbalYaw(double time);
    // 获取符合条件的多项式
    Eigen::VectorXd GetPolynomial(int extremum);
    // 工具函数获取具体多项式
    Eigen::VectorXd ComputeQuinticPoly(const Path_Point& start, const Path_Point& end);
    // 工具函数多项式代入时间
    Path_Point EvalPolyAtT(const Eigen::VectorXd& poly, double t);
    // 工具函数多项式是否满足加速度要求
    bool CheckAccelConstraint(const Eigen::VectorXd& poly, double t0, double t1);
    // 工具函数获取飞行时间和pitch
    tools::Trajectory GetTrajectory(double time);
    // 工具函数获取极值点
    void FindExtremumPoints();
    double hit_time;                          // 集中时间（飞行+预测）
    double predict_time;                      // 预测时间（手调）
    double max_yaw_acc;                       // 最大角加速度
    int left_extremum;                        // 左边极值点
    int right_extremum;                       // 右边极值点
    std::shared_ptr<KinematicModel> m_status; // 系统预测
    std::vector<Path_Point> path;             // 轨迹序列

};
#endif
