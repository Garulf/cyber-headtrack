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
    Offset target;
    target.yaw = std::clamp(pose->yawDeg, -config_.maxYawDeg, config_.maxYawDeg);
    target.pitch = std::clamp(pose->pitchDeg, -config_.maxPitchDeg, config_.maxPitchDeg);
    target.roll = pose->rollDeg;
    current_ = AdaptiveFilterStep(current_, target, dtSeconds, config_.rotSensitivity,
                                  config_.leanSensitivity, config_.rotDeadzoneDeg,
                                  config_.leanDeadzoneCm);
}
