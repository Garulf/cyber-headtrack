#pragma once

struct BodyVisibilityConfig {
    double triggerPitchDeg = -15.0;    // head-tracking pitch (negative = looking down) where the blend starts
    double fullBlendPitchDeg = -40.0;  // pitch at which the offset reaches its maximum
    double maxHeightOffsetCm = 15.0;
    double maxDepthOffsetCm = -8.5;
};

// Returns a 0.0-1.0 blend factor for how much of the body-visibility camera offset to apply,
// given the current head-tracking pitch offset (mouse pitch is not included). 0.0 above
// (less steep than) triggerPitchDeg, 1.0 at or beyond fullBlendPitchDeg, linear ramp
// between. Pure and stateless.
double ComputeBodyVisibilityBlend(double pitchDeg, const BodyVisibilityConfig& config);
