#include "config.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>

#include <toml++/toml.hpp>

TrackingConfig LoadConfig(const std::string& path) {
    TrackingConfig config;
    if (!std::filesystem::exists(path)) {
        return config;
    }

    toml::table table;
    try {
        table = toml::parse_file(path);
    } catch (const toml::parse_error& err) {
        throw std::runtime_error(std::string("failed to parse config at ") + path + ": " +
                                  err.description().data());
    }

    config.rotSensitivity = table["rot_sensitivity"].value_or(config.rotSensitivity);
    config.leanSensitivity = table["lean_sensitivity"].value_or(config.leanSensitivity);
    config.rotDeadzoneDeg = table["rot_deadzone_deg"].value_or(config.rotDeadzoneDeg);
    config.leanDeadzoneCm = table["lean_deadzone_cm"].value_or(config.leanDeadzoneCm);
    config.maxYawDeg = table["max_yaw_deg"].value_or(config.maxYawDeg);
    config.maxPitchDeg = table["max_pitch_deg"].value_or(config.maxPitchDeg);
    return config;
}

void SaveDefaultConfig(const std::string& path) {
    TrackingConfig defaults;
    std::ofstream out(path);
    out << "# cyber-headtrack tracking config\n";
    out << "rot_sensitivity = " << defaults.rotSensitivity << "\n";
    out << "lean_sensitivity = " << defaults.leanSensitivity << "\n";
    out << "rot_deadzone_deg = " << defaults.rotDeadzoneDeg << "\n";
    out << "lean_deadzone_cm = " << defaults.leanDeadzoneCm << "\n";
    out << "max_yaw_deg = " << defaults.maxYawDeg << "\n";
    out << "max_pitch_deg = " << defaults.maxPitchDeg << "\n";
}
