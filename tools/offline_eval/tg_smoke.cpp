// tg_smoke: integration check of the shipped SentryVision TG-ROI wiring.
// Feeds the same real frames through SentryVision with ROI off and on and
// compares observation counts frame by frame.
// usage: tg_smoke <video> <sentry_root> <nframes>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "autoaim/config.hpp"
#include "sentry/sentry.hpp"

using namespace autoaim;

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "usage: tg_smoke <video> <sentry_root> <nframes>\n";
        return 1;
    }
    const std::string video = argv[1], root = argv[2];
    const int want = std::stoi(argv[3]);
    const int W = 1440, H = 1080;

    Config cfg = Config::load(root + "/config/default.yaml");
    if (cfg.detector.model_path.empty()) cfg.detector.model_path = root + "/model/mlp.onnx";
    if (cfg.detector.label_path.empty()) cfg.detector.label_path = root + "/model/label.txt";

    std::vector<cv::Mat> frames;
    {
        const std::string cmd = "ffmpeg -v error -i '" + video + "' -f rawvideo -pix_fmt bgr24 -";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return 1;
        const std::size_t bytes = static_cast<std::size_t>(W) * H * 3;
        std::vector<unsigned char> buf(bytes);
        while (static_cast<int>(frames.size()) < want) {
            if (std::fread(buf.data(), 1, bytes, pipe) != bytes) break;
            frames.push_back(cv::Mat(H, W, CV_8UC3, buf.data()).clone());
        }
        pclose(pipe);
    }
    std::printf("loaded %zu frames\n", frames.size());

    sentry::SentryVision off(cfg), on(cfg);
    on.setRoiEnabled(true);
    long obs_off = 0, obs_on = 0, frames_diff = 0, cmd_off = 0, cmd_on = 0;
    for (std::size_t i = 0; i < frames.size(); ++i) {
        const double stamp = static_cast<double>(i) / 25.0;
        auto a = off.process(frames[i], stamp);
        auto b = on.process(frames[i], stamp);
        obs_off += static_cast<long>(a.observations.armors.size());
        obs_on += static_cast<long>(b.observations.armors.size());
        if (a.observations.armors.size() != b.observations.armors.size()) ++frames_diff;
        if (a.command.valid) ++cmd_off;
        if (b.command.valid) ++cmd_on;
    }
    std::printf("observations: roi-off %ld, roi-on %ld; frames with different counts %ld / %zu\n",
                obs_off, obs_on, frames_diff, frames.size());
    std::printf("valid aim commands: roi-off %ld, roi-on %ld\n", cmd_off, cmd_on);
    return 0;
}
