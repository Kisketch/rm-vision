#include "ArmorTarget.h"
#include "logger.hpp"
#include "math.hpp"

Aim::Aim(Armor armor){
    aim_id = armor.number;
    target_state = Eigen::VectorXd::Zero(10);
    InitEKF(armor);
    m_model_observer = std::make_shared<ExtendedKalmanFilter>(target_state, aim_id);
}

void Aim::visualizeState(const Armors &detected_armors, double dt)
{
    static cv::Mat canvas(cv::Size(800, 800), CV_8UC3);
    canvas = cv::Scalar(255, 255, 255);

    const cv::Point2f origin(400, 400);
    const bool is_outpost = (aim_id == "outpost");
    const float scale = is_outpost ? 50.0f : 100.0f;

    // Build kinematic model from current EKF state
    auto model = createModel(target_state);

    Armors predicted = model->GetArmors(0);

    // --- Draw rotation center (black) ---
    cv::Point2f center_px = origin + cv::Point2f(target_state(0), target_state(2)) * scale;
    cv::circle(canvas, center_px, 5, cv::Scalar(0, 0, 0), -1);

    // --- Draw predicted (filtered) armors: RED circles + yaw lines + radial lines ---
    for (auto &armor : predicted) {
        cv::Point2f px = origin + cv::Point2f(armor.abs_pose.x(), armor.abs_pose.y()) * scale;
        cv::line(canvas, center_px, px, cv::Scalar(200, 200, 200), 1);
        cv::circle(canvas, px, 4, cv::Scalar(0, 0, 255), -1);
        cv::line(canvas, px,
                 px + cv::Point2f(12 * cos(armor.predict_yaw), 12 * sin(armor.predict_yaw)),
                 cv::Scalar(0, 0, 255), 2);
    }

    // --- Draw observed armors: BLUE circles + yaw lines ---
    for (auto &armor : detected_armors) {
        cv::Point2f px = origin + cv::Point2f(armor.abs_pose.x(), armor.abs_pose.y()) * scale;
        cv::circle(canvas, px, 4, cv::Scalar(255, 0, 0), -1);
        cv::line(canvas, px,
                 px + cv::Point2f(12 * cos(armor.yaw), 12 * sin(armor.yaw)),
                 cv::Scalar(255, 0, 0), 2);
    }

    // --- State info overlay ---
    cv::putText(canvas, cv::format("aim: %s  xc:%.2f yc:%.2f  vx:%.2f vy:%.2f  dt:%.1fms",
                aim_id.c_str(), target_state(0), target_state(2), target_state(1), target_state(3), dt * 1000.0),
                cv::Point(10, 20), cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(0, 0, 0), 1);

    if (is_outpost) {
        cv::putText(canvas, cv::format("z: %.2f %.2f %.2f  r:%.2f",
                    target_state(4), target_state(5), target_state(6), target_state(7)),
                    cv::Point(10, 40), cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(0, 0, 0), 1);
    } else {
        cv::putText(canvas, cv::format("z1:%.2f z2:%.2f  r1:%.2f r2:%.2f",
                    target_state(4), target_state(5), target_state(6), target_state(7)),
                    cv::Point(10, 40), cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(0, 0, 0), 1);
    }

    cv::putText(canvas, cv::format("yaw:%.1fdeg  vyaw:%.2f rad/s",
                tools::R2D(target_state(8)), target_state(9)),
                cv::Point(10, 60), cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(0, 0, 0), 1);

    // Legend
    cv::circle(canvas, cv::Point(650, 775), 4, cv::Scalar(255, 0, 0), -1);
    cv::putText(canvas, "Observed", cv::Point(660, 779), cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(0, 0, 0), 1);
    cv::circle(canvas, cv::Point(730, 775), 4, cv::Scalar(0, 0, 255), -1);
    cv::putText(canvas, "Filtered", cv::Point(740, 779), cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(0, 0, 0), 1);

    cv::imshow("Motion State", canvas);
}

std::shared_ptr<KinematicModel> Aim::createModel(const Eigen::VectorXd &state) const
{
    if (aim_id == "outpost")
        return std::make_shared<OutpostModel>(state);
    return std::make_shared<StandardModel>(state);
}

std::shared_ptr<KinematicModel> Aim::Estimate(const Armors &armors, double &time){
    Resolve(armors, time);
    visualizeState(armors, time);
    return createModel(target_state);
}

void Aim::Resolve(const Armors &armors, double time)
{
    if(m_model_observer->is_lost){
        InitEKF(armors[0]);
        m_model_observer = std::make_shared<ExtendedKalmanFilter>(target_state, aim_id);
    }
    auto prior_status = createModel(m_model_observer->predict(time));

    Armors predicted = prior_status->GetArmors(0);

    Eigen::MatrixXd score = GetScoreMat(armors, predicted);
    std::map<int, int> armor_match = m_match.GetMatch(score, 1, prior_status->number);
    vector<int> match;
    for (const auto& [key, value] : armor_match) match.push_back(value);
    target_state = m_model_observer->update(armors, match);
}

void Aim::InitEKF(Armor armor){
    if(aim_id == "outpost"){
        target_state << armor.abs_pose.x() - 0.25 * cos(armor.yaw),0,       // xc,vx                                                     // vx
                armor.abs_pose.y() - 0.25 * sin(armor.yaw),0,               // yc,vy
                armor.abs_pose.z(),armor.abs_pose.z(),armor.abs_pose.z(),   // z1,z2,z3
                0.225,   armor.yaw,0;                                       // r1,yaw,vyaw 
    }else{
        target_state << armor.abs_pose.x() - 0.25 * cos(armor.yaw),0,    // xc,vx
                   armor.abs_pose.y() - 0.25 * sin(armor.yaw),0,         // yc,vy
                   armor.abs_pose.z(),armor.abs_pose.z(),                // z1,z2
                   0.25,0.25,armor.yaw,0;                                // r1,r2,yaw,vyaw
    }
}

Eigen::MatrixXd Aim::GetScoreMat(const Armors &detect_armors, const Armors &predict_armors)
{
    const Eigen::Index m = detect_armors.size();
    const Eigen::Index n = predict_armors.size();
    if (m == 0 || n == 0)
        return Eigen::MatrixXd::Zero(m, n);

    // 1. 计算负向指标矩阵 (m×n × 3)
    Eigen::Matrix<double, Eigen::Dynamic, 3> negative_score(m * n, 3);
    for (Eigen::Index i = 0; i < m; ++i) {
        for (Eigen::Index j = 0; j < n; ++j) {
            const auto &detect = detect_armors[i].abs_pose;
            const auto &standard = predict_armors[j].abs_pose;
            // xy平面距离
            negative_score(i * n + j, 0) = std::hypot(detect.x() - standard.x(), detect.y() - standard.y());
            // z轴距离
            negative_score(i * n + j, 1) = std::abs(detect.z() - standard.z());
            // 角度差
            negative_score(i * n + j, 2) = 4 * std::abs(_std_radian(detect_armors[i].yaw - predict_armors[j].predict_yaw));
        }
    }

    // 2. 标准化 (利用广播和逐元素运算)
    const auto max_vals = negative_score.colwise().maxCoeff();
    const auto min_vals = negative_score.colwise().minCoeff();
    const auto denoms = (max_vals - min_vals).cwiseMax(1e-10);  // 防止除以零
    const auto regular_score = (max_vals.matrix().replicate(m * n, 1) - negative_score).array().rowwise() / denoms.array();

    // 3. 计算权重 (熵权法)
    const auto col_sums = regular_score.colwise().sum().cwiseMax(1e-10);  // 防止除以零
    const auto score_weight = regular_score.array().rowwise() / col_sums.array();

    Eigen::Vector3d entropy = Eigen::Vector3d::Zero();
    for (Eigen::Index i = 0; i < score_weight.rows(); ++i) {
        for (Eigen::Index j = 0; j < 3; ++j) {
            const double val = score_weight(i, j);
            if (val > 0)
                entropy(j) -= val * std::log(val);
        }
    }
    entropy /= std::log(score_weight.rows());

    // 4. 计算最终权重
    const double denom = std::max(3.0 - entropy.sum(), 1e-10);  // 防止除以零
    const Eigen::Vector3d weight = (Eigen::Vector3d::Ones() - entropy) / denom;

    // 5. 计算匹配得分矩阵 (m×n)
    Eigen::MatrixXd score(m, n);
    for (Eigen::Index i = 0; i < m; ++i) {
        for (Eigen::Index j = 0; j < n; ++j) {
            score(i, j) = negative_score.row(i * n + j).dot(weight);
        }
    }

    return score;
}

std::map<int, int> Aim::Match(const Eigen::MatrixXd &score)
{
    std::map<int, int> match;
    for (Eigen::Index i = 0; i < score.rows(); i++)
    {
        Eigen::Index index;
        score.row(i).minCoeff(&index);
        match[i] = index;
    }
    return match;
}

Aim::Match::Match() {}
Aim::Match::~Match() {}

std::map<int, int> Aim::Match::GetMatch(Eigen::MatrixXd matrix, double score_max, int m)
{
    /**
        * @brief 初始化各个变量
        *
        */
    /// 初始化最终结果
    this->row_col.clear();
    for (Eigen::Index i = 0; i < matrix.rows(); i++)
    {
        // 值为-1表示不选择该行中的任何数
        this->row_col[i] = -1;
    }
    /// 初始化min
    for (Eigen::Index i = 0; i < matrix.rows() && i < m; i++)
    {
        this->min += matrix(i, i);
        this->row_col[i] = i;
    }
    /// 初始化行数列
    this->row.clear();
    for (Eigen::Index i = 0; i < matrix.rows(); i++)
    {
        // 有多少行，行数列就有多少个数
        this->row.emplace_back(i);
    }
    /// 初始化列数列，n块装甲板，因此列为n
    this->col.clear();
    for (int i = 0; i < m; i++)
    {
        this->col.emplace_back(i);
    }

    this->tmp_v.clear();
    this->result.clear();
    this->nAfour.clear();
    this->fourAfour.clear();

    /**
        * @brief 用A(n,m)求出可能选择的所有行的组合
        * 因为C(n,m) * A(m,m) = A(n,m)，所以先计算C(n,m)，再计算A(m,m)，最后将结果存到result中
        *
        */
    /// 计算C(n,m)
    if (matrix.rows() <= m)
        this->GetCombinationsNumbers(this->row, this->tmp_v, this->result, 0, matrix.rows());
    else
        this->GetCombinationsNumbers(this->row, this->tmp_v, this->result, 0, m);
    
    if (this->debug) {
        // 输出C(n,m)计算结果
        std::cout << "C(n,m)生成结果" << std::endl;
        for (auto it = this->result.begin(); it != this->result.end(); it++) {
            for (auto it2 = it->begin(); it2 != it->end(); it2++) {
                std::cout << *it2 << " ";
            }
            std::cout << std::endl;
        }
        std::cout << "-------------------------------" << std::endl;
    }
    
    /// 使用上一步的结果计算A(m,m)，最终得到A(n,m)
    for (auto it = this->result.begin(); it != this->result.end(); it++) {
        do {
            this->nAfour.emplace_back(*it);
        } while (next_permutation(it->begin(), it->end())); // stl自带全排列函数
    }
    
    if (this->debug) {
        // 输出A(n,m)计算结果
        std::cout << "A(n,m)生成结果" << std::endl;
        for (auto it = this->nAfour.begin(); it != this->nAfour.end(); it++) {
            for (auto it2 = it->begin(); it2 != it->end(); it2++) {
                std::cout << *it2 << " ";
            }
            std::cout << std::endl;
        }
        std::cout << "-------------------------------" << std::endl;
    }

    /**
        * @brief 用A(m,m)求出可能选择的所有列的组合
        *
        */
    /// 计算A(m,m)
    do {
        fourAfour.push_back(col);
    } while (next_permutation(col.begin(), col.end())); // stl自带全排列函数
    
    if (this->debug) {
        // 输出A(m,m)计算结果
        std::cout << "A(m,m)生成结果" << std::endl;
        for (auto it = this->fourAfour.begin(); it != this->fourAfour.end(); it++) {
            for (auto it2 = it->begin(); it2 != it->end(); it2++) {
                std::cout << *it2 << " ";
            }
            std::cout << std::endl;
        }
        std::cout << "-------------------------------" << std::endl;
    }

    /**
        * @brief 用A(n,m)和A(m,m)求出可能选择的所有行列的组合的结果
        *
        */
    for (auto it = this->nAfour.begin(); it != this->nAfour.end(); it++) {
        for (auto it2 = this->fourAfour.begin(); it2 != this->fourAfour.end(); it2++) {
            // 对于每一种组合，都求一遍它们的值，与min比较，如果小于min，则更新min
            this->tmp = 0;
            for (size_t i = 0; i < it->size(); i++) {
                this->tmp += matrix((*it)[i], (*it2)[i]);
            }
            if (this->tmp < this->min) {
                this->min = this->tmp;
                // 重新初始化map
                this->row_col.clear();
                // 每选择矩阵中的一个数时，就将该位置记录下来存到map中
                for (size_t i = 0; i < it->size(); i++) {
                    this->row_col[it->at(i)] = it2->at(i);
                }
            }
        }
    }
    
    if (this->debug) {
        // 输出删除前的map
        std::cout << "删除-1前的map结果" << std::endl;
        for (auto it = this->row_col.begin(); it != this->row_col.end(); it++) {
            std::cout << it->first << " " << it->second << std::endl;
        }
        std::cout << "-------------------------------" << std::endl;
    }
    
    /// 删除-1的键值对
    for (auto it = this->row_col.begin(); it != this->row_col.end();) {
        if (it->second == -1 || (it->second != -1 && matrix(it->first, it->second) > score_max)) {
            it = this->row_col.erase(it);
        } else {
            ++it;
        }
    }
    
    if (this->debug) {
        // 输出min的值和删除后的map
        std::cout << "min和删除-1后的map结果" << std::endl;
        std::cout << "min = " << min << std::endl;
        for (auto it = this->row_col.begin(); it != this->row_col.end(); ++it) {
            std::cout << it->first << " " << it->second << " " << matrix(it->first, it->second) << std::endl;
        }
        std::cout << "-------------------------------" << std::endl;
    }
    
    /// 输出矩阵
    if (this->debug) {
        std::cout << "matrix:" << std::endl;
        for (Eigen::Index i = 0; i < matrix.rows(); i++) {
            for (Eigen::Index j = 0; j < matrix.cols(); j++) {
                std::cout << matrix(i, j) << " ";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }
    
    return this->row_col;
}

void Aim::Match::GetCombinationsNumbers(std::vector<int> &input, std::vector<int> &tmp_v, 
                                       std::vector<std::vector<int>> &result, size_t start, size_t k)
{
    for (size_t i = start; i < input.size(); i++) {
        tmp_v.emplace_back(input[i]);
        if (tmp_v.size() == k) {
            result.emplace_back(tmp_v);
            tmp_v.pop_back();
            continue;
        }
        // 使用递归计算
        GetCombinationsNumbers(input, tmp_v, result, i + 1, k);
        tmp_v.pop_back();
    }
}