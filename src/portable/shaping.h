#pragma once

#include "config.h"
#include "head_pose.h"
#include "lean.h"
#include "rotation.h"

// Turns a head pose relative to the neutral reference into the target view rotation for one
// profile: each axis is multiplied by its scale, then clamped to that axis's maximum. Pure
// function.
Rotation ShapeHeadRotation(const HeadPose& relative, const ProfileConfig& profile);

// Turns a head pose relative to the neutral reference into the target camera lean for one
// profile: head movement converted to the player's side/up/forward axes, multiplied by
// leanScale, clamped per axis, then inverted where the profile asks. Zero when lean is
// disabled. Pure function.
Lean ShapeHeadLean(const HeadPose& relative, const ProfileConfig& profile);
