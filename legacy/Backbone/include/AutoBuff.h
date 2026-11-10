#include "BuffDetect.h"
#include "BuffSlove.h"
#include "BuffTarget.h"
#include "BuffPlan.h"
#include "yaml-cpp/yaml.h"

class AutoBuff{
public: 
    AutoBuff();
    void AutoAim(cv::Mat &img,const Data_Save &ds,Data_Get &dg);

private:
    void Transmit(const Gimbal_Angle &gimbal_angle,const Data_Save &ds,Data_Get &dg,cv::Mat &img, const std::optional<auto_buff::PowerRune> &power_runes);
    void show(const std::optional<auto_buff::PowerRune> &power_runes, cv::Mat &img);
    YAML::Node readYAML(const std::string& filename);
    std::unique_ptr<auto_buff::BuffDetector> discover_;
    std::unique_ptr<auto_buff::Solver> slover_;
    std::unique_ptr<auto_buff::Target>target_;
    std::unique_ptr<auto_buff::Planner>planner_;
    int last_mode = 1;
    double compensate_pitch = 0;
    double compensate_yaw = 0;
    std::optional<time_point> last_fire_time_;  // 上次发射时刻，用于0.8s冷却
};

