#include "shaping.h"

#include <algorithm>

namespace {

double ScaleAndClamp(double value, double scale, double limit) {
    return std::clamp(value * scale, -limit, limit);
}

double Signed(double value, bool inverted) {
    return inverted ? -value : value;
}

}  // namespace

Rotation ShapeHeadRotation(const HeadPose& relative, const ProfileConfig& profile) {
    Rotation target;
    target.yaw = ScaleAndClamp(relative.yawDeg, profile.yawScale, profile.maxYawDeg);
    target.pitch = ScaleAndClamp(relative.pitchDeg, profile.pitchScale, profile.maxPitchDeg);
    target.roll = ScaleAndClamp(relative.rollDeg, profile.rollScale, profile.maxRollDeg);
    return target;
}

Lean ShapeHeadLean(const HeadPose& relative, const ProfileConfig& profile) {
    if (!profile.leanEnabled) {
        return {};
    }
    const double side = -relative.xCm;
    const double up = relative.yCm;
    const double forward = -relative.zCm;
    Lean lean;
    lean.sideCm = Signed(ScaleAndClamp(side, profile.leanScale, profile.maxLeanSideCm),
                         profile.invertLean.side);
    lean.upCm = Signed(ScaleAndClamp(up, profile.leanScale, profile.maxLeanUpCm),
                       profile.invertLean.up);
    lean.forwardCm = Signed(ScaleAndClamp(forward, profile.leanScale, profile.maxLeanForwardCm),
                            profile.invertLean.forward);
    return lean;
}
