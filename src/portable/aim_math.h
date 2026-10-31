#pragma once

#include "gaze.h"

// Minimal quaternion and vector math for correcting the shot direction. Quaternions are
// (x, y, z, w) like REDengine's Quaternion (i, j, k, r); vectors use the engine's axes
// (X right, Y forward, Z up).
struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Quat {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double w = 1.0;
};

Quat Multiply(const Quat& a, const Quat& b);
Quat Conjugate(const Quat& q);
Vec3 Rotate(const Quat& q, const Vec3& v);

// The orientation that maps the X, Y and Z axes onto right, forward and up (an orthonormal
// basis, e.g. the active camera's axes).
Quat FromBasis(const Vec3& right, const Vec3& forward, const Vec3& up);

// The camera's world orientation is the body aim composed with the head offset
// (camera = body * head). Given a shot direction computed from that camera, returns the
// direction the same shot would have had without the head offset, keeping any spread or
// recoil it carried relative to the camera.
Vec3 CleanAimDirection(const Quat& cameraWorld, const Quat& headLocal, const Vec3& shot);

// True if direction is within maxDegrees of the camera's forward axis. Used to leave alone
// directions that clearly aren't aimed from the camera.
bool IsNearForward(const Quat& cameraWorld, const Vec3& direction, double maxDegrees);

// The angle between two non-zero directions, in degrees.
double AngleDegrees(const Vec3& a, const Vec3& b);

// The camera-space direction (X right, Y forward, Z up) through a point on the screen.
// ndcPerTanX and ndcPerTanY are the projection's scales: how far a direction moves in
// normalized device coordinates per unit of tangent of its angle from forward.
Vec3 ScreenToCameraDirection(ScreenPoint point, double ndcPerTanX, double ndcPerTanY);
