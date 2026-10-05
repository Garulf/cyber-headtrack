#include "one_euro_filter.h"

#include <cmath>
#include <numbers>

namespace {

constexpr double kDerivativeCutoffHz = 1.0;

double SmoothingFactor(double cutoffHz, double dtSeconds) {
    const double timeConstant = 1.0 / (2.0 * std::numbers::pi * cutoffHz);
    return 1.0 / (1.0 + timeConstant / dtSeconds);
}

double Blend(double previous, double current, double alpha) {
    return previous + alpha * (current - previous);
}

}  // namespace

OneEuroFilter::OneEuroFilter(double minCutoffHz, double beta)
    : minCutoffHz_(minCutoffHz), beta_(beta) {}

double OneEuroFilter::Filter(double value, double dtSeconds) {
    if (!initialized_) {
        Reset(value);
        return value;
    }
    if (dtSeconds <= 0.0) {
        return previousValue_;
    }

    const double derivative = (value - previousValue_) / dtSeconds;
    previousDerivative_ =
        Blend(previousDerivative_, derivative, SmoothingFactor(kDerivativeCutoffHz, dtSeconds));

    const double cutoffHz = minCutoffHz_ + beta_ * std::abs(previousDerivative_);
    previousValue_ = Blend(previousValue_, value, SmoothingFactor(cutoffHz, dtSeconds));
    return previousValue_;
}

void OneEuroFilter::Reset(double value) {
    initialized_ = true;
    previousValue_ = value;
    previousDerivative_ = 0.0;
}
