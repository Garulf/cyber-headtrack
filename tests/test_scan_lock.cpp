#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include "scan_lock.h"

using Catch::Approx;

namespace {

constexpr double kFrame = 1.0 / 60.0;
constexpr double kAspect = 16.0 / 9.0;

bool OnScreen(ScreenPoint gaze) {
    return gaze.x >= 0.0 && gaze.x <= 1.0 && gaze.y >= 0.0 && gaze.y <= 1.0;
}

LockResult Run(ScanLock& lock, ScreenPoint gaze, const std::vector<ScanCandidate>& candidates,
               double seconds, double weight = 1.0) {
    LockResult result;
    for (double t = 0.0; t < seconds - 1e-9; t += kFrame) {
        result = lock.Update(gaze, weight, OnScreen(gaze), candidates, kAspect, kFrame);
    }
    return result;
}

LockResult Step(ScanLock& lock, ScreenPoint gaze, const std::vector<ScanCandidate>& candidates,
                double weight = 1.0) {
    return lock.Update(gaze, weight, OnScreen(gaze), candidates, kAspect, kFrame);
}

void RequirePoint(ScreenPoint actual, ScreenPoint expected) {
    REQUIRE(actual.x == Approx(expected.x).margin(1e-9));
    REQUIRE(actual.y == Approx(expected.y).margin(1e-9));
}

}  // namespace

TEST_CASE("ScanLock locks the candidate under the gaze", "[scan_lock]") {
    ScanLock lock{GazeConfig{}};

    const LockResult result = Step(lock, {0.61, 0.5}, {{7, {0.6, 0.5}}});

    REQUIRE(result.lockedId == 7u);
    RequirePoint(result.point, {0.6, 0.5});
}

TEST_CASE("ScanLock ignores candidates outside the lock radius", "[scan_lock]") {
    ScanLock lock{GazeConfig{}};

    const LockResult result = Step(lock, {0.5, 0.5}, {{7, {0.8, 0.5}}});

    REQUIRE_FALSE(result.lockedId);
    RequirePoint(result.point, {0.5, 0.5});
}

TEST_CASE("ScanLock keeps the lock when gaze is on empty space", "[scan_lock]") {
    ScanLock lock{GazeConfig{}};
    const std::vector<ScanCandidate> candidates{{7, {0.6, 0.5}}};
    Step(lock, {0.6, 0.5}, candidates);

    const LockResult result = Run(lock, {0.1, 0.9}, candidates, 2.0);

    REQUIRE(result.lockedId == 7u);
    RequirePoint(result.point, {0.6, 0.5});
}

TEST_CASE("ScanLock releases on empty space when sticky_lock is off", "[scan_lock]") {
    GazeConfig config;
    config.stickyLock = false;
    ScanLock lock{config};
    const std::vector<ScanCandidate> candidates{{7, {0.6, 0.5}}};
    Step(lock, {0.6, 0.5}, candidates);

    const LockResult result = Step(lock, {0.1, 0.9}, candidates);

    REQUIRE_FALSE(result.lockedId);
    RequirePoint(result.point, {0.1, 0.9});
}

TEST_CASE("ScanLock prefers the locked target over a nearer one", "[scan_lock]") {
    ScanLock lock{GazeConfig{}};
    const std::vector<ScanCandidate> candidates{{1, {0.50, 0.5}}, {2, {0.53, 0.5}}};
    REQUIRE(Step(lock, {0.50, 0.5}, candidates).lockedId == 1u);

    const LockResult result = Run(lock, {0.52, 0.5}, candidates, 2.0);

    REQUIRE(result.lockedId == 1u);
}

TEST_CASE("ScanLock switches only after the dwell", "[scan_lock]") {
    ScanLock lock{GazeConfig{}};
    const std::vector<ScanCandidate> candidates{{1, {0.3, 0.5}}, {2, {0.7, 0.5}}};
    Step(lock, {0.3, 0.5}, candidates);

    REQUIRE(Run(lock, {0.7, 0.5}, candidates, 0.15).lockedId == 1u);
    const LockResult switched = Run(lock, {0.7, 0.5}, candidates, 0.1);

    REQUIRE(switched.lockedId == 2u);
    RequirePoint(switched.point, {0.7, 0.5});
}

TEST_CASE("ScanLock restarts the dwell when gaze moves between others", "[scan_lock]") {
    ScanLock lock{GazeConfig{}};
    const std::vector<ScanCandidate> candidates{{1, {0.1, 0.5}}, {2, {0.5, 0.5}}, {3, {0.9, 0.5}}};
    Step(lock, {0.1, 0.5}, candidates);

    Run(lock, {0.5, 0.5}, candidates, 0.15);
    REQUIRE(Run(lock, {0.9, 0.5}, candidates, 0.15).lockedId == 1u);

    REQUIRE(Run(lock, {0.9, 0.5}, candidates, 0.1).lockedId == 3u);
}

TEST_CASE("ScanLock releases a target that is no longer a candidate", "[scan_lock]") {
    ScanLock lock{GazeConfig{}};
    Step(lock, {0.6, 0.5}, {{7, {0.6, 0.5}}});

    const LockResult held = Step(lock, {0.1, 0.9}, {});
    REQUIRE(held.lockedId == 7u);
    RequirePoint(held.point, {0.6, 0.5});

    const LockResult result = Run(lock, {0.1, 0.9}, {}, kLockLossGraceSeconds + 0.05);
    REQUIRE_FALSE(result.lockedId);
    RequirePoint(result.point, {0.1, 0.9});
}

TEST_CASE("ScanLock keeps the lock through a one-frame dropout next to a neighbour", "[scan_lock]") {
    ScanLock lock{GazeConfig{}};
    const ScanCandidate first{1, {0.50, 0.5}};
    const ScanCandidate neighbour{2, {0.53, 0.5}};
    REQUIRE(Step(lock, {0.50, 0.5}, {first, neighbour}).lockedId == 1u);

    REQUIRE(Step(lock, {0.52, 0.5}, {neighbour}).lockedId == 1u);

    REQUIRE(Run(lock, {0.52, 0.5}, {first, neighbour}, 1.0).lockedId == 1u);
}

TEST_CASE("ScanLock treats off-screen gaze as empty space", "[scan_lock]") {
    ScanLock lock{GazeConfig{}};
    const std::vector<ScanCandidate> candidates{{7, {0.3, 0.5}}, {8, {1.0, 0.5}}};
    Step(lock, {0.3, 0.5}, candidates);

    LockResult result;
    for (double t = 0.0; t < 2.0; t += kFrame) {
        result = lock.Update({1.0, 0.5}, 1.0, false, candidates, kAspect, kFrame);
    }

    REQUIRE(result.lockedId == 7u);
}

TEST_CASE("ScanLock does not lock while gaze is easing", "[scan_lock]") {
    ScanLock lock{GazeConfig{}};
    const std::vector<ScanCandidate> candidates{{7, {0.6, 0.5}}};

    REQUIRE_FALSE(Step(lock, {0.6, 0.5}, candidates, 0.6).lockedId);

    REQUIRE(Step(lock, {0.6, 0.5}, candidates).lockedId == 7u);
    const LockResult easing = Step(lock, {0.6, 0.5}, candidates, 0.6);
    REQUIRE_FALSE(easing.lockedId);
    REQUIRE_FALSE(Step(lock, {0.1, 0.9}, candidates).lockedId);
}

TEST_CASE("ScanLock measures the radius in screen heights", "[scan_lock]") {
    ScanLock inside{GazeConfig{}};
    REQUIRE(Step(inside, {0.5, 0.5}, {{1, {0.5 + 0.05 / kAspect, 0.5}}}).lockedId == 1u);

    ScanLock outside{GazeConfig{}};
    REQUIRE_FALSE(Step(outside, {0.5, 0.5}, {{1, {0.5 + 0.07 / kAspect, 0.5}}}).lockedId);
}
