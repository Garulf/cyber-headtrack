#include "tobii_source.h"

#include <chrono>
#include <condition_variable>
#include <string>

#include <tobii/tobii.h>
#include <tobii/tobii_streams.h>

#include "plugin_log.h"

namespace {

constexpr std::chrono::milliseconds kRetryInterval = std::chrono::seconds(3);
constexpr std::chrono::milliseconds kPollInterval{5};
constexpr std::chrono::milliseconds kGazeStaleAfter{100};
constexpr std::chrono::seconds kGazeSilenceWarning{5};

void FirstUrlReceiver(const char* url, void* user_data) {
    auto& first_url = *static_cast<std::string*>(user_data);
    if (first_url.empty() && url != nullptr) {
        first_url = url;
    }
}

bool SampleIsValid(const tobii_head_pose_t& pose) {
    return pose.position_validity == TOBII_VALIDITY_VALID &&
           pose.rotation_validity_xyz[0] == TOBII_VALIDITY_VALID &&
           pose.rotation_validity_xyz[1] == TOBII_VALIDITY_VALID &&
           pose.rotation_validity_xyz[2] == TOBII_VALIDITY_VALID;
}

// Sleeps for the given duration, waking early if a stop is requested. Returns false when the
// caller should exit.
bool SleepUnlessStopped(std::stop_token stop, std::chrono::milliseconds duration) {
    std::mutex mutex;
    std::condition_variable_any wake;
    std::unique_lock lock(mutex);
    wake.wait_for(lock, stop, duration, [] { return false; });
    return !stop.stop_requested();
}

}  // namespace

struct TobiiSourceCallbacks {
    static void OnHeadPose(const tobii_head_pose_t* pose, void* user_data) {
        auto& source = *static_cast<TobiiSource*>(user_data);
        const bool valid = pose != nullptr && SampleIsValid(*pose);
        if (valid != source.poseValid_) {
            source.poseValid_ = valid;
            PluginLog(valid ? "tobii: receiving head pose" : "tobii: head pose lost (no valid sample)");
        }
        if (!valid) {
            source.Publish(std::nullopt);
            return;
        }
        RawTobiiSample raw;
        raw.positionXyzMm[0] = pose->position_xyz[0];
        raw.positionXyzMm[1] = pose->position_xyz[1];
        raw.positionXyzMm[2] = pose->position_xyz[2];
        raw.rotationXyzRad[0] = pose->rotation_xyz[0];
        raw.rotationXyzRad[1] = pose->rotation_xyz[1];
        raw.rotationXyzRad[2] = pose->rotation_xyz[2];
        source.Publish(MapTobiiSample(raw));
    }

    static void OnGazePoint(const tobii_gaze_point_t* gaze, void* user_data) {
        auto& source = *static_cast<TobiiSource*>(user_data);
        const bool valid = gaze != nullptr && gaze->validity == TOBII_VALIDITY_VALID;
        if (valid != source.gazeValid_) {
            source.gazeValid_ = valid;
            PluginLog(valid ? "tobii: receiving gaze point" : "tobii: gaze point lost (no valid sample)");
        }
        if (!valid) {
            source.PublishGaze(std::nullopt);
            return;
        }
        source.gazeEverValid_ = true;
        source.PublishGaze(ScreenPoint{gaze->position_xy[0], gaze->position_xy[1]});
    }
};

TobiiSource::~TobiiSource() {
    Stop();
}

void TobiiSource::Start(std::string fallbackUrl) {
    if (worker_.joinable()) {
        return;
    }
    fallbackUrl_ = std::move(fallbackUrl);
    worker_ = std::jthread([this](std::stop_token stop) { Run(stop); });
}

void TobiiSource::Stop() {
    if (worker_.joinable()) {
        worker_.request_stop();
        worker_.join();
    }
    Publish(std::nullopt);
}

std::optional<HeadPose> TobiiSource::Latest() const {
    std::lock_guard lock(latestMutex_);
    return latest_;
}

void TobiiSource::Publish(std::optional<HeadPose> pose) {
    std::lock_guard lock(latestMutex_);
    latest_ = pose;
}

std::optional<ScreenPoint> TobiiSource::LatestGaze() const {
    std::lock_guard lock(latestMutex_);
    if (std::chrono::steady_clock::now() - latestGazeAt_ > kGazeStaleAfter) {
        return std::nullopt;
    }
    return latestGaze_;
}

void TobiiSource::PublishGaze(std::optional<ScreenPoint> point) {
    std::lock_guard lock(latestMutex_);
    latestGaze_ = point;
    latestGazeAt_ = std::chrono::steady_clock::now();
}

void TobiiSource::Run(std::stop_token stop) {
    PluginLog("tobii: worker started");
    while (!stop.stop_requested()) {
        if (device_ == nullptr && !Connect()) {
            if (!SleepUnlessStopped(stop, kRetryInterval)) break;
            continue;
        }

        const tobii_error_t error = tobii_device_process_callbacks(device_);
        if (error == TOBII_ERROR_NO_ERROR) {
            ReportSilentGaze();
            if (!SleepUnlessStopped(stop, kPollInterval)) break;
            continue;
        }

        ReportFailure("process_callbacks", error);
        Publish(std::nullopt);
        if (!SleepUnlessStopped(stop, kRetryInterval)) break;
        if (error == TOBII_ERROR_CONNECTION_FAILED) {
            ReportFailure("reconnect", tobii_device_reconnect(device_));
        } else {
            Disconnect();
        }
    }
    Disconnect();
}

void TobiiSource::ReportFailure(const std::string& step, int code) {
    const auto error = static_cast<tobii_error_t>(code);
    if (error == TOBII_ERROR_NO_ERROR) {
        lastFailure_.clear();
        return;
    }
    ReportProblem(step + " failed (" + tobii_error_message(error) + ")");
}

void TobiiSource::ReportProblem(const std::string& problem) {
    if (problem != lastFailure_) {
        PluginLog("tobii: %s; retrying every %llds", problem.c_str(),
                  static_cast<long long>(kRetryInterval.count() / 1000));
        lastFailure_ = problem;
    }
}

bool TobiiSource::Connect() {
    tobii_error_t error = tobii_api_create(&api_, nullptr, nullptr);
    if (error != TOBII_ERROR_NO_ERROR) {
        api_ = nullptr;
        ReportFailure("api_create", error);
        return false;
    }

    std::string url;
    error = tobii_enumerate_local_device_urls(api_, FirstUrlReceiver, &url);
    if (error != TOBII_ERROR_NO_ERROR) {
        ReportFailure("enumerate_devices", error);
        Disconnect();
        return false;
    }

    const bool usingFallback = url.empty();
    if (usingFallback) {
        if (fallbackUrl_.empty()) {
            ReportProblem("no tracker found");
            Disconnect();
            return false;
        }
        url = fallbackUrl_;
    }

    error = tobii_device_create(api_, url.c_str(), &device_);
    if (error != TOBII_ERROR_NO_ERROR) {
        device_ = nullptr;
        ReportFailure(usingFallback ? "no tracker found; device_create " + url : "device_create",
                      error);
        Disconnect();
        return false;
    }

    tobii_supported_t supported = TOBII_NOT_SUPPORTED;
    error = tobii_stream_supported(device_, TOBII_STREAM_HEAD_POSE, &supported);
    if (error == TOBII_ERROR_NO_ERROR && supported != TOBII_SUPPORTED) {
        error = TOBII_ERROR_NOT_SUPPORTED;
    }
    if (error == TOBII_ERROR_NO_ERROR) {
        error = tobii_head_pose_subscribe(device_, TobiiSourceCallbacks::OnHeadPose, this);
    }
    if (error != TOBII_ERROR_NO_ERROR) {
        ReportFailure("head_pose_subscribe", error);
        Disconnect();
        return false;
    }

    lastFailure_.clear();
    poseValid_ = false;
    SubscribeGaze();
    connectedAt_ = std::chrono::steady_clock::now();
    PluginLog("tobii: connected to %s%s, subscribed to head pose%s", url.c_str(),
              usingFallback ? " (fallback_url, none enumerated)" : "",
              gazeSubscribed_ ? " and gaze point" : "");
    return true;
}

// Gaze is optional: a tracker without it still drives head tracking.
void TobiiSource::SubscribeGaze() {
    gazeValid_ = false;
    gazeEverValid_ = false;
    gazeSilenceReported_ = false;
    tobii_supported_t supported = TOBII_NOT_SUPPORTED;
    tobii_error_t error = tobii_stream_supported(device_, TOBII_STREAM_GAZE_POINT, &supported);
    if (error == TOBII_ERROR_NO_ERROR && supported != TOBII_SUPPORTED) {
        error = TOBII_ERROR_NOT_SUPPORTED;
    }
    if (error == TOBII_ERROR_NO_ERROR) {
        error = tobii_gaze_point_subscribe(device_, TobiiSourceCallbacks::OnGazePoint, this);
    }
    gazeSubscribed_ = error == TOBII_ERROR_NO_ERROR;
    if (!gazeSubscribed_) {
        PluginLog("tobii: gaze point unavailable (%s); scan gaze off", tobii_error_message(error));
    }
}

void TobiiSource::ReportSilentGaze() {
    if (!gazeSubscribed_ || gazeEverValid_ || gazeSilenceReported_ ||
        std::chrono::steady_clock::now() - connectedAt_ < kGazeSilenceWarning) {
        return;
    }
    gazeSilenceReported_ = true;
    PluginLog("tobii: subscribed to gaze point but no valid samples after %llds (is a display "
              "area and calibration set up?)",
              static_cast<long long>(kGazeSilenceWarning.count()));
}

void TobiiSource::Disconnect() {
    if (device_ != nullptr) {
        if (gazeSubscribed_) {
            tobii_gaze_point_unsubscribe(device_);
            gazeSubscribed_ = false;
        }
        tobii_head_pose_unsubscribe(device_);
        tobii_device_destroy(device_);
        device_ = nullptr;
    }
    if (api_ != nullptr) {
        tobii_api_destroy(api_);
        api_ = nullptr;
    }
    Publish(std::nullopt);
    PublishGaze(std::nullopt);
}
