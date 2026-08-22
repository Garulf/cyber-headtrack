#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "body_visibility.h"

using Catch::Approx;

TEST_CASE("ComputeBodyVisibilityBlend is zero above the trigger pitch", "[body_visibility]") {
    BodyVisibilityConfig config;
    REQUIRE(ComputeBodyVisibilityBlend(0.0, config) == Approx(0.0));
    REQUIRE(ComputeBodyVisibilityBlend(-10.0, config) == Approx(0.0));
}

TEST_CASE("ComputeBodyVisibilityBlend is one at or beyond the full-blend pitch", "[body_visibility]") {
    BodyVisibilityConfig config;
    REQUIRE(ComputeBodyVisibilityBlend(-40.0, config) == Approx(1.0));
    REQUIRE(ComputeBodyVisibilityBlend(-60.0, config) == Approx(1.0));
}

TEST_CASE("ComputeBodyVisibilityBlend ramps linearly between the two pitches", "[body_visibility]") {
    BodyVisibilityConfig config;
    double midPitch = (config.triggerPitchDeg + config.fullBlendPitchDeg) / 2.0;

    double blend = ComputeBodyVisibilityBlend(midPitch, config);

    REQUIRE(blend == Approx(0.5));
}

TEST_CASE("ComputeBodyVisibilityBlend is a pure function of pitch and config", "[body_visibility]") {
    BodyVisibilityConfig config;
    double first = ComputeBodyVisibilityBlend(-25.0, config);
    double second = ComputeBodyVisibilityBlend(-25.0, config);

    REQUIRE(first == second);
}
