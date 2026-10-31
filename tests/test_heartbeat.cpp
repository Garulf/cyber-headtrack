#include <catch2/catch_test_macros.hpp>
#include "heartbeat.h"

TEST_CASE("Heartbeat is not alive before the first beat", "[heartbeat]") {
    const Heartbeat heartbeat{0.2};
    REQUIRE_FALSE(heartbeat.Alive(0.0));
    REQUIRE_FALSE(heartbeat.Alive(100.0));
}

TEST_CASE("Heartbeat stays alive for its timeout after a beat", "[heartbeat]") {
    Heartbeat heartbeat{0.2};
    heartbeat.Beat(10.0);

    REQUIRE(heartbeat.Alive(10.0));
    REQUIRE(heartbeat.Alive(10.19));
    REQUIRE_FALSE(heartbeat.Alive(10.21));

    heartbeat.Beat(10.3);
    REQUIRE(heartbeat.Alive(10.4));
}
