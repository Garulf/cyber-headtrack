#pragma once

// Points the engine's targeting rays through the scan point while scan mode is open, so the
// scan target, scan progress and quickhacks follow the gaze-driven reticle without the camera
// moving.
//
// Two calls in the game module are redirected, each found by byte signature at load:
// - the player's targeting record update converts the crosshair forward into angles; the
//   forward is replaced just before that, so the targeting system measures every target
//   against the scan point (this drives the scanned object);
// - the look-at raycast takes its direction from the camera transform; the direction is
//   replaced just after (this drives the look-at target and its fallback).
// A call is the player's when its origin is at the published camera. Every other call, and
// every call while TrackingState::ScanCameraDirection() has no value, is left untouched.
//
// A site whose signature doesn't match exactly once is skipped and logged; the scanner then
// targets the screen centre as in vanilla.
void InstallScanHooks();
void RemoveScanHooks();

// Logs per-site call counts and the last angle between the engine's own ray and the camera
// forward (about 0 when the site really is the centre ray), if they changed since last call.
void ReportScanStats();
