#ifndef ARMOR_SOLVER_H
#define ARMOR_SOLVER_H

#pragma once
#include <queue>
#include "Base.h"
#include "SerialPort.h"
#include <fstream>
#include "ArmorTypes.h"
#include <yaml-cpp/yaml.h>
using namespace std;
using namespace cv;

class Slover
{
public:
    // 初始化
    Slover();
    // 更新状态
    void UpdateStatus(Data_Save ds);
    // 解算主接口
    void Solve(Armor &armor);
    // BuffPNP
    void BuffPnP(vector<Point2f> Buff_points, Point3d &pose, Point3d &Rpose, Point3d &Aimpose, double &roll, double &yaw);
private:
    // 重投影函数
    vector<Point2f> Reprojection(const double &yaw);
    vector<Point3f> world_points;                                        // 装甲板3维坐标
    vector<Point3f> world_points_big;                                    // 大装甲板3维坐标
    vector<Point3f> world_points_small;                                  // 小装甲板3维坐标
    cv::Mat camera_matrix_;                                              // 相机内参
    cv::Mat distort_coeffs_;                                             // 畸变系数
    Eigen::Matrix3d R_camera2gimbal_;                                    // 相机系->云台系旋转矩阵
    Eigen::Vector3d t_camera2gimbal_;                                    // 相机系->云台系平移向量
    Eigen::Matrix3d R_gimbal2world_;                                     // 云台系->世界系旋转矩阵
    double sys_yaw;                                                      // 云台yaw角
    cv::Mat rvec, tvec;                                                  // 旋转与平移向量
    bool is_outpost_ = false;                                            // 是否未前哨站
    Eigen::Vector3d p_t_camera2gimbal_;                                  // PNP系下相机系->云台系平移向量
    Eigen::Matrix3d p_R_armor2world_;                                    // PNP系下相机系->世界系旋转矩阵
    Eigen::Vector3d p_t_armor2world_;                                    // PNP系下相机系->世界系旋转矩阵

};

#endif
