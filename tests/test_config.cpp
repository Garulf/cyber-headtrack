#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <filesystem>
#include <fstream>
#include "config.h"

using Catch::Approx;

namespace {

void WriteFile(const std::filesystem::path& path, const char* contents) {
    std::ofstream out(path);
    out << contents;
}

}  // namespace

TEST_CASE("LoadConfig returns defaults when the file does not exist", "[config]") {
    TrackingConfig config = LoadConfig("does_not_exist.toml");

    REQUIRE(config.onFoot.smoothingMinCutoffHz == Approx(1.0));
    REQUIRE(config.onFoot.maxYawDeg == Approx(45.0));
    REQUIRE(config.vehicle.yawScale == Approx(1.0));
    REQUIRE(config.vehicle.maxRollDeg == Approx(45.0));
    REQUIRE(config.onFoot.leanEnabled);
    REQUIRE_FALSE(config.vehicle.leanEnabled);
}

TEST_CASE("LoadConfig keeps the vehicle lean default unless a key sets it", "[config]") {
    const std::string path = "test_config_lean.toml";
    WriteFile(path, "max_lean_side_cm = 25.0\n[on_foot]\nlean_scale = 1.5\n");

    TrackingConfig config = LoadConfig(path);

    REQUIRE(config.onFoot.leanEnabled);
    REQUIRE_FALSE(config.vehicle.leanEnabled);
    REQUIRE(config.vehicle.maxLeanSideCm == Approx(25.0));
    REQUIRE(config.onFoot.leanScale == Approx(1.5));
    REQUIRE(config.vehicle.leanScale == Approx(1.0));

    WriteFile(path, "lean_enabled = true\n");
    REQUIRE(LoadConfig(path).vehicle.leanEnabled);

    std::filesystem::remove(path);
}

TEST_CASE("LoadConfig applies top-level keys to both profiles", "[config]") {
    const std::string path = "test_config_override.toml";
    WriteFile(path, "smoothing_beta = 0.2\nmax_yaw_deg = 90.0\n");

    TrackingConfig config = LoadConfig(path);

    REQUIRE(config.onFoot.smoothingBeta == Approx(0.2));
    REQUIRE(config.vehicle.smoothingBeta == Approx(0.2));
    REQUIRE(config.onFoot.maxYawDeg == Approx(90.0));
    REQUIRE(config.vehicle.maxYawDeg == Approx(90.0));
    REQUIRE(config.onFoot.maxPitchDeg == Approx(45.0));

    std::filesystem::remove(path);
}

TEST_CASE("LoadConfig lets a profile section override the top-level keys", "[config]") {
    const std::string path = "test_config_sections.toml";
    WriteFile(path,
              "max_yaw_deg = 60.0\n"
              "[vehicle]\n"
              "max_yaw_deg = 120.0\n"
              "yaw_scale = 2.5\n");

    TrackingConfig config = LoadConfig(path);

    REQUIRE(config.onFoot.maxYawDeg == Approx(60.0));
    REQUIRE(config.vehicle.maxYawDeg == Approx(120.0));
    REQUIRE(config.vehicle.yawScale == Approx(2.5));
    REQUIRE(config.onFoot.yawScale == Approx(1.0));

    std::filesystem::remove(path);
}

TEST_CASE("LoadConfig throws a descriptive error on malformed TOML", "[config]") {
    const std::string path = "test_config_broken.toml";
    WriteFile(path, "this is not valid toml [[[\n");

    REQUIRE_THROWS_AS(LoadConfig(path), std::runtime_error);

    std::filesystem::remove(path);
}

TEST_CASE("SaveDefaultConfig writes a file that LoadConfig can read back", "[config]") {
    const std::string path = "test_config_roundtrip.toml";

    SaveDefaultConfig(path);
    TrackingConfig config = LoadConfig(path);

    REQUIRE(config.onFoot.smoothingMinCutoffHz == Approx(1.0));
    REQUIRE(config.vehicle.smoothingBeta == Approx(0.1));

    std::filesystem::remove(path);
}

TEST_CASE("LoadConfig accepts a non-ASCII path", "[config]") {
    const std::filesystem::path path = std::filesystem::path(u8"test_config_é配置.toml");
    WriteFile(path, "smoothing_min_cutoff_hz = 2.5\n");

    TrackingConfig config = LoadConfig(path);

    REQUIRE(config.onFoot.smoothingMinCutoffHz == Approx(2.5));

    std::filesystem::remove(path);
}

TEST_CASE("LoadConfig rejects out-of-range values", "[config]") {
    const std::string path = "test_config_range.toml";
    const char* badFiles[] = {
        "smoothing_min_cutoff_hz = 0.0\n",
        "smoothing_min_cutoff_hz = -2.0\n",
        "smoothing_beta = -0.1\n",
        "max_yaw_deg = -10.0\n",
        "max_pitch_deg = 200.0\n",
        "max_yaw_deg = nan\n",
        "smoothing_beta = inf\n",
        "yaw_scale = -1.0\n",
        "[vehicle]\npitch_scale = 50.0\n",
        "[on_foot]\nmax_yaw_deg = 999.0\n",
        "[vehicle]\nmax_roll_deg = -5.0\n",
        "max_lean_forward_cm = 80.0\n",
        "lean_scale = 11.0\n",
        "lean_smoothing_min_cutoff_hz = 0.0\n",
    };

    for (const char* contents : badFiles) {
        INFO(contents);
        WriteFile(path, contents);
        REQUIRE_THROWS_AS(LoadConfig(path), std::runtime_error);
    }

    std::filesystem::remove(path);
}

TEST_CASE("LoadConfig reads axis inversion flags", "[config]") {
    const std::string path = "test_config_invert.toml";
    WriteFile(path, "invert_pitch = true\n[vehicle]\ninvert_roll = true\n");

    TrackingConfig config = LoadConfig(path);

    REQUIRE(config.onFoot.invert.pitch);
    REQUIRE_FALSE(config.onFoot.invert.roll);
    REQUIRE(config.vehicle.invert.pitch);
    REQUIRE(config.vehicle.invert.roll);

    std::filesystem::remove(path);
}

TEST_CASE("GetConfigValue reads a profile's setting by section and key", "[config]") {
    TrackingConfig config;
    config.vehicle.maxPitchDeg = 30.0;
    config.onFoot.invert.roll = true;

    REQUIRE(GetConfigValue(config, "vehicle.max_pitch_deg") == Approx(30.0));
    REQUIRE(GetConfigValue(config, "on_foot.max_pitch_deg") == Approx(45.0));
    REQUIRE(GetConfigValue(config, "on_foot.invert_roll") == Approx(1.0));
    REQUIRE(GetConfigValue(config, "vehicle.invert_roll") == Approx(0.0));
    REQUIRE_FALSE(GetConfigValue(config, "vehicle.no_such_key").has_value());
    REQUIRE_FALSE(GetConfigValue(config, "max_pitch_deg").has_value());
    REQUIRE_FALSE(GetConfigValue(config, "boat.max_pitch_deg").has_value());
}

TEST_CASE("SetConfigValue updates only the named profile", "[config]") {
    TrackingConfig config;

    SetConfigValue(config, "vehicle.yaw_scale", 2.0);
    SetConfigValue(config, "on_foot.invert_pitch", 1.0);

    REQUIRE(config.vehicle.yawScale == Approx(2.0));
    REQUIRE(config.onFoot.yawScale == Approx(1.0));
    REQUIRE(config.onFoot.invert.pitch);
    REQUIRE_FALSE(config.vehicle.invert.pitch);
}

TEST_CASE("SetConfigValue rejects bad values and leaves the config untouched", "[config]") {
    TrackingConfig config;

    REQUIRE_THROWS_AS(SetConfigValue(config, "on_foot.smoothing_min_cutoff_hz", 0.0), std::runtime_error);
    REQUIRE_THROWS_AS(SetConfigValue(config, "vehicle.max_yaw_deg", 500.0), std::runtime_error);
    REQUIRE_THROWS_AS(SetConfigValue(config, "vehicle.no_such_key", 1.0), std::runtime_error);
    REQUIRE_THROWS_AS(SetConfigValue(config, "smoothing_beta", 0.5), std::runtime_error);

    REQUIRE(config.onFoot.smoothingMinCutoffHz == Approx(1.0));
    REQUIRE(config.vehicle.maxYawDeg == Approx(45.0));
    REQUIRE(config.onFoot.smoothingBeta == Approx(0.1));
}

TEST_CASE("SaveConfig writes both profiles so LoadConfig reads them back unchanged", "[config]") {
    const std::string path = "test_config_save.toml";
    TrackingConfig saved;
    saved.onFoot.smoothingMinCutoffHz = 0.7;
    saved.onFoot.maxYawDeg = 60.0;
    saved.vehicle.smoothingBeta = 0.25;
    saved.vehicle.yawScale = 2.5;
    saved.vehicle.invert.roll = true;

    SaveConfig(path, saved);
    TrackingConfig loaded = LoadConfig(path);

    REQUIRE(loaded.onFoot.smoothingMinCutoffHz == Approx(0.7));
    REQUIRE(loaded.onFoot.maxYawDeg == Approx(60.0));
    REQUIRE(loaded.onFoot.smoothingBeta == Approx(0.1));
    REQUIRE(loaded.vehicle.smoothingBeta == Approx(0.25));
    REQUIRE(loaded.vehicle.maxYawDeg == Approx(45.0));
    REQUIRE(loaded.vehicle.yawScale == Approx(2.5));
    REQUIRE(loaded.vehicle.invert.roll);
    REQUIRE_FALSE(loaded.onFoot.invert.roll);
    REQUIRE(loaded.onFoot.leanEnabled);
    REQUIRE_FALSE(loaded.vehicle.leanEnabled);

    std::filesystem::remove(path);
}

TEST_CASE("ADS eye defaults to the right eye with half a typical eye spacing", "[config]") {
    TrackingConfig config;

    REQUIRE(config.onFoot.adsEye == Approx(1.0));
    REQUIRE(config.onFoot.adsEyeOffsetCm == Approx(3.2));
    REQUIRE(AdsEyeOffsetCm(config.onFoot) == Approx(3.2));
}

TEST_CASE("AdsEyeOffsetCm is signed by the chosen eye", "[config]") {
    ProfileConfig profile;
    profile.adsEyeOffsetCm = 3.0;

    profile.adsEye = -1.0;
    REQUIRE(AdsEyeOffsetCm(profile) == Approx(-3.0));
    profile.adsEye = 0.0;
    REQUIRE(AdsEyeOffsetCm(profile) == Approx(0.0));
}

TEST_CASE("SetConfigValue accepts only left, centre or right for ads_eye", "[config]") {
    TrackingConfig config;

    SetConfigValue(config, "on_foot.ads_eye", -1.0);
    REQUIRE(config.onFoot.adsEye == Approx(-1.0));
    REQUIRE_THROWS_AS(SetConfigValue(config, "on_foot.ads_eye", 0.5), std::runtime_error);
    REQUIRE_THROWS_AS(SetConfigValue(config, "on_foot.ads_eye_offset_cm", 12.0), std::runtime_error);
}

TEST_CASE("ads_tracking_off is a per-profile flag, off by default", "[config]") {
    TrackingConfig config;
    REQUIRE_FALSE(config.onFoot.adsTrackingOff);

    SetConfigValue(config, "vehicle.ads_tracking_off", 1.0);

    REQUIRE(config.vehicle.adsTrackingOff);
    REQUIRE_FALSE(config.onFoot.adsTrackingOff);
    REQUIRE(GetConfigValue(config, "vehicle.ads_tracking_off") == Approx(1.0));
}

TEST_CASE("ads_ease_seconds defaults to a quarter second and stays within 0 to 2", "[config]") {
    TrackingConfig config;
    REQUIRE(config.onFoot.adsEaseSeconds == Approx(0.25));

    SetConfigValue(config, "on_foot.ads_ease_seconds", 0.0);
    REQUIRE(config.onFoot.adsEaseSeconds == Approx(0.0));
    REQUIRE_THROWS_AS(SetConfigValue(config, "on_foot.ads_ease_seconds", 3.0), std::runtime_error);
}
