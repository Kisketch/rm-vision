#include "ArmorEKF.h"
#include <cmath>
#include <algorithm>
#include "math.hpp"

std::unique_ptr<ExtendedKalmanFilter::UpdateStrategy> 
ExtendedKalmanFilter::createStrategy(const UpdateStrategy::Data& data) {
    if (armor_number_ == "outpost") return std::make_unique<OutpostStrategy>();

    size_t n = data.match_indices.size();
    if (n == 1) return std::make_unique<SingleArmorStrategy>();
    if (n == 2) return std::make_unique<DualArmorStrategy>();
    
    return nullptr;
}

// 单装甲板策略实现
void ExtendedKalmanFilter::SingleArmorStrategy::configure(
    const Eigen::VectorXd& x_pri, const Data& data) {
    x = x_pri;
    armor = data.armors[0];
    index = data.match_indices[0];
    
}

std::tuple<Eigen::VectorXd, Eigen::MatrixXd, Eigen::MatrixXd> 
ExtendedKalmanFilter::SingleArmorStrategy::compute() {
    
    // 预测观测
    double angle = _std_radian(x(8) + index * PI/2);
    double radius = x(6 + index%2);
    Eigen::Vector3d armor_xyz = {x(0) + radius*cos(angle),x(2) + radius*sin(angle),x(4  + index % 2)};
    Eigen::Vector4d h;
    Eigen::Vector3d armor_pyd = tools::xyz2ypd(armor_xyz);
    h << armor_pyd,x(8) + index*PI/2;
    // 实际观测
    Eigen::Vector4d z;
    z << armor.abs_ypd.x(), armor.abs_ypd.y(), armor.abs_ypd.z(), _std_radian(armor.yaw);
    // 残差
    Eigen::Vector4d residual = z - h;
    residual(3) = _std_radian(residual(3));
    // 雅可比矩阵
    Eigen::Matrix<double, 4, 10> H_armor_xyza;
    H_armor_xyza <<  1, 0, 0, 0,           0,       0, ((index+1)%2)*cos(angle), (index%2)*cos(angle), -radius*sin(angle), 0,
                     0, 0, 1, 0,           0,       0, ((index+1)%2)*sin(angle), (index%2)*sin(angle),  radius*cos(angle), 0,
                     0, 0, 0, 0, (index+1)%2, index%2,                        0,                    0,                  0, 0,
                     0, 0, 0, 0,           0,       0,                        0,                    0,                  1, 0;
    Eigen::Matrix4d H_armor_ypda = tools::xyz2ypd_jacobian(armor_xyz);
    Eigen::Matrix<double, 4, 10> H = H_armor_ypda * H_armor_xyza;
    Eigen::Matrix4d R = Eigen::Matrix4d::Zero();
    double delta_angle = _std_radian(armor.abs_ypd[0]-atan2(armor.abs_pose.y(), armor.abs_pose.x()));
    R.diagonal() << 4e-3, 4e-3, log(std::abs(delta_angle) + 1) + 1,
     log(std::abs(armor.abs_ypd[2]) + 1) / 200 + 9e-1;  

    return {residual, H, R};
}

void ExtendedKalmanFilter::OutpostStrategy::configure(
    const Eigen::VectorXd& x_pri, const Data& data) {
    x = x_pri;
    armor = data.armors[0];
    index = data.match_indices[0];
}

std::tuple<Eigen::VectorXd, Eigen::MatrixXd, Eigen::MatrixXd>
ExtendedKalmanFilter::OutpostStrategy::compute() {
    double angle = _std_radian(x(8) + index*2*PI/3);
    double radius = x(7);
    Eigen::Vector3d armor_xyz = {x(0) + radius*cos(angle),x(2) + radius*sin(angle),x(4+index)};
    Eigen::Vector4d h;
    Eigen::Vector3d armor_pyd = tools::xyz2ypd(armor_xyz);
    h << armor_pyd,angle;
    // 实际观测
    Eigen::Vector4d z;
    z << armor.abs_ypd.x(), armor.abs_ypd.y(), armor.abs_ypd.z(), _std_radian(armor.yaw);
    // 残差
    Eigen::Vector4d residual = z - h;
    residual(3) = _std_radian(residual(3));

    Eigen::Matrix<double, 4, 10> H_armor_xyza = Eigen::Matrix<double, 4, 10>::Zero();
    H_armor_xyza <<
        1, 0, 0, 0, 0, 0, 0, cos(angle), -radius*sin(angle), 0,
        0, 0, 1, 0, 0, 0, 0, sin(angle),  radius*cos(angle), 0,
        0, 0, 0, 0, index == 0, index == 1, index == 2, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 1, 0;
    Eigen::Matrix4d H_armor_ypda = tools::xyz2ypd_jacobian(armor_xyz);
    Eigen::Matrix<double, 4, 10> H = H_armor_ypda * H_armor_xyza;

    Eigen::Matrix4d R = Eigen::Matrix4d::Identity();
    double delta_angle = _std_radian(armor.abs_ypd[0]-atan2(armor.abs_pose.y(), armor.abs_pose.x()));
    R.diagonal() << 4e-3, 4e-3, log(std::abs(delta_angle) + 1) + 1,
        log(std::abs(armor.abs_ypd[2]) + 1) / 200 + 9e-1;  
    return {residual, H, R};
}

// 双装甲板策略实现
void ExtendedKalmanFilter::DualArmorStrategy::configure(
    const Eigen::VectorXd& x_pri, const Data& data) {
    x = x_pri;
    armor1 = data.armors[0];
    armor2 = data.armors[1];
    index1 = data.match_indices[0];
    index2 = data.match_indices[1];
}

std::tuple<Eigen::VectorXd, Eigen::MatrixXd, Eigen::MatrixXd> 
ExtendedKalmanFilter::DualArmorStrategy::compute() {
    // 预测观测
    Eigen::Matrix<double, 8, 1> h;
    
    // 第一块装甲板
    double angle1 = _std_radian(x(8) + index1*PI/2);
    double radius1 = x(6 + index1%2);
    Eigen::Vector3d armor1_xyz = {x(0) + radius1*cos(angle1),x(2) + radius1*sin(angle1),x(4 + index1%2)};
    Eigen::Vector3d armor1_pyd = tools::xyz2ypd(armor1_xyz);
    h.segment<4>(0) << armor1_pyd,angle1;
    
    // 第二块装甲板
    double angle2 = _std_radian(x(8) + index2*PI/2);
    double radius2 = x(6 + index2%2);
    Eigen::Vector3d armor2_xyz = {x(0) + radius2*cos(angle2),x(2) + radius2*sin(angle2),x(4 + index2%2)};
    Eigen::Vector3d armor2_pyd = tools::xyz2ypd(armor2_xyz);
    h.segment<4>(4) << armor2_pyd,angle2;

    // 实际观测
    Eigen::Matrix<double, 8, 1> z;
    z << armor1.abs_ypd.x(), armor1.abs_ypd.y(), armor1.abs_ypd.z(), armor1.yaw,
         armor2.abs_ypd.x(), armor2.abs_ypd.y(), armor2.abs_ypd.z(), armor2.yaw;

    // 残差
    Eigen::Matrix<double, 8, 1> residual = z - h;
    residual(3) = _std_radian(residual(3));
    residual(7) = _std_radian(residual(7));
    
    // 雅可比矩阵
    // 第一块装甲板部分
    Eigen::Matrix<double, 4, 10> H_armor1_xyza;
    H_armor1_xyza << 
        1, 0, 0, 0, 0, 0, ((index1+1)%2)*cos(angle1), (index1%2)*cos(angle1), -radius1*sin(angle1), 0,
        0, 0, 1, 0, 0, 0, ((index1+1)%2)*sin(angle1), (index1%2)*sin(angle1), radius1*cos(angle1), 0,
        0, 0, 0, 0, (index1+1)%2, index1%2, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 1, 0;
    Eigen::Matrix4d H_armor1_ypda = tools::xyz2ypd_jacobian(armor1_xyz);
    
    // 第二块装甲板部分
    Eigen::Matrix<double, 4, 10> H_armor2_xyza;
    H_armor2_xyza <<  
        1, 0, 0, 0, 0, 0, ((index2+1)%2)*cos(angle2), (index2%2)*cos(angle2), -radius2*sin(angle2), 0,
        0, 0, 1, 0, 0, 0, ((index2+1)%2)*sin(angle2), (index2%2)*sin(angle2), radius2*cos(angle2), 0,
        0, 0, 0, 0, (index2+1)%2, index2%2, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 1, 0;
    Eigen::Matrix4d H_armor2_ypda = tools::xyz2ypd_jacobian(armor2_xyz);

    // 声明H矩阵并赋值
    Eigen::Matrix<double, 8, 10> H;
    H.block<4,10>(0,0) = H_armor1_ypda * H_armor1_xyza;
    H.block<4,10>(4,0) = H_armor2_ypda * H_armor2_xyza;
    
    Eigen::Matrix<double, 8, 8> R = Eigen::Matrix<double, 8, 8>::Zero();
    double delta_angle1 = _std_radian(armor1.abs_ypd[0]-atan2(armor1.abs_pose.y(), armor1.abs_pose.x()));
    double delta_angle2 = _std_radian(armor2.abs_ypd[0]-atan2(armor2.abs_pose.y(), armor2.abs_pose.x()));
    R.diagonal() << 4e-3, 4e-3, log(std::abs(delta_angle1) + 1) + 1,
     log(std::abs(armor1.abs_ypd[2]) + 1) / 200 + 9e-1,
                    4e-3, 4e-3, log(std::abs(delta_angle2) + 1) + 1,
     log(std::abs(armor2.abs_ypd[2]) + 1) / 200 + 9e-1;

    return std::make_tuple(residual, H, R);  
}

// ExtendedKalmanFilter 成员函数实现
ExtendedKalmanFilter::ExtendedKalmanFilter(const Eigen::VectorXd& initial_state, const std::string& armor_number) :
    x_pri(initial_state), x_post(initial_state),
    I(Eigen::MatrixXd::Identity(10, 10)), armor_number_(armor_number){
        Eigen::VectorXd P0_dig{{1, 64, 1, 64, 1, 1, 1, 1, 0.4, 100}};
        P_pri = P0_dig.asDiagonal();
        P_post = P0_dig.asDiagonal();
        nis_history_.clear();
    }

Eigen::MatrixXd ExtendedKalmanFilter::computeProcessNoise() {
    Eigen::MatrixXd Q(10, 10);
    double t4 = pow(dt_, 4)/4;
    double t3 = pow(dt_, 3)/2;
    double t2 = pow(dt_, 2);
    if(armor_number_ == "outpost"){
        dd = 0.0001;
        da = 0.0001;
    }else{
        dd = 100;
        da = 400;
    }
    Q << t4*dd, t3*dd, 0,     0,     0, 0, 0, 0, 0,     0,
         t3*dd, t2*dd, 0,     0,     0, 0, 0, 0, 0,     0,
         0,     0,     t4*dd, t3*dd, 0, 0, 0, 0, 0,     0,
         0,     0,     t3*dd, t2*dd, 0, 0, 0, 0, 0,     0,
         0,     0,     0,     0,     0, 0, 0, 0, 0,     0,
         0,     0,     0,     0,     0, 0, 0, 0, 0,     0,
         0,     0,     0,     0,     0, 0, 0, 0, 0,     0,
         0,     0,     0,     0,     0, 0, 0, 0, 0,     0,
         0,     0,     0,     0,     0, 0, 0, 0, t4*da, t3*da,
         0,     0,     0,     0,     0, 0, 0, 0, t3*da, t2*da;

    return Q;
}

Eigen::VectorXd ExtendedKalmanFilter::predict(double dt) {
    dt_ = dt;
    // 状态转移矩阵
    Eigen::MatrixXd F(10, 10);
    F << 1, dt, 0,  0, 0, 0, 0, 0, 0,  0,
         0,  1, 0,  0, 0, 0, 0, 0, 0,  0,
         0,  0, 1, dt, 0, 0, 0, 0, 0,  0,
         0,  0, 0,  1, 0, 0, 0, 0, 0,  0,
         0,  0, 0,  0, 1, 0, 0, 0, 0,  0,
         0,  0, 0,  0, 0, 1, 0, 0, 0,  0,
         0,  0, 0,  0, 0, 0, 1, 0, 0,  0,
         0,  0, 0,  0, 0, 0, 0, 1, 0,  0,
         0,  0, 0,  0, 0, 0, 0, 0, 1, dt,
         0,  0, 0,  0, 0, 0, 0, 0, 0,  1;

    // 过程噪声
    Eigen::MatrixXd Q = computeProcessNoise();
    // 预测步骤
    x_pri = F * x_post;
    P_pri = F * P_post * F.transpose() + Q;
    
    return x_pri;
}

Eigen::VectorXd ExtendedKalmanFilter::update(const Armors& armors, const std::vector<int>& match_indices) {
    
    if (match_indices.empty()) {
        is_lost = true;
        return x_pri;
    }
    // 准备策略数据
    UpdateStrategy::Data data{armors, match_indices};
    
    // 创建策略
    auto strategy = createStrategy(data);
    if (!strategy) return x_pri;
    
    // 配置策略
    strategy->configure(x_pri, data);
    
    // 计算更新组件
    auto [residual, H, R] = strategy->compute();
    // 卡尔曼增益
    K = P_pri * H.transpose() * (H * P_pri * H.transpose() + R ).inverse();
    // 状态更新
    x_post = x_pri + K * residual;
    P_post = (I - K * H) * P_pri * (I - K * H).transpose() + K * R * K.transpose();
    
    NIS(strategy->compute());   
    return x_post;
}

// NIS检验，新息卡方检验
void ExtendedKalmanFilter::NIS(std::tuple<Eigen::VectorXd, Eigen::MatrixXd, Eigen::MatrixXd> compute){
    auto [residual, H, R] = compute;
    Eigen::MatrixXd S = H * P_post * H.transpose() + R;
    double nis = residual.transpose() * S.inverse() * residual;
    bool exceeded = nis > nis_threshold;
    // 更新历史记录
    nis_history_.push_back(exceeded);
    if (nis_history_.size() > nis_window) {
        nis_history_.pop_front();
    }
    // 快速统计超过次数
    size_t exceed_count = 0;
    for (bool result : nis_history_) {
        exceed_count += result ? 1 : 0;
        if (exceed_count >= nis_threshold_count) {
            is_lost = true;
            break;
        }
        
    }
}


