#include <opencv2/opencv.hpp>
#include <iostream>
#include "BuffType.h"
#pragma once
using namespace cv;
using namespace std;

namespace auto_buff{

class BuffDetector
{
public:
    BuffDetector();
    std::optional<PowerRune> Detect(cv::Mat & bgr_img);

    // 可开启功能开关（public，方便外部设置）
    bool enable_traditional_ = false;   // 传统CV方法
    bool enable_dataset_ = false;       // 数据集保存
    std::string dataset_path_ = "../obb_dataset";  // 数据集保存路径
    bool debug_ = false;                // debug 可视化
    bool show_ = true;                  // 显示窗口

private:
    void Lose();
    void Show(PowerRune &P);

    // ========== 传统方法 ==========
    void HandleImg();
    void DetectCircles(std::vector<FanBlade>& fanblades);
    void DetectRect(std::vector<std::pair<float, std::vector<cv::Point2f>>> &rects);
    bool GetLightBars(std::vector<FanBlade>& fanblades,
                      std::vector<std::pair<float, std::vector<cv::Point2f>>>& rects);
    cv::Point2f GetR(std::vector<FanBlade>& fanblades);
    bool Verify(cv::Point2f points[4]);

    // ========== 传统方法 MLP 模型 ==========
    cv::dnn::Net net_;
    std::vector<std::vector<cv::Point>> contours_;

    // ========== 状态 ==========
    Track_status status_;
    int lose_;
    std::optional<PowerRune> last_powerrune_ = std::nullopt;

    // ========== 图像缓存 ==========
    cv::Mat img_;              // 裁剪后的图像 (1080×1080)
    cv::Mat gray_img_;         // 灰度图
};
}  // namespace auto_buff
