#include "ArmorTarget.h"
#include "Base.h"
#include "test.h"
#include <random>
#include <algorithm>
#include <cstdlib>  
#include <ctime>   
#include <fstream>
#include <sstream>
#include <vector>
#include <limits>
#include "ArmorPlanner.h"
// 使用录制数据进行仿真
// #define USE_DATA
int random_seed = 42;
#ifdef USE_DATA
// 数据文件路径
const std::string DATA_FILE_PATH = "/home/killer_machine/WRITE/build/input_data.txt";
// 记录最后时间戳
double last_timestamp = 0;
// 数据索引
size_t data_index = 0;
// 存储的数据
std::vector<std::pair<double, Armors>> recorded_data;
#endif

// 观察点坐标
cv::Point2f observation_point(0, 0);
// 仿真画面
cv::Mat background;
// 原点（画面中心点）
cv::Point2f base_point = cv::Point2f(512, 512);

// 仿真参数（这些就是真实状态）
cv::Point2f center(3, 0);  // 中心点初始位置
float z[2] = {0.25, 0.1};  // 装甲板高度
float r[2] = {0.2, 0.4};   // 装甲板旋转半径
float palstance = 4;       // 角速度
float scaling = 100;       // 放大比例
double angle = 0;          // 起始角度
float dt = 1e-2;           // 默认时间步长
float sigma = 0.02;        // 噪声大小
double big_angle = 0;
std::default_random_engine rng;

// 路径点定义
std::vector<cv::Point2f> waypoints = {
    cv::Point2f(2, 2),
    cv::Point2f(-2, 2)
};
int current_target_index = 0;
float movement_speed = 0.8; // 移动速度

/**
 * @brief 获取当前时间（秒）
 */
double now()
{
    timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000;
}

Rate::Rate() : Rate(30) {}

/**
 * @brief 帧率控制器构造函数
 */
Rate::Rate(unsigned int r)
:   m_LoopRate(r),
    m_start_time_with_usec(0)
{}

/**
 * @brief 控制帧率
 */
void Rate::sleep()
{
    double period = 1. / m_LoopRate;
    double cur_time = now();
    double expect_end_time = m_start_time_with_usec + period;

    if (expect_end_time > cur_time)
    {
        usleep((expect_end_time - cur_time) * 1000000);
    }
    m_start_time_with_usec = now();
}

bool init_flag = false;
// 帧率控制器
Rate rate(70);
int i = 1;

/**
 * @brief 生成仿真装甲板数据并只保留距离最近的1个
 */
Armors generateSimulatedArmors() {
    Armors armors;
    armors.resize(4);
    
    // === 更新中心位置：围绕原点做圆周运动，半径2 ===
    big_angle += PI * 1e-3;  // 更新公转角度
    
    // 圆周运动参数方程
    center.x = 5.0f * cos(big_angle);
    center.y = 5.0f * sin(big_angle);
    angle += PI * dt * palstance; 
    angle = _std_radian(angle);
    
    // 生成4个装甲板
    for (size_t i = 0; i < armors.size(); i++) {
        // 添加高斯噪声
        std::normal_distribution<float> dist_pos(0, sigma);
        std::normal_distribution<float> dist_angle(0, sigma * 10);
        
        // 计算装甲板位置（相对中心的自转）
        float armor_r = r[i % 2];
        float armor_z = z[i % 2];
        float armor_angle = angle + 0.5 * i * PI;  // 自转角度 = 公转角度 + 固定偏移
        
        armors[i].abs_pose = {
            center.x + armor_r * cos(armor_angle) + dist_pos(rng),
            center.y + armor_r * sin(armor_angle) + dist_pos(rng),
            armor_z + dist_pos(rng)
        };
        armors[i].abs_ypd = tools::xyz2ypd(armors[i].abs_pose);
        // 计算装甲板朝向角
        armors[i].yaw = _std_radian(armor_angle + dist_angle(rng));
        armors[i].predict_yaw = _std_radian(armor_angle);
        armors[i].confidence = armor_r;
    }
    

    if(init_flag == false){
        init_flag = true;
    }else{
        // 按距离观察点的距离排序（从近到远）
        std::sort(armors.begin(), armors.end(), 
            [](const Armor& a, const Armor& b) {
                return a.abs_pose.norm() < b.abs_pose.norm();
            });
    }
    armors.resize(1 + rand() % 2);
    // 在仿真图上绘制装甲板
    for (const auto& armor : armors) {
        cv::circle(background, 
                  base_point + cv::Point2f(armor.abs_pose.x(), armor.abs_pose.y()) * scaling, 
                  3, cv::Scalar(255, 0, 0), -1, 8);
        cv::line(background,
                base_point + cv::Point2f(armor.abs_pose.x(), armor.abs_pose.y()) * scaling,
                base_point + cv::Point2f(armor.abs_pose.x(), armor.abs_pose.y()) * scaling + 
                    cv::Point2f(10 * cos(armor.yaw), 10 * sin(armor.yaw)),
                cv::Scalar(255, 0, 0), 2, 8);
    }
    
    // 绘制中心点和观察点
    cv::circle(background, base_point + center * scaling, 5, cv::Scalar(0, 0, 0), -1, 8);
    cv::circle(background, base_point + observation_point * scaling, 5, cv::Scalar(127, 127, 127), -1, 8);
    
    return armors;
}

int Test() {
    srand(time(nullptr));
    
    // 初始化随机数生成器
    rng.seed(random_seed);
    srand(random_seed);
    
#ifdef USE_DATA
    // 加载录制的数据
    if (!loadRecordedData()) {
        std::cout << "使用模拟数据替代" << std::endl;
    } else {
        std::cout << "使用录制的数据进行仿真" << std::endl;
    }
#endif
    
    // 初始化预测器和模型
    std::unique_ptr<Aim> aim_ptr;
    std::shared_ptr<KinematicModel> m_status;
    std::unique_ptr<Planner> planner = std::make_unique<Planner>();
    io::SerialPort serial_("/dev/ttyACM0");
    Data_Get dg;
    
    while (true) {
        // 清空背景
        background = cv::Mat(cv::Size(1024, 1024), CV_8UC3, cv::Scalar(255, 255, 255));
        
        // 获取装甲板数据
        Armors armors;
        double actual_dt = dt;  // 默认使用固定时间步长
        
#ifdef USE_DATA
        if (!recorded_data.empty() && data_index < recorded_data.size()) {
            // 使用录制的数据，获取实际的时间步长
            armors = getRecordedArmors(actual_dt);
            // 如果装甲板数据为空，跳过
            if (armors.empty()) {
                if (recorded_data.empty() || data_index >= recorded_data.size()) {
                    std::cout << "数据播放完毕" << std::endl;
                }
                rate.sleep();
                continue;
            }
        } 
        else 
#endif
        {
            // 使用模拟数据，使用固定时间步长
            armors = generateSimulatedArmors();
            actual_dt = dt;  // 对于模拟数据，使用固定的dt
        }
        
        // 如果是第一次运行，初始化预测器
        if (!aim_ptr) {
            aim_ptr = std::make_unique<Aim>(armors[0]);
            m_status = aim_ptr->createModel(aim_ptr->target_state);
        }

        // 预测器解算，使用实际的时间步长
        aim_ptr->Resolve(armors, actual_dt);
        m_status = aim_ptr->createModel(aim_ptr->target_state);
        // 获取当前状态估计
        Armors filter_armors = m_status->GetArmors(0);
        // 绘制状态估计和预测结果
        for (size_t i = 0; i < filter_armors.size(); i++) {
            // 当前状态估计量（红色）
            cv::circle(background, 
                    base_point + cv::Point2f(filter_armors[i].abs_pose.x(), filter_armors[i].abs_pose.y()) * scaling, 
                    3, cv::Scalar(0, 0, 255), -1, 8);
            
            // 计算无噪声的理想装甲板位置（绿色）
            float armor_r = r[i % 2];
            float armor_angle = angle + 0.5 * i * PI;
            
            cv::Point2f ideal_position(
                center.x + armor_r * cos(armor_angle),
                center.y + armor_r * sin(armor_angle)
            );
            
            cv::circle(background, 
                    base_point + ideal_position * scaling, 
                    2, cv::Scalar(0, 255, 0), -1, 10);
        }
        
        // 显示目标装甲板（紫色圆圈）
        cv::circle(background, base_point + cv::Point2f(m_status->center.x, m_status->center.y) * scaling, 
            10, cv::Scalar(255, 0, 255), 2, 8);
        cv::circle(background, base_point + cv::Point2f(m_status->center.x, m_status->center.y) * scaling, 
            10, cv::Scalar(255, 0, 255), 2, 8);
        
        // 显示结果
        cv::imshow("background", background);

        // 检查按键输入
        char c = cv::waitKey(1);
        if (c == 'q'||i==200) {
            break;
        } 
        i++;
        // 控制帧率
        rate.sleep();
    }
    
    return 0;
}