#include "config.h"

#include <cmath>
#include <fstream>
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

struct Setting {
    std::string_view key;
    bool isFlag;
    double (*get)(const ProfileConfig&);
    void (*set)(ProfileConfig&, double);
};

constexpr Setting kSettings[] = {
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

struct ProfileSection {
    std::string_view name;
    Profile profile;
    ProfileConfig TrackingConfig::*member;
};

constexpr ProfileSection kSections[] = {
    {"on_foot", Profile::OnFoot, &TrackingConfig::onFoot},
    {"vehicle", Profile::Vehicle, &TrackingConfig::vehicle},
};

const Setting* FindSetting(std::string_view key) {
    for (const Setting& setting : kSettings) {
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

// Splits "<profile>.<key>" into its section and setting; either is null if unknown.
std::pair<const ProfileSection*, const Setting*> Resolve(std::string_view name) {
    const auto dot = name.find('.');
    if (dot == std::string_view::npos) {
        return {nullptr, nullptr};
    }
    return {FindSection(name.substr(0, dot)), FindSetting(name.substr(dot + 1))};
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

void Validate(const TrackingConfig& config) {
    for (const ProfileSection& section : kSections) {
        Validate(config.*section.member, section.name);
    }
}

void ReadSettings(const toml::table& table, ProfileConfig& profile) {
    for (const Setting& setting : kSettings) {
        const auto node = table[setting.key];
        if (setting.isFlag) {
            setting.set(profile, node.value_or(setting.get(profile) != 0.0) ? 1.0 : 0.0);
        } else {
            setting.set(profile, node.value_or(setting.get(profile)));
        }
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
        ReadSettings(table, profile);
        if (const toml::table* overrides = table[section.name].as_table()) {
            ReadSettings(*overrides, profile);
        }
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
        for (const Setting& setting : kSettings) {
            out << setting.key << " = ";
            if (setting.isFlag) {
                out << (setting.get(profile) != 0.0 ? "true" : "false");
            } else {
                out << setting.get(profile);
            }
            out << "\n";
        }
    }
    out << "\n[tobii]\n" << toml::table{{"fallback_url", config.tobiiFallbackUrl}} << "\n";
}

void SaveDefaultConfig(const std::filesystem::path& path) {
    SaveConfig(path, TrackingConfig{});
}

std::optional<double> GetConfigValue(const TrackingConfig& config, std::string_view name) {
    const auto [section, setting] = Resolve(name);
    if (section == nullptr || setting == nullptr) {
        return std::nullopt;
    }
    return setting->get(config.*section->member);
}

void SetConfigValue(TrackingConfig& config, std::string_view name, double value) {
    const auto [section, setting] = Resolve(name);
    if (section == nullptr || setting == nullptr) {
        throw std::runtime_error("unknown setting " + std::string(name));
    }
    ProfileConfig updated = config.*section->member;
    setting->set(updated, value);
    Validate(updated, section->name);
    config.*section->member = updated;
}
