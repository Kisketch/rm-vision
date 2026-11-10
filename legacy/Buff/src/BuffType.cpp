#include "BuffType.h"
#include <algorithm>
#include "logger.hpp"
#include "Base.h"
namespace auto_buff
{
FanBlade::FanBlade(cv::Point2f &center,float &radius, FanBlade_type t)
: center(center), radius(radius), type(t)
{}

FanBlade::FanBlade(FanBlade_type t) : type(t)
{
    if (t != _unlight) exit(-1);
}

PowerRune::PowerRune(
  std::vector<FanBlade> & ts, const cv::Point2f center, std::optional<PowerRune> last_powerrune)
: r_center(center), light_num(ts.size())
{
    if(light_num == 2){
        ts[0].angle = atan_angle(ts[0].center);
        ts[1].angle = atan_angle(ts[1].center);
        
        FanBlade selected_blade = ts[0];
        if (last_powerrune) {
            double target_angle = last_powerrune->target().angle; // Extract angle from FanBlade
            double delta0 = std::abs(_std_radian(ts[0].angle - target_angle));
            double delta1 = std::abs(_std_radian(ts[1].angle - target_angle));
            selected_blade = (delta0 < delta1) ? ts[0] : ts[1];
        }
        fanblades.emplace_back(selected_blade);
    } else {
        fanblades.emplace_back(ts[0]);
    }
};

double PowerRune::atan_angle(cv::Point2f point) const
{
  auto v = point - r_center;
  auto angle = std::atan2(v.y, v.x);
  return angle >= 0 ? angle : angle + CV_2PI;
}
}  // namespace auto_buff
