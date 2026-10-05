#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <numbers>
#include "aim_math.h"

using Catch::Approx;

namespace {

constexpr Vec3 kForward{0.0, 1.0, 0.0};

Quat AboutZ(double degrees) {
    const double half = degrees * std::numbers::pi / 360.0;
    return {0.0, 0.0, std::sin(half), std::cos(half)};
}

Quat AboutX(double degrees) {
    const double half = degrees * std::numbers::pi / 360.0;
    return {std::sin(half), 0.0, 0.0, std::cos(half)};
}

void RequireSame(const Vec3& a, const Vec3& b) {
    REQUIRE(a.x == Approx(b.x).margin(1e-9));
    REQUIRE(a.y == Approx(b.y).margin(1e-9));
    REQUIRE(a.z == Approx(b.z).margin(1e-9));
}

}  // namespace

TEST_CASE("Rotate turns forward by a yaw quaternion", "[aim_math]") {
    RequireSame(Rotate(AboutZ(90.0), kForward), Vec3{-1.0, 0.0, 0.0});
}

TEST_CASE("CleanAimDirection removes the head rotation from the camera forward", "[aim_math]") {
    const Quat body = Multiply(AboutZ(40.0), AboutX(-10.0));
    const Quat head = Multiply(AboutZ(25.0), AboutX(12.0));
    const Quat camera = Multiply(body, head);

    const Vec3 shot = Rotate(camera, kForward);

    RequireSame(CleanAimDirection(camera, head, shot), Rotate(body, kForward));
}

TEST_CASE("CleanAimDirection keeps the spread around the camera forward", "[aim_math]") {
    const Quat body = AboutZ(-30.0);
    const Quat head = AboutZ(20.0);
    const Quat camera = Multiply(body, head);
    const Quat spread = AboutX(2.0);

    const Vec3 shot = Rotate(Multiply(camera, spread), kForward);

    RequireSame(CleanAimDirection(camera, head, shot), Rotate(Multiply(body, spread), kForward));
}

TEST_CASE("CleanAimDirection leaves the direction alone without head rotation", "[aim_math]") {
    const Quat camera = AboutZ(70.0);
    const Vec3 shot = Rotate(camera, kForward);

    RequireSame(CleanAimDirection(camera, Quat{}, shot), shot);
}

TEST_CASE("IsNearForward accepts only directions close to the camera forward", "[aim_math]") {
    const Quat camera = AboutZ(15.0);

    REQUIRE(IsNearForward(camera, Rotate(Multiply(camera, AboutX(5.0)), kForward), 10.0));
    REQUIRE_FALSE(IsNearForward(camera, Rotate(Multiply(camera, AboutX(30.0)), kForward), 10.0));
    REQUIRE_FALSE(IsNearForward(camera, Vec3{0.0, -1.0, 0.0}, 10.0));
}

TEST_CASE("FromBasis rebuilds the orientation whose axes are right, forward and up", "[aim_math]") {
    const Quat camera = Multiply(Multiply(AboutZ(135.0), AboutX(-25.0)), Quat{0.0, std::sin(0.2), 0.0, std::cos(0.2)});
    const Vec3 right = Rotate(camera, Vec3{1.0, 0.0, 0.0});
    const Vec3 forward = Rotate(camera, kForward);
    const Vec3 up = Rotate(camera, Vec3{0.0, 0.0, 1.0});

    const Quat rebuilt = FromBasis(right, forward, up);

    const Vec3 probe{0.3, -0.7, 0.4};
    RequireSame(Rotate(rebuilt, probe), Rotate(camera, probe));
}

TEST_CASE("FromBasis handles a camera turned half way round", "[aim_math]") {
    const Quat camera = AboutZ(180.0);

    const Quat rebuilt = FromBasis(Rotate(camera, Vec3{1.0, 0.0, 0.0}), Rotate(camera, kForward),
                                   Rotate(camera, Vec3{0.0, 0.0, 1.0}));

    RequireSame(Rotate(rebuilt, kForward), Vec3{0.0, -1.0, 0.0});
}
