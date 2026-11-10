#include "ArmorPlanner.h"
#include <yaml-cpp/yaml.h>
#include <cmath>
#include <fstream>
Planner::Planner(){
    YAML::Node config = YAML::LoadFile(YamlPath);
    predict_time = config["Planer"]["predict_time"].as<double>();
    max_yaw_acc = config["Planer"]["max_yaw_acc"].as<double>();
}

Gimbal_Angle Planner::ComputeGimbalAngle() {
    auto left_poly = GetPolynomial(left_extremum);
    auto right_poly = GetPolynomial(right_extremum);

    // 优雅的选择逻辑
    auto select_poly = [&]() -> Eigen::VectorXd {
        if (left_poly.isZero() && right_poly.isZero()) {
            return Eigen::VectorXd::Zero(6);
        }
        
        if (!left_poly.isZero() && !right_poly.isZero()) {
            int center = (path.size()-1)/2;
            int left_distance = std::abs(left_extremum - center);
            int right_distance = std::abs(right_extremum - center);
            
            if (left_distance <= right_distance) {
                return left_poly;
            } else {
                return right_poly;
            }
        }
        
        return !left_poly.isZero() ? left_poly : right_poly;
    };
    
    // 统一的处理逻辑
    Eigen::VectorXd selected_poly = select_poly();
    int center_index = (path.size()-1)/2;

    auto trajectory_ = GetTrajectory(hit_time);
    Gimbal_Angle gimbal_angle;
    gimbal_angle.target_yaw = path[(path.size()-1)/2].angle;
    gimbal_angle.target_pitch = trajectory_.pitch;
    
    if (selected_poly.isZero()) {
        gimbal_angle.manage_yaw = path[center_index].angle;
        gimbal_angle.yaw_vel = path[center_index].angle_vel;
    } else {
        auto poly_result = EvalPolyAtT(selected_poly, path[center_index].time);
        gimbal_angle.manage_yaw = poly_result.angle;
        gimbal_angle.yaw_vel = poly_result.angle_vel;
    }
    return gimbal_angle;
}

Gimbal_Angle Planner::TrajectoryPlanning(std::shared_ptr<KinematicModel> model){
    m_status = model;
    auto trajectory_ = GetTrajectory(0);
    hit_time = trajectory_.fly_time + predict_time;
    
    GetPath();
    FindExtremumPoints();
    
    Gimbal_Angle gimbal_angle = ComputeGimbalAngle();
    
    return gimbal_angle;
}

double Planner::GetGimbalYaw(double time){
    Armors armors = m_status->GetArmors(time);
    double min_distance = 100;
    Eigen::Vector3d point = Eigen::Vector3d::Zero();
    for(auto armor : armors){
        if(min_distance > armor.abs_pose.norm()){
            point = armor.abs_pose;
            min_distance = armor.abs_pose.norm();
        } 
    }
    double yaw = std::atan2(point.y(), point.x());
    
    return yaw;
}

Eigen::VectorXd Planner::GetPolynomial(int extremum){
    if(extremum == -1) {
        return Eigen::VectorXd::Zero(6);
    }
    
    for(int i = 1; (extremum + i < static_cast<int>(path.size())) && (extremum - i >= 0); ++i){
        int start_idx = extremum - i;
        int end_idx = extremum + i;
        
        auto polynomial = ComputeQuinticPoly(path[start_idx], path[end_idx]);
        
        if(CheckAccelConstraint(polynomial, path[start_idx].time, path[end_idx].time)){
            int center_index = (path.size()-1)/2;
            int distance_to_center = abs(extremum - center_index);
            
            if(distance_to_center <= i){
                return polynomial;
            }else{
                break;
            }
        }
    }
    
    return Eigen::VectorXd::Zero(6);
}

void Planner::FindExtremumPoints(){
    left_extremum = -1;
    right_extremum = -1;
    int center_index = (path.size()-1)/2;
    
    // 从中心点向左遍历
    for(int i = center_index; i >= 1; i--){
        if((path[i].angle > path[i-1].angle && path[i].angle > path[i+1].angle) || 
           (path[i].angle < path[i-1].angle && path[i].angle < path[i+1].angle)){
            left_extremum = i;
            break;
        }
    }
    
    // 从中心点向右遍历
    for(int i = center_index; i < static_cast<int>(path.size())-1; i++){
        if((path[i].angle > path[i-1].angle && path[i].angle > path[i+1].angle) || 
           (path[i].angle < path[i-1].angle && path[i].angle < path[i+1].angle)){
            right_extremum = i;
            break;
        }
    }
}

void Planner::GetPath(){
    path.clear();
    double start_time = -0.5;
    double dt = 0.01;
    int num_points = 101;
    
    // 预计算所有点的角度
    std::vector<double> angles;
    for(int i = 0; i < num_points; i++){
        double t = start_time + i * dt;
        double angle = GetGimbalYaw(hit_time + t);
        angles.push_back(angle);
    }
    
    // 生成路径点
    for(int i = 0; i < num_points; i++){
        double t = i * dt;
        double angle = angles[i];
        double angle_vel = 0.0;
        double angle_acc = 0.0;
        
        // 中间点计算角速度和角加速度
        if(i > 0 && i < num_points - 1){
            double prev_angle = angles[i-1];  
            double next_angle = angles[i+1];  
            angle_vel = (next_angle - prev_angle) / (2 * dt);
            angle_acc = (next_angle - 2 * angle + prev_angle) / (dt * dt);
        } else if (i == 0) {
            // 起点使用前向差分
            angle_vel = (angles[i+1] - angle) / dt;
            angle_acc = 0.0;  // 起点加速度设为0
        } else {
            // 终点使用后向差分
            angle_vel = (angle - angles[i-1]) / dt;
            angle_acc = 0.0;  // 终点加速度设为0
        }
        
        path.emplace_back(t, angle, angle_vel, angle_acc);
    }
}

Eigen::VectorXd Planner::ComputeQuinticPoly(const Path_Point& start, const Path_Point& end) {
    double t0 = start.time;
    double t1 = end.time;
    
    Eigen::MatrixXd A(6,6);
    A << 1, t0, t0*t0, t0*t0*t0, t0*t0*t0*t0, t0*t0*t0*t0*t0,
         0, 1, 2*t0, 3*t0*t0, 4*t0*t0*t0, 5*t0*t0*t0*t0,
         0, 0, 2, 6*t0, 12*t0*t0, 20*t0*t0*t0,
         1, t1, t1*t1, t1*t1*t1, t1*t1*t1*t1, t1*t1*t1*t1*t1,
         0, 1, 2*t1, 3*t1*t1, 4*t1*t1*t1, 5*t1*t1*t1*t1,
         0, 0, 2, 6*t1, 12*t1*t1, 20*t1*t1*t1;
    
    Eigen::VectorXd b(6);
    b << start.angle, start.angle_vel, start.angle_acc,
         end.angle, end.angle_vel, end.angle_acc;
    
    Eigen::VectorXd result = A.colPivHouseholderQr().solve(b);
    
    return result;
}

Path_Point Planner::EvalPolyAtT(const Eigen::VectorXd& poly, double t) {  
    double angle, angle_vel, angle_acc;
    
    // 角度：θ(t) = a0 + a1*t + a2*t^2 + a3*t^3 + a4*t^4 + a5*t^5
    angle = poly[0] + poly[1] * t + poly[2] * t*t +
            poly[3] * t*t*t + poly[4] * t*t*t*t + poly[5] * t*t*t*t*t;
    
    // 角速度：ω(t) = a1 + 2*a2*t + 3*a3*t^2 + 4*a4*t^3 + 5*a5*t^4
    angle_vel = poly[1] + 2 * poly[2] * t + 3 * poly[3] * t*t +
                4 * poly[4] * t*t*t + 5 * poly[5] * t*t*t*t;
    
    // 角加速度：α(t) = 2*a2 + 6*a3*t + 12*a4*t^2 + 20*a5*t^3
    angle_acc = 2 * poly[2] + 6 * poly[3] * t + 
                12 * poly[4] * t*t + 20 * poly[5] * t*t*t;
    
    return {t, angle, angle_vel, angle_acc};
}

bool Planner::CheckAccelConstraint(const Eigen::VectorXd& poly, double t0, double t1) {
    auto check_point = [&](double t) {
        double acc = 2*poly[2] + 6*poly[3]*t + 12*poly[4]*t*t + 20*poly[5]*t*t*t;
        return std::abs(acc) <= max_yaw_acc;
    };
    
    // 检查端点
    if (!check_point(t0) || !check_point(t1)) {
        return false;
    }
    
    // 检查驻点（如果存在且在区间内）
    double a3 = poly[3], a4 = poly[4], a5 = poly[5];
    double discriminant = 4*a4*a4 - 10*a5*a3;
    
    if (discriminant >= 0 && std::abs(a5) > 1e-10) {
        double sqrt_disc = std::sqrt(discriminant);
        for (double sign : {-1.0, 1.0}) {
            double t = (-2*a4 + sign * sqrt_disc) / (10*a5);
            if (t > t0 && t < t1) {
                if (!check_point(t)) {
                    return false;
                }
            }
        }
    }
    
    return true;
}

tools::Trajectory Planner::GetTrajectory(double time){
    Armors armors = m_status->GetArmors(time);
    double min_distance = 100;
    Eigen::Vector3d point;
    for(auto armor : armors){
        if(min_distance > armor.abs_pose.norm()){
            point = armor.abs_pose;
            min_distance = armor.abs_pose.norm();
        } 
    }
    auto distance = sqrt(point.x()*point.x()+point.y()*point.y());
    
    tools::Trajectory trajectory(bullet_speed, distance, point.z());
    
    return trajectory;
}