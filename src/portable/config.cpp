#include "config.h"

#include <cmath>
#include <fstream>
#include <stdexcept>
#include <string>

#include <toml++/toml.hpp>

namespace {

std::string Utf8(const std::filesystem::path& path) {
    const std::u8string utf8 = path.u8string();
    return std::string(utf8.begin(), utf8.end());
}

void Require(bool valid, const char* key, const char* rule, double value) {
    if (!valid) {
        throw std::runtime_error(std::string(key) + " must be " + rule + ", got " +
                                 std::to_string(value));
    }
}

void Validate(const TrackingConfig& config) {
    const auto positive = [](double v) { return std::isfinite(v) && v > 0.0; };
    const auto nonNegative = [](double v) { return std::isfinite(v) && v >= 0.0; };
    const auto angleLimit = [](double v) { return std::isfinite(v) && v >= 0.0 && v <= 180.0; };

    Require(positive(config.smoothingMinCutoffHz), "smoothing_min_cutoff_hz", "> 0",
            config.smoothingMinCutoffHz);
    Require(nonNegative(config.smoothingBeta), "smoothing_beta", ">= 0", config.smoothingBeta);
    Require(angleLimit(config.maxYawDeg), "max_yaw_deg", "between 0 and 180", config.maxYawDeg);
    Require(angleLimit(config.maxPitchDeg), "max_pitch_deg", "between 0 and 180", config.maxPitchDeg);
}

}  // namespace

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

    config.smoothingMinCutoffHz =
        table["smoothing_min_cutoff_hz"].value_or(config.smoothingMinCutoffHz);
    config.smoothingBeta = table["smoothing_beta"].value_or(config.smoothingBeta);
    config.maxYawDeg = table["max_yaw_deg"].value_or(config.maxYawDeg);
    config.maxPitchDeg = table["max_pitch_deg"].value_or(config.maxPitchDeg);
    config.invert.yaw = table["invert_yaw"].value_or(config.invert.yaw);
    config.invert.pitch = table["invert_pitch"].value_or(config.invert.pitch);
    config.invert.roll = table["invert_roll"].value_or(config.invert.roll);
    Validate(config);
    return config;
}

void SaveDefaultConfig(const std::filesystem::path& path) {
    TrackingConfig defaults;
    std::ofstream out(path);
    out << "# cyber-headtrack tracking config\n";
    out << "smoothing_min_cutoff_hz = " << defaults.smoothingMinCutoffHz << "\n";
    out << "smoothing_beta = " << defaults.smoothingBeta << "\n";
    out << "max_yaw_deg = " << defaults.maxYawDeg << "\n";
    out << "max_pitch_deg = " << defaults.maxPitchDeg << "\n";
    out << "invert_yaw = false\n";
    out << "invert_pitch = false\n";
    out << "invert_roll = false\n";
}
