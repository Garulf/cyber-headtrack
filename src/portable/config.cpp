#include "config.h"

#include <cmath>
#include <cstddef>
#include <fstream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>

#include <toml++/toml.hpp>

namespace {

constexpr double kMaxScale = 10.0;
constexpr double kMaxLeanCm = 50.0;

std::string Utf8(const std::filesystem::path& path) {
    const std::u8string utf8 = path.u8string();
    return std::string(utf8.begin(), utf8.end());
}

template <typename Config>
struct Setting {
    std::string_view key;
    bool isFlag;
    double (*get)(const Config&);
    void (*set)(Config&, double);
};

constexpr Setting<ProfileConfig> kSettings[] = {
    {"smoothing_min_cutoff_hz", false,
     [](const ProfileConfig& c) { return c.smoothingMinCutoffHz; },
     [](ProfileConfig& c, double v) { c.smoothingMinCutoffHz = v; }},
    {"smoothing_beta", false,
     [](const ProfileConfig& c) { return c.smoothingBeta; },
     [](ProfileConfig& c, double v) { c.smoothingBeta = v; }},
    {"max_yaw_deg", false,
     [](const ProfileConfig& c) { return c.maxYawDeg; },
     [](ProfileConfig& c, double v) { c.maxYawDeg = v; }},
    {"max_pitch_deg", false,
     [](const ProfileConfig& c) { return c.maxPitchDeg; },
     [](ProfileConfig& c, double v) { c.maxPitchDeg = v; }},
    {"max_roll_deg", false,
     [](const ProfileConfig& c) { return c.maxRollDeg; },
     [](ProfileConfig& c, double v) { c.maxRollDeg = v; }},
    {"yaw_scale", false,
     [](const ProfileConfig& c) { return c.yawScale; },
     [](ProfileConfig& c, double v) { c.yawScale = v; }},
    {"pitch_scale", false,
     [](const ProfileConfig& c) { return c.pitchScale; },
     [](ProfileConfig& c, double v) { c.pitchScale = v; }},
    {"roll_scale", false,
     [](const ProfileConfig& c) { return c.rollScale; },
     [](ProfileConfig& c, double v) { c.rollScale = v; }},
    {"invert_yaw", true,
     [](const ProfileConfig& c) { return c.invert.yaw ? 1.0 : 0.0; },
     [](ProfileConfig& c, double v) { c.invert.yaw = v != 0.0; }},
    {"invert_pitch", true,
     [](const ProfileConfig& c) { return c.invert.pitch ? 1.0 : 0.0; },
     [](ProfileConfig& c, double v) { c.invert.pitch = v != 0.0; }},
    {"invert_roll", true,
     [](const ProfileConfig& c) { return c.invert.roll ? 1.0 : 0.0; },
     [](ProfileConfig& c, double v) { c.invert.roll = v != 0.0; }},
    {"lean_enabled", true,
     [](const ProfileConfig& c) { return c.leanEnabled ? 1.0 : 0.0; },
     [](ProfileConfig& c, double v) { c.leanEnabled = v != 0.0; }},
    {"lean_smoothing_min_cutoff_hz", false,
     [](const ProfileConfig& c) { return c.leanSmoothingMinCutoffHz; },
     [](ProfileConfig& c, double v) { c.leanSmoothingMinCutoffHz = v; }},
    {"lean_smoothing_beta", false,
     [](const ProfileConfig& c) { return c.leanSmoothingBeta; },
     [](ProfileConfig& c, double v) { c.leanSmoothingBeta = v; }},
    {"lean_scale", false,
     [](const ProfileConfig& c) { return c.leanScale; },
     [](ProfileConfig& c, double v) { c.leanScale = v; }},
    {"max_lean_side_cm", false,
     [](const ProfileConfig& c) { return c.maxLeanSideCm; },
     [](ProfileConfig& c, double v) { c.maxLeanSideCm = v; }},
    {"max_lean_up_cm", false,
     [](const ProfileConfig& c) { return c.maxLeanUpCm; },
     [](ProfileConfig& c, double v) { c.maxLeanUpCm = v; }},
    {"max_lean_forward_cm", false,
     [](const ProfileConfig& c) { return c.maxLeanForwardCm; },
     [](ProfileConfig& c, double v) { c.maxLeanForwardCm = v; }},
    {"invert_lean_side", true,
     [](const ProfileConfig& c) { return c.invertLean.side ? 1.0 : 0.0; },
     [](ProfileConfig& c, double v) { c.invertLean.side = v != 0.0; }},
    {"invert_lean_up", true,
     [](const ProfileConfig& c) { return c.invertLean.up ? 1.0 : 0.0; },
     [](ProfileConfig& c, double v) { c.invertLean.up = v != 0.0; }},
    {"ads_tracking_off", true,
     [](const ProfileConfig& c) { return c.adsTrackingOff ? 1.0 : 0.0; },
     [](ProfileConfig& c, double v) { c.adsTrackingOff = v != 0.0; }},
    {"ads_ease_seconds", false,
     [](const ProfileConfig& c) { return c.adsEaseSeconds; },
     [](ProfileConfig& c, double v) { c.adsEaseSeconds = v; }},
    {"ads_eye", false,
     [](const ProfileConfig& c) { return c.adsEye; },
     [](ProfileConfig& c, double v) { c.adsEye = v; }},
    {"ads_eye_offset_cm", false,
     [](const ProfileConfig& c) { return c.adsEyeOffsetCm; },
     [](ProfileConfig& c, double v) { c.adsEyeOffsetCm = v; }},
    {"invert_lean_forward", true,
     [](const ProfileConfig& c) { return c.invertLean.forward ? 1.0 : 0.0; },
     [](ProfileConfig& c, double v) { c.invertLean.forward = v != 0.0; }},
};

constexpr std::string_view kGazeSection = "gaze";

constexpr Setting<GazeConfig> kGazeSettings[] = {
    {"scan_enabled", true,
     [](const GazeConfig& c) { return c.scanEnabled ? 1.0 : 0.0; },
     [](GazeConfig& c, double v) { c.scanEnabled = v != 0.0; }},
    {"smoothing_min_cutoff_hz", false,
     [](const GazeConfig& c) { return c.smoothingMinCutoffHz; },
     [](GazeConfig& c, double v) { c.smoothingMinCutoffHz = v; }},
    {"smoothing_beta", false,
     [](const GazeConfig& c) { return c.smoothingBeta; },
     [](GazeConfig& c, double v) { c.smoothingBeta = v; }},
    {"sticky_lock", true,
     [](const GazeConfig& c) { return c.stickyLock ? 1.0 : 0.0; },
     [](GazeConfig& c, double v) { c.stickyLock = v != 0.0; }},
    {"lock_radius", false,
     [](const GazeConfig& c) { return c.lockRadius; },
     [](GazeConfig& c, double v) { c.lockRadius = v; }},
    {"switch_dwell_seconds", false,
     [](const GazeConfig& c) { return c.switchDwellSeconds; },
     [](GazeConfig& c, double v) { c.switchDwellSeconds = v; }},
};

struct ProfileSection {
    std::string_view name;
    Profile profile;
    ProfileConfig TrackingConfig::*member;
};

constexpr ProfileSection kSections[] = {
    {"on_foot", Profile::OnFoot, &TrackingConfig::onFoot},
    {"vehicle", Profile::Vehicle, &TrackingConfig::vehicle},
};

template <typename Config, size_t N>
const Setting<Config>* FindSetting(const Setting<Config> (&settings)[N], std::string_view key) {
    for (const Setting<Config>& setting : settings) {
        if (setting.key == key) {
            return &setting;
        }
    }
    return nullptr;
}

const ProfileSection* FindSection(std::string_view name) {
    for (const ProfileSection& section : kSections) {
        if (section.name == name) {
            return &section;
        }
    }
    return nullptr;
}

// Splits "<section>.<key>" at the first dot; both parts are empty if there is none.
std::pair<std::string_view, std::string_view> SplitName(std::string_view name) {
    const auto dot = name.find('.');
    if (dot == std::string_view::npos) {
        return {};
    }
    return {name.substr(0, dot), name.substr(dot + 1)};
}

// Resolves "<profile>.<key>"; either is null if unknown.
std::pair<const ProfileSection*, const Setting<ProfileConfig>*> Resolve(std::string_view name) {
    const auto [section, key] = SplitName(name);
    return {FindSection(section), FindSetting(kSettings, key)};
}

// Resolves "gaze.<key>"; null if the name is not a gaze setting.
const Setting<GazeConfig>* ResolveGaze(std::string_view name) {
    const auto [section, key] = SplitName(name);
    return section == kGazeSection ? FindSetting(kGazeSettings, key) : nullptr;
}

void Require(bool valid, std::string_view section, std::string_view key, const char* rule,
             double value) {
    if (!valid) {
        throw std::runtime_error(std::string(section) + "." + std::string(key) + " must be " +
                                 rule + ", got " + std::to_string(value));
    }
}

void Validate(const ProfileConfig& c, std::string_view section) {
    const auto positive = [](double v) { return std::isfinite(v) && v > 0.0; };
    const auto nonNegative = [](double v) { return std::isfinite(v) && v >= 0.0; };
    const auto angleLimit = [](double v) { return std::isfinite(v) && v >= 0.0 && v <= 180.0; };
    const auto scale = [](double v) { return std::isfinite(v) && v >= 0.0 && v <= kMaxScale; };

    Require(positive(c.smoothingMinCutoffHz), section, "smoothing_min_cutoff_hz", "> 0",
            c.smoothingMinCutoffHz);
    Require(nonNegative(c.smoothingBeta), section, "smoothing_beta", ">= 0", c.smoothingBeta);
    Require(angleLimit(c.maxYawDeg), section, "max_yaw_deg", "between 0 and 180", c.maxYawDeg);
    Require(angleLimit(c.maxPitchDeg), section, "max_pitch_deg", "between 0 and 180", c.maxPitchDeg);
    Require(angleLimit(c.maxRollDeg), section, "max_roll_deg", "between 0 and 180", c.maxRollDeg);
    Require(scale(c.yawScale), section, "yaw_scale", "between 0 and 10", c.yawScale);
    Require(scale(c.pitchScale), section, "pitch_scale", "between 0 and 10", c.pitchScale);
    Require(scale(c.rollScale), section, "roll_scale", "between 0 and 10", c.rollScale);

    const auto leanLimit = [](double v) { return std::isfinite(v) && v >= 0.0 && v <= kMaxLeanCm; };
    Require(positive(c.leanSmoothingMinCutoffHz), section, "lean_smoothing_min_cutoff_hz", "> 0",
            c.leanSmoothingMinCutoffHz);
    Require(nonNegative(c.leanSmoothingBeta), section, "lean_smoothing_beta", ">= 0",
            c.leanSmoothingBeta);
    Require(scale(c.leanScale), section, "lean_scale", "between 0 and 10", c.leanScale);
    Require(leanLimit(c.maxLeanSideCm), section, "max_lean_side_cm", "between 0 and 50",
            c.maxLeanSideCm);
    Require(leanLimit(c.maxLeanUpCm), section, "max_lean_up_cm", "between 0 and 50", c.maxLeanUpCm);
    Require(leanLimit(c.maxLeanForwardCm), section, "max_lean_forward_cm", "between 0 and 50",
            c.maxLeanForwardCm);
    Require(std::isfinite(c.adsEaseSeconds) && c.adsEaseSeconds >= 0.0 && c.adsEaseSeconds <= 2.0,
            section, "ads_ease_seconds", "between 0 and 2", c.adsEaseSeconds);
    Require(c.adsEye == -1.0 || c.adsEye == 0.0 || c.adsEye == 1.0, section, "ads_eye",
            "-1 (left), 0 (centre) or 1 (right)", c.adsEye);
    Require(std::isfinite(c.adsEyeOffsetCm) && c.adsEyeOffsetCm >= 0.0 && c.adsEyeOffsetCm <= 10.0,
            section, "ads_eye_offset_cm", "between 0 and 10", c.adsEyeOffsetCm);
}

void Validate(const GazeConfig& c) {
    Require(std::isfinite(c.smoothingMinCutoffHz) && c.smoothingMinCutoffHz > 0.0, kGazeSection,
            "smoothing_min_cutoff_hz", "> 0", c.smoothingMinCutoffHz);
    Require(std::isfinite(c.smoothingBeta) && c.smoothingBeta >= 0.0, kGazeSection,
            "smoothing_beta", ">= 0", c.smoothingBeta);
    Require(std::isfinite(c.lockRadius) && c.lockRadius >= 0.01 && c.lockRadius <= 0.3,
            kGazeSection, "lock_radius", "between 0.01 and 0.3", c.lockRadius);
    Require(std::isfinite(c.switchDwellSeconds) && c.switchDwellSeconds >= 0.0 &&
                c.switchDwellSeconds <= 2.0,
            kGazeSection, "switch_dwell_seconds", "between 0 and 2", c.switchDwellSeconds);
}

void Validate(const TrackingConfig& config) {
    for (const ProfileSection& section : kSections) {
        Validate(config.*section.member, section.name);
    }
    Validate(config.gaze);
}

template <typename Config, size_t N>
void ReadSettings(const toml::table& table, const Setting<Config> (&settings)[N], Config& config) {
    for (const Setting<Config>& setting : settings) {
        const auto node = table[setting.key];
        if (setting.isFlag) {
            setting.set(config, node.value_or(setting.get(config) != 0.0) ? 1.0 : 0.0);
        } else {
            setting.set(config, node.value_or(setting.get(config)));
        }
    }
}

template <typename Config, size_t N>
void WriteSettings(std::ostream& out, const Setting<Config> (&settings)[N], const Config& config) {
    for (const Setting<Config>& setting : settings) {
        out << setting.key << " = ";
        if (setting.isFlag) {
            out << (setting.get(config) != 0.0 ? "true" : "false");
        } else {
            out << setting.get(config);
        }
        out << "\n";
    }
}

void ReadTobiiSettings(const toml::table& table, TrackingConfig& config) {
    const auto node = table["tobii"]["fallback_url"];
    if (!node) {
        return;
    }
    const std::optional<std::string> url = node.value<std::string>();
    if (!url) {
        throw std::runtime_error("tobii.fallback_url must be a string");
    }
    config.tobiiFallbackUrl = *url;
}

}  // namespace

double AdsEyeOffsetCm(const ProfileConfig& profile) {
    return profile.adsEye * profile.adsEyeOffsetCm;
}

ProfileConfig DefaultProfile(Profile profile) {
    ProfileConfig defaults;
    defaults.leanEnabled = profile != Profile::Vehicle;
    return defaults;
}

TrackingConfig LoadConfig(const std::filesystem::path& path) {
    TrackingConfig config;
    if (!std::filesystem::exists(path)) {
        return config;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("failed to open config at " + Utf8(path));
    }

    toml::table table;
    try {
        table = toml::parse(in, Utf8(path));
    } catch (const toml::parse_error& err) {
        throw std::runtime_error("failed to parse config at " + Utf8(path) + ": " +
                                 std::string(err.description()));
    }

    for (const ProfileSection& section : kSections) {
        ProfileConfig& profile = config.*section.member;
        profile = DefaultProfile(section.profile);
        ReadSettings(table, kSettings, profile);
        if (const toml::table* overrides = table[section.name].as_table()) {
            ReadSettings(*overrides, kSettings, profile);
        }
    }
    if (const toml::table* gaze = table[kGazeSection].as_table()) {
        ReadSettings(*gaze, kGazeSettings, config.gaze);
    }
    ReadTobiiSettings(table, config);
    Validate(config);
    return config;
}

void SaveConfig(const std::filesystem::path& path, const TrackingConfig& config) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("failed to write config at " + Utf8(path));
    }
    out << "# cyber-headtrack tracking config\n";
    for (const ProfileSection& section : kSections) {
        const ProfileConfig& profile = config.*section.member;
        out << "\n[" << section.name << "]\n";
        WriteSettings(out, kSettings, profile);
    }
    out << "\n[" << kGazeSection << "]\n";
    WriteSettings(out, kGazeSettings, config.gaze);
    out << "\n[tobii]\n" << toml::table{{"fallback_url", config.tobiiFallbackUrl}} << "\n";
}

void SaveDefaultConfig(const std::filesystem::path& path) {
    SaveConfig(path, TrackingConfig{});
}

std::optional<double> GetConfigValue(const TrackingConfig& config, std::string_view name) {
    if (const auto* gazeSetting = ResolveGaze(name)) {
        return gazeSetting->get(config.gaze);
    }
    const auto [section, setting] = Resolve(name);
    if (section == nullptr || setting == nullptr) {
        return std::nullopt;
    }
    return setting->get(config.*section->member);
}

void SetConfigValue(TrackingConfig& config, std::string_view name, double value) {
    if (const auto* gazeSetting = ResolveGaze(name)) {
        GazeConfig updated = config.gaze;
        gazeSetting->set(updated, value);
        Validate(updated);
        config.gaze = updated;
        return;
    }
    const auto [section, setting] = Resolve(name);
    if (section == nullptr || setting == nullptr) {
        throw std::runtime_error("unknown setting " + std::string(name));
    }
    ProfileConfig updated = config.*section->member;
    setting->set(updated, value);
    Validate(updated, section->name);
    config.*section->member = updated;
}
