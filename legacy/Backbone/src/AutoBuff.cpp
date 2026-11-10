#include "AutoBuff.h"
#include <fstream>
#include "logger.hpp"
AutoBuff::AutoBuff(){
    readYAML(YamlPath);
    discover_ = std::make_unique<auto_buff::BuffDetector>();
    slover_ = std::make_unique<auto_buff::Solver>();
    target_ = std::make_unique<auto_buff::BigTarget>();
    target_->set_solver(slover_.get());
    planner_ = std::make_unique<auto_buff::Planner>();
}

void AutoBuff::show(const std::optional<auto_buff::PowerRune> &power_runes, cv::Mat &img) {
    if (power_runes.has_value()) {
        const auto &p = power_runes.value();
        auto points = slover_->reproject_buff(p.xyz_in_world, p.ypr_in_world.x(), p.ypr_in_world.z());
        for (auto point : points) cv::circle(img, point, 2, cv::Scalar(0, 0, 255), -1);

        // 记录滤波器输出与识别器输出的像素差
        auto ekf_Rxyz = target_->point_buff2world(Eigen::Vector3d(0.0, 0.0, 0.0));
        auto ekf_points = slover_->reproject_buff(ekf_Rxyz, target_->ekf_x()[4], target_->ekf_x()[5]);
        float pixel_diff = cv::norm(ekf_points[4] - p.fanblades[0].points[4]);
        tools::logger()->info("[AutoBuff] Pixel Diff: {:.2f} pixels", pixel_diff);
        // 绘制所有ekf点
        for (auto point : ekf_points) cv::circle(img, point, 4, cv::Scalar(128, 0, 128), -1);
    }
}

void AutoBuff::AutoAim(cv::Mat &img,const Data_Save &ds,Data_Get &dg){

    // if(ds.armor_or_buff != last_mode){
    //     if(ds.armor_or_buff == 1) target_ = std::make_unique<auto_buff::SmallTarget>();
    //     else target_ = std::make_unique<auto_buff::BigTarget>();
    //     last_mode = ds.armor_or_buff;
    // }
    dg.Vision2Control();
    auto power_runes = discover_->Detect(img);
    slover_->UpdateStatus(ds);
    slover_->Solve(power_runes);
    target_->get_target(power_runes,ds.timestamp); 

    // Call the renamed function to process power runes
    show(power_runes, img);

    auto target_copy =  target_->clone();
    auto gimbal_angle = planner_->Plan(*target_copy,ds.timestamp);
    Transmit(gimbal_angle,ds,dg,img,power_runes);
}

void AutoBuff::Transmit(const Gimbal_Angle &gimbal_angle,const Data_Save &ds,Data_Get &dg,cv::Mat &img, const std::optional<auto_buff::PowerRune> &power_runes){
    int32_t yaw = (tools::R2D(gimbal_angle.target_yaw) + compensate_yaw) * 1e4;
    int32_t pitch = (tools::R2D(gimbal_angle.target_pitch) + compensate_pitch) * 1e4;
    bool yaw_delta = abs(tools::R2D(gimbal_angle.target_yaw) + compensate_yaw - tools::R2D(ds.camera_yaw_angle)) < 1;
    bool pitch_delta = abs(tools::R2D(gimbal_angle.target_pitch) + compensate_pitch - tools::R2D(ds.camera_pitch_angle)) < 1;
    bool fire_delta = (yaw_delta && pitch_delta);
    if(!gimbal_angle.manage_yaw){
        yaw = (tools::R2D(ds.camera_yaw_angle)) * 1e4;
        pitch = (tools::R2D(ds.camera_pitch_angle)) * 1e4;
    }
    bool follow_dalta = gimbal_angle.manage_yaw && yaw!=0 && pitch !=0;

    // 0.8s发射冷却：一次fire后需间隔0.8s才能再次发射
    bool can_fire = true;
    if (fire_delta && last_fire_time_.has_value()) {
        double since_last_fire = tools::delta_time(ds.timestamp, *last_fire_time_);
        can_fire = (since_last_fire >= 0.7);
    }
    // 必须同时识别到两片扇叶才允许发射
    bool two_blades = power_runes.has_value() && power_runes->light_num == 2;
    bool fire = fire_delta && two_blades ;

    if (fire) {
        last_fire_time_ = ds.timestamp;
        cv::putText(img, "Fire", cv::Point(40, 40), cv::FONT_HERSHEY_SIMPLEX, 1.2, cv::Scalar(0, 0, 255), 4);
         dg.Vision2Control(yaw,pitch,0,0,follow_dalta,2);
    } else {
        std::string text = "target_yaw " + std::to_string(tools::R2D(gimbal_angle.target_yaw) ) +
                          " target_pitch " + std::to_string(tools::R2D(gimbal_angle.target_pitch) )
                           + " found " + std::to_string(follow_dalta);
        std::string text_now = "now_yaw " + std::to_string(tools::R2D(ds.camera_yaw_angle) ) +
                          " now_pitch " + std::to_string(tools::R2D(ds.camera_pitch_angle) );                   
        cv::putText(img, text, cv::Point(40, 40), cv::FONT_HERSHEY_SIMPLEX, 1.2, cv::Scalar(0, 0, 255), 4);
        cv::putText(img, text_now, cv::Point(40, 80), cv::FONT_HERSHEY_SIMPLEX, 1.2, cv::Scalar(0, 0, 255), 4);
         dg.Vision2Control(yaw,pitch,0,0,follow_dalta,0);
    }

}

YAML::Node AutoBuff::readYAML(const std::string& filename){
    try {
        YAML::Node config = YAML::LoadFile(filename);
        compensate_pitch = config["Buff"]["compensate_pitch"].as<double>();
        compensate_yaw = config["Buff"]["compensate_yaw"].as<double>();
    } catch (const YAML::Exception& e) {
        throw std::runtime_error("YAML Error: " + std::string(e.what()) + "AutoBuff");
    }
    return YAML::LoadFile(filename);
}

