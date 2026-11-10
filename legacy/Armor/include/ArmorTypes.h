#ifndef ARMOR_TYPES_H
#define ARMOR_TYPES_H

#include <Eigen/Dense>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <vector>

using namespace cv;
using namespace std;

/******************* 灯条类定义 ***********************/
class LightBlob
{
public:
    cv::RotatedRect rect; // 灯条位置
    double length;        // 灯条长度
    int blob_color;   // 灯条颜色

    cv::Point2f top, bottom, center;
    LightBlob(cv::RotatedRect &r, int color)
        : rect(r), blob_color(color)
    {
        length = max(rect.size.height, rect.size.width);

        cv::Point2f p[4];
        r.points(p);
        std::sort(p, p + 4, [](const cv::Point2f &a, const cv::Point2f &b){ return a.y < b.y; });
        top = (p[0] + p[1]) / 2;
        bottom = (p[2] + p[3]) / 2;
        center = (top + bottom) / 2;
    };
    LightBlob() = default;
};

typedef std::vector<LightBlob> LightBlobs;

/******************* 装甲板类型定义　**********************/

enum class ArmorType
{
    SMALL,
    LARGE,
    INVALID
};
const std::string ARMOR_TYPE_STR[3] = {"small", "large", "invalid"};


struct Armor
{
    Armor() = default;
    Armor(const std::vector<cv::Point2f>& points, ArmorType type)
    {
        this->points = points;
        this->type = type;
    }

    cv::Point2f center;
    double yaw;
    double predict_yaw;
    // Number part
    cv::Mat number_img;
    std::string number;
    float confidence;
    vector<Point2f> points;
    vector<Point2f> re_points;
    ArmorType type;
    Eigen::Vector3d gim_pose;
    Eigen::Vector3d abs_pose;
    Eigen::Vector3d abs_ypd;
};
typedef vector<Armor> Armors;

#define ENEMY_BLUE 0
#define ENEMY_RED 1

#define BLOB_RED ENEMY_RED
#define BLOB_BLUE ENEMY_BLUE

#endif
