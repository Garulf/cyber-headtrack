#pragma once

// Head rotation relative to the recentered neutral pose, in degrees. HeadPose convention:
// positive yaw = head turned right, positive pitch = head tilted up.
struct Rotation {
    double yaw = 0.0;
    double pitch = 0.0;
    double roll = 0.0;
};
