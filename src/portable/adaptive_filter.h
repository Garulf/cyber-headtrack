#pragma once

struct Offset {
    double leanX = 0.0;
    double leanY = 0.0;
    double leanZ = 0.0;
    double yaw = 0.0;
    double pitch = 0.0;
    double roll = 0.0;
};

// Velocity-adaptive filter ported from HeadGlance's AdaptiveFilter (itself ported from
// opentrack's filter-accela): classifies movement by how fast it's happening (frame-to-frame
// delta against the previous smoothed output), not how far from center it is. Pure and
// stateless; the caller owns and supplies lastSmoothed each call.
Offset AdaptiveFilterStep(const Offset& lastSmoothed, const Offset& target, double dtSeconds,
                           double rotSensitivity, double leanSensitivity);
