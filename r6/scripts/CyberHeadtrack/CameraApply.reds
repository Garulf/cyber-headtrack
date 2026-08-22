// CyberHeadtrack: applies head-tracking yaw/pitch/roll (degrees, from the RED4ext
// plugin's RTTI globals) as a first-person camera rotation offset every frame, plus
// a pitch-gated height/depth position offset that keeps the player's body and
// clothes visible when looking down instead of clipping through the FPP view.
//
// Camera API (verified against CDPR's published scripts, scripts/core/components/
// placedComponent.script + cameraComponent.script): FPPCameraComponent inherits
// GetLocalOrientation() -> Quaternion / SetLocalOrientation(rot: Quaternion) and
// GetLocalPosition() -> Vector4 / SetLocalPosition(pos: Vector4)
// from IPlacedComponent; PlayerPuppet.GetFPPCameraComponent() returns it.
//
// Per-frame hook: vanilla PlayerPuppet has no OnUpdate, so this wraps
// LocomotionEventsTransition.OnUpdate (player state machine, runs once per frame
// for the active on-foot locomotion state). Most vanilla overrides call super, but
// a few transient states (e.g. ClimbEvents, SuperheroLandEvents) do not, so the
// offset briefly stops applying there and resumes with normal locomotion.
//
// Composition assumes the game rewrites the camera's local orientation and local
// position from look input / camera animation each frame before this hook runs, so
// "current * offset" (rotation) and "current + offset" (position) start from a
// fresh base and do not accumulate. If in-game testing shows either offset winding
// up instead, switch that write to the absolute form.

native func GetHeadtrackYaw() -> Float;
native func GetHeadtrackPitch() -> Float;
native func GetHeadtrackRoll() -> Float;
native func GetHeadtrackBodyVisibilityBlend() -> Float;
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
  ApplyBodyVisibilityOffset(camera);
}

// Pitch-gated body-visibility offset: as the head-tracking pitch drops past the
// plugin's trigger angle the blend ramps 0..1, raising the camera (+Z, up to 15cm)
// and pulling it back (-Y, up to 8.5cm) so V's body stays in frame when looking
// down. Blend comes from the plugin (ComputeBodyVisibilityBlend); the max offsets
// here mirror BodyVisibilityConfig's maxHeightOffsetCm/maxDepthOffsetCm in meters.
func ApplyBodyVisibilityOffset(camera: ref<FPPCameraComponent>) -> Void {
  let blend: Float = GetHeadtrackBodyVisibilityBlend();
  if blend <= 0.0 {
    return;
  }
  let pos: Vector4 = camera.GetLocalPosition();
  pos.Z += 0.15 * blend;
  pos.Y += -0.085 * blend;
  camera.SetLocalPosition(pos);
}

@wrapMethod(LocomotionEventsTransition)
public func OnUpdate(timeDelta: Float, stateContext: ref<StateContext>, scriptInterface: ref<StateGameScriptInterface>) -> Void {
  wrappedMethod(timeDelta, stateContext, scriptInterface);
  if GetHeadtrackEnabled() {
    ApplyHeadtrackCameraOffset(scriptInterface);
  }
}
