// CyberHeadtrack: applies head-tracking yaw/pitch/roll (degrees, from the RED4ext
// plugin's RTTI globals) as a first-person camera rotation offset every frame.
//
// Camera API (verified against CDPR's published scripts, scripts/core/components/
// placedComponent.script + cameraComponent.script): FPPCameraComponent inherits
// GetLocalOrientation() -> Quaternion and SetLocalOrientation(rot: Quaternion)
// from IPlacedComponent; PlayerPuppet.GetFPPCameraComponent() returns it.
//
// Per-frame hook: vanilla PlayerPuppet has no OnUpdate, so this wraps
// LocomotionEventsTransition.OnUpdate (player state machine, runs once per frame
// for the active on-foot locomotion state; every vanilla override calls super).
//
// Composition assumes the game rewrites the camera's local orientation from look
// input each frame before this hook runs, so "current * offset" starts from a
// fresh mouselook base and does not accumulate. If in-game testing shows the
// offset winding up instead, switch the write to the absolute form:
// camera.SetLocalOrientation(offset).

native func GetHeadtrackYaw() -> Float;
native func GetHeadtrackPitch() -> Float;
native func GetHeadtrackRoll() -> Float;
native func GetHeadtrackEnabled() -> Bool;
native func SetHeadtrackEnabled(enabled: Bool) -> Void;
native func RecenterHeadtrack() -> Void;

func ApplyHeadtrackCameraOffset(scriptInterface: ref<StateGameScriptInterface>) -> Void {
  let player: ref<PlayerPuppet> = scriptInterface.executionOwner as PlayerPuppet;
  if !IsDefined(player) {
    return;
  }
  let camera: ref<FPPCameraComponent> = player.GetFPPCameraComponent();
  if !IsDefined(camera) {
    return;
  }
  let offset: Quaternion = EulerAngles.ToQuat(new EulerAngles(GetHeadtrackRoll(), GetHeadtrackPitch(), GetHeadtrackYaw()));
  camera.SetLocalOrientation(camera.GetLocalOrientation() * offset);
}

@wrapMethod(LocomotionEventsTransition)
public func OnUpdate(timeDelta: Float, stateContext: ref<StateContext>, scriptInterface: ref<StateGameScriptInterface>) -> Void {
  wrappedMethod(timeDelta, stateContext, scriptInterface);
  if GetHeadtrackEnabled() {
    ApplyHeadtrackCameraOffset(scriptInterface);
  }
}
