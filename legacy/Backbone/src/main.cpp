#include <unistd.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <systemd/sd-daemon.h>
#include "test.h"
#include "AutoArmor.h"
#include "hikrobot.hpp"
#include "exiter.hpp"
#include "SerialPort.h"

void drawAngleInfo(cv::Mat& img, const Data_Save& data) {
    cv::putText(img, cv::format("Y:%.1f P:%.1f R:%.1f",
                tools::R2D(data.camera_yaw_angle),
                tools::R2D(data.camera_pitch_angle),
                tools::R2D(data.camera_roll_angle)),
                cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 0), 2);
}

// 瞄准状态机：Found → HoldFound(0.5s) → Lost
class AimStateMachine {
public:
    enum State { Found, HoldFound, Lost };

    void Step(bool detected, const time_point &now) {
        if (detected) {
            state_ = Found;
        } else if (state_ == Found) {
            state_ = HoldFound;
            hold_deadline_ = now + std::chrono::milliseconds(500);
        } else if (state_ == HoldFound && now >= hold_deadline_) {
            state_ = Lost;
        }
    }

    bool IsTracking() const { return state_ == Found || state_ == HoldFound; }

private:
    State state_ = Lost;
    time_point hold_deadline_{};
};

auto yaml = YAML::LoadFile(YamlPath);
auto exposure_time = yaml["Camera"]["exposure_time"].as<double>();
int main()
{
    tools::Exiter exiter;
    std::unique_ptr<io::HikRobot> camera_ = std::make_unique<io::HikRobot>(exposure_time, 0, "2bdf:0001");
    std::unique_ptr<io::SerialPort> serial_ = std::make_unique<io::SerialPort>("/dev/ttyACM0");
    std::unique_ptr<AutoArmor> armor_ = std::make_unique<AutoArmor>();
    Mat img; time_point t; Data_Save ds; Data_Get dg;
    AimStateMachine state_machine;
    Data_Get last_dg;

    while(!exiter.exit()){
        camera_->Query(img,t);
        serial_->Query(ds,t);

        bool found = armor_->AutoAim(img, ds, dg);
        state_machine.Step(found, t);

        if (found) {
            last_dg = dg;
        } else if (state_machine.IsTracking()) {
            dg = last_dg;
        }

        serial_->SendData(dg);
        drawAngleInfo(img,ds);
        imshow("image",img);
        waitKey(1);
    }
    return 0;
}