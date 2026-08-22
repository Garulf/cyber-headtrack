#include "tobii_source.h"

#include <chrono>
#include <string>

#include <tobii/tobii.h>
#include <tobii/tobii_streams.h>

namespace {

// Low-frequency retry interval for both "never connected" Start() retries and mid-session
// tobii_device_reconnect attempts, so a missing tracker is not polled in a tight loop.
constexpr auto kRetryInterval = std::chrono::seconds(3);

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

}  // namespace

struct TobiiSourceCallbacks {
    static void OnHeadPose(const tobii_head_pose_t* pose, void* user_data) {
        auto& source = *static_cast<TobiiSource*>(user_data);
        if (pose == nullptr || !SampleIsValid(*pose)) {
            source.latest_.reset();
            return;
        }
        RawTobiiSample raw;
        raw.positionXyzMm[0] = pose->position_xyz[0];
        raw.positionXyzMm[1] = pose->position_xyz[1];
        raw.positionXyzMm[2] = pose->position_xyz[2];
        raw.rotationXyzRad[0] = pose->rotation_xyz[0];
        raw.rotationXyzRad[1] = pose->rotation_xyz[1];
        raw.rotationXyzRad[2] = pose->rotation_xyz[2];
        source.latest_ = MapTobiiSample(raw);
    }
};

TobiiSource::~TobiiSource() {
    Stop();
}

bool TobiiSource::RetryDue() {
    const auto now = std::chrono::steady_clock::now();
    if (now < nextRetry_) {
        return false;
    }
    nextRetry_ = now + kRetryInterval;
    return true;
}

bool TobiiSource::Start() {
    if (subscribed_) {
        return true;
    }
    Stop();
    nextRetry_ = std::chrono::steady_clock::now() + kRetryInterval;

    if (tobii_api_create(&api_, nullptr, nullptr) != TOBII_ERROR_NO_ERROR) {
        api_ = nullptr;
        return false;
    }

    std::string url;
    if (tobii_enumerate_local_device_urls(api_, FirstUrlReceiver, &url) != TOBII_ERROR_NO_ERROR ||
        url.empty()) {
        Stop();
        return false;
    }

    if (tobii_device_create(api_, url.c_str(), &device_) != TOBII_ERROR_NO_ERROR) {
        device_ = nullptr;
        Stop();
        return false;
    }

    tobii_supported_t supported = TOBII_NOT_SUPPORTED;
    if (tobii_stream_supported(device_, TOBII_STREAM_HEAD_POSE, &supported) !=
            TOBII_ERROR_NO_ERROR ||
        supported != TOBII_SUPPORTED) {
        Stop();
        return false;
    }

    if (tobii_head_pose_subscribe(device_, TobiiSourceCallbacks::OnHeadPose, this) !=
        TOBII_ERROR_NO_ERROR) {
        Stop();
        return false;
    }

    subscribed_ = true;
    nextRetry_ = {};
    return true;
}

void TobiiSource::Stop() {
    if (device_ != nullptr) {
        if (subscribed_) {
            tobii_head_pose_unsubscribe(device_);
        }
        tobii_device_destroy(device_);
        device_ = nullptr;
    }
    if (api_ != nullptr) {
        tobii_api_destroy(api_);
        api_ = nullptr;
    }
    subscribed_ = false;
    latest_.reset();
}

void TobiiSource::Poll() {
    if (!subscribed_) {
        if (RetryDue()) {
            Start();
        }
        return;
    }
    const tobii_error_t error = tobii_device_process_callbacks(device_);
    if (error == TOBII_ERROR_NO_ERROR) {
        nextRetry_ = {};
        return;
    }
    latest_.reset();
    if (error == TOBII_ERROR_CONNECTION_FAILED && RetryDue()) {
        tobii_device_reconnect(device_);
    }
}

std::optional<HeadPose> TobiiSource::Latest() const {
    return latest_;
}
