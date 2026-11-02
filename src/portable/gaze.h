#pragma once

#include <optional>

#include "config.h"
#include "one_euro_filter.h"

// A point on the display, normalized like Tobii's gaze point: (0, 0) top-left, (1, 1)
// bottom-right.
struct ScreenPoint {
    double x = 0.5;
    double y = 0.5;
};

inline constexpr ScreenPoint kScreenCentre{};

// How long the gaze may drop out (a blink) before the reticle starts easing back to centre.
inline constexpr double kGazeLossGraceSeconds = 0.2;
// How long easing in or out between centre and the gaze point takes.
inline constexpr double kGazeEaseSeconds = 0.25;

struct GazeOutput {
    // The reticle position: centre + weight * (filtered gaze - centre).
    ScreenPoint point;
    // 0 = vanilla (centre, no redirect), 1 = full gaze.
    double weight = 0.0;
    // False while the latest sample lies outside the screen (before clamping) or is missing.
    bool onScreen = false;
};

// Smooths the gaze point for the scanner reticle and eases between it and screen centre when
// scan mode opens or closes or the tracker loses the eyes.
class GazeTracker {
public:
    explicit GazeTracker(const GazeConfig& config);

    // Applies new smoothing settings, continuing from the current filtered point.
    void Configure(const GazeConfig& config);

    // sample is the latest valid gaze point, or std::nullopt while the tracker has none.
    // While active is false the output is centred with zero weight.
    GazeOutput Update(std::optional<ScreenPoint> sample, bool active, double dtSeconds);
    GazeOutput Current() const;

private:
    OneEuroFilter xFilter_;
    OneEuroFilter yFilter_;
    ScreenPoint filtered_;
    double weight_ = 0.0;
    double secondsWithoutSample_ = 0.0;
    bool onScreen_ = false;
};
