#ifndef ARMOR_PREDICTOR_H
#define ARMOR_PREDICTOR_H

#include <iostream>
#include <cmath>
#include <Eigen/Dense>
#include "ArmorTypes.h"
#include <algorithm>
#include "math.hpp"

/**
 * @brief 运动学状态
 */
class KinematicModel
{
public:
    KinematicModel();
    ~KinematicModel();

    virtual Armors GetArmors(double predict_time = 0) = 0;

public:
    cv::Point2d center;             // 旋转中心位置
    cv::Point2d velocity;
    std::vector<double> height;     // 装甲高度：0对应索引0、2，1对应索引1、3
    std::vector<double> radius;     // 旋转半径：0对应索引0、2，1对应索引1、3
    double phase;                   // 角度（并非定值）
    double palstance;               // 角速度

    int number;                     // 装甲板数量
};

class StandardModel : public KinematicModel
{
public:
    StandardModel();
    StandardModel(const Eigen::Matrix<double, 10, 1> &X);
    ~StandardModel();

    /**
     * @brief 获取该运动状态下所有装甲板
     * @param predict_time 预测时间
     */
    Armors GetArmors(double predict_time = 0);

};

class OutpostModel : public KinematicModel
{
public:
    OutpostModel();
    OutpostModel(const Eigen::Matrix<double, 10, 1> &X);
    ~OutpostModel();

    /**
     * @brief 获取该运动状态下所有装甲板
     * @param predict_time 预测时间
     */
    Armors GetArmors(double predict_time = 0);


};
#endif
