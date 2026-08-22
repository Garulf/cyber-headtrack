#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <filesystem>
#include <fstream>
#include "config.h"

using Catch::Approx;

TEST_CASE("LoadConfig returns defaults when the file does not exist", "[config]") {
    TrackingConfig config = LoadConfig("does_not_exist.toml");

    REQUIRE(config.rotSensitivity == Approx(6.0));
    REQUIRE(config.maxYawDeg == Approx(45.0));
}

TEST_CASE("LoadConfig reads overridden values from a real file", "[config]") {
    const std::string path = "test_config_override.toml";
    {
        std::ofstream out(path);
        out << "rot_sensitivity = 8.0\n";
        out << "max_yaw_deg = 90.0\n";
    }

    TrackingConfig config = LoadConfig(path);

    REQUIRE(config.rotSensitivity == Approx(8.0));
    REQUIRE(config.maxYawDeg == Approx(90.0));
    REQUIRE(config.maxPitchDeg == Approx(45.0));  // untouched key keeps its default

    std::filesystem::remove(path);
}

TEST_CASE("LoadConfig throws a descriptive error on malformed TOML", "[config]") {
    const std::string path = "test_config_broken.toml";
    {
        std::ofstream out(path);
        out << "this is not valid toml [[[\n";
    }

    REQUIRE_THROWS_AS(LoadConfig(path), std::runtime_error);

    std::filesystem::remove(path);
}

TEST_CASE("SaveDefaultConfig writes a file that LoadConfig can read back", "[config]") {
    const std::string path = "test_config_roundtrip.toml";

    SaveDefaultConfig(path);
    TrackingConfig config = LoadConfig(path);

    REQUIRE(config.rotSensitivity == Approx(6.0));

    std::filesystem::remove(path);
}
