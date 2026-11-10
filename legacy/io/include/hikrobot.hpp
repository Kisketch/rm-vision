#ifndef IO__HIKROBOT_HPP
#define IO__HIKROBOT_HPP

#include <atomic>
#include <chrono>
#include <opencv2/opencv.hpp>
#include <string>
#include <thread>
#include "Base.h"
#include "MvCameraControl.h"
#include "thread_safe_queue.hpp"

namespace io
{
class HikRobot 
{
public:
  HikRobot(double exposure_us, double gain, const std::string & vid_pid);
  ~HikRobot();
  void Query(cv::Mat& img,time_point& timestamp);

private:
  struct CameraData
  {
    cv::Mat img;
    time_point timestamp;
  };

  double exposure_us_;
  double gain_;

  std::thread daemon_thread_;                        // 采集线程监控线程
  std::atomic<bool> daemon_quit_;                    // 守护线程退出标志

  void * handle_;
  std::thread capture_thread_;                       // 图像采集线程
  std::atomic<bool> capturing_;                      // 相机是否正在正常进行图像采集
  std::atomic<bool> capture_quit_;                   // 相机推出采集
  tools::ThreadSafeQueue<CameraData> queue_;         // 线程通讯队列

  int vid_, pid_;

  void capture_start();
  void capture_stop();

  void set_float_value(const std::string & name, double value);
  void set_enum_value(const std::string & name, unsigned int value);

  void set_vid_pid(const std::string & vid_pid);
  void reset_usb() const;
};

}  // namespace io

#endif  // IO__HIKROBOT_HPP