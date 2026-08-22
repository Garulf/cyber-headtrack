#include "body_visibility.h"

#include <algorithm>

double ComputeBodyVisibilityBlend(double pitchDeg, const BodyVisibilityConfig& config) {
    if (pitchDeg >= config.triggerPitchDeg) {
        return 0.0;
    }
    if (pitchDeg <= config.fullBlendPitchDeg) {
        return 1.0;
    }
    double range = config.fullBlendPitchDeg - config.triggerPitchDeg;
    double progress = (pitchDeg - config.triggerPitchDeg) / range;
    return std::clamp(progress, 0.0, 1.0);
}
