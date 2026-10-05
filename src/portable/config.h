#pragma once

#include <filesystem>

#include "engine_angles.h"

struct TrackingConfig {
    double smoothingMinCutoffHz = 1.0;
    double smoothingBeta = 0.1;
    double maxYawDeg = 45.0;
    double maxPitchDeg = 45.0;
    AxisInversion invert;
};

// Loads a TrackingConfig from a TOML file at path. Returns defaults if the file does not
// exist. Throws std::runtime_error with a descriptive message if the file exists but fails
// to parse or holds an out-of-range value.
TrackingConfig LoadConfig(const std::filesystem::path& path);

// Writes a default config file at path, for first-run bootstrapping.
void SaveDefaultConfig(const std::filesystem::path& path);
