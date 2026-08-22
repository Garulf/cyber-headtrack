#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "head_pose.h"

using Catch::Approx;

TEST_CASE("MapTobiiSample converts position mm to cm with axis flips", "[head_pose]") {
    RawTobiiSample sample{};
    sample.positionXyzMm[0] = 10.0f;  // +x = right in Tobii space
    sample.positionXyzMm[1] = 20.0f;  // +y = up
    sample.positionXyzMm[2] = 30.0f;  // +z = towards user
    sample.rotationXyzRad[0] = 0.0f;
    sample.rotationXyzRad[1] = 0.0f;
    sample.rotationXyzRad[2] = 0.0f;

    HeadPose pose = MapTobiiSample(sample);

    REQUIRE(pose.xCm == Approx(-1.0));  // negated, mm->cm
    REQUIRE(pose.yCm == Approx(2.0));
    REQUIRE(pose.zCm == Approx(3.0));
}

TEST_CASE("MapTobiiSample converts rotation rad to deg with yaw negated", "[head_pose]") {
    RawTobiiSample sample{};
    const double pi = 3.14159265358979323846;
    sample.rotationXyzRad[0] = static_cast<float>(pi / 4.0);  // pitch: 45 deg
    sample.rotationXyzRad[1] = static_cast<float>(pi / 6.0);  // yaw source: 30 deg, expect -30
    sample.rotationXyzRad[2] = static_cast<float>(pi / 2.0);  // roll: 90 deg

    HeadPose pose = MapTobiiSample(sample);

    REQUIRE(pose.pitchDeg == Approx(45.0).margin(0.01));
    REQUIRE(pose.yawDeg == Approx(-30.0).margin(0.01));
    REQUIRE(pose.rollDeg == Approx(90.0).margin(0.01));
}

TEST_CASE("MapTobiiSample is a pure function of its input", "[head_pose]") {
    RawTobiiSample sample{};
    sample.positionXyzMm[0] = 5.0f;
    sample.rotationXyzRad[1] = 0.1f;

    HeadPose first = MapTobiiSample(sample);
    HeadPose second = MapTobiiSample(sample);

    REQUIRE(first.xCm == second.xCm);
    REQUIRE(first.yawDeg == second.yawDeg);
}

TEST_CASE("SubtractReference maps the reference pose itself to all-zero", "[head_pose]") {
    HeadPose reference{};
    reference.xCm = 2.0;
    reference.yCm = -1.5;
    reference.zCm = 60.0;
    reference.yawDeg = 5.0;
    reference.pitchDeg = -3.0;
    reference.rollDeg = 1.0;

    HeadPose relative = SubtractReference(reference, reference);

    REQUIRE(relative.xCm == Approx(0.0));
    REQUIRE(relative.yCm == Approx(0.0));
    REQUIRE(relative.zCm == Approx(0.0));
    REQUIRE(relative.yawDeg == Approx(0.0));
    REQUIRE(relative.pitchDeg == Approx(0.0));
    REQUIRE(relative.rollDeg == Approx(0.0));
}

TEST_CASE("SubtractReference yields the delta from the reference on every axis", "[head_pose]") {
    HeadPose reference{};
    reference.yawDeg = 5.0;
    reference.pitchDeg = -3.0;
    reference.rollDeg = 1.0;
    reference.xCm = 2.0;

    HeadPose pose{};
    pose.yawDeg = 15.0;
    pose.pitchDeg = -1.0;
    pose.rollDeg = -2.0;
    pose.xCm = 2.5;

    HeadPose relative = SubtractReference(pose, reference);

    REQUIRE(relative.yawDeg == Approx(10.0));
    REQUIRE(relative.pitchDeg == Approx(2.0));
    REQUIRE(relative.rollDeg == Approx(-3.0));
    REQUIRE(relative.xCm == Approx(0.5));
}

TEST_CASE("SubtractReference with a default reference is the identity", "[head_pose]") {
    HeadPose pose{};
    pose.yawDeg = 12.0;
    pose.pitchDeg = -4.0;
    pose.zCm = 55.0;

    HeadPose relative = SubtractReference(pose, HeadPose{});

    REQUIRE(relative.yawDeg == Approx(12.0));
    REQUIRE(relative.pitchDeg == Approx(-4.0));
    REQUIRE(relative.zCm == Approx(55.0));
}
