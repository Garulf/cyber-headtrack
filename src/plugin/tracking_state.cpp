#include "tracking_state.h"

#include <algorithm>
#include <exception>

TrackingState& TrackingState::Instance() {
    static TrackingState instance;
    return instance;
}

void TrackingState::Initialize() {
    try {
        config_ = LoadConfig("red4ext/plugins/CyberHeadtrack/config.toml");
    } catch (const std::exception&) {
        config_ = TrackingConfig{};
    }
    source_.Start();
}

void TrackingState::Shutdown() {
    source_.Stop();
}

void TrackingState::Update(double dtSeconds) {
    source_.Poll();
    if (!enabled_) {
        return;
    }
    auto pose = source_.Latest();
    if (!pose) {
        return;
    }
    const HeadPose relative = SubtractReference(*pose, reference_);
    Offset target;
    target.yaw = std::clamp(relative.yawDeg, -config_.maxYawDeg, config_.maxYawDeg);
    target.pitch = std::clamp(relative.pitchDeg, -config_.maxPitchDeg, config_.maxPitchDeg);
    target.roll = relative.rollDeg;
    current_ = AdaptiveFilterStep(current_, target, dtSeconds, config_.rotSensitivity,
                                  config_.leanSensitivity, config_.rotDeadzoneDeg,
                                  config_.leanDeadzoneCm);
}

void TrackingState::Recenter() {
    if (auto pose = source_.Latest()) {
        reference_ = *pose;
    }
    current_ = Offset{};
}
