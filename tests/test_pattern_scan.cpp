#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <stdexcept>
#include <vector>
#include "pattern_scan.h"

TEST_CASE("ParsePattern reads hex bytes and ?? wildcards", "[pattern_scan]") {
    const Pattern pattern = ParsePattern("48 8B ?? E8");

    REQUIRE(pattern.size() == 4);
    REQUIRE(pattern[0] == std::optional<uint8_t>(0x48));
    REQUIRE_FALSE(pattern[2].has_value());
    REQUIRE(pattern[3] == std::optional<uint8_t>(0xE8));
}

TEST_CASE("ParsePattern rejects malformed text", "[pattern_scan]") {
    REQUIRE_THROWS_AS(ParsePattern("48 8"), std::invalid_argument);
    REQUIRE_THROWS_AS(ParsePattern("ZZ"), std::invalid_argument);
    REQUIRE_THROWS_AS(ParsePattern(""), std::invalid_argument);
}

TEST_CASE("FindPattern returns every match offset, honouring wildcards", "[pattern_scan]") {
    const std::vector<uint8_t> bytes = {0x90, 0x48, 0x8B, 0x01, 0xE8, 0x48, 0x8B, 0x7F, 0xE8, 0x48};

    const std::vector<size_t> matches = FindPattern(bytes, ParsePattern("48 8B ?? E8"));

    REQUIRE(matches == std::vector<size_t>{1, 5});
}

TEST_CASE("FindPattern finds nothing when the pattern runs past the end", "[pattern_scan]") {
    const std::vector<uint8_t> bytes = {0x48, 0x8B};

    REQUIRE(FindPattern(bytes, ParsePattern("48 8B ?? E8")).empty());
}

TEST_CASE("ResolveRelativeCall follows an E8 rel32 call to its target", "[pattern_scan]") {
    const std::vector<uint8_t> bytes = {0x00, 0xE8, 0x10, 0x00, 0x00, 0x00, 0x00};

    REQUIRE(ResolveRelativeCall(bytes, 1) == 1 + 5 + 0x10);
}
