#include "aim_hook.h"

#include <Windows.h>

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <span>

#include "code_patch.h"
#include "pattern_scan.h"
#include "plugin_log.h"

namespace {

using EvaluateFn = char (*)(void* parameter, void* blackboard, float* out);

constexpr double kMaxOriginDistanceMeters = 1.5;
constexpr double kMaxDegreesFromForward = 45.0;
constexpr char kEvaluateSignature[] =
    "48 83 EC 28 48 8B 49 08 48 85 C9 74 09 48 8B 01 FF 90 D8 00 00 00 48 83 C4 28 C3";

struct AimFrame {
    Quat cameraWorld;
    Vec3 cameraPosition;
    Quat head;
    bool active = false;
};

struct CallSite {
    const char* name;
    const char* signature;
    size_t callOffset;
    void* replacement;

    uint8_t* call = nullptr;
    int32_t originalDisplacement = 0;
};

EvaluateFn gEvaluate = nullptr;
std::mutex gFrameMutex;
AimFrame gFrame;

std::atomic<uint64_t> gCorrected{0};
std::atomic<uint64_t> gTrackingOff{0};
std::atomic<uint64_t> gFarFromForward{0};
uint64_t gReportedTotal = 0;

// Set by the origin evaluation and consumed by the direction evaluation a few instructions
// later in the same PhysicalRay::Execute call, on the same thread.
thread_local bool tPlayerRay = false;
thread_local AimFrame tFrame;

AimFrame CurrentFrame() {
    std::lock_guard lock(gFrameMutex);
    return gFrame;
}

double DistanceSquared(const float* point, const Vec3& other) {
    const double dx = point[0] - other.x;
    const double dy = point[1] - other.y;
    const double dz = point[2] - other.z;
    return dx * dx + dy * dy + dz * dz;
}

char EvaluateOrigin(void* parameter, void* blackboard, float* out) {
    const char result = gEvaluate(parameter, blackboard, out);
    tPlayerRay = false;
    if (!result || out == nullptr) {
        return result;
    }
    const AimFrame frame = CurrentFrame();
    const double distanceSq = DistanceSquared(out, frame.cameraPosition);
    if (std::isfinite(distanceSq) &&
        distanceSq <= kMaxOriginDistanceMeters * kMaxOriginDistanceMeters) {
        tPlayerRay = true;
        tFrame = frame;
    }
    return result;
}

char EvaluateDirection(void* parameter, void* blackboard, float* out) {
    const char result = gEvaluate(parameter, blackboard, out);
    const bool playerRay = tPlayerRay;
    tPlayerRay = false;
    if (!result || out == nullptr || !playerRay) {
        return result;
    }
    if (!tFrame.active) {
        gTrackingOff.fetch_add(1, std::memory_order_relaxed);
        return result;
    }
    const Vec3 shot{out[0], out[1], out[2]};
    if (!IsNearForward(tFrame.cameraWorld, shot, kMaxDegreesFromForward)) {
        gFarFromForward.fetch_add(1, std::memory_order_relaxed);
        return result;
    }
    const Vec3 clean = CleanAimDirection(tFrame.cameraWorld, tFrame.head, shot);
    out[0] = static_cast<float>(clean.x);
    out[1] = static_cast<float>(clean.y);
    out[2] = static_cast<float>(clean.z);
    gCorrected.fetch_add(1, std::memory_order_relaxed);
    return result;
}

std::array<CallSite, 2> gSites{{
    {"hitscan ray origin", "4C 8D 45 97 0F 29 45 97 E8 ?? ?? ?? ?? 49 8B 4E 48", 8,
     reinterpret_cast<void*>(&EvaluateOrigin)},
    {"hitscan ray direction", "48 8D 4E 58 4C 8D 45 B7 E8 ?? ?? ?? ?? 49 8B 56 48 48 8D 4E 70", 8,
     reinterpret_cast<void*>(&EvaluateDirection)},
}};

// Finds the site and checks it calls the evaluator, without patching anything.
uint8_t* LocateSite(const CallSite& site, std::span<const uint8_t> text) {
    const auto matches = FindPattern(text, ParsePattern(site.signature));
    if (matches.size() != 1) {
        PluginLog("aim: %s signature matched %zu times", site.name, matches.size());
        return nullptr;
    }
    const size_t callOffset = matches.front() + site.callOffset;
    const size_t targetOffset = ResolveRelativeCall(text, callOffset);
    const auto evaluate = ParsePattern(kEvaluateSignature);
    if (targetOffset + evaluate.size() > text.size() ||
        FindPattern(text.subspan(targetOffset, evaluate.size()), evaluate).empty()) {
        PluginLog("aim: %s does not call the expected evaluator", site.name);
        return nullptr;
    }
    gEvaluate = reinterpret_cast<EvaluateFn>(const_cast<uint8_t*>(text.data() + targetOffset));
    return const_cast<uint8_t*>(text.data() + callOffset);
}

}  // namespace

void InstallAimHooks() {
    auto* module = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
    const auto text = TextSection(module);
    if (text.empty()) {
        PluginLog("aim: game code section not found, shots follow the camera");
        return;
    }

    std::array<uint8_t*, 2> calls{};
    for (size_t i = 0; i < gSites.size(); ++i) {
        calls[i] = LocateSite(gSites[i], text);
        if (calls[i] == nullptr) {
            PluginLog("aim: hitscan correction disabled, shots follow the camera");
            return;
        }
    }

    for (size_t i = 0; i < gSites.size(); ++i) {
        CallSite& site = gSites[i];
        if (!RedirectCall(calls[i], site.replacement, site.originalDisplacement)) {
            PluginLog("aim: no memory within reach of %s, hitscan correction disabled", site.name);
            RemoveAimHooks();
            return;
        }
        site.call = calls[i];
        PluginLog("aim: hooked %s at +0x%zX", site.name, static_cast<size_t>(site.call - module));
    }
}

void RemoveAimHooks() {
    for (CallSite& site : gSites) {
        if (site.call == nullptr) {
            continue;
        }
        RestoreCall(site.call, site.originalDisplacement);
        site.call = nullptr;
    }
}

void SetAimFrame(const Quat& cameraWorld, const Vec3& cameraPosition, const Quat& head, bool active) {
    std::lock_guard lock(gFrameMutex);
    gFrame = {cameraWorld, cameraPosition, head, active};
}

PublishedCamera CurrentPublishedCamera() {
    const AimFrame frame = CurrentFrame();
    return {frame.cameraWorld, frame.cameraPosition};
}

void ReportAimStats() {
    const uint64_t corrected = gCorrected.load(std::memory_order_relaxed);
    const uint64_t trackingOff = gTrackingOff.load(std::memory_order_relaxed);
    const uint64_t farFromForward = gFarFromForward.load(std::memory_order_relaxed);
    const uint64_t total = corrected + trackingOff + farFromForward;
    if (total == gReportedTotal) {
        return;
    }
    gReportedTotal = total;
    PluginLog("aim: player hitscan rays corrected=%llu tracking_off=%llu far_from_forward=%llu",
              static_cast<unsigned long long>(corrected),
              static_cast<unsigned long long>(trackingOff),
              static_cast<unsigned long long>(farFromForward));
}
