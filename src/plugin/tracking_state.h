#pragma once

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

#include "body_visibility.h"
#include "config.h"
#include "engine_angles.h"
#include "head_pose.h"
#include "one_euro_filter.h"
#include "rotation.h"
#include "shaping.h"
#include "tobii_source.h"

// Owns the single process-wide tracking pipeline: TobiiSource -> per-axis One Euro filters ->
// the current Rotation exposed to redscript. The per-frame Running-state hook runs on the game's
// main thread, but scripts call the RTTI-registered functions from job threads, so every
// public member locks mutex_.
class TrackingState {
public:
    static TrackingState& Instance();

    // configPath must be an absolute path to config.toml (anchored to the plugin DLL's own
    // directory by the caller); the game's CWD is bin/x64, so a relative path would never
    // resolve. Writes a default file if none exists (so upgrades never overwrite the user's
    // settings), and falls back to defaults if the file is invalid, returning the reason in
    // that case so the caller can log it.
    std::optional<std::string> Initialize(const std::filesystem::path& configPath);
    void Shutdown();
    void Update(double dtSeconds);

    EngineAngles Angles() const;

    // Smoothed positional lean for the camera, in centimeters (see lean.h for the axes).
    Lean CurrentLean() const;

    // Pitch-gated camera raise/pull-back that keeps V's body in frame when the head tilts
    // down, in meters along the camera's local up and forward axes.
    double BodyHeightOffsetMeters() const;
    double BodyDepthOffsetMeters() const;

    bool Enabled() const;

    // Enabling starts from a zero offset so a value left over from the last session never
    // snaps the camera; the filter eases in toward the live head pose.
    void SetEnabled(bool enabled);

    // Captures the current raw head pose (if one is available) as the new neutral
    // reference and zeroes the applied offset, so "recenter" means "this head position is
    // now straight ahead" rather than "snap back toward the tracker's own zero". If no
    // pose is available (tracker disconnected), only the offset is reset and the previous
    // reference is kept.
    void Recenter();

    // Live settings access for the in-game settings window, by "<profile>.<key>" (config.h).
    // SetSetting applies the value immediately and returns an error message, or an empty
    // string on success; SaveSettings writes the current values back to config.toml.
    std::optional<double> Setting(std::string_view key) const;
    std::string SetSetting(std::string_view key, double value);
    std::string SaveSettings();

    // Which settings profile drives tracking. The camera script switches it when the player
    // mounts or leaves a vehicle; filters are rebuilt from the current view so it eases into
    // the new profile's limits instead of jumping.
    Profile ActiveProfile() const;
    void SetActiveProfile(Profile profile);

private:
    void LogStatusPeriodically(double dtSeconds);
    void ResetFilters();
    void RebuildFilters();
    const ProfileConfig& Active() const { return config_.For(profile_); }

    double BodyVisibilityBlend() const {
        return ComputeBodyVisibilityBlend(current_.pitch, bodyVisibilityConfig_);
    }

    mutable std::mutex mutex_;
    TobiiSource source_;
    std::filesystem::path configPath_;
    TrackingConfig config_;
    BodyVisibilityConfig bodyVisibilityConfig_;
    HeadPose reference_;
    OneEuroFilter yawFilter_{1.0, 0.0};
    OneEuroFilter pitchFilter_{1.0, 0.0};
    OneEuroFilter rollFilter_{1.0, 0.0};
    OneEuroFilter sideFilter_{1.0, 0.0};
    OneEuroFilter upFilter_{1.0, 0.0};
    OneEuroFilter forwardFilter_{1.0, 0.0};
    Rotation current_;
    Lean currentLean_;
    double secondsWithoutPose_ = 0.0;
    double secondsSinceStatusLog_ = 0.0;
    Profile profile_ = Profile::OnFoot;
    bool enabled_ = false;
};
