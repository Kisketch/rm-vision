#include "ArmorSolver.h"
#include "math.hpp"
#include <opencv2/opencv.hpp>
#include <opencv2/core/eigen.hpp>
#include "Base.h"
#include <numeric>
#include <string>


vector<Point3f> GetWorldPoints(double width,double height){
    return vector<Point3f> {
        Point3f(-width, -height, 0),
        Point3f(-width,  height, 0),
        Point3f( width,  height, 0),
        Point3f( width, -height, 0)
    };
}

Slover::Slover()
{
    auto yaml = YAML::LoadFile(YamlPath);
    auto R_camera2gimbal_data = yaml["Transform"]["R_camera2gimbal"].as<std::vector<double>>();
    auto t_camera2gimbal_data = yaml["Transform"]["t_camera2gimbal"].as<std::vector<double>>();
    R_camera2gimbal_ = Eigen::Matrix<double, 3, 3, Eigen::RowMajor>(R_camera2gimbal_data.data());
    t_camera2gimbal_ = Eigen::Matrix<double, 3, 1>(t_camera2gimbal_data.data());
    auto camera_matrix_data = yaml["Camera"]["camera_matrix"].as<std::vector<double>>();
    auto distort_coeffs_data = yaml["Camera"]["distort_coeffs"].as<std::vector<double>>();
    Eigen::Matrix<double, 3, 3, Eigen::RowMajor> camera_matrix(camera_matrix_data.data());
    Eigen::Matrix<double, 1, 5> distort_coeffs(distort_coeffs_data.data());
    cv::eigen2cv(camera_matrix, camera_matrix_);
    cv::eigen2cv(distort_coeffs, distort_coeffs_); 
    world_points_big = GetWorldPoints(0.115,0.025);
    world_points_small = GetWorldPoints(0.065,0.025);
    p_t_camera2gimbal_ = R_camera2gimbal_.transpose() * t_camera2gimbal_;
}

void Slover::UpdateStatus(Data_Save ds){
    double cosx = cos(ds.camera_roll_angle), sinx = sin(ds.camera_roll_angle);
    double cosy = cos(-ds.camera_pitch_angle), siny = sin(-ds.camera_pitch_angle);
    double cosz = cos(ds.camera_yaw_angle), sinz = sin(ds.camera_yaw_angle);
    R_gimbal2world_ << cosy*cosz,  cosz*sinx*siny - cosx*sinz,  cosx*cosz*siny + sinx*sinz,
                       cosy*sinz,  cosx*cosz + sinx*siny*sinz, -cosz*sinx + cosx*siny*sinz,
                        -siny,                cosy*sinx,                cosx*cosy;

    cosx = cos(ds.camera_pitch_angle);sinx = sin(ds.camera_pitch_angle);
    cosy = cos(-ds.camera_yaw_angle); siny = sin(-ds.camera_yaw_angle);
    cosz = cos(ds.camera_roll_angle); sinz = sin(ds.camera_roll_angle);                       
    p_R_armor2world_ << cosz*cosy - sinz*sinx*siny,   -sinz*cosx, cosz*siny + sinz*sinx*cosy,
                        sinz*cosy + cosz*sinx*siny,    cosz*cosx, sinz*siny - cosz*sinx*cosy,
                                     -cosx*siny,          sinx,            cosx*cosy;

    sys_yaw = ds.camera_yaw_angle;
}
    

void Slover::Solve(Armor &armor){
    if(armor.type == ArmorType::LARGE){
        world_points = world_points_big;
    }else{
        world_points = world_points_small;
    }
    is_outpost_ = armor.number == "outpost";
    cv::solvePnP(world_points, armor.points, camera_matrix_, distort_coeffs_, rvec, tvec, false, cv::SOLVEPNP_IPPE);
    Eigen::Vector3d xyz_in_camera;
    cv::cv2eigen(tvec, xyz_in_camera);
    armor.gim_pose = R_camera2gimbal_ * xyz_in_camera + t_camera2gimbal_;
    armor.abs_pose = R_gimbal2world_ * armor.gim_pose;
    armor.abs_ypd = tools::xyz2ypd(armor.abs_pose);

    p_t_armor2world_ = p_R_armor2world_ * xyz_in_camera + p_t_camera2gimbal_;
    double min_error = 1e10;
    double best_yaw = 0;
    for(int i=-70;i<70;i+=1){
        double yaw = sys_yaw + tools::D2R(i);
        auto image_points = Reprojection(yaw);
        double error = 0.0;
        for (int j = 0; j < 4; j++) {
            error += sqrt(pow(armor.points[j].x - image_points[j].x, 2) + pow(armor.points[j].y - image_points[j].y, 2));
        }
        if(error<min_error){
            min_error = error;
            best_yaw = yaw;
        }  
    }
    
    armor.yaw = best_yaw+PI;
    armor.predict_yaw = sys_yaw+PI;
    armor.re_points = Reprojection(best_yaw);
}

vector<Point2f> Slover::Reprojection(const double &yaw){
    auto sin_yaw = std::sin(yaw);
    auto cos_yaw = std::cos(yaw);
    auto sin_pitch = is_outpost_ ? std::sin(11*PI/12) : std::sin(13*PI/12);
    auto cos_pitch = is_outpost_ ? std::cos(11*PI/12) : std::cos(13*PI/12);
    Eigen::Matrix3d _p_R_armor2world_;
    _p_R_armor2world_ << cos_yaw,   sin_yaw * sin_pitch,   -sin_yaw * cos_pitch,
                               0,             cos_pitch,              sin_pitch,
                         sin_yaw,  -cos_yaw * sin_pitch,    cos_yaw * cos_pitch;
    Eigen::Matrix3d _R_armor2camera = p_R_armor2world_.transpose() * _p_R_armor2world_;
    cv::Vec3d _rvec;
    cv::Mat R_armor2camera_cv;
    cv::eigen2cv(_R_armor2camera, R_armor2camera_cv);
    cv::Rodrigues(R_armor2camera_cv, _rvec);
    std::vector<cv::Point2f> image_points;
    cv::projectPoints(world_points, _rvec, tvec, camera_matrix_,  distort_coeffs_, image_points);
    return image_points;
}