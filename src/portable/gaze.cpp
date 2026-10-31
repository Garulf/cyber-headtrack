#include "gaze.h"

#include <algorithm>

namespace {

bool IsOnScreen(ScreenPoint point) {
    return point.x >= 0.0 && point.x <= 1.0 && point.y >= 0.0 && point.y <= 1.0;
}

ScreenPoint ClampToScreen(ScreenPoint point) {
    return {std::clamp(point.x, 0.0, 1.0), std::clamp(point.y, 0.0, 1.0)};
}

}  // namespace

GazeTracker::GazeTracker(const GazeConfig& config)
    : xFilter_(config.smoothingMinCutoffHz, config.smoothingBeta),
      yFilter_(config.smoothingMinCutoffHz, config.smoothingBeta) {}

void GazeTracker::Configure(const GazeConfig& config) {
    xFilter_ = OneEuroFilter(config.smoothingMinCutoffHz, config.smoothingBeta);
    yFilter_ = OneEuroFilter(config.smoothingMinCutoffHz, config.smoothingBeta);
    xFilter_.Reset(filtered_.x);
    yFilter_.Reset(filtered_.y);
}

GazeOutput GazeTracker::Update(std::optional<ScreenPoint> sample, bool active, double dtSeconds) {
    if (dtSeconds <= 0.0) {
        return Current();
    }
    if (!active) {
        weight_ = 0.0;
        return Current();
    }

    const double easeStep = dtSeconds / kGazeEaseSeconds;
    onScreen_ = sample && IsOnScreen(*sample);
    if (sample) {
        const ScreenPoint point = ClampToScreen(*sample);
        if (weight_ == 0.0) {
            xFilter_.Reset(point.x);
            yFilter_.Reset(point.y);
            filtered_ = point;
        } else {
            filtered_ = {xFilter_.Filter(point.x, dtSeconds), yFilter_.Filter(point.y, dtSeconds)};
        }
        secondsWithoutSample_ = 0.0;
        weight_ = std::min(1.0, weight_ + easeStep);
    } else {
        secondsWithoutSample_ += dtSeconds;
        if (secondsWithoutSample_ > kGazeLossGraceSeconds) {
            weight_ = std::max(0.0, weight_ - easeStep);
        }
    }
    return Current();
}

GazeOutput GazeTracker::Current() const {
    return {{kScreenCentre.x + weight_ * (filtered_.x - kScreenCentre.x),
             kScreenCentre.y + weight_ * (filtered_.y - kScreenCentre.y)},
            weight_, onScreen_};
}
