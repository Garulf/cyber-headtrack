#pragma once

#include <string>

struct TrackingConfig {
    double rotSensitivity = 6.0;
    double leanSensitivity = 1.0;
    double rotDeadzoneDeg = 0.5;
    double leanDeadzoneCm = 0.5;
    double maxYawDeg = 45.0;
    double maxPitchDeg = 45.0;
};

// Loads a TrackingConfig from a TOML file at path. Returns defaults if the file does not
// exist. Throws std::runtime_error with a descriptive message if the file exists but fails
// to parse.
TrackingConfig LoadConfig(const std::string& path);

// Writes a default config file at path, for first-run bootstrapping.
void SaveDefaultConfig(const std::string& path);
