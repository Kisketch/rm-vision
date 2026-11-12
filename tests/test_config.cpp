// Config persistence contract (round 6): Config::saveTuningOverrides writes
// ONLY the tuning subset (camera / tracker / aimer).  This test pins that
// contract: the written values round-trip, and nothing else is promised.
#include <cstdio>
#include <string>

#include "autoaim/config.hpp"

#include "check.hpp"

using namespace autoaim;

void testConfigTuningRoundTrip() {
    const std::string src = std::string(AUTOAIM_ROOT) + "/config/default.yaml";
    const Config cfg = Config::load(src);
    const std::string out = "/tmp/rm_vision_tuning_roundtrip.yaml";
    cfg.saveTuningOverrides(out);

    cv::FileStorage fs(out, cv::FileStorage::READ);
    CHECK(fs.isOpened(), "saved tuning file opens");
    if (!fs.isOpened()) return;
    const cv::FileNode cam = fs["camera"];
    CHECK(!cam.empty(), "camera section written");
    CHECK(std::abs(double(cam["fx"]) - cfg.camera.fx) < 1e-9, "fx round-trips");
    CHECK(std::abs(double(cam["fy"]) - cfg.camera.fy) < 1e-9, "fy round-trips");
    const cv::FileNode trk = fs["tracker"];
    CHECK(!trk.empty(), "tracker section written");
    CHECK(std::abs(double(trk["q_xyz"]) - cfg.tracker.q_xyz) < 1e-12, "q_xyz round-trips");
    const cv::FileNode aim = fs["aimer"];
    CHECK(!aim.empty(), "aimer section written");
    CHECK(std::abs(double(aim["latency"]) - cfg.aimer.latency) < 1e-12, "latency round-trips");
    // The partial semantics are the point: no detector/ballistics sections.
    CHECK(fs["detector"].empty(), "detector section is NOT written (partial save)");
    CHECK(fs["ballistics"].empty(), "ballistics section is NOT written (partial save)");
}
