#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "shaping.h"

using Catch::Approx;

TEST_CASE("ShapeHeadRotation scales each axis by its own factor", "[shaping]") {
    HeadPose relative;
    relative.yawDeg = 10.0;
    relative.pitchDeg = -4.0;
    relative.rollDeg = 3.0;
    ProfileConfig profile;
    profile.yawScale = 2.5;
    profile.pitchScale = 2.0;
    profile.rollScale = 0.5;

    Rotation target = ShapeHeadRotation(relative, profile);

    REQUIRE(target.yaw == Approx(25.0));
    REQUIRE(target.pitch == Approx(-8.0));
    REQUIRE(target.roll == Approx(1.5));
}

TEST_CASE("ShapeHeadRotation clamps each scaled axis to the profile maximum", "[shaping]") {
    HeadPose relative;
    relative.yawDeg = 30.0;
    relative.pitchDeg = -30.0;
    relative.rollDeg = 20.0;
    ProfileConfig profile;
    profile.yawScale = 3.0;
    profile.pitchScale = 3.0;
    profile.rollScale = 2.0;
    profile.maxYawDeg = 70.0;
    profile.maxPitchDeg = 50.0;
    profile.maxRollDeg = 15.0;

    Rotation target = ShapeHeadRotation(relative, profile);

    REQUIRE(target.yaw == Approx(70.0));
    REQUIRE(target.pitch == Approx(-50.0));
    REQUIRE(target.roll == Approx(15.0));
}

TEST_CASE("ShapeHeadRotation with a zero scale holds that axis still", "[shaping]") {
    HeadPose relative;
    relative.rollDeg = 12.0;
    ProfileConfig profile;
    profile.rollScale = 0.0;

    REQUIRE(ShapeHeadRotation(relative, profile).roll == Approx(0.0));
}

TEST_CASE("ShapeHeadLean maps head movement to side, up and forward lean", "[shaping]") {
    HeadPose relative;
    relative.xCm = -4.0;
    relative.yCm = 2.0;
    relative.zCm = -6.0;
    ProfileConfig profile;

    Lean lean = ShapeHeadLean(relative, profile);

    REQUIRE(lean.sideCm == Approx(4.0));
    REQUIRE(lean.upCm == Approx(2.0));
    REQUIRE(lean.forwardCm == Approx(6.0));
}

TEST_CASE("ShapeHeadLean scales, clamps and inverts each axis", "[shaping]") {
    HeadPose relative;
    relative.xCm = -10.0;
    relative.yCm = 10.0;
    relative.zCm = -3.0;
    ProfileConfig profile;
    profile.leanScale = 2.0;
    profile.maxLeanSideCm = 12.0;
    profile.maxLeanUpCm = 5.0;
    profile.invertLean.forward = true;

    Lean lean = ShapeHeadLean(relative, profile);

    REQUIRE(lean.sideCm == Approx(12.0));
    REQUIRE(lean.upCm == Approx(5.0));
    REQUIRE(lean.forwardCm == Approx(-6.0));
}

TEST_CASE("ShapeHeadLean is zero when lean is disabled", "[shaping]") {
    HeadPose relative;
    relative.xCm = 8.0;
    relative.zCm = -8.0;
    ProfileConfig profile;
    profile.leanEnabled = false;

    Lean lean = ShapeHeadLean(relative, profile);

    REQUIRE(lean.sideCm == Approx(0.0));
    REQUIRE(lean.forwardCm == Approx(0.0));
}
