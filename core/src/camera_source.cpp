#include "autoaim/camera_source.hpp"

#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <stdexcept>

#include "autoaim/pose_buffer.hpp"

namespace autoaim {

namespace {

double steadySeconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

cv::VideoCapture& capOf(void* p) { return *static_cast<cv::VideoCapture*>(p); }

}  // namespace

// ---------------------------------------------------------------- VideoFile

VideoFileSource::VideoFileSource(const std::string& path) : path_(path) {
    auto* cap = new cv::VideoCapture(path);
    if (!cap->isOpened()) {
        delete cap;
        throw std::runtime_error("cannot open video " + path);
    }
    cap_ = cap;
    fps_ = cap->get(cv::CAP_PROP_FPS);
    if (!(fps_ > 0) || fps_ > 1000) fps_ = 25.0;
}

VideoFileSource::~VideoFileSource() { delete static_cast<cv::VideoCapture*>(cap_); }

bool VideoFileSource::grab(SourceFrame& out) {
    cv::Mat frame;
    if (!capOf(cap_).read(frame)) return false;
    out.image = frame;
    out.exposure_stamp = index_ / fps_;
    out.index = index_++;
    out.hw_stamp = false;
    return true;
}

// ------------------------------------------------------------------- Device

DeviceSource::DeviceSource(int index, double nominal_fps, int width, int height) {
    auto* cap = new cv::VideoCapture(index, cv::CAP_V4L2);
    if (!cap->isOpened()) {
        delete cap;
        throw std::runtime_error("cannot open camera device");
    }
    if (width > 0) cap->set(cv::CAP_PROP_FRAME_WIDTH, width);
    if (height > 0) cap->set(cv::CAP_PROP_FRAME_HEIGHT, height);
    cap_ = cap;
    fps_ = nominal_fps > 0 ? nominal_fps : cap->get(cv::CAP_PROP_FPS);
    if (!(fps_ > 0) || fps_ > 1000) fps_ = 30.0;
}

DeviceSource::~DeviceSource() { delete static_cast<cv::VideoCapture*>(cap_); }

bool DeviceSource::grab(SourceFrame& out) {
    cv::Mat frame;
    if (!capOf(cap_).read(frame)) return false;
    // Receipt time minus half a frame period approximates the exposure
    // centre of a free-running UVC camera. Jitter is milliseconds, not the
    // 0.3 ms of the G-02 budget - hw_stamp stays false so nothing downstream
    // can mistake this for an aligned stamp.
    out.image = frame;
    out.exposure_stamp = steadySeconds() - 0.5 / fps_;
    out.index = index_++;
    out.hw_stamp = false;
    return true;
}

// ---------------------------------------------------------------------- MVS
#ifdef AUTOAIM_WITH_MVS

#include "MvCameraControl.h"

namespace {

std::string mvError(const char* what, int code) {
    char buf[160];
    std::snprintf(buf, sizeof(buf), "MVS %s failed: 0x%08x", what, static_cast<unsigned>(code));
    return buf;
}

}  // namespace

class MvsSource::Impl {
public:
    void* handle = nullptr;
    ClockAligner aligner;
    double tick_freq = 0;              // set from the camera (or a verified override) in open()
    double exposure_us_actual = 0;     // read back from the camera in open()
    int nodata_streak = 0;             // consecutive grab timeouts

    ~Impl() { close(); }

    void close() {
        if (!handle) return;
        MV_CC_StopGrabbing(handle);
        MV_CC_CloseDevice(handle);
        MV_CC_DestroyHandle(handle);
        handle = nullptr;
    }

    void open(const MvsConfig& cfg) {
        MV_CC_DEVICE_INFO_LIST list;
        std::memset(&list, 0, sizeof(list));
        int ret = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &list);
        if (ret != MV_OK) throw std::runtime_error(mvError("EnumDevices", ret));
        if (cfg.device_index < 0 || static_cast<unsigned>(cfg.device_index) >= list.nDeviceNum)
            throw std::runtime_error("MVS device index out of range");
        ret = MV_CC_CreateHandle(&handle, list.pDeviceInfo[cfg.device_index]);
        if (ret != MV_OK) throw std::runtime_error(mvError("CreateHandle", ret));
        ret = MV_CC_OpenDevice(handle, MV_ACCESS_Exclusive, 0);
        if (ret != MV_OK) throw std::runtime_error(mvError("OpenDevice", ret));

        // GigE: use the recommended packet size when the camera offers one
        // (an optimisation hint - the only set that stays best-effort).
        const int packet = MV_CC_GetOptimalPacketSize(handle);
        if (packet > 0) MV_CC_SetIntValueEx(handle, "GevSCPSPacketSize", packet);

        // Every other parameter is load-bearing (the exposure centre math
        // below assumes the configured exposure): a failed set must stop
        // startup, not silently diverge from what the software believes.
        auto set = [](const char* what, int r) {
            if (r != MV_OK) throw std::runtime_error(mvError(what, r));
        };
        if (cfg.hardware_trigger) {
            set("SetTriggerMode", MV_CC_SetEnumValueByString(handle, "TriggerMode", "On"));
            set("SetTriggerSource", MV_CC_SetEnumValueByString(handle, "TriggerSource", "Line0"));
        } else {
            set("SetTriggerMode", MV_CC_SetEnumValueByString(handle, "TriggerMode", "Off"));
        }
        set("SetExposureAuto", MV_CC_SetEnumValueByString(handle, "ExposureAuto", "Off"));
        set("SetExposureTime", MV_CC_SetFloatValue(handle, "ExposureTime", static_cast<float>(cfg.exposure_us)));
        set("SetFrameRateEnable", MV_CC_SetBoolValue(handle, "AcquisitionFrameRateEnable", true));
        set("SetFrameRate", MV_CC_SetFloatValue(handle, "AcquisitionFrameRate", static_cast<float>(cfg.frame_rate)));

        // The value the camera actually runs is the one the stamp math
        // must use (cameras quantise exposure to their own steps).
        MVCC_FLOATVALUE expv;
        std::memset(&expv, 0, sizeof(expv));
        ret = MV_CC_GetFloatValue(handle, "ExposureTime", &expv);
        if (ret != MV_OK) throw std::runtime_error(mvError("GetExposureTime(readback)", ret));
        exposure_us_actual = expv.fCurValue;

        // A wrong tick frequency is worse than a wrong offset: the time
        // SCALE breaks and the error grows for as long as the camera runs.
        // Never guess it - refuse to start without a reported value or an
        // explicitly configured, model-verified override.
        MVCC_INTVALUE_EX tick;
        std::memset(&tick, 0, sizeof(tick));
        if (MV_CC_GetIntValueEx(handle, "TimestampTickFrequency", &tick) == MV_OK && tick.nCurValue > 0) {
            tick_freq = static_cast<double>(tick.nCurValue);
        } else if (cfg.tick_freq_override > 0) {
            tick_freq = cfg.tick_freq_override;
        } else {
            throw std::runtime_error(
                "MVS: TimestampTickFrequency unavailable and no tick_freq_override configured; "
                "hardware timestamps would be unscaled");
        }
        std::fprintf(stderr, "MvsSource: tick frequency %.0f Hz, exposure %.0f us read back (configured %.0f us)\n",
                     tick_freq, exposure_us_actual, cfg.exposure_us);

        ret = MV_CC_StartGrabbing(handle);
        if (ret != MV_OK) throw std::runtime_error(mvError("StartGrabbing", ret));
    }
};

MvsSource::MvsSource(const MvsConfig& cfg) : cfg_(cfg), impl_(new Impl) {
    try {
        impl_->open(cfg);
    } catch (...) {
        impl_->close();
        throw;
    }
}

MvsSource::~MvsSource() = default;

bool MvsSource::grab(SourceFrame& out) {
    MV_FRAME_OUT frame;
    std::memset(&frame, 0, sizeof(frame));
    // A grab timeout (MV_E_NODATA: GigE hiccup, a late hardware trigger)
    // is a skipped frame, not the end of the stream - callers run
    // `while (grab(...))`, so returning false here would kill the whole
    // process on one transient stall.  Retry a bounded streak; only hard
    // errors (and a long, unbroken stall) end the stream.
    int ret = MV_OK;
    for (;;) {
        ret = MV_CC_GetImageBuffer(impl_->handle, &frame, 1000);
        if (ret == MV_OK) {
            impl_->nodata_streak = 0;
            break;
        }
        if (ret == static_cast<int>(MV_E_NODATA) && ++impl_->nodata_streak <= 10) {
            if (impl_->nodata_streak == 1 || impl_->nodata_streak == 10)
                std::fprintf(stderr, "MvsSource: grab timeout (streak %d)\n", impl_->nodata_streak);
            continue;
        }
        return false;
    }

    const MV_FRAME_OUT_INFO_EX& info = frame.stFrameInfo;
    const std::uint64_t ticks =
        (static_cast<std::uint64_t>(info.nTimeStampHigh) << 32) | info.nTimeStampLow;
    const double device_start = static_cast<double>(ticks) / impl_->tick_freq;
    impl_->aligner.addPair(device_start, steadySeconds());
    // Device stamp = start of exposure; the pose that belongs to the image
    // is the one at its centre (with the exposure the camera READ BACK).
    out.exposure_stamp =
        impl_->aligner.toLocal(device_start) + impl_->exposure_us_actual * 0.5e-6 - cfg_.stamp_offset_us * 1e-6;
    out.index = index_++;
    out.hw_stamp = true;

    const int w = info.nWidth, h = info.nHeight;
    const cv::Mat raw(h, w, CV_8UC1, frame.pBufAddr);
    bool ok = true;
    switch (info.enPixelType) {
        case PixelType_Gvsp_Mono8:
            out.image = raw.clone();
            break;
        case PixelType_Gvsp_BayerRG8:
            cv::cvtColor(raw, out.image, cv::COLOR_BayerRG2BGR);
            break;
        case PixelType_Gvsp_RGB8_Packed:
            cv::cvtColor(cv::Mat(h, w, CV_8UC3, frame.pBufAddr), out.image, cv::COLOR_RGB2BGR);
            break;
        case PixelType_Gvsp_BGR8_Packed:
            out.image = cv::Mat(h, w, CV_8UC3, frame.pBufAddr).clone();
            break;
        case PixelType_Gvsp_YUV422_Packed:
            cv::cvtColor(cv::Mat(h, w, CV_8UC2, frame.pBufAddr), out.image, cv::COLOR_YUV2BGR_Y422);
            break;
        default:
            ok = false;  // add a ConvertPixelType path if the camera is set to another format
            break;
    }
    MV_CC_FreeImageBuffer(impl_->handle, &frame);
    return ok;
}

#endif  // AUTOAIM_WITH_MVS

}  // namespace autoaim
