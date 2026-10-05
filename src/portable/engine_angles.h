#pragma once

#include "rotation.h"

struct AxisInversion {
    bool yaw = false;
    bool pitch = false;
    bool roll = false;
};

// Rotation in REDengine's EulerAngles convention, in degrees: positive yaw turns the view
// left, positive pitch looks up.
struct EngineAngles {
    double yaw = 0.0;
    double pitch = 0.0;
    double roll = 0.0;
};

// Converts a smoothed head rotation into engine angles, then applies the user's per-axis
// inversion on top. Yaw and roll flip sign between the two conventions (confirmed in game
// for roll); pitch carries over. Pure function.
EngineAngles ToEngineAngles(const Rotation& offset, const AxisInversion& invert);
