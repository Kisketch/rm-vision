#include <opencv2/highgui.hpp>
#include <eigen3/Eigen/Dense>
#include "ArmorDetector.h"
#include "Base.h"
#include "math.hpp"

#include <iostream>
#include <string>
#include <fstream>

// ==================== 灯条检测 静态辅助函数 (来自 find_lights.cpp) ====================

// 旋转矩形的长宽比
static double lw_rate(const cv::RotatedRect &rect) {
    return rect.size.height > rect.size.width ?
           rect.size.height / rect.size.width :
           rect.size.width / rect.size.height;
}
// 检查轮廓面积是否在合理范围内
static bool isValidContourArea(const std::vector<cv::Point> &contour) {
    double area = cv::contourArea(contour);
    return area > 15 && area < 20000;
}
// 判断轮廓是否为一个灯条
static bool IsValidLightBlob(const cv::RotatedRect &rect) {
    return (1.5 < lw_rate(rect) && lw_rate(rect) < 40);
}

// 倾斜角度检查
void SlopeValidation(LightBlobs &lightblobs)
{
    for (auto it = lightblobs.begin(); it != lightblobs.end(); ) {
        cv::Point2f direction = it->top - it->bottom;
        float tilt_angle = std::atan2(std::abs(direction.x), std::abs(direction.y));
        if (tilt_angle > tools::D2R(45)) {
            it = lightblobs.erase(it); // 删除并获取下一个迭代器
        } else {
            ++it;
        }
    }
}

static void ImagePreProcessDim(cv::Mat &src) {

    static cv::Mat kernel_dilate = getStructuringElement(cv::MORPH_RECT, cv::Size(3, 5));
    cv::morphologyEx(src, src, cv::MORPH_CLOSE, kernel_dilate);
}

static int GetBlobColor(const cv::Mat &src, const cv::RotatedRect &blobPos) {
    // 1. 计算ROI区域
    cv::Rect region = blobPos.boundingRect();
    int offset_x = std::max(3, static_cast<int>(region.width * 0.1));
    int offset_y = std::max(3, static_cast<int>(region.height * 0.05));
    region.x -= offset_x;
    region.y -= offset_y;
    region.width += 2 * offset_x;
    region.height += 2 * offset_y;
    region &= cv::Rect(0, 0, src.cols, src.rows);
    if (region.empty()) {
        return BLOB_BLUE;
    }

    // 2. 提取ROI
    cv::Mat roi = src(region);

    // 3. 使用split分离通道并计算总和
    std::vector<cv::Mat> channels;
    cv::split(roi, channels);  // 分离为B,G,R通道

    // 4. 计算B和R通道的总和
    double sum_b = cv::sum(channels[0])[0]; // B通道总和
    double sum_r = cv::sum(channels[2])[0]; // R通道总和

    return (sum_r > sum_b) ? BLOB_RED : BLOB_BLUE;
}

// ==================== 装甲板匹配 静态辅助函数 (来自 find_armor.cpp) ====================

static bool AngleJudge(const LightBlob &light_blob_i, const LightBlob &light_blob_j)
{
    cv::Point2f direction = light_blob_i.center - light_blob_j.center;
    float angle = std::atan2(std::abs(direction.y), std::abs(direction.x));
    return angle<tools::D2R(45);
}
// 判断装甲板的长宽比
static bool LengthWidthJudge(const LightBlob &light_blob_i, const LightBlob &light_blob_j)
{
    // 计算两个灯条中心点的距离
    double side_length = cv::norm(light_blob_i.rect.center - light_blob_j.rect.center);
    // 获取两个灯条中较长的长度
    double max_length = std::max(light_blob_i.length, light_blob_j.length);
    // 检查距离与灯条长度的比例是否在合理范围内
    return (side_length / max_length < 5.0 && side_length / max_length > 1.0);
}
// 判断两个灯条的长度比
static bool LengthRatioJudge(const LightBlob &light_blob_i, const LightBlob &light_blob_j)
{
    // 获取较长和较短的灯条长度
    double longer_length = std::max(light_blob_i.length, light_blob_j.length);
    double shorter_length = std::min(light_blob_i.length, light_blob_j.length);

    // 判断较长灯条与较短灯条的比值是否在合理范围内
    return (longer_length / shorter_length < 2.5);
}
// 两个灯条中是否夹了灯条
static bool ContainLight(const LightBlob &light_blob_i, const LightBlob &light_blob_j, const LightBlobs &lights)
{
    auto points = std::vector<cv::Point2f>{light_blob_i.top, light_blob_i.bottom, light_blob_j.top, light_blob_j.bottom};
    auto bounding_rect = cv::boundingRect(points);

    for (const auto &test_light : lights)
    {
        if (test_light.center == light_blob_i.center || test_light.center == light_blob_j.center)
            continue;

        if (
            bounding_rect.contains(test_light.top) || bounding_rect.contains(test_light.bottom) ||
            bounding_rect.contains(test_light.center))
        {
            return true;
        }
    }

    return false;
}

// 判断两个灯条是否可以匹配
static bool IsCoupleLight(const LightBlob &light_blob_i, const LightBlob &light_blob_j, int enemy_color)
{
    return light_blob_i.blob_color == enemy_color &&
           light_blob_j.blob_color == enemy_color &&
           LengthRatioJudge(light_blob_i, light_blob_j) &&
           LengthWidthJudge(light_blob_i, light_blob_j) &&
           AngleJudge(light_blob_i, light_blob_j);
}

void Judge_Type(const LightBlob &light_blob_i, const LightBlob &light_blob_j, ArmorType &type)
{
    // 两个灯条的间距
    double side_length;
    cv::Point2f centers = light_blob_i.rect.center - light_blob_j.rect.center;
    side_length = sqrt(centers.ddot(centers));
    // 灯条的高度
    double light_height;
    light_height = (light_blob_i.length + light_blob_j.length) / 2;
    if ((side_length / light_height) > 2.8)
    {
        type = ArmorType::LARGE;
    }
    else
    {
        type = ArmorType::SMALL;
    }
}

static cv::Point2f GreyLevelCentroid(const cv::Point2f& point, double radius, const cv::Mat &src) {

    cv::Rect roi_rect(cvRound(point.x - radius), cvRound(point.y - radius),
                     cvRound(2 * radius), cvRound(2 * radius));

    // 边界检查
    roi_rect &= cv::Rect(0, 0, src.cols, src.rows);
    if (roi_rect.width <= 0 || roi_rect.height <= 0) {
        return point;
    }

    cv::Mat roi = src(roi_rect);
    // 转换为灰度
    cv::Mat gray_roi;
    cv::cvtColor(roi, gray_roi, cv::COLOR_BGR2GRAY);

    // 创建圆形掩码并直接应用
    cv::Mat mask = cv::Mat::zeros(gray_roi.size(), CV_8U);
    cv::circle(mask, cv::Point(gray_roi.cols/2, gray_roi.rows/2), cvRound(radius), 255, -1);

    // 应用掩码：将掩码外的像素设为0
    cv::bitwise_and(gray_roi, gray_roi, gray_roi, mask);

    // 计算矩
    cv::Moments m = cv::moments(gray_roi, false);

    if (m.m00 <= 0) {
        return point;
    }

    // 计算重心坐标
    double cx = m.m10 / m.m00;
    double cy = m.m01 / m.m00;
    return cv::Point2f(point.x - radius + cx, point.y - radius + cy);
}

std::vector<cv::Point2f> GetPoints(const LightBlob &blob_i, const LightBlob &blob_j, const cv::Mat &src) {
    std::vector<cv::Point2f> points;
    // 确定左右灯条

    const LightBlob* left_blob = &blob_i;
    const LightBlob* right_blob = &blob_j;
    if (blob_i.center.x > blob_j.center.x) {
        left_blob = &blob_j;
        right_blob = &blob_i;
    }

    // 对每个灯条的四个角点进行灰度重心精修
    cv::Point2f left_bottom_refined = GreyLevelCentroid(left_blob->bottom, left_blob->length*0.1, src);
    cv::Point2f left_top_refined = GreyLevelCentroid(left_blob->top, left_blob->length*0.1, src);
    cv::Point2f right_top_refined = GreyLevelCentroid(right_blob->top, right_blob->length*0.1, src);
    cv::Point2f right_bottom_refined = GreyLevelCentroid(right_blob->bottom, right_blob->length*0.1, src);

    /*
        1-----2
        |     |
        0-----3
    */
    // 按顺时针顺序返回四个角点：左下、左上、右上、右下
    points.push_back(left_bottom_refined);
    points.push_back(left_top_refined);
    points.push_back(right_top_refined);
    points.push_back(right_bottom_refined);

    return points;
}

// ==================== ArmorDetector 成员函数实现 ====================

// ---- 灯条检测 ----

LightBlobs ArmorDetector::FindLight(Mat &src, int enemy_color) {
    LightBlobs light_blobs;
    enemy_color_ = enemy_color;

    cv::Mat color_channel;
    cv::Mat src_bin;  // 只需要一个二值图像
    std::vector<cv::Mat> channels;
    cv::split(src, channels);

    if (enemy_color_ == ENEMY_BLUE) color_channel = channels[0];
    else color_channel = channels[2];

    cv::threshold(color_channel, src_bin, threshold_, 255, THRESH_BINARY);
    ImagePreProcessDim(src_bin);
    std::vector<std::vector<cv::Point>> light_contours;
    std::vector<cv::Vec4i> hierarchy;

    cv::findContours(src_bin, light_contours, hierarchy, cv::RETR_CCOMP, cv::CHAIN_APPROX_SIMPLE);

    for (size_t i = 0; i < light_contours.size(); i++) {
        if (hierarchy[i][2] == -1) {
            if (!isValidContourArea(light_contours[i])) continue;
            cv::RotatedRect rect = cv::minAreaRect(light_contours[i]);
            if (IsValidLightBlob(rect)) light_blobs.emplace_back(rect, GetBlobColor(src, rect));
        }
    }
    SlopeValidation(light_blobs);
    return light_blobs;
}

// ---- 装甲板匹配 ----

Armors ArmorDetector::FindArmor(cv::Mat &src, LightBlobs &light_blobs)
{
    Armors armors;
    ArmorType type;
    std::vector<bool> paired(light_blobs.size(), false);

    for (size_t i = 0; i < light_blobs.size() - 1; ++i)
    {
        const auto& blob_i = light_blobs[i];
        for (size_t j = i + 1; j < light_blobs.size(); ++j)
        {
            const auto& blob_j = light_blobs[j];
            if (!IsCoupleLight(blob_i, blob_j, enemy_color_))
            {
                continue;
            }

            if (ContainLight(blob_i, blob_j, light_blobs))
            {
                continue;
            }
            Judge_Type(blob_i, blob_j, type);
            std::vector<cv::Point2f> points = GetPoints(blob_i,blob_j,src);
            armors.emplace_back(points,type);
            paired[i] = true;
            paired[j] = true;
        }
    }

    return armors;
}

// ---- 数字分类 (原 NumberClassifier) ----

std::mutex mutex_;

void ArmorDetector::ExtractNumbers(const cv::Mat &src, std::vector<Armor> &armors)
{

    /*
    1---2
    |   |
    0---3
    */

    // Light length in image
    const int light_length = 12;
    // Image size after warp
    const int warp_height = 28;
    const int small_armor_width = 32;
    const int large_armor_width = 52;
    // Number ROI size
    const cv::Size roi_size(28, 28);
    const cv::Size input_size(28, 28);
    for (auto &armor : armors)
    {

        // Warp perspective transform
        cv::Point2f lights_vertices[4] = {armor.points[0], armor.points[1], armor.points[2], armor.points[3]};

        const int top_light_y = (warp_height - light_length) / 2 - 1;
        const int bottom_light_y = top_light_y + light_length;
        const int warp_width = armor.type == ArmorType::SMALL ? small_armor_width : large_armor_width;
        string big_or_small = ARMOR_TYPE_STR[static_cast<int>(armor.type)];
        // cout<<"ARMOR TYPE:"<<big_or_small<<endl;
        cv::Point2f target_vertices[4] = {
            cv::Point(0, bottom_light_y),
            cv::Point(0, top_light_y),
            cv::Point(warp_width - 1, top_light_y),
            cv::Point(warp_width - 1, bottom_light_y),
        };
        cv::Mat number_image;
        auto rotation_matrix = cv::getPerspectiveTransform(lights_vertices, target_vertices);
        cv::warpPerspective(src, number_image, rotation_matrix, cv::Size(warp_width, warp_height));

        // Get ROI
        number_image =
            number_image(cv::Rect(cv::Point((warp_width - roi_size.width) / 2, 0), roi_size));

        // Binarize
        cv::cvtColor(number_image, number_image, cv::COLOR_RGB2GRAY);
        // cv::threshold(number_image, number_image, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

        /*
        * gray_image: 这是输入的灰度图像，即待处理的图像。
        * binary_image: 这是输出的二值化图像，函数将在此处存储结果。
        * 255: 这是输出的最大像素值。在二值化过程中，大于阈值的像素将被设置为这个值（在这里是255），小于等于阈值的像素将被设置为0。
        * cv::ADAPTIVE_THRESH_GAUSSIAN_C: 这是自适应阈值化的方法之一。它表示使用高斯加权求和的方法计算邻域的加权平均值作为阈值。
        * cv::THRESH_BINARY: 这是阈值类型。在这个例子中，表示将大于阈值的像素设置为最大值（255），小于等于阈值的像素设置为0。
        * 11: 这是阈值化计算中使用的邻域大小（block size）。它指定了在每个像素周围多大的邻域内计算局部阈值。通常选择一个奇数，比如 3、5、7、11 等。
        * 2: 这是从邻域均值或加权均值中减去的常数（C）。它可以用来微调阈值的结果，以适应不同的图像对比度和光照变化。
        调整建议：
        === blockSize (第六个参数)：增大该值会使得阈值的计算更加平滑，适用于更大尺寸的图像区域。但是也会导致细节信息丢失，因此需要根据具体情况选择一个合适的值。
        === C (第七个参数)：增大该值会使得阈值升高，这意味着更多的像素可能会被视为背景或非目标物体。减小该值则会降低阈值，这样更多的像素会被视为目标物体。
        */
        cv::adaptiveThreshold(number_image, number_image, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY, 19, -3);

        cv::resize(number_image, number_image, input_size);
        // Set the top 2 rows and bottom 2 rows to black (value 0)
        // number_image.rowRange(0, 2).setTo(cv::Scalar(0));  // Top 2 rows
        // number_image.rowRange(number_image.rows - 2, number_image.rows).setTo(cv::Scalar(0));  // Bottom 2 rows

        armor.number_img = number_image;

    }
}
void ArmorDetector::Classify(std::vector<Armor> &armors)
{
    for (auto &armor : armors)
    {
        // Normalize
        cv::Mat input = armor.number_img / 255.0;
        // Create blob from image
        cv::Mat blob;
        cv::dnn::blobFromImage(input, blob);

        // Set the input blob for the neural network
        mutex_.lock();
        net_.setInput(blob);

        // Forward pass the image blob through the model
        cv::Mat outputs = net_.forward().clone();
        mutex_.unlock();

        //  Do softmax
        float max_prob = *std::max_element(outputs.begin<float>(), outputs.end<float>());
        cv::Mat softmax_prob;
        cv::exp(outputs - max_prob, softmax_prob);
        float sum = static_cast<float>(cv::sum(softmax_prob)[0]);
        softmax_prob /= sum;

        // Decode the output
        double confidence;
        cv::Point class_id_point;
        minMaxLoc(softmax_prob.reshape(1, 1), nullptr, &confidence, nullptr, &class_id_point);
        int label_id = class_id_point.x;

        armor.confidence = confidence;
        armor.number = class_names_[label_id];

    }
    armors.erase(
        std::remove_if(
            armors.begin(), armors.end(),
            [this](const Armor &armor)
            {
                bool mismatch_armor_type = false;
                // 检查是否应该忽略这个类别
                if (armor.confidence < classify_threshold_)
                {
                    return true;
                }

                for (const auto &ignore_class : ignore_classes_)
                {
                    if (armor.number == ignore_class)
                    {
                        return true;
                    }
                }

#ifdef SENTRY
                if (armor.type == ArmorType::LARGE)
                {
                    mismatch_armor_type =
                        armor.number == "outpost" || armor.number == "guard";
                }
                else if (armor.type == ArmorType::SMALL)
                {
                    mismatch_armor_type = armor.number == "1" || armor.number == "base" || armor.number == "guard";
                }
#else
                if (armor.type == ArmorType::LARGE)
                {
                    mismatch_armor_type =
                        armor.number == "outpost" || armor.number == "guard" || armor.number == "3" || armor.number == "4" || armor.number == "base";
                }
                else if (armor.type == ArmorType::SMALL)
                {
                    mismatch_armor_type = armor.number == "1" || armor.number == "base";
                }
#endif
                return mismatch_armor_type;
            }),
        armors.end());
}

void ArmorDetector::VisualizeResult(cv::Mat &src, const std::vector<Armor> &armors)
{
    for (const auto &armor : armors)
    {
        // --- Picture-in-picture on source image ---
        if (!armor.number_img.empty() && armor.points.size() >= 4)
        {
            const int scale = 3;
            const int border = 2;
            const int pip_w = armor.number_img.cols * scale;
            const int pip_h = armor.number_img.rows * scale;

            // Place PiP to the right of the armor's top-right corner (points[2])
            int pip_x = static_cast<int>(armor.points[2].x) + 10;
            int pip_y = static_cast<int>(armor.points[2].y);

            // Clamp to image bounds (all four sides)
            if (pip_x < 0) pip_x = 0;
            if (pip_y < 0) pip_y = 0;
            if (pip_x + pip_w + border * 2 > src.cols) pip_x = src.cols - pip_w - border * 2;
            if (pip_y + pip_h + border * 2 > src.rows) pip_y = src.rows - pip_h - border * 2;

            // Skip if PiP doesn't fit at all
            if (pip_x >= 0 && pip_y >= 0)
            {
                cv::Mat pip_color;
                cv::resize(armor.number_img, pip_color, cv::Size(pip_w, pip_h), 0, 0, cv::INTER_NEAREST);
                cv::cvtColor(pip_color, pip_color, cv::COLOR_GRAY2BGR);

                cv::Rect roi_outer(pip_x, pip_y, pip_w + border * 2, pip_h + border * 2);
                cv::rectangle(src, roi_outer, cv::Scalar(0, 255, 255), border);

                cv::Rect roi_inner(pip_x + border, pip_y + border, pip_w, pip_h);
                pip_color.copyTo(src(roi_inner));
            }
        }
    }
}

// ---- 一体化主接口 ----

Armors ArmorDetector::Detect(Mat &img, int enemy_color) {
    LightBlobs light_blobs = FindLight(img, enemy_color);
    if (light_blobs.size() < 2) return Armors();

    Armors armors = FindArmor(img, light_blobs);
    if (armors.empty()) return Armors();

    ExtractNumbers(img, armors);
    Classify(armors);
    VisualizeResult(img, armors);

    return armors;
}

// ---- 构造函数 (合并 Tradition + NumberClassifier) ----

ArmorDetector::ArmorDetector(
    const std::string &model_path, const std::string &label_path, const double threshold,
    const std::vector<std::string> &ignore_classes)
    : ignore_classes_(ignore_classes), classify_threshold_(threshold) {
    // 原Tradition部分
    enemy_color_ = 0;
    threshold_ = 180;
    // 原NumberClassifier部分
    net_ = cv::dnn::readNetFromONNX(model_path);

    std::ifstream label_file(label_path);
    std::string line;
    while (std::getline(label_file, line))
    {
        class_names_.push_back(line);
    }
}
