#pragma once

#include "adaptive_filter.h"
#include "body_visibility.h"
#include "config.h"
#include "head_pose.h"
#include "tobii_source.h"

// Owns the single process-wide tracking pipeline: TobiiSource -> AdaptiveFilterStep -> the
// current Offset exposed to redscript. Not thread-safe by design -- all access happens from
// the game's main thread via the RTTI-registered functions and the per-frame Running-state
// update hook, which RED4ext runs on that thread.
class TrackingState {
public:
    static TrackingState& Instance();

    void Initialize();
    void Shutdown();
    void Update(double dtSeconds);

    float Yaw() const { return static_cast<float>(current_.yaw); }
    float Pitch() const { return static_cast<float>(current_.pitch); }
    float Roll() const { return static_cast<float>(current_.roll); }

    float BodyVisibilityBlend() const {
        return static_cast<float>(ComputeBodyVisibilityBlend(current_.pitch, bodyVisibilityConfig_));
    }

    bool Enabled() const { return enabled_; }
    void SetEnabled(bool enabled) { enabled_ = enabled; }

    // Captures the current raw head pose (if one is available) as the new neutral
    // reference and zeroes the applied offset, so "recenter" means "this head position is
    // now straight ahead" rather than "snap back toward the tracker's own zero". If no
    // pose is available (tracker disconnected), only the offset is reset and the previous
    // reference is kept.
    void Recenter();

private:
    TobiiSource source_;
    TrackingConfig config_;
    BodyVisibilityConfig bodyVisibilityConfig_;
    HeadPose reference_;
    Offset current_;
    bool enabled_ = false;
};
