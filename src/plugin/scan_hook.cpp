#include "scan_hook.h"

#include <Windows.h>

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>

#include "aim_hook.h"
#include "aim_math.h"
#include "code_patch.h"
#include "pattern_scan.h"
#include "plugin_log.h"
#include "tracking_state.h"

namespace {

using AnglesFromForwardFn = void* (*)(float* forward, float* outAngles);
using GetForwardFn = float* (*)(const float* transform, float* out);

constexpr double kMaxOriginDistanceMeters = 1.5;
constexpr Vec3 kForward{0.0, 1.0, 0.0};

// The targeting record update passes &record[+0x31200] (the crosshair forward); the record's
// crosshair origin sits at +0x311F0 and this frame's camera quaternion at +0x31110. In floats:
constexpr ptrdiff_t kRecordOriginFromForward = -0x10 / 4;
constexpr ptrdiff_t kRecordCameraFromForward = -0xF0 / 4;
// The look-at transform holds its position at +0x0 and its quaternion at +0x10.
constexpr ptrdiff_t kTransformQuaternion = 0x10 / 4;

struct SiteStats {
    std::atomic<uint64_t> calls{0};
    std::atomic<uint64_t> player{0};
    std::atomic<uint64_t> redirected{0};
    // Angle between the engine's own ray and the camera forward on the last player call.
    std::atomic<float> angleDegrees{0.0f};
    bool reportedFirstCall = false;
};

struct ScanSite {
    const char* name;
    const char* signature;
    size_t callOffset;
    const char* calleeSignature;
    void* replacement;

    void* original = nullptr;
    uint8_t* call = nullptr;
    int32_t originalDisplacement = 0;
};

enum SiteIndex { kRecordSite, kLookAtSite };

void* RecordAnglesFromForward(float* forward, float* outAngles);
float* LookAtForward(const float* transform, float* out);

ScanSite gSites[] = {
    {"targeting record forward",
     "48 8D 55 C0 49 8B CF E8 ?? ?? ?? ?? F3 0F 10 40 04 F3 0F 10 48 08 8B 00 89 87 10 12 03 00", 7,
     "48 8B C4 48 89 58 08 57 48 83 EC 50 0F 29 70 E8 48 8B DA 0F 29 78 D8 48 8B F9 44 0F 29 40 C8 "
     "0F 57 FF F3 44 0F 10 01",
     reinterpret_cast<void*>(&RecordAnglesFromForward)},
    {"look-at ray direction",
     "41 FF D1 48 8B 45 90 48 8D 55 F0 48 89 44 24 30 48 8D 4D 90 48 8B 45 98 48 89 44 24 38 "
     "E8 ?? ?? ?? ??",
     29, "48 83 EC 28 48 83 C1 10 4C 8B CA E8 ?? ?? ?? ?? 49 8B C1 48 83 C4 28 C3",
     reinterpret_cast<void*>(&LookAtForward)},
};

SiteStats gStats[std::size(gSites)];

Vec3 ToVec3(const float* v) {
    return {v[0], v[1], v[2]};
}

Quat ToQuat(const float* q) {
    return {q[0], q[1], q[2], q[3]};
}

bool NearCamera(const float* point, const Vec3& camera) {
    const double dx = point[0] - camera.x;
    const double dy = point[1] - camera.y;
    const double dz = point[2] - camera.z;
    const double distanceSq = dx * dx + dy * dy + dz * dz;
    return std::isfinite(distanceSq) &&
           distanceSq <= kMaxOriginDistanceMeters * kMaxOriginDistanceMeters;
}

// Replaces the player's ray direction (xyz only) with the scan point seen from camera.
void Redirect(SiteStats& stats, float* direction, const Quat& camera, const Vec3& cameraForward) {
    stats.player.fetch_add(1, std::memory_order_relaxed);
    stats.angleDegrees.store(static_cast<float>(AngleDegrees(ToVec3(direction), cameraForward)),
                             std::memory_order_relaxed);
    const auto scan = TrackingState::Instance().ScanCameraDirection();
    if (!scan) {
        return;
    }
    const Vec3 world = Rotate(camera, *scan);
    direction[0] = static_cast<float>(world.x);
    direction[1] = static_cast<float>(world.y);
    direction[2] = static_cast<float>(world.z);
    stats.redirected.fetch_add(1, std::memory_order_relaxed);
}

void* RecordAnglesFromForward(float* forward, float* outAngles) {
    SiteStats& stats = gStats[kRecordSite];
    stats.calls.fetch_add(1, std::memory_order_relaxed);
    if (forward != nullptr &&
        NearCamera(forward + kRecordOriginFromForward, CurrentPublishedCamera().position)) {
        const Quat camera = ToQuat(forward + kRecordCameraFromForward);
        Redirect(stats, forward, camera, Rotate(camera, kForward));
    }
    return reinterpret_cast<AnglesFromForwardFn>(gSites[kRecordSite].original)(forward, outAngles);
}

float* LookAtForward(const float* transform, float* out) {
    SiteStats& stats = gStats[kLookAtSite];
    float* result = reinterpret_cast<GetForwardFn>(gSites[kLookAtSite].original)(transform, out);
    stats.calls.fetch_add(1, std::memory_order_relaxed);
    if (transform == nullptr || result == nullptr) {
        return result;
    }
    const PublishedCamera camera = CurrentPublishedCamera();
    if (NearCamera(transform, camera.position)) {
        Redirect(stats, result, ToQuat(transform + kTransformQuaternion),
                 Rotate(camera.world, kForward));
    }
    return result;
}

// Finds the site and checks it calls the expected function, without patching anything.
uint8_t* LocateSite(ScanSite& site, std::span<const uint8_t> text) {
    const auto matches = FindPattern(text, ParsePattern(site.signature));
    if (matches.size() != 1) {
        PluginLog("scan: %s signature matched %zu times, skipped", site.name, matches.size());
        return nullptr;
    }
    const size_t callOffset = matches.front() + site.callOffset;
    const size_t targetOffset = ResolveRelativeCall(text, callOffset);
    const auto callee = ParsePattern(site.calleeSignature);
    if (targetOffset + callee.size() > text.size() ||
        FindPattern(text.subspan(targetOffset, callee.size()), callee).empty()) {
        PluginLog("scan: %s does not call the expected function, skipped", site.name);
        return nullptr;
    }
    site.original = const_cast<uint8_t*>(text.data() + targetOffset);
    return const_cast<uint8_t*>(text.data() + callOffset);
}

}  // namespace

void InstallScanHooks() {
    auto* module = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
    const auto text = TextSection(module);
    if (text.empty()) {
        PluginLog("scan: game code section not found, the scanner targets the screen centre");
        return;
    }
    for (ScanSite& site : gSites) {
        uint8_t* call = LocateSite(site, text);
        if (call == nullptr) {
            continue;
        }
        if (!RedirectCall(call, site.replacement, site.originalDisplacement)) {
            PluginLog("scan: no memory within reach of %s, skipped", site.name);
            continue;
        }
        site.call = call;
        PluginLog("scan: hooked %s at +0x%zX", site.name, static_cast<size_t>(call - module));
    }
}

void RemoveScanHooks() {
    for (ScanSite& site : gSites) {
        if (site.call == nullptr) {
            continue;
        }
        RestoreCall(site.call, site.originalDisplacement);
        site.call = nullptr;
    }
}

void ReportScanStats() {
    const bool scanning = TrackingState::Instance().CurrentScanPoint().weight > 0.0;
    for (size_t i = 0; i < std::size(gSites); ++i) {
        const ScanSite& site = gSites[i];
        SiteStats& stats = gStats[i];
        const uint64_t calls = stats.calls.load(std::memory_order_relaxed);
        const bool firstCall = calls > 0 && !stats.reportedFirstCall;
        if (!scanning && !firstCall) {
            continue;
        }
        stats.reportedFirstCall = stats.reportedFirstCall || calls > 0;
        PluginLog("scan: %s calls=%llu player=%llu redirected=%llu angle_to_camera=%.1f deg", site.name,
                  static_cast<unsigned long long>(calls),
                  static_cast<unsigned long long>(stats.player.load(std::memory_order_relaxed)),
                  static_cast<unsigned long long>(stats.redirected.load(std::memory_order_relaxed)),
                  stats.angleDegrees.load(std::memory_order_relaxed));
    }
}
