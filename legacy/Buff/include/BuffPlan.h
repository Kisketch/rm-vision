#ifndef AUTO_BUFF__AIMER_HPP
#define AUTO_BUFF__AIMER_HPP

#include <yaml-cpp/yaml.h>

#include <Eigen/Dense>
#include <chrono>
#include <cmath>
#include <vector>

#include "BuffTarget.h"
#include "BuffType.h"
#include "Base.h"

namespace auto_buff
{
class Planner
{
public:
    Planner();

    Gimbal_Angle Plan(Target & target, time_point timestamp);
private:
    double fire_gap_time_;
    double predict_time_;

    int mistake_count_ = 0;
    bool switch_fanblade_;

    double last_yaw_ = 0;
    double last_pitch_ = 0;
    bool GetSendAngle(Target & target, const double predict_time, double & yaw, double & pitch);
};
}  // namespace auto_buff
#endif  // AUTO_AIM__AIMER_HPP