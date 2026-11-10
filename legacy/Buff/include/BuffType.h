#ifndef BUFF__TYPE_HPP
#define BUFF__TYPE_HPP

#include <algorithm>
#include <deque>
#include <eigen3/Eigen/Dense>  
#include <opencv2/core/eigen.hpp>
#include <opencv2/opencv.hpp>
#include <optional>
#include <string>
#include <vector>

namespace auto_buff
{

const int INF = 1000000;
enum PowerRune_type { SMALL, BIG };
enum FanBlade_type { _target, _unlight, _light };
enum Track_status { TRACK, TEM_LOSE, LOSE };

class FanBlade
{
public:
    cv::Point2f center;               // 待击打区域中心
    float radius;                     // 待击打区域半径
    std::vector<cv::Point2f> points;  // 四个点从左上角开始逆时针
    double angle, width, height;
    FanBlade_type type;  // 类型
    
    explicit FanBlade() = default;
    explicit FanBlade(cv::Point2f &center,float &radius, FanBlade_type t);
    explicit FanBlade(FanBlade_type t);
};

class PowerRune
{
public:
    cv::Point2f r_center;
    std::vector<FanBlade> fanblades;  // 按target开始顺时针
    bool adjacent;                    // 是否相邻
    int light_num;
    // 第一片扇页
    Eigen::Vector3d xyz_in_world;  // 单位：m
    Eigen::Vector3d ypr_in_world;  // 单位：rad
    Eigen::Vector3d ypd_in_world;  // 球坐标系
    Eigen::Vector3d blade_xyz_in_world;  // 单位：m
    Eigen::Vector3d blade_ypd_in_world;  // 球坐标系, 单位: m
   
    explicit PowerRune(
        std::vector<FanBlade> & ts, const cv::Point2f r_center,
        std::optional<PowerRune> last_powerrune);
    explicit PowerRune() = default;

    FanBlade & target() { return fanblades[0]; };
    FanBlade & target_next() { return fanblades[1]; };
    bool is_unsolve() const { return unsolvable_; }

private:
    double target_angle_;
    bool unsolvable_ = false;

    double atan_angle(cv::Point2f v) const;  // [0, 2CV_PI]
};
}  // namespace auto_buff
#endif  // BUFF_TYPE_HPP
