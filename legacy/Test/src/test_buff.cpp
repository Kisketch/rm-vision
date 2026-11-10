#include "AutoBuff.h"
#include "test.h"
#include <random>
#include <fstream>
#include <chrono>
#include <cmath>
#include <vector>
#include <memory>
#include <opencv2/opencv.hpp>
#include <iomanip>

auto_buff::BigTarget target;

// 2D可视化类
class Buff2DVisualizer {
private:
    cv::Mat canvas_;
    const int CANVAS_SIZE = 800;
    const int CANVAS_MARGIN = 50;
    const int CENTER_X = 400;
    const int CENTER_Y = 400;
    const float SCALE = 300.0f;
    
    std::vector<cv::Point2f> actual_trajectory_;
    std::vector<cv::Point2f> ekf_trajectory_;
    std::vector<cv::Point2f> noisy_trajectory_;
    
    double total_angle_error_rad_ = 0.0;
    double total_noisy_error_rad_ = 0.0;
    double max_angle_error_rad_ = 0.0;
    double max_noisy_error_rad_ = 0.0;
    int sample_count_ = 0;
    
public:
    Buff2DVisualizer() {
        canvas_ = cv::Mat(CANVAS_SIZE, CANVAS_SIZE, CV_8UC3, cv::Scalar(255, 255, 255));
    }
    
    cv::Point2f worldToImageCoords(const Eigen::Vector3d& world_pos) {
        float img_x = CENTER_X + world_pos[1] * SCALE;
        float img_y = CENTER_Y - world_pos[2] * SCALE;
        return cv::Point2f(img_x, img_y);
    }
    
    void drawCoordinateSystem() {
        cv::arrowedLine(canvas_, 
                       cv::Point(CENTER_X, CANVAS_SIZE - CANVAS_MARGIN), 
                       cv::Point(CENTER_X, CANVAS_MARGIN), 
                       cv::Scalar(0, 0, 0), 2);
        cv::arrowedLine(canvas_, 
                       cv::Point(CANVAS_MARGIN, CENTER_Y), 
                       cv::Point(CANVAS_SIZE - CANVAS_MARGIN, CENTER_Y), 
                       cv::Scalar(0, 0, 0), 2);
        
        cv::putText(canvas_, "Y", cv::Point(CANVAS_SIZE - 30, CENTER_Y - 10), 
                   cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 0, 0), 2);
        cv::putText(canvas_, "Z", cv::Point(CENTER_X + 10, CANVAS_MARGIN + 20), 
                   cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 0, 0), 2);
        
        cv::circle(canvas_, cv::Point(CENTER_X, CENTER_Y), 8, cv::Scalar(128, 128, 128), -1);
    }
    
    void update(const Eigen::Vector3d& actual_pos, const Eigen::Vector3d& ekf_pos, 
                const Eigen::Vector3d& noisy_pos, double actual_angle, double ekf_roll, 
                double noisy_angle, double time, double actual_spd, double ekf_spd) {
        canvas_.setTo(cv::Scalar(255, 255, 255));
        drawCoordinateSystem();
        
        cv::Point2f actual_img_pos = worldToImageCoords(actual_pos);
        cv::Point2f ekf_img_pos = worldToImageCoords(ekf_pos);
        cv::Point2f noisy_img_pos = worldToImageCoords(noisy_pos);
        
        actual_trajectory_.push_back(actual_img_pos);
        ekf_trajectory_.push_back(ekf_img_pos);
        noisy_trajectory_.push_back(noisy_img_pos);
        
        if (actual_trajectory_.size() > 200) {
            actual_trajectory_.erase(actual_trajectory_.begin());
            ekf_trajectory_.erase(ekf_trajectory_.begin());
            noisy_trajectory_.erase(noisy_trajectory_.begin());
        }
        
        for (size_t i = 1; i < actual_trajectory_.size(); i++) {
            cv::line(canvas_, actual_trajectory_[i-1], actual_trajectory_[i], 
                    cv::Scalar(0, 0, 255), 2);
        }
        
        for (size_t i = 1; i < noisy_trajectory_.size(); i++) {
            cv::line(canvas_, noisy_trajectory_[i-1], noisy_trajectory_[i], 
                    cv::Scalar(128, 128, 128), 1);
        }
        
        for (size_t i = 1; i < ekf_trajectory_.size(); i++) {
            cv::line(canvas_, ekf_trajectory_[i-1], ekf_trajectory_[i], 
                    cv::Scalar(0, 255, 0), 2);
        }
        
        cv::circle(canvas_, actual_img_pos, 8, cv::Scalar(0, 0, 255), -1);
        cv::circle(canvas_, noisy_img_pos, 6, cv::Scalar(128, 128, 128), -1);
        cv::circle(canvas_, ekf_img_pos, 8, cv::Scalar(0, 255, 0), -1);
        
        double ekf_error_rad = calculateAngleError(actual_angle, ekf_roll);
        double noisy_error_rad = calculateAngleError(actual_angle, noisy_angle);
        
        total_angle_error_rad_ += ekf_error_rad;
        total_noisy_error_rad_ += noisy_error_rad;
        if (ekf_error_rad > max_angle_error_rad_) {
            max_angle_error_rad_ = ekf_error_rad;
        }
        if (noisy_error_rad > max_noisy_error_rad_) {
            max_noisy_error_rad_ = noisy_error_rad;
        }
        sample_count_++;
        
        drawInfoPanel(actual_angle, ekf_roll, noisy_angle, 
                     radToDeg(ekf_error_rad), radToDeg(noisy_error_rad),
                     actual_spd, ekf_spd, time);
        
        cv::imshow("Buff Motion Visualization (YZ Plane)", canvas_);
    }
    
private:
    void drawInfoPanel(double actual_angle, double ekf_angle, double noisy_angle, 
                      double ekf_error_deg, double noisy_error_deg,
                      double actual_spd, double ekf_spd, double time) {
        int panel_x = 20;
        int panel_y = 20;
        int line_height = 25;
        
        cv::rectangle(canvas_, cv::Rect(panel_x - 10, panel_y - 10, 350, 180), 
                     cv::Scalar(240, 240, 240), -1);
        cv::rectangle(canvas_, cv::Rect(panel_x - 10, panel_y - 10, 350, 180), 
                     cv::Scalar(200, 200, 200), 2);
        
        cv::putText(canvas_, "Buff Motion Analysis (5% Noise)", cv::Point(panel_x, panel_y), 
                   cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 0, 0), 2);
        
        std::string time_str = cv::format("Time: %.2f s", time);
        cv::putText(canvas_, time_str, cv::Point(panel_x, panel_y + line_height), 
                   cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 0), 1);
        
        std::string angle_str = cv::format("Actual Angle:  %.1f deg", radToDeg(actual_angle));
        cv::putText(canvas_, angle_str, cv::Point(panel_x, panel_y + line_height * 2), 
                   cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 255), 1);
        
        std::string noisy_angle_str = cv::format("Noisy Angle:  %.1f deg", radToDeg(noisy_angle));
        cv::putText(canvas_, noisy_angle_str, cv::Point(panel_x, panel_y + line_height * 3), 
                   cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(128, 128, 128), 1);
        
        std::string ekf_angle_str = cv::format("EKF Angle:    %.1f deg", radToDeg(ekf_angle));
        cv::putText(canvas_, ekf_angle_str, cv::Point(panel_x, panel_y + line_height * 4), 
                   cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 1);
        
        std::string noisy_error_str = cv::format("Noisy Error:  %.2f deg", noisy_error_deg);
        cv::putText(canvas_, noisy_error_str, cv::Point(panel_x, panel_y + line_height * 5), 
                   cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(128, 128, 128), 1);
        
        std::string ekf_error_str = cv::format("EKF Error:    %.2f deg", ekf_error_deg);
        cv::putText(canvas_, ekf_error_str, cv::Point(panel_x, panel_y + line_height * 6), 
                   cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 1);
    }
    
    double calculateAngleError(double actual_angle, double estimated_angle) {
        double error = actual_angle - estimated_angle;
        while (error > M_PI) error -= 2 * M_PI;
        while (error < -M_PI) error += 2 * M_PI;
        return std::abs(error);
    }
    
    double radToDeg(double rad) {
        return rad * 180.0 / M_PI;
    }
    
public:
    void printStatistics() {
        if (sample_count_ > 0) {
            double avg_ekf_error_deg = radToDeg(total_angle_error_rad_) / sample_count_;
            double avg_noisy_error_deg = radToDeg(total_noisy_error_rad_) / sample_count_;
            double max_ekf_error_deg = radToDeg(max_angle_error_rad_);
            double max_noisy_error_deg = radToDeg(max_noisy_error_rad_);
            
            std::cout << "\n=== 统计数据 (5%噪声) ===" << std::endl;
            std::cout << "总样本数: " << sample_count_ << std::endl;
            std::cout << "带噪声观测平均角度误差: " << avg_noisy_error_deg << "°" << std::endl;
            std::cout << "EKF估计平均角度误差: " << avg_ekf_error_deg << "°" << std::endl;
            std::cout << "带噪声观测最大角度误差: " << max_noisy_error_deg << "°" << std::endl;
            std::cout << "EKF估计最大角度误差: " << max_ekf_error_deg << "°" << std::endl;
            std::cout << "EKF滤波效果提升: " << (avg_noisy_error_deg - avg_ekf_error_deg) << "°" << std::endl;
            std::cout << "==========================" << std::endl;
        }
    }
    
    int waitKey(int delay = 1) {
        return cv::waitKey(delay);
    }
    
    void saveImage(const std::string& filename) {
        cv::imwrite(filename, canvas_);
        std::cout << "图像已保存到: " << filename << std::endl;
    }
};

class DataGenerator {
private:
    std::default_random_engine gen_;
    double a_min_, a_max_, w_min_, w_max_;
    std::chrono::steady_clock::time_point start_time_;
    Buff2DVisualizer visualizer_;
    
    double actual_a_, actual_w_, actual_b_;
    std::normal_distribution<double> noise_distribution_;
    int sample_count_ = 0;
    
public:
    // 修改构造函数：使用固定随机数种子
    DataGenerator(unsigned int seed = 42) : noise_distribution_(0.0, 0.02) {
        // 使用固定种子保证可复现性
        gen_.seed(seed);
        
        a_min_ = 0.780;
        a_max_ = 1.045;
        w_min_ = 1.884;
        w_max_ = 2.000;
        start_time_ = std::chrono::steady_clock::now();
        
        std::cout << "使用随机数种子: " << seed << " (保证结果可复现)" << std::endl;
    }
    
    double addNoise(double value) {
        return value * (1.0 + noise_distribution_(gen_));
    }
    
    void generateTestData(int num_samples, double time_interval) {
        actual_a_ = 0.9;
        actual_w_ = 1.9;
        actual_b_ = 2.090 - actual_a_;
        
        std::cout << "生成的参数: a=" << actual_a_ << ", w=" << actual_w_ << ", b=" << actual_b_ << std::endl;
        std::cout << "添加5%高斯噪声到ypr和camera_pos" << std::endl;
        std::cout << "开始EKF滤波器测试..." << std::endl;
        std::cout << "按ESC键退出测试" << std::endl;
        
        for (int i = 0; i < num_samples; ++i) {
            double t = i * time_interval;
            
            auto timestamp = start_time_ + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::duration<double>(t));
            
            double spd_actual = actual_a_ * sin(actual_w_ * t) + actual_b_;
            double roll_angle = (-actual_a_/actual_w_ * cos(actual_w_ * t) + actual_b_ * t) + (actual_a_/actual_w_);
            if(roll_angle>2*PI)roll_angle-=2*PI;
            if (!testEKF(timestamp, roll_angle, spd_actual, t, num_samples)) {
                break;
            }
            
            int key = visualizer_.waitKey(1);
            if (key == 27) {
                std::cout << "用户中断测试" << std::endl;
                break;
            }
            
            usleep(10000);
        }
        
        // visualizer_.saveImage("buff_motion_with_noise.png");
        visualizer_.printStatistics();
    }
    
private:
    auto_buff::PowerRune createSimulatedPowerRune(double roll_angle) {
        std::vector<auto_buff::FanBlade> fanblades;
        
        cv::Point2f center(320, 240);
        float radius = 50.0f;
        auto_buff::FanBlade target(center, radius, auto_buff::_light);
        std::vector<cv::Point2f> points = {
            cv::Point2f(300, 220), cv::Point2f(300, 260),
            cv::Point2f(340, 260), cv::Point2f(340, 220),
            center, cv::Point2f(320, 190)
        };
        target.points = points;
        fanblades.push_back(target);
        auto_buff::PowerRune powerrune(fanblades, center, std::nullopt);
    
        Eigen::Vector3d ypr(0.0, 0.0, addNoise(roll_angle));
        double distance = addNoise(7.0);
        
        Eigen::Matrix3d R = tools::rotation_matrix(ypr);
        Eigen::Vector3d camera_pos(distance, 0.0, 0.0);
        Eigen::Vector3d world_pos = R * camera_pos;
        Eigen::Vector3d ypd = tools::xyz2ypd(world_pos);
        powerrune.ypd_in_world = ypd;
        powerrune.ypr_in_world = ypr;
        
        Eigen::Vector3d blade_xyz_in_buff{{distance, 0, 700e-3}};
        Eigen::Vector3d blade_world_pos = R * blade_xyz_in_buff;
        powerrune.blade_ypd_in_world = tools::xyz2ypd(blade_world_pos);
    
        return powerrune;
    }
    
    Eigen::Vector3d calculateNoisyPosition(double roll_angle) {
        Eigen::Vector3d noisy_ypr(0.0, 0.0, addNoise(roll_angle));
        Eigen::Matrix3d noisy_R = tools::rotation_matrix(noisy_ypr);
        Eigen::Vector3d blade_xyz_in_buff{{0, 0, 0.7}};
        Eigen::Vector3d noisy_blade_pos = noisy_R * blade_xyz_in_buff;
        
        return noisy_blade_pos;
    }
    
    bool testEKF(std::chrono::steady_clock::time_point time, double roll_angle, 
                 double actual_spd, double current_time, int num_samples) {
        auto simulated_powerrune = createSimulatedPowerRune(roll_angle);
        std::optional<auto_buff::PowerRune> power_runes = simulated_powerrune;

        target.get_target(power_runes, time);
        
        if (!target.is_unsolve()) {
            auto x = target.ekf_x();
            double ekf_spd = x[6];
            double ekf_roll = x[5];
            
            // 计算实际位置
            Eigen::Vector3d actual_ypr(0.0, 0.0, roll_angle);
            Eigen::Matrix3d actual_R = tools::rotation_matrix(actual_ypr);
            Eigen::Vector3d blade_xyz_in_buff{{0, 0, 0.7}};
            Eigen::Vector3d actual_blade_pos = actual_R * blade_xyz_in_buff;
            
            // 计算EKF估计位置
            Eigen::Vector3d ekf_ypr(0.0, 0.0, ekf_roll);
            Eigen::Matrix3d ekf_R = tools::rotation_matrix(ekf_ypr);
            Eigen::Vector3d ekf_blade_pos = ekf_R * blade_xyz_in_buff;
            
            // 计算带噪声的观测位置和角度
            Eigen::Vector3d noisy_blade_pos = calculateNoisyPosition(roll_angle);
            double noisy_angle = addNoise(roll_angle);
            
            // 更新可视化
            visualizer_.update(actual_blade_pos, ekf_blade_pos, noisy_blade_pos,
                             roll_angle, ekf_roll, noisy_angle, current_time, 
                             actual_spd, ekf_spd);
            
            // 控制台输出
            if (sample_count_ % 50 == 0) {
                std::cout << "\n=== 样本 " << sample_count_ << " ===" << std::endl;
                std::cout << "时间: " << std::fixed << std::setprecision(2) << current_time << "s" << std::endl;
                std::cout << "实际角度: " << std::setprecision(3) << (roll_angle * 180/M_PI) << "°" << std::endl;
                std::cout << "噪声角度: " << std::setprecision(3) << (noisy_angle * 180/M_PI) << "°" << std::endl;
                std::cout << "EKF角度: " << std::setprecision(3) << (ekf_roll * 180/M_PI) << "°" << std::endl;
                std::cout << "噪声误差: " << std::setprecision(3) << std::abs(noisy_angle - roll_angle) * 180/M_PI << "°" << std::endl;
                std::cout << "EKF误差: " << std::setprecision(3) << std::abs(ekf_roll - roll_angle) * 180/M_PI << "°" << std::endl;
                std::cout << "实际角速度: " << std::setprecision(4) << actual_spd << " rad/s" << std::endl;
                std::cout << "EKF角速度: " << std::setprecision(4) << ekf_spd << " rad/s" << std::endl;
                std::cout << "角速度误差: " << std::setprecision(4) << std::abs(actual_spd - ekf_spd) << " rad/s" << std::endl;
            }
            
            sample_count_++;
            return true;
        } else {
            if (sample_count_ == 0) {
                std::cout << "EKF滤波器初始化中..." << std::endl;
            }
            return false;
        }
    }
};

void TestBuff() {
    try {
        std::cout << "开始EKF滤波器仿真测试 (带5%噪声)" << std::endl;
        std::cout << "========================================" << std::endl;
        
        // 使用固定种子42保证可复现性
        unsigned int seed = 42;
        DataGenerator generator(seed);
        
        int num_samples = 500;
        double time_interval = 0.01;
        
        generator.generateTestData(num_samples, time_interval);
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "仿真测试完成!" << std::endl;
        std::cout << "随机数种子: " << seed << " (可用于复现本次结果)" << std::endl;
        
        cv::waitKey(0);
        
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
    }
}

