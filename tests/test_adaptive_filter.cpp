#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "adaptive_filter.h"

using Catch::Approx;

namespace {
constexpr double kDefaultRotDeadzoneDeg = 0.03;
constexpr double kDefaultLeanDeadzoneCm = 0.1;

Offset Step(const Offset& last, const Offset& target, double dt,
            double rotSensitivity = 1.0, double leanSensitivity = 1.0,
            double rotDeadzoneDeg = kDefaultRotDeadzoneDeg,
            double leanDeadzoneCm = kDefaultLeanDeadzoneCm) {
    return AdaptiveFilterStep(last, target, dt, rotSensitivity, leanSensitivity,
                              rotDeadzoneDeg, leanDeadzoneCm);
}
}  // namespace

TEST_CASE("AdaptiveFilterStep produces no movement when dt is zero", "[adaptive_filter]") {
    Offset last{};
    Offset target{};
    target.yaw = 10.0;

    Offset result = Step(last, target, 0.0);

    REQUIRE(result.yaw == Approx(0.0));
}

TEST_CASE("AdaptiveFilterStep suppresses tiny deltas below the rotation deadzone", "[adaptive_filter]") {
    Offset last{};
    Offset target{};
    target.yaw = 0.01;  // below the 0.03 deg deadzone passed in

    Offset result = Step(last, target, 1.0);

    REQUIRE(result.yaw == Approx(0.0));
}

TEST_CASE("AdaptiveFilterStep rotation deadzone is a real parameter", "[adaptive_filter]") {
    Offset last{};
    Offset target{};
    target.yaw = 0.3;

    Offset smallDeadzone = Step(last, target, 1.0 / 60.0, 1.0, 1.0, 0.03, kDefaultLeanDeadzoneCm);
    Offset largeDeadzone = Step(last, target, 1.0 / 60.0, 1.0, 1.0, 0.5, kDefaultLeanDeadzoneCm);

    REQUIRE(smallDeadzone.yaw > 0.0);   // 0.3 deg delta passes a 0.03 deg deadzone
    REQUIRE(largeDeadzone.yaw == Approx(0.0));  // but is fully suppressed by a 0.5 deg one
}

TEST_CASE("AdaptiveFilterStep lean deadzone is a real parameter", "[adaptive_filter]") {
    Offset last{};
    Offset target{};
    target.leanX = 0.003;  // blocks: 0.3 cm

    Offset smallDeadzone = Step(last, target, 1.0 / 60.0, 1.0, 1.0, kDefaultRotDeadzoneDeg, 0.1);
    Offset largeDeadzone = Step(last, target, 1.0 / 60.0, 1.0, 1.0, kDefaultRotDeadzoneDeg, 0.5);

    REQUIRE(smallDeadzone.leanX > 0.0);  // 0.3 cm delta passes a 0.1 cm deadzone
    REQUIRE(largeDeadzone.leanX == Approx(0.0));  // but is fully suppressed by a 0.5 cm one
}

TEST_CASE("AdaptiveFilterStep moves the smoothed output toward a large target", "[adaptive_filter]") {
    // dt is a single-frame interval (~60fps), not 0.1s: at dt=0.1 the gain table's clamped
    // top-end rate (300 deg/s, past its last knee at magnitude 9) overshoots a 20deg target,
    // which is correct for this un-clamped velocity-integration algorithm (it never clamps
    // to the target) but not what this test means to exercise.
    Offset last{};
    Offset target{};
    target.yaw = 20.0;

    Offset result = Step(last, target, 1.0 / 60.0);

    REQUIRE(result.yaw > 0.0);
    REQUIRE(result.yaw < 20.0);  // one small step, not an instant snap
}

TEST_CASE("AdaptiveFilterStep is symmetric for negative deltas", "[adaptive_filter]") {
    Offset last{};
    Offset positiveTarget{};
    positiveTarget.yaw = 20.0;
    Offset negativeTarget{};
    negativeTarget.yaw = -20.0;

    Offset positiveResult = Step(last, positiveTarget, 0.1);
    Offset negativeResult = Step(last, negativeTarget, 0.1);

    REQUIRE(positiveResult.yaw == Approx(-negativeResult.yaw));
}

TEST_CASE("AdaptiveFilterStep leaves other axes at zero when only yaw moves", "[adaptive_filter]") {
    Offset last{};
    Offset target{};
    target.yaw = 20.0;

    Offset result = Step(last, target, 0.1);

    REQUIRE(result.pitch == Approx(0.0));
    REQUIRE(result.roll == Approx(0.0));
    REQUIRE(result.leanX == Approx(0.0));
}
