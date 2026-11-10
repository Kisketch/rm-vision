#ifndef ARMOR_DETECTOR_H
#define ARMOR_DETECTOR_H

#include "ArmorTypes.h"
#include <opencv2/dnn.hpp>
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <mutex>

class ArmorDetector {
public:
    ArmorDetector(
        const std::string &model_path, const std::string &label_path, const double threshold,
        const std::vector<std::string> &ignore_classes = {});

    // 一体化主接口：灯条检测 → 装甲板匹配 → 数字分类 → 可视化
    Armors Detect(Mat &img, int enemy_color);

private:
    // 找灯条
    LightBlobs FindLight(Mat &src, int enemy_color);
    // 找装甲板
    Armors FindArmor(Mat &src, LightBlobs &light_blobs);
    // 提取数字ROI
    void ExtractNumbers(const Mat &src, Armors &armors);
    // 分类
    void Classify(Armors &armors);
    // 可视化结果
    void VisualizeResult(Mat &src, const Armors &armors);

    // 检测参数
    int enemy_color_;
    int threshold_;

    // 分类参数
    cv::dnn::Net net_;
    std::vector<std::string> class_names_;
    std::vector<std::string> ignore_classes_;
    double classify_threshold_;
    std::mutex mutex_;
};

#endif
