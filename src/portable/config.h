#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "engine_angles.h"
#include "lean.h"

// Tracking settings for one context (on foot, or seated in a vehicle).
struct ProfileConfig {
    double smoothingMinCutoffHz = 1.0;
    double smoothingBeta = 0.1;
    double maxYawDeg = 45.0;
    double maxPitchDeg = 45.0;
    double maxRollDeg = 45.0;
    double yawScale = 1.0;
    double pitchScale = 1.0;
    double rollScale = 1.0;
    AxisInversion invert;

    bool leanEnabled = true;
    double leanSmoothingMinCutoffHz = 0.5;
    double leanSmoothingBeta = 0.05;
    double leanScale = 1.0;
    double maxLeanSideCm = 20.0;
    double maxLeanUpCm = 10.0;
    double maxLeanForwardCm = 15.0;
    LeanInversion invertLean;

    // While aiming down sights the view shifts sideways toward the dominant eye: -1 left,
    // 0 centre, 1 right, by adsEyeOffsetCm (about half the distance between the eyes).
    double adsEye = 1.0;
    double adsEyeOffsetCm = 3.2;
    // Ease head rotation and lean out while aiming down sights (the eye shift is then unused).
    bool adsTrackingOff = false;
    // How long the ADS transitions take (0 = instant).
    double adsEaseSeconds = 0.25;
};

// Signed sideways shift for aiming down sights, in centimeters (positive = right).
double AdsEyeOffsetCm(const ProfileConfig& profile);

enum class Profile { OnFoot, Vehicle };

// Built-in defaults per profile: identical except that lean starts off in vehicles, where
// moving the camera easily pushes it through the roof or windshield.
ProfileConfig DefaultProfile(Profile profile);

// Eye tracking in scan mode: the scanner reticle follows the gaze point and locks onto the
// target under it. Shared by both profiles.
struct GazeConfig {
    bool scanEnabled = true;
    double smoothingMinCutoffHz = 0.5;
    double smoothingBeta = 1.0;
    // Keep the locked target while the gaze rests on empty space or the HUD.
    bool stickyLock = true;
    // How close the gaze must come to a target to lock it, in screen heights.
    double lockRadius = 0.06;
    // How long the gaze must rest on another target before the lock moves to it.
    double switchDwellSeconds = 0.2;
};

struct TrackingConfig {
    ProfileConfig onFoot = DefaultProfile(Profile::OnFoot);
    ProfileConfig vehicle = DefaultProfile(Profile::Vehicle);
    GazeConfig gaze;

    // Tracker URL to open when Stream Engine enumerates none, which happens under Wine and
    // Proton: they can't list the named-pipe root that Stream Engine scans for PRP trackers,
    // but can open a pipe by name. Empty disables the fallback.
    std::string tobiiFallbackUrl = "tobii-prp://ET5-LINUX";

    const ProfileConfig& For(Profile profile) const {
        return profile == Profile::Vehicle ? vehicle : onFoot;
    }
};

// Loads a TrackingConfig from a TOML file at path. Each profile starts from its
// DefaultProfile; top-level keys then set both profiles, and keys in an [on_foot] or
// [vehicle] table override that profile; [gaze] holds the scan-mode eye tracking settings and
// [tobii] holds fallback_url. Returns defaults if the file
// does not exist. Throws std::runtime_error with a descriptive message if the file exists but
// fails to parse or holds an out-of-range value.
TrackingConfig LoadConfig(const std::filesystem::path& path);

// Writes config to path as [on_foot], [vehicle], [gaze] and [tobii] tables, replacing the file. Throws
// std::runtime_error if the file cannot be opened for writing.
void SaveConfig(const std::filesystem::path& path, const TrackingConfig& config);

// Writes a default config file at path, for first-run bootstrapping.
void SaveDefaultConfig(const std::filesystem::path& path);

// Reads or writes one setting by "<section>.<key>", e.g. "vehicle.yaw_scale", where section
// is on_foot, vehicle or gaze and key is the TOML name. Flags such as "invert_roll" read as
// 1.0 / 0.0 and treat any non-zero value as true. GetConfigValue returns std::nullopt for an
// unknown name; SetConfigValue throws std::runtime_error for an unknown name or an
// out-of-range value and leaves config unchanged in that case.
std::optional<double> GetConfigValue(const TrackingConfig& config, std::string_view name);
void SetConfigValue(TrackingConfig& config, std::string_view name, double value);
