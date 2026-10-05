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
};

TobiiSource::~TobiiSource() {
    Stop();
}

void TobiiSource::Start() {
    if (worker_.joinable()) {
        return;
    }
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

void TobiiSource::Run(std::stop_token stop) {
    PluginLog("tobii: worker started");
    while (!stop.stop_requested()) {
        if (device_ == nullptr && !Connect()) {
            if (!SleepUnlessStopped(stop, kRetryInterval)) break;
            continue;
        }

        const tobii_error_t error = tobii_device_process_callbacks(device_);
        if (error == TOBII_ERROR_NO_ERROR) {
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

void TobiiSource::ReportFailure(const char* step, int code) {
    const auto error = static_cast<tobii_error_t>(code);
    if (error == TOBII_ERROR_NO_ERROR) {
        lastFailure_.clear();
        return;
    }
    std::string failure = std::string(step) + ": " + tobii_error_message(error);
    if (failure != lastFailure_) {
        PluginLog("tobii: %s failed (%s); retrying every %llds", step, tobii_error_message(error),
                  static_cast<long long>(kRetryInterval.count() / 1000));
        lastFailure_ = std::move(failure);
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
    if (error != TOBII_ERROR_NO_ERROR || url.empty()) {
        if (error == TOBII_ERROR_NO_ERROR && lastFailure_ != "no device") {
            PluginLog("tobii: no tracker found; retrying every %llds",
                      static_cast<long long>(kRetryInterval.count() / 1000));
            lastFailure_ = "no device";
        }
        ReportFailure("enumerate_devices", error);
        Disconnect();
        return false;
    }

    error = tobii_device_create(api_, url.c_str(), &device_);
    if (error != TOBII_ERROR_NO_ERROR) {
        device_ = nullptr;
        ReportFailure("device_create", error);
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
    PluginLog("tobii: connected to %s, subscribed to head pose", url.c_str());
    return true;
}

void TobiiSource::Disconnect() {
    if (device_ != nullptr) {
        tobii_head_pose_unsubscribe(device_);
        tobii_device_destroy(device_);
        device_ = nullptr;
    }
    if (api_ != nullptr) {
        tobii_api_destroy(api_);
        api_ = nullptr;
    }
    Publish(std::nullopt);
}
