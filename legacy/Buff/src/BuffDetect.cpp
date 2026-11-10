#include "BuffDetect.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>


namespace auto_buff{
namespace {
constexpr int kObbClassId = 1;
const std::string kObbRootDir = "../obb_dataset";
const std::string kObbImagesDir = kObbRootDir + "/images";
const std::string kObbLabelsDir = kObbRootDir + "/labels";

void EnsureObbDirs() {
    static bool initialized = false;
    if (initialized) return;
    std::filesystem::create_directories(kObbImagesDir);
    std::filesystem::create_directories(kObbLabelsDir);
    initialized = true;
}

cv::Point2f NormalizePoint(const cv::Point2f& p, int width, int height) {
    return cv::Point2f(p.x / static_cast<float>(width), p.y / static_cast<float>(height));
}

static void WriteObbLabelLine(std::ofstream& out, int class_id, int width, int height,
                              const std::vector<cv::Point2f>& points, const cv::Point2f& r_center,
                              cv::Mat* debug_img = nullptr) {
    if (points.size() < 5) return;

    cv::Point2f p1 = points[0];
    cv::Vec2f line1 = r_center - p1;
    float line1_length = cv::norm(line1);

    // w: 第1个点和r_center相连的线段再往两端各延伸 x0.1
    float w_pixel = line1_length * 1.2f;

    // 旋转角度
    float angle = atan2(line1[1], line1[0]) * 180.0f / CV_PI;

    // 中心: p1 和 r_center 的中点
    cv::Point2f center = (p1 + r_center) / 2.0f;

    // h: points[1] 和 points[3] 相连线段延伸 x0.2
    cv::Vec2f line2 = points[3] - points[1];
    float line2_length = cv::norm(line2);
    float h_pixel = line2_length * 1.4f;

    float cx = center.x / static_cast<float>(width);
    float cy = center.y / static_cast<float>(height);
    float w = w_pixel / static_cast<float>(width);
    float h = h_pixel / static_cast<float>(height);

    out << class_id << " " << cx << " " << cy << " " << w << " " << h << " " << angle;
    for (const auto& p : points) {
        const auto np = NormalizePoint(p, width, height);
        out << " " << np.x << " " << np.y;
    }
    const auto rc = NormalizePoint(r_center, width, height);
    out << " " << rc.x << " " << rc.y << "\n";

    if (debug_img) {
        cv::Mat& dbg = *debug_img;
        for (size_t i = 0; i < points.size(); ++i) {
            cv::circle(dbg, points[i], 4, cv::Scalar(0, 255, 0), -1);
            cv::putText(dbg, std::to_string(i), points[i] + cv::Point2f(6, -6),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
        }
        cv::circle(dbg, r_center, 5, cv::Scalar(0, 0, 255), -1);
        cv::putText(dbg, "R", r_center + cv::Point2f(6, -6),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 255), 1);
        cv::line(dbg, points[0], r_center, cv::Scalar(255, 0, 0), 2);
        cv::RotatedRect rrect(center, cv::Size2f(w_pixel, h_pixel), angle);
        cv::Point2f rect_pts[4];
        rrect.points(rect_pts);
        for (int i = 0; i < 4; ++i)
            cv::line(dbg, rect_pts[i], rect_pts[(i + 1) % 4], cv::Scalar(255, 0, 255), 2);
        cv::putText(dbg, cv::format("w=%.1f h=%.1f ang=%.1f", w_pixel, h_pixel, angle),
                    center + cv::Point2f(-40, -20), cv::FONT_HERSHEY_SIMPLEX, 0.4,
                    cv::Scalar(255, 255, 0), 1);
    }
}


void SaveObbSample(const cv::Mat& image, const PowerRune& powerrune) {
    if (image.empty() || image.cols <= 0 || image.rows <= 0) return;
    cv::Mat gray_image;
    cv::cvtColor(image, gray_image, cv::COLOR_BGR2GRAY);
    EnsureObbDirs();

    static uint64_t sample_id = 0;
    ++sample_id;
    std::ostringstream name;
    name << std::setw(8) << std::setfill('0') << sample_id;

    const std::string image_path = kObbImagesDir + "/" + name.str() + ".png";
    const std::string label_path = kObbLabelsDir + "/" + name.str() + ".txt";

    cv::imwrite(image_path, gray_image);

    std::ofstream out(label_path);
    if (!out.is_open()) return;
    out << std::fixed << std::setprecision(6);

    for (const auto& fan : powerrune.fanblades) {
        std::vector<cv::Point2f> fit_points = fan.points;
        fit_points.emplace_back(powerrune.r_center);
        WriteObbLabelLine(out, kObbClassId, image.cols, image.rows, fan.points, powerrune.r_center);
    }
}
}  // namespace

BuffDetector::BuffDetector(){
    net_ = cv::dnn::readNetFromONNX("../model/mlp_bh4.onnx");
}

void BuffDetector::HandleImg(){
    // 彩色图转灰度图
    cv::cvtColor(img_, gray_img_, cv::COLOR_BGR2GRAY);  
    // cv::imshow("gray", gray_img_); 
    // 进行二值化 
    cv::threshold(gray_img_, gray_img_, 60, 255, cv::THRESH_BINARY);
    // 闭运算（3x3）
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    cv::morphologyEx(gray_img_, gray_img_, cv::MORPH_CLOSE, kernel);
    // cv::imshow("gray", gray_img_);
    // 找轮廓
    cv::findContours(gray_img_, contours_, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    auto it = std::remove_if(contours_.begin(), contours_.end(),
        [](const std::vector<cv::Point>& c) { return c.size() < 2; });
    contours_.erase(it, contours_.end());
}

void BuffDetector::Lose()
{
    lose_++;
    if (lose_ >= 20) {
        status_ = LOSE;
        last_powerrune_ = std::nullopt;
    }
    status_ = TEM_LOSE;
}

std::optional<PowerRune> BuffDetector::Detect(cv::Mat & img){
    // 存放扇叶
    vector<FanBlade> fanblades;                              
    std::vector<std::pair<float,std::vector<cv::Point2f>>>rects;   
    // ROI截取中心1080x1080区域
    // [robustness fix 2026-11-10] clamp the hardcoded ROI to the actual
    // frame: the original line threw cv::Exception (killing the process)
    // for any input that is not exactly the expected 1440x1080 camera
    // frame. Behaviour on a 1440x1080 frame is unchanged.
    const cv::Rect want_roi(180, 0, 1080, 1080);
    const cv::Rect roi = want_roi & cv::Rect(0, 0, img.cols, img.rows);
    if (roi.empty()) {
        Lose();
        return std::nullopt;
    }
    img_ = img(roi).clone();
    // 预处理
    HandleImg();         
    // 进行圆拟合，获得待机击打区域
    DetectCircles(fanblades);
    if(fanblades.empty()||fanblades.size()>2){
        Lose();
        return std::nullopt;
    }
     
    // 进行矩形拟合
    DetectRect(rects);
    if(rects.empty()){
        Lose();
        return std::nullopt;
    }
    // 获得流水灯
    GetLightBars(fanblades,rects);
    if(fanblades.empty()){
        Lose();
        return std::nullopt;
    }
    /// 生成PowerRune
    auto r_center = GetR(fanblades);    
    PowerRune powerrune(fanblades, r_center, last_powerrune_);
    if (powerrune.is_unsolve()) {
        Lose();
        return std::nullopt;
    }
    status_ = TRACK;
    lose_ = 0;

    if (enable_dataset_) SaveObbSample(img_, powerrune);
    if(show_)Show(powerrune);
    
    // 转换坐标回原图坐标系（x 坐标平移 180）
    for (auto& blade : powerrune.fanblades) {
        blade.center.x += 180;
        for (auto& p : blade.points) p.x += 180;
    }
    powerrune.r_center.x += 180;
    // cv::imshow("img_",img_);
    std::optional<PowerRune> P;
    P.emplace(std::move(powerrune));
    last_powerrune_ = P;
    return P;
    
}

void BuffDetector::Show(PowerRune &P){
    // 绘制选中的扇叶关键点
    for(auto &fan : P.fanblades){
        for(int i = 0; i < 6; i++){
            circle(img_, fan.points[i], 2, Scalar(0, 200, 0), -1);
            putText(img_, std::to_string(i), fan.points[i],
                    FONT_HERSHEY_SIMPLEX, 0.4, Scalar(0, 200, 0), 0.2);
        }
    }
    // 绘制R标中心
    circle(img_, P.r_center, 3, Scalar(0, 0, 255), -1);
    putText(img_, "R", P.r_center + cv::Point2f(5, -5), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(0, 0, 255), 1);
}

cv::Point2f BuffDetector::GetR(std::vector<FanBlade> & fanblades)
{

    /// 算出大概位置
    cv::Point2f r_center_t = {0, 0};
    for (auto & fanblade : fanblades) {
        r_center_t += (fanblade.points[5]-fanblade.points[4])+fanblade.points[5];  // TODO
    }
    r_center_t /= float(fanblades.size());
    return r_center_t;
    /// 处理图片,mask选出大概范围
    double radius = cv::norm(fanblades[0].points[2] - fanblades[0].center) * 0.8;
    cv::Mat mask = cv::Mat::zeros(gray_img_.size(), CV_8U);  // mask
    circle(mask, r_center_t, radius, cv::Scalar(255), -1);
    bitwise_and(gray_img_, mask, gray_img_);                 // 将遮罩应用于二值化图像
    // cv::imshow("Dilated Image", gray_img_);               // 调试用

    std::vector<std::vector<cv::Point>> contours;
    auto r_center = r_center_t;
    cv::findContours(gray_img_, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);  // external找外部区域
    double ratio_1 = INF;
    for (auto & it : contours) {
            auto rotated_rect = cv::minAreaRect(it);
            double ratio = rotated_rect.size.height > rotated_rect.size.width
                            ? rotated_rect.size.height / rotated_rect.size.width
                            : rotated_rect.size.width / rotated_rect.size.height;
            ratio += cv::norm(rotated_rect.center - r_center_t) / (radius / 3);
            if (ratio < ratio_1) {
            ratio_1 = ratio;
            r_center = rotated_rect.center;
        }
    }
    return r_center;
}

void SaveImg(Mat &input_image){
    if (input_image.empty()) cout << "warpPerspective失败，图像为空!" << endl;
    static int number = 40000;
    number++;
    if(number%5==0){
        string filename = "../photos/" + to_string(number) + ".png";
        imwrite(filename, input_image); 
    }
    
    
}

bool BuffDetector::Verify(cv::Point2f points[4]) {
    cv::Point2f target_vertices[4] = {cv::Point2f(0, 0), cv::Point2f(0, 27), cv::Point2f(27, 27), cv::Point2f(27, 0)};
    auto rotation_matrix = cv::getPerspectiveTransform(points, target_vertices);
    cv::Mat input_image;
    cv::warpPerspective(gray_img_, input_image, rotation_matrix, cv::Size(28, 28));
    // SaveImg(input_image);
    input_image.convertTo(input_image, CV_32F, 1.0 / 255.0);
    cv::Mat blob = cv::dnn::blobFromImage(input_image, 1.0, cv::Size(28, 28),
                                          cv::Scalar(0), false, false, CV_32F);
    net_.setInput(blob);
    cv::Mat outputs = net_.forward().clone();
    // 获取类别1的概率
    float probability = outputs.at<float>(0, 0);
    float sigmoid_prob = 1.0f / (1.0f + exp(-probability));
    return sigmoid_prob > 0.5f;
}

void BuffDetector::DetectCircles(vector<FanBlade>& fanblades){
    int min_area = 3500;
    int max_area = 6000;
    for (const auto& contour : contours_) {
        cv::Point2f center;float radius;
        cv::minEnclosingCircle(contour, center, radius);
        double area = CV_PI * radius * radius;  // 使用外接圆面积
        if(this->debug_){
            circle(img_,center,radius,Scalar(200,0,200),2);
            putText(img_,"Size: "+to_string(area)+"Radius: "+to_string(radius),center, cv::FONT_HERSHEY_COMPLEX, 0.6, Scalar(205, 100, 225), 1);
        }

        if ((area>min_area) && (area<max_area)){
            cv::RotatedRect rect = cv::minAreaRect(contour);
            cv::Point2f vertices[4];
            rect.points(vertices);
            if(Verify(vertices)){
                fanblades.emplace_back(center,radius,_light);
                // 绘制符合条件的扇叶
                circle(img_, center, radius, Scalar(0, 255, 0), 2);
            }
        }

    }
}

void BuffDetector::DetectRect(std::vector<std::pair<float,std::vector<cv::Point2f>>> &rects){
    float min_area = 800;
    float max_area = 2000;
    float min_aspect = 0.05f;
    float max_aspect = 0.35f;
    for (const auto& contour : contours_) {
        cv::RotatedRect rect = cv::minAreaRect(contour);
        cv::Point2f vertices[4];
        rect.points(vertices);
        float area = rect.size.area();
        bool w_h = (rect.size.width < rect.size.height);
        float aspect_ratio = w_h ? (rect.size.width / rect.size.height) : (rect.size.height / rect.size.width);
        if(debug_){
            // for (int i = 0; i < 4; ++i) cv::line(img_, vertices[i], vertices[(i+1)%4], cv::Scalar(0, 255, 0), 2, cv::LINE_AA); 
            // putText(img_,"Size: "+to_string(area)+"Bili :"+to_string(aspect_ratio),vertices[1], cv::FONT_HERSHEY_COMPLEX,0.6, Scalar(0, 255, 0), 1);
        }
        if ((area>min_area) && (area<max_area) && (aspect_ratio>min_aspect) && (aspect_ratio<max_aspect)) {
            if(w_h)rect.angle+=90;
            std::vector<cv::Point2f> points(vertices, vertices + 4);
            points.emplace_back(rect.center);
            rects.emplace_back(rect.angle,points);
        }
    }
}

std::vector<cv::Point2f> GetPoints(
    const cv::Point2f& center, float radius, const cv::Point2f& ref_point) 
{
    std::vector<cv::Point2f> all_points;

    cv::Vec2f AB = cv::Vec2f(ref_point - center);
    float n = cv::norm(AB);
    cv::Vec2f dir_L1 = (n > 1e-8f) ? AB / n : AB;
    cv::Vec2f dir_L2(-dir_L1[1], dir_L1[0]);  // 垂直向量

    // L1: A ± dir_L1 * r,L2: A ± dir_L2 * r
    all_points.push_back(center-cv::Point2f(dir_L1[0]*radius, dir_L1[1]*radius));
    all_points.push_back(center+cv::Point2f(dir_L2[0]*radius, dir_L2[1]*radius));
    all_points.push_back(center+cv::Point2f(dir_L1[0]*radius, dir_L1[1]*radius));
    all_points.push_back(center-cv::Point2f(dir_L2[0]*radius, dir_L2[1]*radius));
    all_points.push_back(center);
    all_points.push_back(ref_point);
    return all_points;
}

bool BuffDetector::GetLightBars(vector<FanBlade>& fanblades, std::vector<std::pair<float, std::vector<cv::Point2f>>>& rects) {
    for (auto& fan : fanblades) {

        float min_distance = std::numeric_limits<float>::max();
        const std::vector<cv::Point2f>* best_rect_points = nullptr;

        for (const auto& rect : rects) {
            const cv::Point2f& ref_point = rect.second.back(); 
            float dist = cv::norm(ref_point - fan.center);
            if (dist < min_distance) {
                min_distance = dist;
                best_rect_points = &(rect.second);
            }
        }

        if (min_distance < fan.radius *3.5 && best_rect_points) {
            cv::Point2f ref_point = best_rect_points->back();
            float r = fan.radius - 4;
            std::vector<cv::Point2f> all_points = GetPoints(fan.center, r, ref_point);
            fan.points = all_points;

        }

    }

    // 剔除未匹配到灯条的扇叶，防止后续 GetR 越界访问
    fanblades.erase(
        std::remove_if(fanblades.begin(), fanblades.end(),
            [](const FanBlade& f) { return f.points.empty(); }),
        fanblades.end());
    return !fanblades.empty();
}
}// namespace


