#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <optional>
#include "gaze.h"

using Catch::Approx;

namespace {

constexpr double kFrame = 1.0 / 60.0;

GazeOutput Run(GazeTracker& tracker, std::optional<ScreenPoint> sample, double seconds) {
    GazeOutput output = tracker.Current();
    for (double t = 0.0; t < seconds - 1e-9; t += kFrame) {
        output = tracker.Update(sample, true, kFrame);
    }
    return output;
}

}  // namespace

TEST_CASE("GazeTracker inactive output is centred with zero weight", "[gaze]") {
    GazeTracker tracker{GazeConfig{}};

    const GazeOutput output = tracker.Update(ScreenPoint{0.9, 0.1}, false, kFrame);

    REQUIRE(output.point.x == Approx(0.5));
    REQUIRE(output.point.y == Approx(0.5));
    REQUIRE(output.weight == 0.0);
}

TEST_CASE("GazeTracker eases in over a quarter second", "[gaze]") {
    GazeTracker tracker{GazeConfig{}};

    REQUIRE(Run(tracker, ScreenPoint{0.8, 0.5}, 0.125).weight == Approx(0.5).margin(0.05));

    const GazeOutput settled = Run(tracker, ScreenPoint{0.8, 0.5}, 0.3 - 0.125);
    REQUIRE(settled.weight == 1.0);
    REQUIRE(settled.point.x == Approx(0.8).margin(0.01));
}

TEST_CASE("GazeTracker clamps off-screen samples", "[gaze]") {
    GazeTracker tracker{GazeConfig{}};

    const GazeOutput output = Run(tracker, ScreenPoint{1.4, -0.3}, 1.0);

    REQUIRE(output.point.x == Approx(1.0).margin(1e-6));
    REQUIRE(output.point.y == Approx(0.0).margin(1e-6));
}

TEST_CASE("GazeTracker holds the last point through a short dropout", "[gaze]") {
    GazeTracker tracker{GazeConfig{}};
    Run(tracker, ScreenPoint{0.7, 0.4}, 1.0);

    const GazeOutput output = Run(tracker, std::nullopt, 0.15);

    REQUIRE(output.weight == 1.0);
    REQUIRE(output.point.x == Approx(0.7).margin(1e-6));
    REQUIRE(output.point.y == Approx(0.4).margin(1e-6));
}

TEST_CASE("GazeTracker eases back to centre after losing the eyes", "[gaze]") {
    GazeTracker tracker{GazeConfig{}};
    Run(tracker, ScreenPoint{0.9, 0.5}, 1.0);

    const GazeOutput easing = Run(tracker, std::nullopt, 0.2 + 0.125);
    REQUIRE(easing.weight == Approx(0.5).margin(0.05));
    REQUIRE(easing.point.x == Approx(0.7).margin(0.03));

    const GazeOutput lost = Run(tracker, std::nullopt, 0.5 - 0.325);
    REQUIRE(lost.weight == 0.0);
    REQUIRE(lost.point.x == Approx(0.5));
    REQUIRE(lost.point.y == Approx(0.5));
}

TEST_CASE("GazeTracker restarts from the new sample after a full loss", "[gaze]") {
    GazeTracker tracker{GazeConfig{}};
    Run(tracker, ScreenPoint{0.9, 0.5}, 1.0);
    REQUIRE(Run(tracker, std::nullopt, 0.6).weight == 0.0);

    const GazeOutput output = tracker.Update(ScreenPoint{0.2, 0.5}, true, kFrame);

    REQUIRE(output.weight > 0.0);
    REQUIRE(output.point.x == Approx(0.5 + output.weight * (0.2 - 0.5)));
}

TEST_CASE("GazeTracker ignores non-positive dt", "[gaze]") {
    GazeTracker tracker{GazeConfig{}};
    const GazeOutput before = Run(tracker, ScreenPoint{0.3, 0.6}, 0.1);

    const GazeOutput after = tracker.Update(ScreenPoint{0.9, 0.9}, true, 0.0);

    REQUIRE(after.weight == before.weight);
    REQUIRE(after.point.x == before.point.x);
    REQUIRE(after.point.y == before.point.y);
}

TEST_CASE("GazeTracker flags samples outside the screen", "[gaze]") {
    GazeTracker tracker{GazeConfig{}};

    REQUIRE(tracker.Update(ScreenPoint{0.5, 0.5}, true, kFrame).onScreen);
    REQUIRE_FALSE(tracker.Update(ScreenPoint{1.4, 0.5}, true, kFrame).onScreen);
    REQUIRE_FALSE(tracker.Update(std::nullopt, true, kFrame).onScreen);
    REQUIRE(tracker.Update(ScreenPoint{0.2, 0.9}, true, kFrame).onScreen);
}
