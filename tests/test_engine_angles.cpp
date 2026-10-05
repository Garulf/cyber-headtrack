#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "engine_angles.h"

using Catch::Approx;

TEST_CASE("ToEngineAngles turns a rightward head yaw into a negative engine yaw", "[engine_angles]") {
    Rotation offset{};
    offset.yaw = 20.0;

    EngineAngles angles = ToEngineAngles(offset, AxisInversion{});

    REQUIRE(angles.yaw == Approx(-20.0));
}

TEST_CASE("ToEngineAngles passes pitch through by default", "[engine_angles]") {
    Rotation offset{};
    offset.pitch = -12.0;

    EngineAngles angles = ToEngineAngles(offset, AxisInversion{});

    REQUIRE(angles.pitch == Approx(-12.0));
}

TEST_CASE("ToEngineAngles turns a rightward head roll into a negative engine roll", "[engine_angles]") {
    Rotation offset{};
    offset.roll = 5.0;

    EngineAngles angles = ToEngineAngles(offset, AxisInversion{});

    REQUIRE(angles.roll == Approx(-5.0));
}

TEST_CASE("ToEngineAngles flips each inverted axis independently", "[engine_angles]") {
    Rotation offset{};
    offset.yaw = 20.0;
    offset.pitch = -12.0;
    offset.roll = 5.0;
    AxisInversion invert{};
    invert.yaw = true;
    invert.roll = true;

    EngineAngles angles = ToEngineAngles(offset, invert);

    REQUIRE(angles.yaw == Approx(20.0));
    REQUIRE(angles.pitch == Approx(-12.0));
    REQUIRE(angles.roll == Approx(5.0));
}
