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
