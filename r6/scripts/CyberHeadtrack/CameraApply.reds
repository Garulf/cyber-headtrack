// CyberHeadtrack: applies head-tracking yaw/pitch/roll (degrees, already in EulerAngles
// convention, from the RED4ext plugin's RTTI globals) as a first-person camera rotation
// offset every frame, plus positional lean (meters along the camera parent's X right,
// Y forward, Z up) and a pitch-gated height/depth offset that keeps the player's body and
// clothes visible when looking down on foot.
//
// Camera API (CDPR scripts, placedComponent.script + cameraComponent.script):
// FPPCameraComponent inherits Get/SetLocalOrientation and Get/SetLocalPosition from
// IPlacedComponent; PlayerPuppet.GetFPPCameraComponent() returns it. That component is the
// on-screen camera on foot and in first-person vehicle seats, but not in the third-person
// vehicle cameras, so tracking pauses while one of those is active.
//
// The engine only sometimes rewrites the component's local transform (aiming down sights,
// weapon swaps, mounting a vehicle, scenes) and otherwise keeps the last written value. Each
// frame therefore compares the current value with what was written last frame: a match
// means the engine left it alone and the previous base still holds; anything else is a
// fresh engine value and becomes the new base. The offset is always applied to the base,
// never to its own previous output, so it cannot accumulate.
//
// The plugin keeps separate settings for on foot (profile 0) and seated in a vehicle
// (profile 1); OnFrame switches it whenever the player mounts or leaves a vehicle.
//
// While tracking is on, the plugin is also given the camera's world orientation and the head
// offset each frame so it can turn hitscan shots back onto the body's aim, and the crosshair
// widgets are moved to that aim point. Dodge and dash impulses are turned the same way, so
// they follow the body rather than where the head looks.
//
// Per-frame drivers: the player state machine's OnUpdate for the active on-foot locomotion
// state (LocomotionEventsTransition) and for the driver and passenger vehicle states
// (DriveEvents, PassengerEvents), plus the high-level scene tier states
// (SceneTierAbstractEvents). Story scenes at tiers 4 and 5 swap in their own locomotion
// state machines, which stop the on-foot and vehicle drivers, while the scene tier state
// keeps ticking. Each forwards to HeadtrackCameraSystem.OnFrame, which runs once per game
// frame however many drivers tick. A few transient on-foot states (climbing, superhero
// landing) don't call their base OnUpdate, so outside scenes tracking holds still there
// until normal locomotion resumes.
//
// Each profile's track_in_scenes setting can turn tracking off during story scenes (scene
// tiers 3 to 5: limited gameplay, first-person cinematic, cinematic).

native func GetHeadtrackYaw() -> Float;
native func GetHeadtrackPitch() -> Float;
native func GetHeadtrackRoll() -> Float;
native func GetHeadtrackLeanSide() -> Float;
native func GetHeadtrackLeanUp() -> Float;
native func GetHeadtrackLeanForward() -> Float;
native func GetHeadtrackBodyHeightOffset() -> Float;
native func GetHeadtrackBodyDepthOffset() -> Float;
native func GetHeadtrackEnabled() -> Bool;
native func SetHeadtrackEnabled(enabled: Bool) -> Void;
native func RecenterHeadtrack() -> Void;
native func GetHeadtrackProfile() -> Int32;
native func SetHeadtrackProfile(profile: Int32) -> Void;
native func HeadtrackLog(message: String) -> Void;
native func SetHeadtrackAimFrame(cameraRight: Vector4, cameraForward: Vector4, cameraUp: Vector4, cameraPosition: Vector4, head: Quaternion, active: Bool) -> Void;

// HUD widgets are laid out in a 3840x2160 virtual space regardless of display resolution.
func HeadtrackHalfHudWidth() -> Float = 1920.0
func HeadtrackHalfHudHeight() -> Float = 1080.0
func HeadtrackCrosshairDistance() -> Float = 20.0
// In projected units, where the screen spans -1 to 1: far enough to be off any edge.
func HeadtrackCrosshairOffscreenReach() -> Float = 2.0
// Below this forward component (about 87 degrees off the view) the aim point is not projected.
func HeadtrackMinProjectableForward() -> Float = 0.05

func HeadtrackFlagChanged(current: Bool, previous: Bool) -> Bool {
  return (current && !previous) || (!current && previous);
}

func HeadtrackConjugate(q: Quaternion) -> Quaternion {
  return new Quaternion(-q.i, -q.j, -q.k, q.r);
}

func HeadtrackIdentity() -> Quaternion {
  return new Quaternion(0.0, 0.0, 0.0, 1.0);
}

func HeadtrackNearlyEqual(a: Float, b: Float) -> Bool {
  return AbsF(a - b) < 0.0001;
}

func HeadtrackSameRotation(a: Quaternion, b: Quaternion) -> Bool {
  let same: Bool = HeadtrackNearlyEqual(a.i, b.i) && HeadtrackNearlyEqual(a.j, b.j)
    && HeadtrackNearlyEqual(a.k, b.k) && HeadtrackNearlyEqual(a.r, b.r);
  let negated: Bool = HeadtrackNearlyEqual(a.i, -b.i) && HeadtrackNearlyEqual(a.j, -b.j)
    && HeadtrackNearlyEqual(a.k, -b.k) && HeadtrackNearlyEqual(a.r, -b.r);
  return same || negated;
}

func HeadtrackSameVector2(a: Vector2, b: Vector2) -> Bool {
  return HeadtrackNearlyEqual(a.X, b.X) && HeadtrackNearlyEqual(a.Y, b.Y);
}

func HeadtrackProfileSetting(profile: Int32, key: String) -> Float {
  return GetHeadtrackSetting((profile == 1 ? "vehicle." : "on_foot.") + key);
}

func HeadtrackIsStoryScene(highLevelState: Int32) -> Bool {
  return highLevelState >= EnumInt(GameplayTier.Tier3_LimitedGameplay)
    && highLevelState <= EnumInt(GameplayTier.Tier5_Cinematic);
}

// The body's aim in world space, from the rendered camera's world axes and the head offset
// (camera = body * head, so the aim in the camera's own frame is forward turned back by it).
func HeadtrackBodyAim(right: Vector4, forward: Vector4, up: Vector4, head: Quaternion) -> Vector4 {
  let aim: Vector4 = Quaternion.GetForward(HeadtrackConjugate(head));
  return right * aim.X + forward * aim.Y + up * aim.Z;
}

// Crosshair translation in HUD pixels for the body's aim. An aim point off the screen, or one
// too far to the side to project, is pushed past the edge in the aim's direction, so the
// crosshair leaves the view rather than landing where the head looks.
func HeadtrackCrosshairTranslation(aim: Vector4, projected: Vector4, projectable: Bool) -> Vector2 {
  let x: Float = projectable ? projected.X : aim.X;
  let y: Float = projectable ? projected.Y : aim.Z;
  let reach: Float = MaxF(AbsF(x), AbsF(y));
  if !projectable && reach < 0.0001 {
    x = 1.0;
    reach = 1.0;
  }
  let limit: Float = HeadtrackCrosshairOffscreenReach();
  if !projectable || reach > limit {
    x *= limit / reach;
    y *= limit / reach;
  }
  return new Vector2(x * HeadtrackHalfHudWidth(), -y * HeadtrackHalfHudHeight());
}

func HeadtrackSamePosition(a: Vector4, b: Vector4) -> Bool {
  return HeadtrackNearlyEqual(a.X, b.X) && HeadtrackNearlyEqual(a.Y, b.Y)
    && HeadtrackNearlyEqual(a.Z, b.Z);
}

public class HeadtrackCameraSystem extends ScriptableSystem {
  private let player: wref<PlayerPuppet>;
  private let input: ref<HeadtrackInputListener>;
  private let applied: Bool;
  private let baseOrientation: Quaternion;
  private let writtenOrientation: Quaternion;
  private let basePosition: Vector4;
  private let writtenPosition: Vector4;
  private let loggedLoopRunning: Bool;
  private let loggedMissingCamera: Bool;
  private let lastEnabled: Bool;
  private let lastThirdPerson: Bool;
  private let lastSceneTracking: Bool;
  private let lastHighLevelState: Int32;
  private let framed: Bool;
  private let lastFrameTime: Float;
  private let framesSinceReport: Int32;
  private let engineRewritesSinceReport: Int32;
  private let crosshairs: array<wref<inkWidget>>;
  private let headOffset: Quaternion;
  private let framesSinceCrosshairLog: Int32;
  private let adsBlend: Float;
  private let writtenSpread: Vector2;
  private let spreadCorrected: Bool;
  private let lastRawSpread: Vector2;

  private final func OnPlayerAttach(request: ref<PlayerAttachRequest>) -> Void {
    let player: ref<PlayerPuppet> = request.owner as PlayerPuppet;
    if !IsDefined(player) {
      return;
    }
    this.UnregisterInput();
    this.player = player;
    this.input = new HeadtrackInputListener();
    this.input.Register(player);
    this.applied = false;
    this.framed = false;
    this.loggedLoopRunning = false;
    HeadtrackLog("camera system attached to player, key listener registered");
  }

  private final func OnPlayerDetach(request: ref<PlayerDetachRequest>) -> Void {
    this.UnregisterInput();
    this.applied = false;
    this.player = null;
  }

  private final func UnregisterInput() -> Void {
    let player: ref<PlayerPuppet> = this.player;
    if IsDefined(this.input) && IsDefined(player) {
      this.input.Unregister(player);
    }
    this.input = null;
  }

  public static func Get(game: GameInstance) -> ref<HeadtrackCameraSystem> {
    return GameInstance.GetScriptableSystemsContainer(game).Get(n"HeadtrackCameraSystem") as HeadtrackCameraSystem;
  }

  public final func RegisterCrosshair(root: ref<inkWidget>) -> Void {
    ArrayPush(this.crosshairs, root);
  }

  public final func OnFrame(player: ref<PlayerPuppet>, source: String, timeDelta: Float) -> Void {
    let frameTime: Float = EngineTime.ToFloat(GameInstance.GetSimTime(player.GetGame()));
    if this.framed && frameTime == this.lastFrameTime {
      return;
    }
    this.framed = true;
    this.lastFrameTime = frameTime;
    if !this.loggedLoopRunning {
      this.loggedLoopRunning = true;
      HeadtrackLog("per-frame camera updates running (first from " + source + ")");
    }
    let camera: ref<FPPCameraComponent> = player.GetFPPCameraComponent();
    if !IsDefined(camera) {
      if !this.loggedMissingCamera {
        this.loggedMissingCamera = true;
        HeadtrackLog("player has no FPP camera component");
      }
      return;
    }
    let vehicle: ref<VehicleObject> = player.GetMountedVehicle();
    let profile: Int32 = IsDefined(vehicle) ? 1 : 0;
    if profile != GetHeadtrackProfile() {
      SetHeadtrackProfile(profile);
    }
    let enabled: Bool = GetHeadtrackEnabled();
    let thirdPerson: Bool = this.IsThirdPersonVehicleCamera(vehicle);
    let highLevelState: Int32 = this.HighLevelState(player);
    let sceneTracking: Bool = !HeadtrackIsStoryScene(highLevelState)
      || HeadtrackProfileSetting(profile, "track_in_scenes") != 0.0;
    if HeadtrackFlagChanged(enabled, this.lastEnabled) || HeadtrackFlagChanged(thirdPerson, this.lastThirdPerson)
      || HeadtrackFlagChanged(sceneTracking, this.lastSceneTracking) || highLevelState != this.lastHighLevelState {
      this.lastEnabled = enabled;
      this.lastThirdPerson = thirdPerson;
      this.lastSceneTracking = sceneTracking;
      this.lastHighLevelState = highLevelState;
      HeadtrackLog("camera loop sees enabled=" + ToString(enabled) + " mounted=" + ToString(IsDefined(vehicle))
        + " vehicleThirdPerson=" + ToString(thirdPerson) + " sceneTier=" + ToString(highLevelState)
        + " sceneTracking=" + ToString(sceneTracking) + " (from " + source + ")");
    }
    if enabled {
      this.ReportPeriodically();
    }
    this.UpdateAdsBlend(player, profile, timeDelta);
    let aimActive: Bool = enabled && !thirdPerson && sceneTracking;
    if aimActive {
      this.Apply(camera, IsDefined(vehicle), profile);
    } else {
      this.Restore(camera);
    }
    this.UpdateAim(player, aimActive);
  }

  // Publishes the rendered camera's axes and the head offset so the plugin can turn hitscan
  // shots back onto the body's aim, and moves every crosshair to that aim point. The body's
  // aim in the camera's own frame is the forward axis rotated back by the head offset
  // (camera = body * head), so nothing here depends on world-matrix conventions.
  private final func UpdateAim(player: ref<PlayerPuppet>, active: Bool) -> Void {
    let cameraSystem: ref<CameraSystem> = GameInstance.GetCameraSystem(player.GetGame());
    let right: Vector4 = cameraSystem.GetActiveCameraRight();
    let forward: Vector4 = cameraSystem.GetActiveCameraForward();
    let up: Vector4 = cameraSystem.GetActiveCameraUp();
    let transform: Transform;
    cameraSystem.GetActiveCameraWorldTransform(transform);
    let head: Quaternion = active ? this.headOffset : HeadtrackIdentity();
    SetHeadtrackAimFrame(right, forward, up, transform.position, head, active);

    let aim: Vector4 = Quaternion.GetForward(HeadtrackConjugate(head));
    this.CorrectSpread(player, active, aim.Y);

    let translation: Vector2 = new Vector2(0.0, 0.0);
    if active {
      let projectable: Bool = aim.Y > HeadtrackMinProjectableForward();
      let projected: Vector4;
      if projectable {
        let point: Vector4 = transform.position + HeadtrackBodyAim(right, forward, up, head) * HeadtrackCrosshairDistance();
        point.W = 1.0;
        projected = cameraSystem.ProjectPoint(point);
      }
      translation = HeadtrackCrosshairTranslation(aim, projected, projectable);
      this.framesSinceCrosshairLog += 1;
      if this.framesSinceCrosshairLog >= 150 {
        this.framesSinceCrosshairLog = 0;
        HeadtrackLog("crosshair: aim in camera frame " + FloatToString(aim.X) + ", " + FloatToString(aim.Y) + ", "
          + FloatToString(aim.Z) + (projectable ? " projected " + FloatToString(projected.X) + ", " + FloatToString(projected.Y) : " not projectable")
          + " moved to " + FloatToString(translation.X) + ", " + FloatToString(translation.Y)
          + " on " + ToString(ArraySize(this.crosshairs)) + " widgets; spread raw "
          + FloatToString(this.lastRawSpread.X) + ", " + FloatToString(this.lastRawSpread.Y) + " written "
          + FloatToString(this.writtenSpread.X) + ", " + FloatToString(this.writtenSpread.Y));
      }
    }
    let i: Int32 = ArraySize(this.crosshairs) - 1;
    while i >= 0 {
      let root: wref<inkWidget> = this.crosshairs[i];
      if IsDefined(root) {
        root.SetTranslation(translation);
      } else {
        ArrayErase(this.crosshairs, i);
      }
      i -= 1;
    }
  }

  // The engine's crosshair spread (UI_ActiveWeaponData.BulletSpread, in HUD pixels) shrinks
  // as the head turns away from the gun, roughly by the cosine of the angle between them.
  // Each frame a fresh engine value (anything other than what was last written here) is
  // divided by that cosine and written back, so every crosshair type sees the corrected size.
  private final func CorrectSpread(player: ref<PlayerPuppet>, active: Bool, aimCosine: Float) -> Void {
    let weaponData: ref<IBlackboard> = GameInstance.GetBlackboardSystem(player.GetGame()).Get(GetAllBlackboardDefs().UI_ActiveWeaponData);
    if !IsDefined(weaponData) {
      return;
    }
    let current: Vector2 = weaponData.GetVector2(GetAllBlackboardDefs().UI_ActiveWeaponData.BulletSpread);
    let fresh: Bool = !this.spreadCorrected || !HeadtrackSameVector2(current, this.writtenSpread);
    if fresh {
      this.lastRawSpread = current;
    }
    if !active || aimCosine >= 0.9999 {
      if this.spreadCorrected && !fresh {
        weaponData.SetVector2(GetAllBlackboardDefs().UI_ActiveWeaponData.BulletSpread, this.lastRawSpread, true);
      }
      this.spreadCorrected = false;
      return;
    }
    let corrected: Vector2 = new Vector2(this.lastRawSpread.X / MaxF(aimCosine, 0.5), this.lastRawSpread.Y / MaxF(aimCosine, 0.5));
    if !this.spreadCorrected || !HeadtrackSameVector2(corrected, this.writtenSpread) {
      weaponData.SetVector2(GetAllBlackboardDefs().UI_ActiveWeaponData.BulletSpread, corrected, true);
    }
    this.writtenSpread = corrected;
    this.spreadCorrected = true;
  }

  private final func HighLevelState(player: ref<PlayerPuppet>) -> Int32 {
    let psm: ref<IBlackboard> = player.GetPlayerStateMachineBlackboard();
    return IsDefined(psm) ? psm.GetInt(GetAllBlackboardDefs().PlayerStateMachine.HighLevel) : 0;
  }

  // Dodge and dash queue an impulse along the camera's yaw, which carries the head offset.
  // Turning it by the yaw between the camera and the body's aim makes it follow the body.
  public final func TurnImpulseToBody(stateContext: ref<StateContext>, scriptInterface: ref<StateGameScriptInterface>) -> Void {
    if !this.applied {
      return;
    }
    let queued: StateResultVector = stateContext.GetTemporaryVectorParameter(n"impulse");
    if !queued.valid {
      return;
    }
    let camera: Transform = scriptInterface.GetCameraWorldTransform();
    let bodyAim: Vector4 = HeadtrackBodyAim(Transform.GetRight(camera), Transform.GetForward(camera), Transform.GetUp(camera), this.headOffset);
    let turn: Float = AngleNormalize180(Vector4.Heading(bodyAim) - Transform.GetYaw(camera));
    let impulse: Vector4 = queued.value;
    let turned: Vector4 = Vector4.FromHeading(AngleNormalize180(Vector4.Heading(impulse) + turn)) * Vector4.Length2D(impulse);
    turned.Z = impulse.Z;
    stateContext.SetTemporaryVectorParameter(n"impulse", turned, true);
  }

  private final func IsThirdPersonVehicleCamera(vehicle: ref<VehicleObject>) -> Bool {
    if !IsDefined(vehicle) {
      return false;
    }
    let cameraManager: ref<VehicleCameraManager> = vehicle.GetCameraManager();
    return IsDefined(cameraManager) && cameraManager.IsTPPActive();
  }

  // Eases adsBlend between 0 (hip) and 1 (aiming down sights) over ads_ease_seconds.
  private final func UpdateAdsBlend(player: ref<PlayerPuppet>, profile: Int32, timeDelta: Float) -> Void {
    let psm: ref<IBlackboard> = player.GetPlayerStateMachineBlackboard();
    let aiming: Bool = IsDefined(psm)
      && psm.GetInt(GetAllBlackboardDefs().PlayerStateMachine.UpperBody) == EnumInt(gamePSMUpperBodyStates.Aim);
    let target: Float = aiming ? 1.0 : 0.0;
    let easeSeconds: Float = HeadtrackProfileSetting(profile, "ads_ease_seconds");
    if easeSeconds <= 0.0 {
      this.adsBlend = target;
      return;
    }
    let step: Float = timeDelta / easeSeconds;
    if this.adsBlend < target {
      this.adsBlend = MinF(target, this.adsBlend + step);
    } else {
      this.adsBlend = MaxF(target, this.adsBlend - step);
    }
  }

  private final func Apply(camera: ref<FPPCameraComponent>, mounted: Bool, profile: Int32) -> Void {
    let orientation: Quaternion = camera.GetLocalOrientation();
    if !this.applied || !HeadtrackSameRotation(orientation, this.writtenOrientation) {
      if this.applied {
        this.engineRewritesSinceReport += 1;
      }
      this.baseOrientation = orientation;
    }
    let position: Vector4 = camera.GetLocalPosition();
    if !this.applied || !HeadtrackSamePosition(position, this.writtenPosition) {
      this.basePosition = position;
    }

    let angles: EulerAngles = new EulerAngles();
    angles.Roll = GetHeadtrackRoll();
    angles.Pitch = GetHeadtrackPitch();
    angles.Yaw = GetHeadtrackYaw();
    let offset: Quaternion = EulerAngles.ToQuat(angles);
    // With ads_tracking_off, aiming eases the head offset out so the view settles on the
    // sights; otherwise tracking stays on and the view shifts toward the dominant eye.
    let trackingWeight: Float = 1.0;
    let eyeShift: Float = 0.0;
    if HeadtrackProfileSetting(profile, "ads_tracking_off") != 0.0 {
      trackingWeight = 1.0 - this.adsBlend;
      offset = Quaternion.Slerp(HeadtrackIdentity(), offset, trackingWeight);
    } else {
      eyeShift = this.adsBlend * HeadtrackProfileSetting(profile, "ads_eye")
        * HeadtrackProfileSetting(profile, "ads_eye_offset_cm") / 100.0;
    }
    let targetOrientation: Quaternion = this.baseOrientation * offset;
    let targetPosition: Vector4 = this.basePosition;
    targetPosition.X += GetHeadtrackLeanSide() * trackingWeight + eyeShift;
    targetPosition.Y += GetHeadtrackLeanForward() * trackingWeight;
    targetPosition.Z += GetHeadtrackLeanUp() * trackingWeight;
    if !mounted {
      targetPosition.Z += GetHeadtrackBodyHeightOffset() * trackingWeight;
      targetPosition.Y += GetHeadtrackBodyDepthOffset() * trackingWeight;
    }

    camera.SetLocalOrientation(targetOrientation);
    camera.SetLocalPosition(targetPosition);
    this.writtenOrientation = targetOrientation;
    this.writtenPosition = targetPosition;
    this.applied = true;

    this.headOffset = offset;
  }

  private final func ReportPeriodically() -> Void {
    this.framesSinceReport += 1;
    if this.framesSinceReport < 300 {
      return;
    }
    HeadtrackLog("camera loop: " + ToString(this.framesSinceReport) + " frames, engine replaced our rotation on "
      + ToString(this.engineRewritesSinceReport) + "; last written quat i=" + FloatToString(this.writtenOrientation.i)
      + " j=" + FloatToString(this.writtenOrientation.j) + " k=" + FloatToString(this.writtenOrientation.k)
      + " r=" + FloatToString(this.writtenOrientation.r));
    this.framesSinceReport = 0;
    this.engineRewritesSinceReport = 0;
  }

  private final func Restore(camera: ref<FPPCameraComponent>) -> Void {
    if !this.applied {
      return;
    }
    if HeadtrackSameRotation(camera.GetLocalOrientation(), this.writtenOrientation) {
      camera.SetLocalOrientation(this.baseOrientation);
    }
    if HeadtrackSamePosition(camera.GetLocalPosition(), this.writtenPosition) {
      camera.SetLocalPosition(this.basePosition);
    }
    this.applied = false;
  }
}

func HeadtrackOnStateUpdate(scriptInterface: ref<StateGameScriptInterface>, source: String, timeDelta: Float) -> Void {
  let player: ref<PlayerPuppet> = scriptInterface.executionOwner as PlayerPuppet;
  if !IsDefined(player) {
    return;
  }
  let system: ref<HeadtrackCameraSystem> = HeadtrackCameraSystem.Get(player.GetGame());
  if IsDefined(system) {
    system.OnFrame(player, source, timeDelta);
  }
}

@wrapMethod(LocomotionEventsTransition)
public func OnUpdate(timeDelta: Float, stateContext: ref<StateContext>, scriptInterface: ref<StateGameScriptInterface>) -> Void {
  wrappedMethod(timeDelta, stateContext, scriptInterface);
  HeadtrackOnStateUpdate(scriptInterface, "on foot", timeDelta);
}

@wrapMethod(DriveEvents)
public func OnUpdate(timeDelta: Float, stateContext: ref<StateContext>, scriptInterface: ref<StateGameScriptInterface>) -> Void {
  wrappedMethod(timeDelta, stateContext, scriptInterface);
  HeadtrackOnStateUpdate(scriptInterface, "driving", timeDelta);
}

@wrapMethod(PassengerEvents)
public func OnUpdate(timeDelta: Float, stateContext: ref<StateContext>, scriptInterface: ref<StateGameScriptInterface>) -> Void {
  wrappedMethod(timeDelta, stateContext, scriptInterface);
  HeadtrackOnStateUpdate(scriptInterface, "passenger", timeDelta);
}

@wrapMethod(SceneTierAbstractEvents)
protected func OnUpdate(timeDelta: Float, stateContext: ref<StateContext>, scriptInterface: ref<StateGameScriptInterface>) -> Void {
  wrappedMethod(timeDelta, stateContext, scriptInterface);
  HeadtrackOnStateUpdate(scriptInterface, "scene", timeDelta);
}

func HeadtrackTurnImpulseToBody(stateContext: ref<StateContext>, scriptInterface: ref<StateGameScriptInterface>) -> Void {
  let player: ref<PlayerPuppet> = scriptInterface.executionOwner as PlayerPuppet;
  if !IsDefined(player) {
    return;
  }
  let system: ref<HeadtrackCameraSystem> = HeadtrackCameraSystem.Get(player.GetGame());
  if IsDefined(system) {
    system.TurnImpulseToBody(stateContext, scriptInterface);
  }
}

@wrapMethod(DodgeEvents)
protected final func Dash(stateContext: ref<StateContext>, scriptInterface: ref<StateGameScriptInterface>, isExhausted: Bool, treatDashAsAirDash: Bool) -> Void {
  wrappedMethod(stateContext, scriptInterface, isExhausted, treatDashAsAirDash);
  HeadtrackTurnImpulseToBody(stateContext, scriptInterface);
}

@wrapMethod(DodgeEvents)
protected final func Dodge(stateContext: ref<StateContext>, scriptInterface: ref<StateGameScriptInterface>, isExhausted: Bool) -> Void {
  wrappedMethod(stateContext, scriptInterface, isExhausted);
  HeadtrackTurnImpulseToBody(stateContext, scriptInterface);
}

@wrapMethod(DodgeAirEvents)
protected final func Dodge(stateContext: ref<StateContext>, scriptInterface: ref<StateGameScriptInterface>) -> Void {
  wrappedMethod(stateContext, scriptInterface);
  HeadtrackTurnImpulseToBody(stateContext, scriptInterface);
}

@wrapMethod(gameuiCrosshairBaseGameController)
protected cb func OnInitialize() -> Bool {
  let result: Bool = wrappedMethod();
  let owner: ref<GameObject> = this.GetPlayerControlledObject();
  if IsDefined(owner) {
    let system: ref<HeadtrackCameraSystem> = HeadtrackCameraSystem.Get(owner.GetGame());
    if IsDefined(system) {
      system.RegisterCrosshair(this.GetRootWidget());
    }
  }
  return result;
}
