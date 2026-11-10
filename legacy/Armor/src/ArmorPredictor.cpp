#include "ArmorPredictor.h"
#include <fstream>


KinematicModel::KinematicModel()
{
    height.clear();
    radius.clear();
}
KinematicModel::~KinematicModel()
{}

StandardModel::StandardModel()
:   StandardModel(Eigen::Matrix<double, 10, 1>::Zero())
{}
StandardModel::StandardModel(const Eigen::Matrix<double, 10, 1> &X)
:   KinematicModel()
{
    height.resize(2);
    radius.resize(2);
    number = 4;

    center = cv::Point2d(X(0), X(2));        
    velocity = cv::Point2d(X(1), X(3));
    height[0] = X(4);     //z1
    height[1] = X(5);     //z2
    radius[0] = X(6);     //r1
    radius[1] = X(7);     //r2
    phase = X(8);         //yaw
    palstance = X(9);     //角速度
}
StandardModel::~StandardModel(){}

Armors StandardModel::GetArmors(double predict_time)
{
    Armors armors(number);
    for (int i = 0; i < number; i++)
    {
        double angle = _std_radian(phase + palstance * predict_time + i * PI / 2);
        cv::Point2d point = center + velocity * predict_time +
                            cv::Point2d(radius[i % 2] * cos(angle), radius[i % 2] * sin(angle));
        armors[i].abs_pose = {point.x, point.y, height[i % 2]};
        armors[i].predict_yaw = angle;
    }
    return armors;
}


OutpostModel::OutpostModel()
:   OutpostModel(Eigen::Matrix<double, 10, 1>::Zero())
{}
OutpostModel::OutpostModel(const Eigen::Matrix<double, 10, 1> &X)
:   KinematicModel()
{
    height.resize(3);
    radius.resize(1);
    number = 3;

    center = cv::Point2d(X(0), X(2));        
    velocity = cv::Point2d(X(1), X(3));
    height[0] = X(4);     //z1
    height[1] = X(5);     //z2
    height[2] = X(6);     //z3
    radius[0] = X(7);     //r1
    phase = X(8);         //yaw
    palstance = X(9);     //角速度
}
OutpostModel::~OutpostModel(){}

Armors OutpostModel::GetArmors(double predict_time)
{
    Armors armors(number);
    for (int i = 0; i < number; i++)
    {
        double angle = _std_radian(phase + palstance * predict_time + i * 2*PI/3);
        cv::Point2d point = center + velocity * predict_time +
                            cv::Point2d(radius[0] * cos(angle), radius[0] * sin(angle));
        armors[i].abs_pose = {point.x, point.y, height[i % 3]};
        armors[i].predict_yaw = angle;
    }
    return armors;
}

