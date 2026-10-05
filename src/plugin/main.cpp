#include <RED4ext/RED4ext.hpp>

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <filesystem>

#include "plugin_log.h"
#include "tracking_state.h"

namespace {

const RED4ext::v1::Sdk* gSdk = nullptr;
RED4ext::v1::PluginHandle gHandle = nullptr;

// RED4ext's PluginHandle is the plugin DLL's own HMODULE (Api/v1/PluginHandle.hpp), so
// GetModuleFileNameW resolves the DLL's on-disk path. The config lives next to the DLL;
// anchoring to it is required because the game's CWD is bin/x64, not the install root.
std::filesystem::path ConfigPathForPlugin(RED4ext::v1::PluginHandle aHandle) {
    wchar_t buffer[MAX_PATH];
    const DWORD length = GetModuleFileNameW(aHandle, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return {};
    }
    return std::filesystem::path(buffer).parent_path() / L"config.toml";
}

void GetHeadtrackYaw(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, float* aOut, int64_t) {
    aFrame->code++;
    if (aOut) *aOut = static_cast<float>(TrackingState::Instance().Angles().yaw);
}

void GetHeadtrackPitch(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, float* aOut, int64_t) {
    aFrame->code++;
    if (aOut) *aOut = static_cast<float>(TrackingState::Instance().Angles().pitch);
}

void GetHeadtrackRoll(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, float* aOut, int64_t) {
    aFrame->code++;
    if (aOut) *aOut = static_cast<float>(TrackingState::Instance().Angles().roll);
}

void GetHeadtrackBodyHeightOffset(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, float* aOut, int64_t) {
    aFrame->code++;
    if (aOut) *aOut = static_cast<float>(TrackingState::Instance().BodyHeightOffsetMeters());
}

void GetHeadtrackBodyDepthOffset(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, float* aOut, int64_t) {
    aFrame->code++;
    if (aOut) *aOut = static_cast<float>(TrackingState::Instance().BodyDepthOffsetMeters());
}

void GetHeadtrackEnabled(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t) {
    aFrame->code++;
    if (aOut) *aOut = TrackingState::Instance().Enabled();
}

void SetHeadtrackEnabled(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, void*, int64_t) {
    bool enabled = false;
    RED4ext::GetParameter(aFrame, &enabled);
    aFrame->code++;
    TrackingState::Instance().SetEnabled(enabled);
}

void RecenterHeadtrack(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, void*, int64_t) {
    aFrame->code++;
    TrackingState::Instance().Recenter();
}

void HeadtrackLog(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, void*, int64_t) {
    RED4ext::CString message;
    RED4ext::GetParameter(aFrame, &message);
    aFrame->code++;
    PluginLog("[script] %s", message.c_str());
}

// Per-frame update, registered as a custom "Running" game state. RED4ext calls OnUpdate
// every frame while the game's running state is active; for the Running state the return
// value is documented as ignored (RED4ext.SDK Api/v1/GameState.hpp).
bool OnRunningUpdate(RED4ext::CGameApplication*) {
    using Clock = std::chrono::steady_clock;
    static Clock::time_point lastFrame = Clock::now();
    const auto now = Clock::now();
    const double dtSeconds = std::chrono::duration<double>(now - lastFrame).count();
    lastFrame = now;
    static bool firstUpdate = true;
    if (firstUpdate) {
        firstUpdate = false;
        PluginLog("running-state update hook is ticking");
    }
    TrackingState::Instance().Update(std::min(dtSeconds, 0.25));
    return false;
}

}  // namespace

void PluginLog(const char* format, ...) {
    if (gSdk == nullptr) {
        return;
    }
    char buffer[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    gSdk->logger->Info(gHandle, buffer);
}

RED4EXT_C_EXPORT void RED4EXT_CALL RegisterTypes() {
}

RED4EXT_C_EXPORT void RED4EXT_CALL PostRegisterTypes() {
    auto rtti = RED4ext::CRTTISystem::Get();
    RED4ext::CBaseFunction::Flags flags{};
    flags.isNative = true;
    flags.isStatic = true;

    auto yaw = RED4ext::CGlobalFunction::Create("GetHeadtrackYaw", "GetHeadtrackYaw", &GetHeadtrackYaw);
    yaw->flags = flags;
    yaw->SetReturnType("Float");
    rtti->RegisterFunction(yaw);

    auto pitch = RED4ext::CGlobalFunction::Create("GetHeadtrackPitch", "GetHeadtrackPitch", &GetHeadtrackPitch);
    pitch->flags = flags;
    pitch->SetReturnType("Float");
    rtti->RegisterFunction(pitch);

    auto roll = RED4ext::CGlobalFunction::Create("GetHeadtrackRoll", "GetHeadtrackRoll", &GetHeadtrackRoll);
    roll->flags = flags;
    roll->SetReturnType("Float");
    rtti->RegisterFunction(roll);

    auto bodyHeight = RED4ext::CGlobalFunction::Create("GetHeadtrackBodyHeightOffset", "GetHeadtrackBodyHeightOffset", &GetHeadtrackBodyHeightOffset);
    bodyHeight->flags = flags;
    bodyHeight->SetReturnType("Float");
    rtti->RegisterFunction(bodyHeight);

    auto bodyDepth = RED4ext::CGlobalFunction::Create("GetHeadtrackBodyDepthOffset", "GetHeadtrackBodyDepthOffset", &GetHeadtrackBodyDepthOffset);
    bodyDepth->flags = flags;
    bodyDepth->SetReturnType("Float");
    rtti->RegisterFunction(bodyDepth);

    auto getEnabled = RED4ext::CGlobalFunction::Create("GetHeadtrackEnabled", "GetHeadtrackEnabled", &GetHeadtrackEnabled);
    getEnabled->flags = flags;
    getEnabled->SetReturnType("Bool");
    rtti->RegisterFunction(getEnabled);

    auto setEnabled = RED4ext::CGlobalFunction::Create("SetHeadtrackEnabled", "SetHeadtrackEnabled", &SetHeadtrackEnabled);
    setEnabled->flags = flags;
    setEnabled->AddParam("Bool", "enabled");
    rtti->RegisterFunction(setEnabled);

    auto recenter = RED4ext::CGlobalFunction::Create("RecenterHeadtrack", "RecenterHeadtrack", &RecenterHeadtrack);
    recenter->flags = flags;
    rtti->RegisterFunction(recenter);

    auto log = RED4ext::CGlobalFunction::Create("HeadtrackLog", "HeadtrackLog", &HeadtrackLog);
    log->flags = flags;
    log->AddParam("String", "message");
    rtti->RegisterFunction(log);
}

RED4EXT_C_EXPORT bool RED4EXT_CALL Main(RED4ext::v1::PluginHandle aHandle,
                                         RED4ext::v1::EMainReason aReason,
                                         const RED4ext::v1::Sdk* aSdk) {
    switch (aReason) {
    case RED4ext::v1::EMainReason::Load: {
        gSdk = aSdk;
        gHandle = aHandle;
        PluginLog("loaded");
        auto rtti = RED4ext::CRTTISystem::Get();
        rtti->AddRegisterCallback(RegisterTypes);
        rtti->AddPostRegisterCallback(PostRegisterTypes);

        if (auto configError = TrackingState::Instance().Initialize(ConfigPathForPlugin(aHandle))) {
            PluginLog("config.toml ignored, using defaults: %s", configError->c_str());
        }

        RED4ext::v1::GameState runningState{
            .OnEnter = nullptr,
            .OnUpdate = &OnRunningUpdate,
            .OnExit = nullptr,
        };
        aSdk->gameStates->Add(aHandle, RED4ext::EGameStateType::Running, &runningState);
        break;
    }
    case RED4ext::v1::EMainReason::Unload: {
        TrackingState::Instance().Shutdown();
        gSdk = nullptr;
        break;
    }
    }

    return true;
}

RED4EXT_C_EXPORT void RED4EXT_CALL Query(RED4ext::v1::PluginInfo* aInfo) {
    aInfo->name = L"CyberHeadtrack";
    aInfo->author = L"Garulf";
    aInfo->version = RED4EXT_V1_SEMVER(0, 1, 0);
    aInfo->runtime = RED4EXT_V1_RUNTIME_VERSION_LATEST;
    aInfo->sdk = RED4EXT_V1_SDK_VERSION_CURRENT;
}

RED4EXT_C_EXPORT uint32_t RED4EXT_CALL Supports() {
    return RED4EXT_API_VERSION_1;
}
