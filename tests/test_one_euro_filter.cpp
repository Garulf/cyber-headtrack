#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "one_euro_filter.h"

#include <algorithm>
#include <cmath>

using Catch::Approx;

namespace {
constexpr double kFrame = 1.0 / 60.0;

double RunRamp(OneEuroFilter& filter, double degreesPerSecond, double seconds) {
    double output = 0.0;
    for (double t = 0.0; t < seconds; t += kFrame) {
        output = filter.Filter(degreesPerSecond * t, kFrame);
    }
    return output;
}
}  // namespace

TEST_CASE("OneEuroFilter passes the first sample through unchanged", "[one_euro_filter]") {
    OneEuroFilter filter(1.0, 0.05);

    REQUIRE(filter.Filter(12.5, kFrame) == Approx(12.5));
}

TEST_CASE("OneEuroFilter holds a constant input steady", "[one_euro_filter]") {
    OneEuroFilter filter(1.0, 0.05);
    double output = 0.0;

    for (int i = 0; i < 120; ++i) {
        output = filter.Filter(7.0, kFrame);
    }

    REQUIRE(output == Approx(7.0));
}

TEST_CASE("OneEuroFilter smooths jitter while the head is still", "[one_euro_filter]") {
    OneEuroFilter filter(1.0, 0.05);
    filter.Filter(0.0, kFrame);
    double largestSwing = 0.0;

    for (int i = 0; i < 120; ++i) {
        const double jitter = (i % 2 == 0) ? 0.3 : -0.3;
        largestSwing = std::max(largestSwing, std::abs(filter.Filter(jitter, kFrame)));
    }

    REQUIRE(largestSwing < 0.1);
}

TEST_CASE("OneEuroFilter follows fast movement closely when beta is raised", "[one_euro_filter]") {
    OneEuroFilter slow(1.0, 0.0);
    OneEuroFilter responsive(1.0, 0.05);

    const double slowLag = 90.0 * 0.5 - RunRamp(slow, 90.0, 0.5);
    const double responsiveLag = 90.0 * 0.5 - RunRamp(responsive, 90.0, 0.5);

    REQUIRE(responsiveLag < slowLag / 3.0);
    REQUIRE(responsiveLag < 2.0);
}

TEST_CASE("OneEuroFilter ignores a non-positive time step", "[one_euro_filter]") {
    OneEuroFilter filter(1.0, 0.05);
    filter.Filter(3.0, kFrame);

    REQUIRE(filter.Filter(50.0, 0.0) == Approx(3.0));
}

TEST_CASE("OneEuroFilter eases away from the value it was reset to", "[one_euro_filter]") {
    OneEuroFilter filter(1.0, 0.05);
    filter.Filter(30.0, kFrame);

    filter.Reset(0.0);
    const double first = filter.Filter(-4.0, kFrame);

    REQUIRE(first < 0.0);
    REQUIRE(first > -4.0);
}
