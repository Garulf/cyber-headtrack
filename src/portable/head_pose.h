#pragma once

struct RawTobiiSample {
    float positionXyzMm[3] = {0.0f, 0.0f, 0.0f};
    float rotationXyzRad[3] = {0.0f, 0.0f, 0.0f};
};

struct HeadPose {
    double xCm = 0.0;
    double yCm = 0.0;
    double zCm = 0.0;
    double yawDeg = 0.0;
    double pitchDeg = 0.0;
    double rollDeg = 0.0;
};

// Converts a raw Tobii head-pose sample (mm/rad, Tobii's coordinate system) into a
// HeadPose (cm/deg). Pure function: same axis/sign mapping as HeadGlance's
// TobiiPoseMapper (Tobii: +x right, +y up, +z towards user; rotation x=pitch,
// y=yaw, z=roll).
HeadPose MapTobiiSample(const RawTobiiSample& sample);

// Re-expresses pose relative to reference (component-wise subtraction on every axis), so
// the reference pose itself maps to all-zero. Used to make "recenter" capture the user's
// current head pose as the new neutral baseline instead of assuming the tracker's own
// zero is neutral. Pure function.
HeadPose SubtractReference(const HeadPose& pose, const HeadPose& reference);
