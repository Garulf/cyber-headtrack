#include "scan_lock.h"

#include <cmath>

namespace {

double ScreenDistance(ScreenPoint a, ScreenPoint b, double aspect) {
    return std::hypot((a.x - b.x) * aspect, a.y - b.y);
}

const ScanCandidate* FindCandidate(std::span<const ScanCandidate> candidates, uint64_t id) {
    for (const ScanCandidate& candidate : candidates) {
        if (candidate.id == id) {
            return &candidate;
        }
    }
    return nullptr;
}

}  // namespace

ScanLock::ScanLock(const GazeConfig& config) : config_(config) {}

void ScanLock::Configure(const GazeConfig& config) {
    config_ = config;
}

void ScanLock::Reset() {
    lockedId_.reset();
    dwellId_.reset();
    dwellSeconds_ = 0.0;
}

LockResult ScanLock::Update(ScreenPoint gaze, double gazeWeight,
                            std::span<const ScanCandidate> candidates, double aspect,
                            double dtSeconds) {
    if (gazeWeight < 1.0) {
        Reset();
        return {gaze, std::nullopt};
    }

    const ScanCandidate* locked = lockedId_ ? FindCandidate(candidates, *lockedId_) : nullptr;
    if (locked == nullptr) {
        lockedId_.reset();
    }

    const auto withinRadius = [&](const ScanCandidate& candidate) {
        return ScreenDistance(gaze, candidate.point, aspect) <= config_.lockRadius;
    };
    const ScanCandidate* gazed = nullptr;
    if (locked != nullptr && withinRadius(*locked)) {
        gazed = locked;
    } else {
        double nearest = config_.lockRadius;
        for (const ScanCandidate& candidate : candidates) {
            const double distance = ScreenDistance(gaze, candidate.point, aspect);
            if (distance <= nearest) {
                nearest = distance;
                gazed = &candidate;
            }
        }
    }

    if (locked == nullptr) {
        if (gazed != nullptr) {
            locked = gazed;
        }
    } else if (gazed == nullptr || gazed == locked) {
        if (gazed == nullptr && !config_.stickyLock) {
            locked = nullptr;
        }
    } else {
        if (dwellId_ != gazed->id) {
            dwellId_ = gazed->id;
            dwellSeconds_ = 0.0;
        }
        dwellSeconds_ += dtSeconds;
        if (dwellSeconds_ >= config_.switchDwellSeconds) {
            locked = gazed;
        }
    }

    if (gazed == nullptr || gazed == locked) {
        dwellId_.reset();
        dwellSeconds_ = 0.0;
    }
    if (locked == nullptr) {
        lockedId_.reset();
        return {gaze, std::nullopt};
    }
    lockedId_ = locked->id;
    return {locked->point, lockedId_};
}
