#pragma once

#include "aim_math.h"

// Keeps hitscan shots on the body's aim while head tracking rotates the camera.
//
// Hitscan damage rays are built by the engine's PhysicalRay effect, which asks an evaluator
// for the ray's origin and then its direction; for the player's gun both come from the
// rendered camera, so a head-rotated camera drags the shot with it. The hook replaces those
// two evaluator calls, found by byte signature in the game module at load. A ray is treated
// as the player's when its origin is at the player's camera; its direction is then rotated
// back by the head offset, keeping the weapon's own spread. Every other ray (NPCs, turrets,
// security cameras) is left untouched.
//
// If a signature doesn't match exactly once (e.g. after a game patch) the hook is skipped and
// logged, and shots simply follow the camera as before.
void InstallAimHooks();
void RemoveAimHooks();

// Published by the camera script every frame from the rendered camera: its world orientation
// (including the head offset) and position, plus the head offset itself
// (camera = body * head). Correction is off while active is false (tracking disabled,
// third-person vehicle camera).
void SetAimFrame(const Quat& cameraWorld, const Vec3& cameraPosition, const Quat& head, bool active);

// The rendered camera as last published by SetAimFrame (published every frame, whether or not
// tracking is on).
struct PublishedCamera {
    Quat world;
    Vec3 position;
};
PublishedCamera CurrentPublishedCamera();

// Logs correction counts if they changed since the last call.
void ReportAimStats();
