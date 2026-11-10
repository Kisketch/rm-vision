#ifndef AUTO_BUFF__TARGET_HPP
#define AUTO_BUFF__TARGET_HPP

#include <Eigen/Dense>
#include <opencv2/opencv.hpp>
#include <optional>
#include <string>
#include <vector>

#include "BuffDetect.h"
#include "extended_kalman_filter.hpp"
#include "logger.hpp"
#include "math_tools.hpp"
#include "Base.h"
namespace auto_buff
{
class Solver;  // 前向声明
class Voter
{
public:
    Voter();
    void vote(const double angle_last, const double angle_now);
    int clockwise();

private:
    int clockwise_;
};

/// Target 基类

class Target
{
public:
    Target();
    virtual void get_target(const std::optional<PowerRune> & p,const time_point & timestamp) = 0;  // 纯虚函数

    virtual void predict(double dt) = 0;  // 纯虚函数

    Eigen::Vector3d point_buff2world(const Eigen::Vector3d & point_in_buff) const;

    Eigen::Vector3d fire_buff2world(const Eigen::Vector3d & point_in_buff) const;

    bool is_unsolve() const;

    void set_unsolvable(bool v) { unsolvable_ = v; }

    void set_solver(Solver* s) { solver_ = s; }

    Eigen::VectorXd ekf_x() const;

    double spd = 0;  //调试用

    virtual std::unique_ptr<Target> clone() const = 0;
protected:
    virtual void init(double nowtime, const PowerRune & p) = 0;  // 纯虚函数

    virtual void update(double nowtime, const PowerRune & p) = 0;  // 纯虚函数

    Eigen::VectorXd x0_;
    Eigen::MatrixXd P0_;
    Eigen::MatrixXd A_;
    Eigen::MatrixXd Q_;
    Eigen::MatrixXd H_;
    Eigen::MatrixXd R_;
    tools::ExtendedKalmanFilter ekf_;
    double lasttime_ = 0;
    Voter voter;  // 逆时针-1 顺时针1
    bool first_in_;
    bool unsolvable_;
    double fire_mode = 0;
    Solver* solver_ = nullptr;  // 由AutoBuff注入，用于重投影偏差检查
    
};

/// SmallTarget子类

class SmallTarget : public Target
{
public:
    SmallTarget();

    void get_target(const std::optional<PowerRune> & p, const time_point & timestamp) override;

    void predict(double dt) override;

    std::unique_ptr<Target> clone() const override {
        return std::make_unique<SmallTarget>(*this);
    }

private:
    void init(double nowtime, const PowerRune & p) override;

    void update(double nowtime, const PowerRune & p) override;

    Eigen::MatrixXd h_jacobian() const;

    // const double SMALL_W = CV_PI / 3;

    const double SMALL_W = 0;
};

/// BigTarget子类

class BigTarget : public Target
{
public:
    BigTarget();

    void get_target(const std::optional<PowerRune> & p, const time_point & timestamp) override;

    void predict(double dt) override;

    std::unique_ptr<Target> clone() const override {
        return std::make_unique<BigTarget>(*this);
    }

private:
    void init(double nowtime, const PowerRune & p) override;

    void update(double nowtime, const PowerRune & p) override;

    Eigen::MatrixXd h_jacobian() const;

    int jump_counter_ = 0;  // 扇叶跳变连续帧计数
};

}  // namespace auto_buff
#endif