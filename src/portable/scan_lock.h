#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "config.h"
#include "gaze.h"

// A scannable target on screen this frame. id is stable across frames (the entity's hash).
// How long a locked target may be missing from the candidates (hidden for a frame behind
// someone, say) before the lock is released; meanwhile it stays at its last position.
inline constexpr double kLockLossGraceSeconds = 0.25;

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
// or the HUD (gaze off the screen counts as empty space). Distances are in screen heights.
class ScanLock {
public:
    explicit ScanLock(const GazeConfig& config);
    void Configure(const GazeConfig& config);

    // gazeWeight and gazeOnScreen come from GazeOutput; no lock is held until the gaze has
    // fully eased in. aspect is the screen's width divided by its height.
    LockResult Update(ScreenPoint gaze, double gazeWeight, bool gazeOnScreen,
                      std::span<const ScanCandidate> candidates, double aspect, double dtSeconds);
    void Reset();

private:
    GazeConfig config_;
    std::optional<uint64_t> lockedId_;
    ScreenPoint lockedPoint_;
    double lockedMissingSeconds_ = 0.0;
    std::optional<uint64_t> dwellId_;
    double dwellSeconds_ = 0.0;
};
