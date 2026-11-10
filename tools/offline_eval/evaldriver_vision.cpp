// evaldriver: replicates apps/autoaim_video.cpp pass 1 + pass 2 exactly,
// but reads frames from an ffmpeg rawvideo pipe instead of cv::VideoCapture
// (this VM's OpenCV videoio cannot load: missing GStreamer/ffmpeg6 libs).
//
// usage: evaldriver <video> <config.yaml> <repo_root> <out_prefix> <fps> <W> <H> [--detect]
// - if <out_prefix>_detections.txt exists and --detect is absent, detections
//   are loaded and restamped exactly like autoaim_video does.
// - otherwise detection runs and the cache is written.
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "autoaim/config.hpp"
#include "autoaim/detector.hpp"
#include "autoaim/evaluation.hpp"
#include "sentry/evaluation_sentry.hpp"
#include "autoaim/pose.hpp"

using namespace autoaim;

namespace {

void writeCsv(const std::string& path, const EvalResult& r) {
    std::ofstream o(path);
    o << "frame,state,xc,yc,zc,vxc,vyc,yaw,w,r1,r2,dz,matched,cmd_valid,center_mode,armor_k,facing_deg,"
         "cmd_yaw_deg,cmd_pitch_deg,t_fly,gimbal_yaw_deg,gimbal_pitch_deg,shot,shot_result,miss_px,off_u,off_v\n";
    for (const auto& l : r.log) {
        const auto& x = l.x;
        o << l.frame << ',' << toString(l.state) << ',' << x[0] << ',' << x[2] << ',' << x[4] << ',' << x[1] << ','
          << x[3] << ',' << x[6] << ',' << x[7] << ',' << x[8] << ',' << x[9] << ',' << x[10] << ',' << l.matched
          << ',' << l.cmd.valid << ',' << l.cmd.center_mode << ',' << l.cmd.armor_k << ',' << l.cmd.facing_deg
          << ',' << l.cmd.yaw * 180 / kPi << ',' << l.cmd.pitch * 180 / kPi << ',' << l.cmd.t_fly << ','
          << l.gimbal_yaw * 180 / kPi << ',' << l.gimbal_pitch * 180 / kPi << ',' << l.shot << ','
          << static_cast<int>(l.shot_rec.result) << ',' << l.shot_rec.miss_px << ',' << l.shot_rec.off_u << ','
          << l.shot_rec.off_v << '\n';
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 8) {
        std::cerr << "usage: evaldriver <video> <config> <repo_root> <out_prefix> <fps> <W> <H> [--detect]\n";
        return 1;
    }
    const std::string video = argv[1];
    const std::string config_path = argv[2];
    const std::string root = argv[3];
    const std::string prefix = argv[4];
    const double fps = std::stod(argv[5]);
    const int W = std::stoi(argv[6]);
    const int H = std::stoi(argv[7]);
    bool force_detect = false;
    for (int i = 8; i < argc; ++i)
        if (std::string(argv[i]) == "--detect") force_detect = true;

    Config cfg = Config::load(config_path);
    cfg.eval.fps = fps;
    if (cfg.detector.model_path.empty()) cfg.detector.model_path = root + "/model/mlp.onnx";
    if (cfg.detector.label_path.empty()) cfg.detector.label_path = root + "/model/label.txt";

    const std::string cache = prefix + "_detections.txt";
    std::vector<FrameDetections> dets;
    double det_ms = 0;
    bool loaded = false;
    if (!force_detect) {
        std::ifstream probe(cache);
        if (probe && loadDetections(cache, dets) && !dets.empty()) {
            for (auto& f : dets) f.stamp = f.frame / cfg.eval.fps;
            loaded = true;
            std::cout << "loaded " << dets.size() << " frames of detections from " << cache << '\n';
        }
    }
    if (!loaded) {
        Detector det(cfg.detector);
        const std::string cmd =
            "ffmpeg -v error -i '" + video + "' -f rawvideo -pix_fmt bgr24 -";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) throw std::runtime_error("cannot start ffmpeg for " + video);
        const std::size_t bytes = static_cast<std::size_t>(W) * H * 3;
        std::vector<unsigned char> buf(bytes);
        int n = 0;
        double total = 0;
        while (true) {
            const std::size_t got = std::fread(buf.data(), 1, bytes, pipe);
            if (got == 0) break;
            if (got != bytes) {
                std::cerr << "short frame at " << n << ": " << got << " of " << bytes << " bytes\n";
                break;
            }
            cv::Mat frame(H, W, CV_8UC3, buf.data());
            const auto t0 = std::chrono::steady_clock::now();
            FrameDetections f;
            f.frame = n;
            f.stamp = n / cfg.eval.fps;
            f.armors = det.detect(frame.clone());
            total += std::chrono::duration<double, std::milli>(
                         std::chrono::steady_clock::now() - t0)
                         .count();
            dets.push_back(std::move(f));
            if (++n % 200 == 0) std::cerr << "  detect " << n << " frames\r";
        }
        std::cerr << "\n";
        pclose(pipe);
        det_ms = n ? total / n : 0;
        if (!saveDetections(cache, dets)) throw std::runtime_error("cannot save detections: " + cache);
        std::cout << "detected " << dets.size() << " frames, " << det_ms << " ms/frame -> " << cache << '\n';
    }

    CameraModel camera(cfg.camera);
    PoseSolver solver(camera, cfg.pose);
    const auto t0 = std::chrono::steady_clock::now();
    const auto obs = computeObservations(dets, solver);
    const auto t1 = std::chrono::steady_clock::now();
    EvalResult res = evaluate(cfg, obs, camera, true);
    appendSentryStrategy(cfg, obs, camera, res);
    const auto t2 = std::chrono::steady_clock::now();
    const double pose_ms =
        std::chrono::duration<double, std::milli>(t1 - t0).count() / std::max<std::size_t>(1, obs.size());
    const double eval_ms =
        std::chrono::duration<double, std::milli>(t2 - t1).count() / std::max<std::size_t>(1, obs.size());

    const std::string report = formatReport(res, cfg.eval.fps);
    std::cout << "\n" << report;
    std::printf("timing: pose %.3f ms/frame, trackers+aimers+judge %.3f ms/frame%s\n", pose_ms, eval_ms,
                det_ms > 0 ? (", detector " + std::to_string(det_ms) + " ms/frame").c_str() : "");
    {
        std::ofstream o(prefix + "_report.md");
        o << report;
    }
    writeCsv(prefix + "_track.csv", res);
    return 0;
}
