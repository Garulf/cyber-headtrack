// CyberHeadtrack: applies head-tracking yaw/pitch/roll (degrees, already in EulerAngles
// convention, from the RED4ext plugin's RTTI globals) as a first-person camera rotation
// offset every frame, plus a pitch-gated height/depth offset that keeps the player's body
// and clothes visible when looking down on foot.
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
// Per-frame drivers: the player state machine's OnUpdate for the active on-foot locomotion
// state (LocomotionEventsTransition) and for the driver and passenger vehicle states
// (DriveEvents, PassengerEvents). Each forwards to HeadtrackCameraSystem.OnFrame. A few
// transient on-foot states (climbing, superhero landing) don't call their base OnUpdate,
// so tracking holds still there until normal locomotion resumes.

native func GetHeadtrackYaw() -> Float;
native func GetHeadtrackPitch() -> Float;
native func GetHeadtrackRoll() -> Float;
native func GetHeadtrackBodyHeightOffset() -> Float;
native func GetHeadtrackBodyDepthOffset() -> Float;
native func GetHeadtrackEnabled() -> Bool;
native func SetHeadtrackEnabled(enabled: Bool) -> Void;
native func RecenterHeadtrack() -> Void;
native func HeadtrackLog(message: String) -> Void;

func HeadtrackFlagChanged(current: Bool, previous: Bool) -> Bool {
  return (current && !previous) || (!current && previous);
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
  private let framesSinceReport: Int32;
  private let engineRewritesSinceReport: Int32;

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

  public final func OnFrame(player: ref<PlayerPuppet>, source: String) -> Void {
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
    let enabled: Bool = GetHeadtrackEnabled();
    let thirdPerson: Bool = this.IsThirdPersonVehicleCamera(vehicle);
    if HeadtrackFlagChanged(enabled, this.lastEnabled) || HeadtrackFlagChanged(thirdPerson, this.lastThirdPerson) {
      this.lastEnabled = enabled;
      this.lastThirdPerson = thirdPerson;
      HeadtrackLog("camera loop sees enabled=" + ToString(enabled) + " mounted=" + ToString(IsDefined(vehicle))
        + " vehicleThirdPerson=" + ToString(thirdPerson));
    }
    if enabled {
      this.ReportPeriodically();
    }
    if enabled && !thirdPerson {
      this.Apply(camera, IsDefined(vehicle));
    } else {
      this.Restore(camera);
    }
  }

  private final func IsThirdPersonVehicleCamera(vehicle: ref<VehicleObject>) -> Bool {
    if !IsDefined(vehicle) {
      return false;
    }
    let cameraManager: ref<VehicleCameraManager> = vehicle.GetCameraManager();
    return IsDefined(cameraManager) && cameraManager.IsTPPActive();
  }

  private final func Apply(camera: ref<FPPCameraComponent>, mounted: Bool) -> Void {
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
    let targetOrientation: Quaternion = this.baseOrientation * offset;
    let targetPosition: Vector4 = this.basePosition;
    if !mounted {
      targetPosition.Z += GetHeadtrackBodyHeightOffset();
      targetPosition.Y += GetHeadtrackBodyDepthOffset();
    }

    camera.SetLocalOrientation(targetOrientation);
    camera.SetLocalPosition(targetPosition);
    this.writtenOrientation = targetOrientation;
    this.writtenPosition = targetPosition;
    this.applied = true;
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

func HeadtrackOnStateUpdate(scriptInterface: ref<StateGameScriptInterface>, source: String) -> Void {
  let player: ref<PlayerPuppet> = scriptInterface.executionOwner as PlayerPuppet;
  if !IsDefined(player) {
    return;
  }
  let system: ref<HeadtrackCameraSystem> = HeadtrackCameraSystem.Get(player.GetGame());
  if IsDefined(system) {
    system.OnFrame(player, source);
  }
}

@wrapMethod(LocomotionEventsTransition)
public func OnUpdate(timeDelta: Float, stateContext: ref<StateContext>, scriptInterface: ref<StateGameScriptInterface>) -> Void {
  wrappedMethod(timeDelta, stateContext, scriptInterface);
  HeadtrackOnStateUpdate(scriptInterface, "on foot");
}

@wrapMethod(DriveEvents)
public func OnUpdate(timeDelta: Float, stateContext: ref<StateContext>, scriptInterface: ref<StateGameScriptInterface>) -> Void {
  wrappedMethod(timeDelta, stateContext, scriptInterface);
  HeadtrackOnStateUpdate(scriptInterface, "driving");
}

@wrapMethod(PassengerEvents)
public func OnUpdate(timeDelta: Float, stateContext: ref<StateContext>, scriptInterface: ref<StateGameScriptInterface>) -> Void {
  wrappedMethod(timeDelta, stateContext, scriptInterface);
  HeadtrackOnStateUpdate(scriptInterface, "passenger");
}
