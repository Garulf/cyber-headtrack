#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "config.h"
#include "gaze.h"

// A scannable target on screen this frame. id is stable across frames (the entity's hash).
struct ScanCandidate {
    uint64_t id = 0;
    ScreenPoint point;
};

struct LockResult {
    // Where the reticle and the targeting ray point: the locked target, or else the gaze.
    ScreenPoint point;
    std::optional<uint64_t> lockedId;
};

// Locks the scanner reticle onto the target under the gaze so it doesn't flick between people
// standing close together. The lock moves to another target only after the gaze has rested
// on it for switchDwellSeconds; with stickyLock it survives the gaze wandering to empty space
// or the HUD. Distances are in screen heights.
class ScanLock {
public:
    explicit ScanLock(const GazeConfig& config);
    void Configure(const GazeConfig& config);

    // gazeWeight is GazeOutput::weight; no lock is held until the gaze has fully eased in.
    // aspect is the screen's width divided by its height.
    LockResult Update(ScreenPoint gaze, double gazeWeight, std::span<const ScanCandidate> candidates,
                      double aspect, double dtSeconds);
    void Reset();

private:
    GazeConfig config_;
    std::optional<uint64_t> lockedId_;
    std::optional<uint64_t> dwellId_;
    double dwellSeconds_ = 0.0;
};
