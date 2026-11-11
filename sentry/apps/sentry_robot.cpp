// Robot-side loop: camera source + optional old serial protocol ->
// integrated sentry vision -> 21-byte control frame.  All capture goes
// through autoaim::CameraSource (core), the same interface the infantry
// apps use - there is no sentry-private capture path:
//
//   --mvs     HikRobot MVS source (needs -DAUTOAIM_WITH_MVS=ON): hardware
//             device timestamps, ClockAligner-aligned, exposure-centre
//             stamps (hw_stamp = true). --exposure-offset-ms is folded
//             into MvsConfig::stamp_offset_us.
//   --camera  V4L2/UVC device (DeviceSource): stamp = receipt minus half
//             the frame period, minus --exposure-offset-ms.
//   --video   file replay (VideoFileSource): stamps are MEDIA time
//             (frame index / fps), deterministic and independent of how
//             fast the host processes.  A live serial attitude stream
//             runs on the host clock, so for serial bench work and for
//             --calib-delay use a live source, not a file.
//
// Attitude timing (G-02): the gimbal attitude is SLERP-interpolated at
// the frame's exposure stamp by io::SerialPort.  When no attitude
// brackets the stamp the frame is skipped (never processed with a
// stale or zero attitude) and counted in the summary line.
//
//   ./build/sentry/sentry_robot --camera 0 --serial /dev/ttyACM0 --config sentry/config/sentry.yaml
//   ./build/sentry/sentry_robot --video video/3.avi --serial "" --render
//
// --max-extrap-ms (default 10): a frame stamped at receipt is usually newer
// than the last attitude sample that has arrived; up to this much the
// attitude is extrapolated at the last measured rate instead of skipping the
// frame (with 0 and --exposure-offset-ms 0 every live frame was skipped).
//
// --calib-delay: measure --exposure-offset-ms.  Aim at a STATIC plate, sweep
// the gimbal (>= 0.3 rad/s RMS) for ~10 s, quit; the summary prints the offset
// that makes the plate's world position stand still (autoaim/
// delay_calibration.hpp).  A wrong offset rotates every measurement by
// gimbal_rate x error - the error is largest exactly during slews/plate switches.
#include <opencv2/opencv.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <iostream>
#include <string>

#include "autoaim/camera_source.hpp"
#include "autoaim/config.hpp"
#include "autoaim/delay_calibration.hpp"
#include "autoaim/pose_buffer.hpp"
#include "io/serial_port.hpp"
#include "sentry/sentry.hpp"
#include "sentry/serial_protocol.hpp"

namespace {

double nowSeconds() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

}  // namespace

int main(int argc, char** argv) {
    std::string config_path = std::string(SENTRY_ROOT) + "/config/sentry.yaml";
    std::string video;
    std::string serial_device;
    int camera_index = -1;
    bool use_mvs = false;
    bool render = false;
    int max_frames = -1;
    double comp_yaw = 0, comp_pitch = 0;
    double exposure_offset_ms = 0.0;  // live camera: receipt -> exposure lag
    bool calib_delay = false;         // measure exposure_offset_ms (see header)
    double stamp_delay_ms = 1.5;      // serial: MCU sample -> host receipt lag
    double max_gap_ms = 50.0;         // serial: max attitude gap to interpolate
    double max_extrap_ms = 10.0;      // serial: max extrapolation past the newest sample
    bool require_crc = false;         // serial: require the byte-2 CRC-8 extension
    bool roi = false;                 // tracking-guided windowed detection

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--help" || a == "-h") {
            std::cout << "usage: sentry_robot (--camera INDEX | --video FILE | --mvs) [options]\n"
                         "  --config FILE --serial DEVICE --render --roi --max-frames N\n"
                         "  --calib-delay (never fires) --exposure-offset-ms MS\n"
                         "  --stamp-delay-ms MS --max-gap-ms MS --max-extrap-ms MS --require-crc\n"
                         "  --comp-yaw DEG --comp-pitch DEG\n";
            return 0;
        }
        else if (a == "--config" && i + 1 < argc) config_path = argv[++i];
        else if (a == "--video" && i + 1 < argc) video = argv[++i];
        else if (a == "--camera" && i + 1 < argc) camera_index = std::stoi(argv[++i]);
        else if (a == "--mvs") use_mvs = true;
        else if (a == "--serial" && i + 1 < argc) serial_device = argv[++i];
        else if (a == "--render") render = true;
        else if (a == "--max-frames" && i + 1 < argc) max_frames = std::stoi(argv[++i]);
        else if (a == "--comp-yaw" && i + 1 < argc) comp_yaw = std::stod(argv[++i]);
        else if (a == "--comp-pitch" && i + 1 < argc) comp_pitch = std::stod(argv[++i]);
        else if (a == "--exposure-offset-ms" && i + 1 < argc) exposure_offset_ms = std::stod(argv[++i]);
        else if (a == "--calib-delay") calib_delay = true;
        else if (a == "--stamp-delay-ms" && i + 1 < argc) stamp_delay_ms = std::stod(argv[++i]);
        else if (a == "--max-gap-ms" && i + 1 < argc) max_gap_ms = std::stod(argv[++i]);
        else if (a == "--max-extrap-ms" && i + 1 < argc) max_extrap_ms = std::stod(argv[++i]);
        else if (a == "--require-crc") require_crc = true;
        else if (a == "--roi") roi = true;
        else {
            std::cerr << "unknown argument: " << a << "\n";
            return 1;
        }
    }

    autoaim::Config cfg;
    try {
        cfg = autoaim::Config::load(config_path);
    } catch (const std::exception& e) {
        std::cerr << "config: " << e.what() << "\n";
        return 1;
    }
    sentry::SentryVision vision(cfg);
    if (roi) vision.setRoiEnabled(true);

    std::unique_ptr<autoaim::CameraSource> source;
    try {
        if (use_mvs) {
#ifdef AUTOAIM_WITH_MVS
            autoaim::MvsConfig mc;
            mc.stamp_offset_us = exposure_offset_ms * 1000.0;
            source = std::make_unique<autoaim::MvsSource>(mc);
#else
            std::cerr << "--mvs needs a build with -DAUTOAIM_WITH_MVS=ON\n";
            return 1;
#endif
        } else if (!video.empty()) {
            source = std::make_unique<autoaim::VideoFileSource>(video);
        } else if (camera_index >= 0) {
            source = std::make_unique<autoaim::DeviceSource>(camera_index, cfg.eval.fps,
                                                             cfg.camera.width, cfg.camera.height);
        }
    } catch (const std::exception& e) {
        std::cerr << "cannot open source: " << e.what() << "\n";
        return 1;
    }
    if (!source) {
        std::cerr << "one of --camera, --video or --mvs is required\n";
        return 1;
    }

    std::unique_ptr<io::SerialPort> serial;
    if (!serial_device.empty())
        serial = std::make_unique<io::SerialPort>(serial_device, 115200, stamp_delay_ms / 1000.0,
                                                  max_gap_ms / 1000.0, require_crc,
                                                  max_extrap_ms / 1000.0);

    if (calib_delay && !serial) {
        std::cerr << "--calib-delay needs --serial (attitude stream)\n";
        return 1;
    }
    autoaim::DelayCalibrator calibrator(-0.01, 0.05, 0.0005);
    // Attitude at host time t, in the same convention as setGimbalAttitude.
    const autoaim::DelayCalibrator::AttitudeFn attitude_at = [&serial](double t, cv::Matx33d& R) {
        sentry::AttitudeFrame a;
        if (!serial->query(t, a)) return false;
        R = autoaim::quatToMat(autoaim::quatFromYawPitchRoll(a.yaw, -a.pitch, a.roll));
        return true;
    };

    autoaim::AimCommand last_cmd;
    double last_valid = -1e9;
    double last_cmd_stamp = -1e9;
    double last_cmd_yaw = 0;
    std::string last_cmd_id;
    int processed = 0;
    std::uint64_t attitude_misses = 0;
    cv::Mat frame;
    autoaim::SourceFrame sf;
    while (source->grab(sf)) {
        if (max_frames > 0 && processed >= max_frames) break;
        frame = sf.image;
        // Exposure stamp from the source.  The MVS offset is already
        // folded into the source; the UVC device stamp still takes the
        // app-level --exposure-offset-ms correction on top of its
        // half-period estimate.  Video files use media time as-is.
        const double stamp = use_mvs ? sf.exposure_stamp
                                     : (video.empty() ? sf.exposure_stamp - exposure_offset_ms / 1000.0
                                                      : sf.exposure_stamp);

        sentry::AttitudeFrame attitude;
        bool have_attitude = false;
        if (serial) {
            if (serial->query(stamp, attitude)) {
                have_attitude = true;
                vision.setEnemyColor(attitude.color == 1 ? autoaim::Color::RED
                                                         : autoaim::Color::BLUE);
            } else {
                ++attitude_misses;
            }
        }

        // With a serial link, a frame without a bracketed attitude is skipped
        // entirely: processing it with a stale or zero attitude would poison
        // the tracker with a wrong world frame.  The hold logic below still
        // runs, so the MCU keeps getting non-firing commands meanwhile.
        sentry::SentryOutput out;
        const bool have_out = !serial || have_attitude;
        if (have_out) {
            out = vision.process(frame, stamp, have_attitude ? attitude.yaw : 0.0,
                                 have_attitude ? attitude.pitch : 0.0,
                                 have_attitude ? attitude.roll : 0.0);
        }

        if (calib_delay && have_attitude && !out.observations.armors.empty()) {
            // the plate nearest the image centre, in the gimbal frame
            const auto& obs = out.observations.armors;
            const auto it = std::min_element(obs.begin(), obs.end(), [](const auto& a, const auto& b) {
                return a.image_center_dist < b.image_center_dist;
            });
            calibrator.add(stamp, vision.camera().R_world_gimbal().t() * it->position);
            // resolve once the attitude for stamp + 10 ms has surely arrived
            calibrator.resolve(nowSeconds() - 0.1, attitude_at);
        }

        sentry::SerialCommand sc;
        if (have_out && out.command.valid) {
            // Feed-forward rate from consecutive commands of the SAME target
            // only: across a target switch or a gap (skipped frames) the
            // finite difference is a jump, not a rate, and used to saturate
            // at gimbal_max_rate for one frame.
            const std::string& id = out.targets[out.selected].id;
            double yaw_vel = 0;
            const double dt_cmd = stamp - last_cmd_stamp;
            if (id == last_cmd_id && dt_cmd > 0 && dt_cmd < 0.1)
                yaw_vel = autoaim::wrapAngle(out.command.yaw - last_cmd_yaw) / dt_cmd;
            yaw_vel = std::clamp(yaw_vel, -cfg.aimer.gimbal_max_rate, cfg.aimer.gimbal_max_rate);
            sc = sentry::toSerial(out.command, comp_yaw, comp_pitch, yaw_vel);
            last_cmd = out.command;
            last_cmd_yaw = out.command.yaw;
            last_cmd_id = id;
            last_cmd_stamp = stamp;
            last_valid = stamp;
        } else if (stamp - last_valid < 0.5 && last_cmd.valid) {
            // HoldFound: keep sending the last solution for 0.5 s, but never fire.
            sc = sentry::toSerial(last_cmd, comp_yaw, comp_pitch);
            sc.fire = 0;
        }
        // Calibration observes a static plate while the operator sweeps the gimbal.
        // Do not let an otherwise valid tracking solution authorize a shot.
        if (calib_delay) sc.fire = 0;
        if (serial) serial->send(sentry::packControl(sc));

        if (render && have_out) {
            for (const auto& o : out.observations.armors)
                for (int i = 0; i < 4; ++i)
                    cv::line(frame, o.det.corners[i], o.det.corners[(i + 1) % 4],
                             cv::Scalar(0, 255, 0), 2);
            if (out.command.valid) {
                const auto center = vision.camera().project(out.targets[out.selected].centerAt(stamp));
                cv::drawMarker(frame, center, cv::Scalar(0, 255, 255), cv::MARKER_CROSS, 18, 2);
                cv::putText(frame,
                            cv::format("%s %s yaw %.1f pitch %.1f %s", out.targets[out.selected].label.c_str(),
                                       autoaim::toString(out.targets[out.selected].state),
                                       out.command.yaw * 180 / autoaim::kPi,
                                       out.command.pitch * 180 / autoaim::kPi,
                                       out.command.fire ? "FIRE" : ""),
                            cv::Point(20, 40), cv::FONT_HERSHEY_SIMPLEX, 0.8,
                            out.command.fire ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 255, 0), 2);
            }
            cv::imshow("sentry", frame);
            if (cv::waitKey(1) == 27) break;
        }
        ++processed;
    }
    if (serial) serial->send(sentry::packControl({}));  // normal exit: invalidate the last command
    if (calib_delay) {
        calibrator.resolve(nowSeconds(), attitude_at);
        const autoaim::DelayEstimate e = calibrator.estimate();
        std::cout << "delay calibration: " << e.frames << " frames, gimbal rate RMS " << e.gimbal_rate_rms
                  << " rad/s, plate scatter " << e.spread_at_zero * 100 << " cm -> " << e.spread * 100
                  << " cm\n";
        if (e.ok)
            std::cout << "  -> use --exposure-offset-ms " << exposure_offset_ms + e.delay * 1000.0
                      << " (current " << exposure_offset_ms << ", correction " << e.delay * 1000.0 << ")\n";
        else
            std::cout << "  -> no estimate: " << e.reason << "\n";
    }
    std::cout << "processed " << processed << " frames";
    if (serial)
        std::cout << ", attitude misses " << attitude_misses << " (serial received "
                  << serial->receivedFrames() << ", failed queries " << serial->failedQueries()
                  << ", crc failures " << serial->crcFailures() << ", reconnects "
                  << serial->reconnects() << ")";
    std::cout << "\n";
    return 0;
}
