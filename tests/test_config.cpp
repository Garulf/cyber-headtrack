#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <filesystem>
#include <fstream>
#include "config.h"

using Catch::Approx;

TEST_CASE("LoadConfig returns defaults when the file does not exist", "[config]") {
    TrackingConfig config = LoadConfig("does_not_exist.toml");

    REQUIRE(config.smoothingMinCutoffHz == Approx(1.0));
    REQUIRE(config.maxYawDeg == Approx(45.0));
}

TEST_CASE("LoadConfig reads overridden values from a real file", "[config]") {
    const std::string path = "test_config_override.toml";
    {
        std::ofstream out(path);
        out << "smoothing_beta = 0.2\n";
        out << "max_yaw_deg = 90.0\n";
    }

    TrackingConfig config = LoadConfig(path);

    REQUIRE(config.smoothingBeta == Approx(0.2));
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

    REQUIRE(config.smoothingMinCutoffHz == Approx(1.0));
    REQUIRE(config.smoothingBeta == Approx(0.1));

    std::filesystem::remove(path);
}

TEST_CASE("LoadConfig accepts a non-ASCII path", "[config]") {
    const std::filesystem::path path = std::filesystem::path(u8"test_config_é配置.toml");
    {
        std::ofstream out(path);
        out << "smoothing_min_cutoff_hz = 2.5\n";
    }

    TrackingConfig config = LoadConfig(path);

    REQUIRE(config.smoothingMinCutoffHz == Approx(2.5));

    std::filesystem::remove(path);
}

TEST_CASE("LoadConfig rejects out-of-range values", "[config]") {
    const std::string path = "test_config_range.toml";
    const char* badLines[] = {
        "smoothing_min_cutoff_hz = 0.0\n",
        "smoothing_min_cutoff_hz = -2.0\n",
        "smoothing_beta = -0.1\n",
        "max_yaw_deg = -10.0\n",
        "max_pitch_deg = 200.0\n",
        "max_yaw_deg = nan\n",
        "smoothing_beta = inf\n",
    };

    for (const char* line : badLines) {
        INFO(line);
        {
            std::ofstream out(path);
            out << line;
        }
        REQUIRE_THROWS_AS(LoadConfig(path), std::runtime_error);
    }

    std::filesystem::remove(path);
}

TEST_CASE("LoadConfig reads axis inversion flags", "[config]") {
    const std::string path = "test_config_invert.toml";
    {
        std::ofstream out(path);
        out << "invert_pitch = true\n";
    }

    TrackingConfig config = LoadConfig(path);

    REQUIRE_FALSE(config.invert.yaw);
    REQUIRE(config.invert.pitch);
    REQUIRE_FALSE(config.invert.roll);

    std::filesystem::remove(path);
}
