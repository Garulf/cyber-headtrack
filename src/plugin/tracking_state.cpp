#include "tracking_state.h"

#include <algorithm>
#include <exception>

#include "plugin_log.h"

namespace {

constexpr double kPoseLossGraceSeconds = 0.5;
constexpr double kStatusLogIntervalSeconds = 2.0;

}  // namespace

TrackingState& TrackingState::Instance() {
    static TrackingState instance;
    return instance;
}

std::optional<std::string> TrackingState::Initialize(const std::filesystem::path& configPath) {
    std::optional<std::string> configError;
    try {
        config_ = LoadConfig(configPath);
    } catch (const std::exception& error) {
        config_ = TrackingConfig{};
        configError = error.what();
    }
    yawFilter_ = OneEuroFilter(config_.smoothingMinCutoffHz, config_.smoothingBeta);
    pitchFilter_ = OneEuroFilter(config_.smoothingMinCutoffHz, config_.smoothingBeta);
    rollFilter_ = OneEuroFilter(config_.smoothingMinCutoffHz, config_.smoothingBeta);
    ResetFilters();
    source_.Start();
    return configError;
}

void TrackingState::Shutdown() {
    source_.Stop();
}

EngineAngles TrackingState::Angles() const {
    std::lock_guard lock(mutex_);
    return ToEngineAngles(current_, config_.invert);
}

double TrackingState::BodyHeightOffsetMeters() const {
    std::lock_guard lock(mutex_);
    return BodyVisibilityBlend() * bodyVisibilityConfig_.maxHeightOffsetCm / 100.0;
}

double TrackingState::BodyDepthOffsetMeters() const {
    std::lock_guard lock(mutex_);
    return BodyVisibilityBlend() * bodyVisibilityConfig_.maxDepthOffsetCm / 100.0;
}

bool TrackingState::Enabled() const {
    std::lock_guard lock(mutex_);
    return enabled_;
}

void TrackingState::SetEnabled(bool enabled) {
    std::lock_guard lock(mutex_);
    PluginLog("tracking %s", enabled ? "enabled" : "disabled");
    if (enabled && !enabled_) {
        ResetFilters();
        secondsWithoutPose_ = 0.0;
    }
    enabled_ = enabled;
}

void TrackingState::Update(double dtSeconds) {
    std::lock_guard lock(mutex_);
    if (!enabled_) {
        return;
    }

    Rotation target;
    if (auto pose = source_.Latest()) {
        secondsWithoutPose_ = 0.0;
        const HeadPose relative = SubtractReference(*pose, reference_);
        target.yaw = std::clamp(relative.yawDeg, -config_.maxYawDeg, config_.maxYawDeg);
        target.pitch = std::clamp(relative.pitchDeg, -config_.maxPitchDeg, config_.maxPitchDeg);
        target.roll = relative.rollDeg;
    } else {
        secondsWithoutPose_ += dtSeconds;
        if (secondsWithoutPose_ < kPoseLossGraceSeconds) {
            return;
        }
    }

    current_.yaw = yawFilter_.Filter(target.yaw, dtSeconds);
    current_.pitch = pitchFilter_.Filter(target.pitch, dtSeconds);
    current_.roll = rollFilter_.Filter(target.roll, dtSeconds);
    LogStatusPeriodically(dtSeconds);
}

void TrackingState::LogStatusPeriodically(double dtSeconds) {
    secondsSinceStatusLog_ += dtSeconds;
    if (secondsSinceStatusLog_ < kStatusLogIntervalSeconds) {
        return;
    }
    secondsSinceStatusLog_ = 0.0;
    const EngineAngles angles = ToEngineAngles(current_, config_.invert);
    PluginLog("status: pose=%s yaw=%.1f pitch=%.1f roll=%.1f",
              source_.Latest() ? "yes" : "no", angles.yaw, angles.pitch, angles.roll);
}

void TrackingState::Recenter() {
    std::lock_guard lock(mutex_);
    auto pose = source_.Latest();
    PluginLog("recenter (%s)", pose ? "captured current head pose" : "no pose; offset reset only");
    if (pose) {
        reference_ = *pose;
    }
    ResetFilters();
}

void TrackingState::ResetFilters() {
    current_ = Rotation{};
    yawFilter_.Reset(0.0);
    pitchFilter_.Reset(0.0);
    rollFilter_.Reset(0.0);
}
