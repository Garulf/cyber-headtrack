#include "head_pose.h"

namespace {
constexpr double kMmToCm = 0.1;
constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;
}  // namespace

HeadPose MapTobiiSample(const RawTobiiSample& sample) {
    HeadPose pose;
    pose.xCm = -static_cast<double>(sample.positionXyzMm[0]) * kMmToCm;
    pose.yCm = static_cast<double>(sample.positionXyzMm[1]) * kMmToCm;
    pose.zCm = static_cast<double>(sample.positionXyzMm[2]) * kMmToCm;
    pose.pitchDeg = static_cast<double>(sample.rotationXyzRad[0]) * kRadToDeg;
    pose.yawDeg = -static_cast<double>(sample.rotationXyzRad[1]) * kRadToDeg;
    pose.rollDeg = static_cast<double>(sample.rotationXyzRad[2]) * kRadToDeg;
    return pose;
}
