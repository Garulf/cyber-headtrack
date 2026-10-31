#include "tracking_state.h"

#include <chrono>
#include <exception>

#include "plugin_log.h"

namespace {

constexpr double kPoseLossGraceSeconds = 0.5;
constexpr double kStatusLogIntervalSeconds = 2.0;
constexpr size_t kMaxScanCandidates = 64;
constexpr double kDefaultAspect = 16.0 / 9.0;

double SteadySeconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

}  // namespace

TrackingState& TrackingState::Instance() {
    static TrackingState instance;
    return instance;
}

std::optional<std::string> TrackingState::Initialize(const std::filesystem::path& configPath) {
    configPath_ = configPath;
    std::optional<std::string> configError;
    if (!configPath.empty() && !std::filesystem::exists(configPath)) {
        try {
            SaveDefaultConfig(configPath);
            PluginLog("wrote default config.toml");
        } catch (const std::exception& error) {
            PluginLog("could not write a default config.toml: %s", error.what());
        }
    }
    try {
        config_ = LoadConfig(configPath);
    } catch (const std::exception& error) {
        config_ = TrackingConfig{};
        configError = error.what();
    }
    RebuildFilters();
    ConfigureGaze();
    source_.Start(config_.tobiiFallbackUrl);
    return configError;
}

void TrackingState::Shutdown() {
    source_.Stop();
}

EngineAngles TrackingState::Angles() const {
    std::lock_guard lock(mutex_);
    return ToEngineAngles(current_, Active().invert);
}

Lean TrackingState::CurrentLean() const {
    std::lock_guard lock(mutex_);
    return currentLean_;
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
    ExpireStaleScanFrame();
    const bool gazeActive = enabled_ && scanActive_ && scanRedirectLive_ && config_.gaze.scanEnabled;
    gaze_.Update(enabled_ ? source_.LatestGaze() : std::nullopt, gazeActive, dtSeconds);
    if (!enabled_) {
        return;
    }

    Rotation target;
    Lean targetLean;
    if (auto pose = source_.Latest()) {
        secondsWithoutPose_ = 0.0;
        const HeadPose relative = SubtractReference(*pose, reference_);
        target = ShapeHeadRotation(relative, Active());
        targetLean = ShapeHeadLean(relative, Active());
    } else {
        secondsWithoutPose_ += dtSeconds;
        if (secondsWithoutPose_ < kPoseLossGraceSeconds) {
            return;
        }
    }

    current_.yaw = yawFilter_.Filter(target.yaw, dtSeconds);
    current_.pitch = pitchFilter_.Filter(target.pitch, dtSeconds);
    current_.roll = rollFilter_.Filter(target.roll, dtSeconds);
    currentLean_.sideCm = sideFilter_.Filter(targetLean.sideCm, dtSeconds);
    currentLean_.upCm = upFilter_.Filter(targetLean.upCm, dtSeconds);
    currentLean_.forwardCm = forwardFilter_.Filter(targetLean.forwardCm, dtSeconds);
    LogStatusPeriodically(dtSeconds);
}

void TrackingState::LogStatusPeriodically(double dtSeconds) {
    secondsSinceStatusLog_ += dtSeconds;
    if (secondsSinceStatusLog_ < kStatusLogIntervalSeconds) {
        return;
    }
    secondsSinceStatusLog_ = 0.0;
    const EngineAngles angles = ToEngineAngles(current_, Active().invert);
    const ScanPoint scan = ScanPointLocked();
    const std::string lockedId = scan.lockedId ? std::to_string(*scan.lockedId) : "none";
    PluginLog("status: profile=%s pose=%s yaw=%.1f pitch=%.1f roll=%.1f lean side=%.1f up=%.1f "
              "forward=%.1f cm gaze=%.3f,%.3f w=%.2f scan=%s redirect=%s lock=%s candidates=%zu "
              "ndc_per_tan=%.3f,%.3f",
              profile_ == Profile::Vehicle ? "vehicle" : "on_foot",
              source_.Latest() ? "yes" : "no", angles.yaw, angles.pitch, angles.roll,
              currentLean_.sideCm, currentLean_.upCm, currentLean_.forwardCm, scan.point.x,
              scan.point.y, scan.weight, scanActive_ ? "on" : "off",
              scanRedirectLive_ ? "live" : "none", lockedId.c_str(), scanCandidates_.size(),
              ndcPerTanX_, ndcPerTanY_);
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

std::optional<double> TrackingState::Setting(std::string_view key) const {
    std::lock_guard lock(mutex_);
    return GetConfigValue(config_, key);
}

std::string TrackingState::SetSetting(std::string_view key, double value) {
    std::lock_guard lock(mutex_);
    try {
        SetConfigValue(config_, key, value);
    } catch (const std::exception& error) {
        return error.what();
    }
    RebuildFilters();
    ConfigureGaze();
    return {};
}

std::string TrackingState::SaveSettings() {
    std::lock_guard lock(mutex_);
    try {
        SaveConfig(configPath_, config_);
    } catch (const std::exception& error) {
        PluginLog("saving settings failed: %s", error.what());
        return error.what();
    }
    PluginLog("settings saved to config.toml");
    return {};
}

Profile TrackingState::ActiveProfile() const {
    std::lock_guard lock(mutex_);
    return profile_;
}

void TrackingState::SetActiveProfile(Profile profile) {
    std::lock_guard lock(mutex_);
    if (profile == profile_) {
        return;
    }
    profile_ = profile;
    PluginLog("profile switched to %s", profile == Profile::Vehicle ? "vehicle" : "on_foot");
    RebuildFilters();
}

void TrackingState::RebuildFilters() {
    const ProfileConfig& active = Active();
    yawFilter_ = OneEuroFilter(active.smoothingMinCutoffHz, active.smoothingBeta);
    pitchFilter_ = OneEuroFilter(active.smoothingMinCutoffHz, active.smoothingBeta);
    rollFilter_ = OneEuroFilter(active.smoothingMinCutoffHz, active.smoothingBeta);
    yawFilter_.Reset(current_.yaw);
    pitchFilter_.Reset(current_.pitch);
    rollFilter_.Reset(current_.roll);
    sideFilter_ = OneEuroFilter(active.leanSmoothingMinCutoffHz, active.leanSmoothingBeta);
    upFilter_ = OneEuroFilter(active.leanSmoothingMinCutoffHz, active.leanSmoothingBeta);
    forwardFilter_ = OneEuroFilter(active.leanSmoothingMinCutoffHz, active.leanSmoothingBeta);
    sideFilter_.Reset(currentLean_.sideCm);
    upFilter_.Reset(currentLean_.upCm);
    forwardFilter_.Reset(currentLean_.forwardCm);
}

void TrackingState::ResetFilters() {
    current_ = Rotation{};
    currentLean_ = Lean{};
    yawFilter_.Reset(0.0);
    pitchFilter_.Reset(0.0);
    rollFilter_.Reset(0.0);
    sideFilter_.Reset(0.0);
    upFilter_.Reset(0.0);
    forwardFilter_.Reset(0.0);
}

void TrackingState::ConfigureGaze() {
    gaze_.Configure(config_.gaze);
    scanLock_.Configure(config_.gaze);
}

void TrackingState::SetScanFrame(bool active, double ndcPerTanX, double ndcPerTanY) {
    std::lock_guard lock(mutex_);
    scanFrames_.Beat(SteadySeconds());
    if (active != scanActive_) {
        PluginLog("scan gaze: scan mode %s", active ? "entered" : "left");
        scanLock_.Reset();
        scanLockResult_ = {};
    }
    scanActive_ = active;
    ndcPerTanX_ = ndcPerTanX;
    ndcPerTanY_ = ndcPerTanY;
}

void TrackingState::ClearScanCandidates() {
    std::lock_guard lock(mutex_);
    scanCandidates_.clear();
}

void TrackingState::AddScanCandidate(ScanCandidate candidate) {
    std::lock_guard lock(mutex_);
    if (scanCandidates_.size() < kMaxScanCandidates) {
        scanCandidates_.push_back(candidate);
    }
}

void TrackingState::SetScanRedirectLive(bool live) {
    std::lock_guard lock(mutex_);
    if (live != scanRedirectLive_) {
        PluginLog("scan gaze: targeting redirect %s", live ? "sees the player's rays" : "idle");
    }
    scanRedirectLive_ = live;
}

void TrackingState::ExpireStaleScanFrame() {
    if (scanActive_ && !scanFrames_.Alive(SteadySeconds())) {
        PluginLog("scan gaze: scan mode left (scan frames stopped)");
        scanActive_ = false;
        scanLock_.Reset();
        scanLockResult_ = {};
    }
}

LockResult TrackingState::UpdateScanLock(double dtSeconds) {
    std::lock_guard lock(mutex_);
    const bool projectionKnown = ndcPerTanX_ > 0.0 && ndcPerTanY_ > 0.0;
    const double aspect = projectionKnown ? ndcPerTanY_ / ndcPerTanX_ : kDefaultAspect;
    const GazeOutput gaze = gaze_.Current();
    scanLockResult_ = scanLock_.Update(gaze.point, gaze.weight, gaze.onScreen, scanCandidates_, aspect,
                                       dtSeconds);
    return scanLockResult_;
}

TrackingState::ScanPoint TrackingState::CurrentScanPoint() const {
    std::lock_guard lock(mutex_);
    return ScanPointLocked();
}

TrackingState::ScanPoint TrackingState::ScanPointLocked() const {
    const GazeOutput gaze = gaze_.Current();
    if (scanLockResult_.lockedId && gaze.weight >= 1.0) {
        return {scanLockResult_.point, gaze.weight, scanLockResult_.lockedId};
    }
    return {gaze.point, gaze.weight, std::nullopt};
}

std::optional<Vec3> TrackingState::ScanCameraDirection() const {
    std::lock_guard lock(mutex_);
    const ScanPoint scan = ScanPointLocked();
    const bool scanFresh = scanActive_ && scanFrames_.Alive(SteadySeconds());
    if (!scanFresh || scan.weight <= 0.0 || ndcPerTanX_ <= 0.0 || ndcPerTanY_ <= 0.0) {
        return std::nullopt;
    }
    return ScreenToCameraDirection(scan.point, ndcPerTanX_, ndcPerTanY_);
}
