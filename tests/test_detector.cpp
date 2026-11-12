// Split from the former single-file test_main.cpp (round 6): same tests,
// grouped by module so a failure points at its subsystem.  Harness: check.hpp.
#include <cmath>
#include <cstdio>
#include <limits>
#include <random>
#include <utility>

#include "autoaim/aimer.hpp"
#include "autoaim/ballistics.hpp"
#include "autoaim/config.hpp"
#include "autoaim/delay_calibration.hpp"
#include "autoaim/detector.hpp"
#include "autoaim/evaluation.hpp"
#include "autoaim/keypoint_detector.hpp"
#include "autoaim/pose.hpp"
#include "autoaim/pose_buffer.hpp"
#include "autoaim/tracker.hpp"

#include "check.hpp"

using namespace autoaim;

void testDetectorDegenerateInputs() {
    // Regression: detect() used to segfault on empty/degenerate frames
    // (cv::split yields zero channels on an empty Mat, then ch[2] was read).
    // A dropped camera frame must read as "no armors", never kill the
    // vision process.
    DetectorConfig dc;
    dc.model_path = "";
    Detector det(dc);
    cv::Mat empty;
    CHECK(det.detect(empty).empty(), "empty frame");
    cv::Mat tiny(1, 1, CV_8UC3, cv::Scalar(0, 0, 255));
    CHECK(det.detect(tiny).empty(), "1x1 frame");
    cv::Mat gray(64, 64, CV_8UC1, cv::Scalar(200));
    CHECK(det.detect(gray).empty(), "gray frame");
    cv::Mat fl(64, 64, CV_32FC3, cv::Scalar(0.5f, 0.5f, 0.5f));
    CHECK(det.detect(fl).empty(), "float frame");
    cv::Mat black(1080, 1440, CV_8UC3, cv::Scalar(0, 0, 0));
    CHECK(det.detect(black).empty(), "black frame");
}

void testWindowedDetection() {
    // Windowed (ROI) detection must reproduce full-frame results for
    // armors inside the windows and drop armors outside them.
    DetectorConfig dc;
    dc.model_path = "";  // numbers stay "unknown"; geometry is what matters
    Detector det(dc);
    cv::Mat scene(1080, 1440, CV_8UC3, cv::Scalar(8, 8, 8));
    auto drawArmor = [&](int cx, int cy) {
        for (int s = -1; s <= 1; s += 2)
            cv::rectangle(scene, cv::Rect(cx + s * 55 - 4, cy - 26, 8, 52),
                          cv::Scalar(30, 30, 230), -1);
        cv::rectangle(scene, cv::Rect(cx - 20, cy - 14, 40, 28), cv::Scalar(200, 200, 200), -1);
    };
    drawArmor(400, 400);
    drawArmor(1000, 650);
    auto full = det.detect(scene);
    CHECK(full.size() == 2, "windowed test scene: full detect finds 2, got %zu", full.size());
    // Windows around both armors (with margin) -> same detections.
    std::vector<cv::Rect> both{{250, 250, 300, 300}, {850, 500, 300, 300}};
    auto win = det.detect(scene, both);
    CHECK(win.size() == full.size(), "windowed finds both: %zu vs %zu", win.size(), full.size());
    for (const auto& a : full) {
        bool matched = false;
        for (const auto& b : win)
            if (cv::norm(a.center - b.center) < 1.5f) matched = true;
        CHECK(matched, "windowed centre matches full within 1.5 px");
    }
    // Window around only the first armor -> the second is not reported.
    auto one = det.detect(scene, std::vector<cv::Rect>{{250, 250, 300, 300}});
    CHECK(one.size() == 1, "single window finds 1, got %zu", one.size());
    // Empty window list == full-frame detection.
    auto viaEmpty = det.detect(scene, std::vector<cv::Rect>{});
    CHECK(viaEmpty.size() == full.size(), "empty windows == full detect");
    // Overlapping windows must not duplicate a light/armor.
    auto overlap = det.detect(scene, std::vector<cv::Rect>{{250, 250, 300, 300}, {280, 280, 300, 300}});
    CHECK(overlap.size() == 1, "overlapping windows dedupe: %zu", overlap.size());
}

void testKeypointDetector() {
    // Synthetic YOLO-pose output: 2 classes -> 18 channels, letterboxed
    // 1440x1080 into 640 (scale 4/9, pad_y 80).  No trained model involved.
    const int C = 18, N = 4;
    const int sizes[3] = {1, C, N};
    cv::Mat out(3, sizes, CV_32F, cv::Scalar(0));
    auto set = [&](int c, int i, float v) { out.ptr<float>()[c * N + i] = v; };
    auto anchor = [&](int i, float cx, float cy, float w, float h, int cls, float score,
                      const std::array<cv::Point2f, 4>& kp) {
        set(0, i, cx); set(1, i, cy); set(2, i, w); set(3, i, h);
        set(4 + cls, i, score);
        for (int k = 0; k < 4; ++k) {
            set(6 + 3 * k, i, kp[k].x);
            set(6 + 3 * k + 1, i, kp[k].y);
            set(6 + 3 * k + 2, i, 0.99f);
        }
    };
    const std::array<cv::Point2f, 4> kp0 = {cv::Point2f(270, 300), cv::Point2f(370, 300),
                                            cv::Point2f(375, 345), cv::Point2f(265, 345)};
    anchor(0, 320, 322, 120, 50, 0, 0.9f, kp0);
    anchor(1, 324, 324, 120, 50, 0, 0.8f, kp0);  // duplicate -> NMS drops it
    anchor(2, 100, 100, 60, 30, 1, 0.2f, kp0);   // below conf -> dropped
    const std::array<cv::Point2f, 4> kp3 = {cv::Point2f(65, 520), cv::Point2f(135, 520),
                                            cv::Point2f(138, 550), cv::Point2f(62, 550)};
    anchor(3, 100, 560, 80, 40, 1, 0.7f, kp3);

    const auto cands = parseKeypointOutput(out, 2, 0.25f, 0.45f, 640, 1440, 1080);
    CHECK(cands.size() == 2, "candidates %zu", cands.size());
    CHECK(std::abs(cands[0].score - 0.9f) < 1e-6 && cands[0].cls == 0, "best candidate first");
    CHECK(cands[1].cls == 1 && std::abs(cands[1].score - 0.7f) < 1e-6, "second candidate");

    DetectorConfig dc;
    dc.onnx_classes = {"3", "1"};
    const auto armor = candidateToArmor(cands[0], dc);
    CHECK(armor.number == "3", "number %s", armor.number.c_str());
    CHECK(armor.type == ArmorType::SMALL, "small plate by aspect");
    const std::array<cv::Point2f, 4> want = {cv::Point2f(596.25f, 596.25f), cv::Point2f(607.5f, 495.f),
                                             cv::Point2f(832.5f, 495.f), cv::Point2f(843.75f, 596.25f)};
    for (int i = 0; i < 4; ++i)
        CHECK(cv::norm(armor.corners[i] - want[i]) < 0.01, "corner %d (%.2f, %.2f)", i,
              armor.corners[i].x, armor.corners[i].y);
    CHECK(armor.color == dc.enemy_color, "colour filled");

    // The transposed export layout ([1, N, C]) parses identically.
    const int sizes_t[3] = {1, N, C};
    cv::Mat out_t(3, sizes_t, CV_32F, cv::Scalar(0));
    for (int c = 0; c < C; ++c)
        for (int i = 0; i < N; ++i) out_t.ptr<float>()[i * C + c] = out.ptr<float>()[c * N + i];
    CHECK(parseKeypointOutput(out_t, 2, 0.25f, 0.45f, 640, 1440, 1080).size() == 2,
          "transposed layout");

    // Invalid high-score detections must be removed before NMS, otherwise they
    // can suppress a valid lower-score observation of the same plate.
    const cv::Mat good = out.clone();
    set(8, 0, 0.01f);  // first corner confidence, best anchor
    const auto low = parseKeypointOutput(out, 2, 0.25f, 0.45f, 640, 1440, 1080);
    CHECK(low.size() == 2 && std::abs(low[0].score - 0.8f) < 1e-6, "low-confidence anchor rejected before NMS");
    out = good.clone();
    for (int i : {0, 1}) set(6, i, std::numeric_limits<float>::quiet_NaN());
    CHECK(parseKeypointOutput(out, 2, 0.25f, 0.45f, 640, 1440, 1080).size() == 1, "NaN corners rejected");
    out = good.clone();
    for (int i : {0, 1}) set(7, i, 10);  // inside letterbox padding, outside source image
    CHECK(parseKeypointOutput(out, 2, 0.25f, 0.45f, 640, 1440, 1080).size() == 1, "padding corners rejected");
    out = good.clone();
    for (int i : {0, 1})
        for (int k = 0; k < 4; ++k) { set(6 + 3*k, i, 320); set(7 + 3*k, i, 320); }
    CHECK(parseKeypointOutput(out, 2, 0.25f, 0.45f, 640, 1440, 1080).size() == 1, "collapsed quad rejected");

    // Detector fallback: backend=onnx with no model must run traditional.
    DetectorConfig fc;
    fc.backend = "onnx";
    fc.onnx_path = "";
    Detector det(fc);
    CHECK(!det.usingOnnx(), "fallback when no model");
    const cv::Mat black(100, 100, CV_8UC3, cv::Scalar(0, 0, 0));
    CHECK(det.detect(black).empty(), "traditional path on black frame");
    KeypointDetector missing(fc);
    CHECK(!missing.ok() && missing.detect(black).empty(), "missing model is inert");
    std::printf("keypoint detector: synthetic-tensor postprocess + fallback ok\n");
}

void testSplitLightMerge() {
    // Measured on 3.avi frames 1966-2050: at close range a bar's core
    // saturates to white, R-B collapses there and the mask splits one bar
    // into a top and a bottom fragment, so the plate failed max_length_ratio.
    DetectorConfig dc;
    dc.model_path = "";
    const cv::Scalar red(30, 30, 230), white(255, 255, 255), dark(8, 8, 8);
    auto scene = [&](const cv::Scalar& core) {
        cv::Mat img(600, 800, CV_8UC3, dark);
        const int cx = 400, cy = 300;
        // left bar: red 18 px, 16 px core, red 18 px; right bar intact 52 px
        cv::rectangle(img, cv::Rect(cx - 59, cy - 26, 8, 18), red, -1);
        cv::rectangle(img, cv::Rect(cx - 59, cy - 8, 8, 16), core, -1);
        cv::rectangle(img, cv::Rect(cx - 59, cy + 8, 8, 18), red, -1);
        cv::rectangle(img, cv::Rect(cx + 51, cy - 26, 8, 52), red, -1);
        return img;
    };
    dc.merge_split_lights = false;
    CHECK(Detector(dc).detect(scene(white)).empty(), "split bar without merge: no plate");
    dc.merge_split_lights = true;
    const auto healed = Detector(dc).detect(scene(white));
    CHECK(healed.size() == 1, "split bar with merge: 1 plate, got %zu", healed.size());
    if (healed.size() == 1) {
        const float len = healed[0].left.length;
        CHECK(std::abs(len - 52.f) < 5.f, "merged bar spans the whole bar: %.1f px", len);
    }
    // A dark gap is a bar and its floor reflection, not a saturated core.
    CHECK(Detector(dc).detect(scene(dark)).empty(), "dark gap stays split");

    // Additive merge: stray light stacked on an intact bar (4.avi frame 654)
    // must not break the correct pair the bar alone makes.
    cv::Mat stray(600, 800, CV_8UC3, dark);
    cv::rectangle(stray, cv::Rect(341, 274, 8, 52), red, -1);
    cv::rectangle(stray, cv::Rect(451, 274, 8, 52), red, -1);
    cv::rectangle(stray, cv::Rect(451, 236, 8, 30), red, -1);  // streak above
    cv::rectangle(stray, cv::Rect(451, 266, 8, 8), white, -1);
    const auto kept = Detector(dc).detect(stray);
    CHECK(kept.size() == 1, "stray streak: still 1 plate, got %zu", kept.size());
    if (kept.size() == 1)
        CHECK(std::abs(kept[0].right.length - 52.f) < 5.f, "pairs the real bar: %.1f px",
              kept[0].right.length);
    std::printf("detector: split-bar merge heals saturated bars, keeps reflections apart\n");
}

void testHorizontalBlobRejected() {
    // makeLight measures length top-to-bottom, so a horizontal strip used to
    // pass as a bar of length = its thickness and veto the plate around it
    // through reject_contained_lights.
    DetectorConfig dc;
    dc.model_path = "";
    cv::Mat img(600, 800, CV_8UC3, cv::Scalar(8, 8, 8));
    const cv::Scalar red(30, 30, 230);
    cv::rectangle(img, cv::Rect(341, 274, 8, 52), red, -1);
    cv::rectangle(img, cv::Rect(451, 274, 8, 52), red, -1);
    cv::rectangle(img, cv::Rect(380, 290, 40, 8), red, -1);  // lying-down strip
    dc.reject_horizontal_lights = false;
    CHECK(Detector(dc).detect(img).empty(), "old behaviour: strip vetoes the plate");
    dc.reject_horizontal_lights = true;
    DetectorDebug dbg;
    const auto armors = Detector(dc).detect(img, &dbg);
    CHECK(armors.size() == 1, "strip rejected: plate found, got %zu", armors.size());
    for (const auto& l : dbg.lights)
        CHECK(l.length >= l.width, "no lying-down light survives (len %.1f w %.1f)", l.length, l.width);
    std::printf("detector: horizontal blobs are not light bars\n");
}
