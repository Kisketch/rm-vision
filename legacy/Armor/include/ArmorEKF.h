#ifndef ARMOR_EKF_H
#define ARMOR_EKF_H

#include <Eigen/Dense>
#include <vector>
#include <map>
#include <memory>
#include "ArmorTypes.h"

class ExtendedKalmanFilter {
public:
    explicit ExtendedKalmanFilter(const Eigen::VectorXd& initial_state, const std::string& armor_number);

    // 核心接口
    Eigen::VectorXd predict(double dt);
    Eigen::VectorXd update(const Armors& armors, const std::vector<int>& match_indices);

    // NIS检验，新息卡方检验
    void NIS(std::tuple<Eigen::VectorXd, Eigen::MatrixXd, Eigen::MatrixXd> compute);
    bool is_lost = false;
private:
    // 状态变量
    Eigen::VectorXd x_pri;  // 先验状态
    Eigen::VectorXd x_post; // 后验状态
    Eigen::MatrixXd P_pri;  // 先验协方差
    Eigen::MatrixXd P_post; // 后验协方差
    Eigen::MatrixXd K;      // 卡尔曼增益
    Eigen::MatrixXd I;      // 单位矩阵

    // 噪声参数
    std::string armor_number_;   // 装甲类型标识
    double dd;      // 平面距离过程噪声
    double da;      // 角度过程噪声
    double dt_;     // 时间步长
    std::deque<bool> nis_history_;                  // NIS历史记录队列
    static constexpr size_t nis_window = 15;        // 历史窗口大小
    static constexpr size_t nis_threshold_count = 7;// 触发阈值次数
    static constexpr double nis_threshold = 0.511;  // 卡方检验阈值

    // 策略基类
    class UpdateStrategy {
    public:
        struct Data {
            const Armors& armors;
            std::vector<int> match_indices;
        };
        virtual ~UpdateStrategy() = default;
        virtual void configure(const Eigen::VectorXd& x_pri, const Data& data) = 0;
        virtual std::tuple<Eigen::VectorXd, Eigen::MatrixXd, Eigen::MatrixXd> compute() = 0;
    };

    // 单装甲板策略
    class SingleArmorStrategy : public UpdateStrategy {
    public:
        void configure(const Eigen::VectorXd& x_pri, const Data& data) override;
        std::tuple<Eigen::VectorXd, Eigen::MatrixXd, Eigen::MatrixXd> compute() override;

    private:
        Eigen::VectorXd x;
        Armor armor;
        int index;
    };

    class OutpostStrategy : public UpdateStrategy {
    public:
        void configure(const Eigen::VectorXd& x_pri, const Data& data) override;
        std::tuple<Eigen::VectorXd, Eigen::MatrixXd, Eigen::MatrixXd> compute() override;

    private:
        Eigen::VectorXd x;
        Armor armor;
        int index;
    };

    // 双装甲板策略
    class DualArmorStrategy : public UpdateStrategy {
    public:
        void configure(const Eigen::VectorXd& x_pri, const Data& data) override;
        std::tuple<Eigen::VectorXd, Eigen::MatrixXd, Eigen::MatrixXd> compute() override;

    private:
        Eigen::VectorXd x;
        Armor armor1;
        Armor armor2;
        int index1;
        int index2;
    };

    // 内部方法
    Eigen::MatrixXd computeProcessNoise();
    std::unique_ptr<UpdateStrategy> createStrategy(const UpdateStrategy::Data& data);
    void constrainState(Eigen::VectorXd& state);
};

#endif // ARMOR_EKF_H
