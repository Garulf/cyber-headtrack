#include <RED4ext/RED4ext.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>

#include "tracking_state.h"

namespace {

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
    if (aOut) *aOut = TrackingState::Instance().Yaw();
}

void GetHeadtrackPitch(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, float* aOut, int64_t) {
    aFrame->code++;
    if (aOut) *aOut = TrackingState::Instance().Pitch();
}

void GetHeadtrackRoll(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, float* aOut, int64_t) {
    aFrame->code++;
    if (aOut) *aOut = TrackingState::Instance().Roll();
}

void GetHeadtrackBodyVisibilityBlend(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, float* aOut, int64_t) {
    aFrame->code++;
    if (aOut) *aOut = TrackingState::Instance().BodyVisibilityBlend();
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

// Per-frame update, registered as a custom "Running" game state. RED4ext calls OnUpdate
// every frame while the game's running state is active; for the Running state the return
// value is documented as ignored (RED4ext.SDK Api/v1/GameState.hpp).
bool OnRunningUpdate(RED4ext::CGameApplication*) {
    using Clock = std::chrono::steady_clock;
    static Clock::time_point lastFrame = Clock::now();
    const auto now = Clock::now();
    const double dtSeconds = std::chrono::duration<double>(now - lastFrame).count();
    lastFrame = now;
    TrackingState::Instance().Update(std::min(dtSeconds, 0.25));
    return false;
}

}  // namespace

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

    auto bodyBlend = RED4ext::CGlobalFunction::Create("GetHeadtrackBodyVisibilityBlend", "GetHeadtrackBodyVisibilityBlend", &GetHeadtrackBodyVisibilityBlend);
    bodyBlend->flags = flags;
    bodyBlend->SetReturnType("Float");
    rtti->RegisterFunction(bodyBlend);

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
}

RED4EXT_C_EXPORT bool RED4EXT_CALL Main(RED4ext::v1::PluginHandle aHandle,
                                         RED4ext::v1::EMainReason aReason,
                                         const RED4ext::v1::Sdk* aSdk) {
    switch (aReason) {
    case RED4ext::v1::EMainReason::Load: {
        auto rtti = RED4ext::CRTTISystem::Get();
        rtti->AddRegisterCallback(RegisterTypes);
        rtti->AddPostRegisterCallback(PostRegisterTypes);

        TrackingState::Instance().Initialize(ConfigPathForPlugin(aHandle));

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
        break;
    }
    }

    return true;
}

RED4EXT_C_EXPORT void RED4EXT_CALL Query(RED4ext::v1::PluginInfo* aInfo) {
    aInfo->name = L"CyberHeadtrack";
    aInfo->author = L"";
    aInfo->version = RED4EXT_V1_SEMVER(0, 1, 0);
    aInfo->runtime = RED4EXT_V1_RUNTIME_VERSION_LATEST;
    aInfo->sdk = RED4EXT_V1_SDK_VERSION_CURRENT;
}

RED4EXT_C_EXPORT uint32_t RED4EXT_CALL Supports() {
    return RED4EXT_API_VERSION_1;
}
