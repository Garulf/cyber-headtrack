#include "adaptive_filter.h"

#include <algorithm>
#include <cmath>

namespace {

// {magnitude, gain} pairs, piecewise-linear between points, clamped at the ends.
// Ported directly from opentrack's rot_gains/pos_gains defaults (accela-settings.hpp),
// via HeadGlance's AdaptiveFilter.
constexpr double kRotGains[7][2] = {
    {0.5, 0.4}, {1, 1.5}, {1.5, 8}, {2.5, 35}, {5, 100}, {8, 200}, {9, 300}
};
constexpr double kLeanGains[11][2] = {
    {0, 0}, {0.33, 0.375}, {0.66, 0.75}, {1.33, 2.25}, {1.66, 4.5},
    {2, 7.5}, {3, 24}, {5, 60}, {7, 110}, {8, 150}, {9, 200}
};

constexpr double kRotFilterDeadzoneDeg = 0.03;
constexpr double kLeanFilterDeadzoneCm = 0.1;

double Interpolate(const double table[][2], int rows, double x) {
    if (x <= table[0][0]) return table[0][1];
    for (int i = 1; i < rows; ++i) {
        if (x <= table[i][0]) {
            double x0 = table[i - 1][0], y0 = table[i - 1][1];
            double x1 = table[i][0], y1 = table[i][1];
            double f = (x - x0) / (x1 - x0);
            return y0 + (y1 - y0) * f;
        }
    }
    return table[rows - 1][1];
}

// Per-axis output rate (units/second) for one 3-axis group, ported from opentrack's
// do_deltas via HeadGlance's AdaptiveFilter.axisGroupRate.
void AxisGroupRate(const double delta[3], double deadzone, double sensitivity,
                    const double gainTable[][2], int gainRows, double outRate[3]) {
    double adjusted[3];
    for (int i = 0; i < 3; ++i) {
        double d = delta[i];
        d = std::abs(d) > deadzone ? d - std::copysign(deadzone, d) : 0.0;
        adjusted[i] = d / sensitivity;
    }
    double dist = std::sqrt(adjusted[0] * adjusted[0] + adjusted[1] * adjusted[1] +
                             adjusted[2] * adjusted[2]);
    double gain = Interpolate(gainTable, gainRows, dist);

    double weight[3] = {0.0, 0.0, 0.0};
    double weightSum = 0.0;
    if (dist > 1e-6) {
        for (int i = 0; i < 3; ++i) {
            weight[i] = std::min(1.0, std::abs(adjusted[i]) / dist);
            weightSum += weight[i];
        }
    }

    for (int i = 0; i < 3; ++i) {
        outRate[i] = 0.0;
        if (weightSum > 1e-6) {
            outRate[i] = std::copysign(1.0, adjusted[i]) * (weight[i] / weightSum) * gain;
            if (adjusted[i] == 0.0) outRate[i] = 0.0;
        }
    }
}

}  // namespace

Offset AdaptiveFilterStep(const Offset& lastSmoothed, const Offset& target, double dtSeconds,
                           double rotSensitivity, double leanSensitivity) {
    double rotDelta[3] = {
        target.yaw - lastSmoothed.yaw,
        target.pitch - lastSmoothed.pitch,
        target.roll - lastSmoothed.roll,
    };
    double rotRate[3];
    AxisGroupRate(rotDelta, kRotFilterDeadzoneDeg, rotSensitivity, kRotGains, 7, rotRate);

    // Gain tables are calibrated for cm-scale deltas; Offset's lean fields are in the same
    // "blocks" convention HeadGlance uses (1 block == 100cm), so convert to cm before the
    // lookup and the resulting cm/s rate back to blocks/s before integrating.
    double leanDelta[3] = {
        (target.leanX - lastSmoothed.leanX) * 100.0,
        (target.leanY - lastSmoothed.leanY) * 100.0,
        (target.leanZ - lastSmoothed.leanZ) * 100.0,
    };
    double leanRate[3];
    AxisGroupRate(leanDelta, kLeanFilterDeadzoneCm, leanSensitivity, kLeanGains, 11, leanRate);

    Offset result;
    result.yaw = lastSmoothed.yaw + rotRate[0] * dtSeconds;
    result.pitch = lastSmoothed.pitch + rotRate[1] * dtSeconds;
    result.roll = lastSmoothed.roll + rotRate[2] * dtSeconds;
    result.leanX = lastSmoothed.leanX + (leanRate[0] / 100.0) * dtSeconds;
    result.leanY = lastSmoothed.leanY + (leanRate[1] / 100.0) * dtSeconds;
    result.leanZ = lastSmoothed.leanZ + (leanRate[2] / 100.0) * dtSeconds;
    return result;
}
