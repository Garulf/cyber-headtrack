#pragma once

#include "adaptive_filter.h"
#include "config.h"
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

    bool Enabled() const { return enabled_; }
    void SetEnabled(bool enabled) { enabled_ = enabled; }
    void Recenter() { current_ = Offset{}; }

private:
    TobiiSource source_;
    TrackingConfig config_;
    Offset current_;
    bool enabled_ = false;
};
