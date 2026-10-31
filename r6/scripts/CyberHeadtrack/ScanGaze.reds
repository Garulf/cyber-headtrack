// CyberHeadtrack: eye tracking in scan mode (Tab). While the player state machine's Vision
// state is Focus, the scanner reticle moves to the plugin's scan point: the gaze point, or the
// target locked under it. The plugin also bends the engine's targeting ray through that point,
// so scanning and quickhacks follow the reticle without the camera moving.
//
// Each frame this publishes the camera projection's scales and the quickhackable targets on
// screen (by entity hash, in screen coordinates: 0..1, top-left origin) for the plugin's lock.
// Screen coordinates map to NDC as x = (ndcX + 1) / 2, y = (1 - ndcY) / 2.
//
// Two widgets are candidates for the red centre reticle: the scanner HUD's "crosshair" and the
// scanner border's crosshair container. Both are moved; the log says which were found.

native func SetHeadtrackScanFrame(active: Bool, ndcPerTanX: Float, ndcPerTanY: Float) -> Void;
native func ClearHeadtrackScanCandidates() -> Void;
native func AddHeadtrackScanCandidate(id: Uint64, x: Float, y: Float) -> Void;
native func UpdateHeadtrackScanLock(timeDelta: Float) -> Void;
native func GetHeadtrackScanPoint() -> Vector4;
native func GetHeadtrackScanLockedId() -> Uint64;

func HeadtrackMaxScanCandidates() -> Int32 = 64
func HeadtrackScanTargetRange() -> Float = 50.0
func HeadtrackPuppetAimHeight() -> Float = 1.1
func HeadtrackScanReportSeconds() -> Float = 2.0
// The projection is measured through a point this far ahead and one meter to the side.
func HeadtrackProjectionProbeDistance() -> Float = 10.0

func HeadtrackEntityHash(id: EntityID) -> String {
  return EntityID.IsDefined(id) ? ToString(EntityID.GetHash(id)) : "none";
}

func HeadtrackChildNames(container: ref<inkCompoundWidget>) -> String {
  let names: String = "";
  let i: Int32 = 0;
  while i < container.GetNumChildren() {
    names += NameToString(container.GetWidgetByIndex(i).GetName()) + " ";
    i += 1;
  }
  return names;
}

public class HeadtrackScanGaze {
  private let reticles: array<wref<inkWidget>>;
  private let translation: Vector2;
  private let candidateCount: Int32;
  private let secondsSinceReport: Float;
  private let loggedWindowMode: Bool;

  public final func RegisterReticle(reticle: ref<inkWidget>, source: String) -> Void {
    ArrayPush(this.reticles, reticle);
    HeadtrackLog("scan gaze: reticle " + NameToString(reticle.GetName()) + " found in " + source);
  }

  public final func Update(player: ref<PlayerPuppet>, timeDelta: Float) -> Void {
    let scanning: Bool = this.IsScanning(player);
    let cameraSystem: ref<CameraSystem> = GameInstance.GetCameraSystem(player.GetGame());
    let transform: Transform;
    cameraSystem.GetActiveCameraWorldTransform(transform);
    let forward: Vector4 = cameraSystem.GetActiveCameraForward();
    let tangent: Float = 1.0 / HeadtrackProjectionProbeDistance();
    let rightProbe: Vector4 = this.ProjectProbe(cameraSystem, transform.position, forward, cameraSystem.GetActiveCameraRight());
    let upProbe: Vector4 = this.ProjectProbe(cameraSystem, transform.position, forward, cameraSystem.GetActiveCameraUp());
    SetHeadtrackScanFrame(scanning, rightProbe.X / tangent, upProbe.Y / tangent);

    if scanning {
      this.LogWindowModeOnce(player);
      this.PublishCandidates(player, cameraSystem, transform.position, forward);
      UpdateHeadtrackScanLock(timeDelta);
    }
    let scan: Vector4 = GetHeadtrackScanPoint();
    let translation: Vector2 = new Vector2(0.0, 0.0);
    if scanning {
      translation = new Vector2((scan.X - 0.5) * 2.0 * HeadtrackHalfHudWidth(), (scan.Y - 0.5) * 2.0 * HeadtrackHalfHudHeight());
      this.ReportPeriodically(player, scan, timeDelta);
    } else {
      this.secondsSinceReport = 0.0;
    }
    this.MoveReticles(translation);
  }

  private final func IsScanning(player: ref<PlayerPuppet>) -> Bool {
    let psm: ref<IBlackboard> = player.GetPlayerStateMachineBlackboard();
    return IsDefined(psm) && psm.GetInt(GetAllBlackboardDefs().PlayerStateMachine.Vision) == EnumInt(gamePSMVision.Focus);
  }

  private final func ProjectProbe(cameraSystem: ref<CameraSystem>, origin: Vector4, forward: Vector4, axis: Vector4) -> Vector4 {
    let point: Vector4 = origin + forward * HeadtrackProjectionProbeDistance() + axis;
    point.W = 1.0;
    return cameraSystem.ProjectPoint(point);
  }

  private final func PublishCandidates(player: ref<PlayerPuppet>, cameraSystem: ref<CameraSystem>, origin: Vector4, forward: Vector4) -> Void {
    ClearHeadtrackScanCandidates();
    let query: TargetSearchQuery;
    TargetSearchQuery.SetComponentFilter(query, TargetComponentFilterType.QuickHack);
    query.searchFilter = TSF_Quickhackable();
    query.testedSet = TargetingSet.Visible;
    query.maxDistance = HeadtrackScanTargetRange();
    let parts: array<TS_TargetPartInfo>;
    GameInstance.GetTargetingSystem(player.GetGame()).GetTargetParts(player, query, parts);

    let published: array<Uint32>;
    for part in parts {
      let component: wref<TargetingComponent> = TS_TargetPartInfo.GetComponent(part);
      let entity: ref<Entity> = IsDefined(component) ? component.GetEntity() : null;
      if IsDefined(entity) && ArraySize(published) < HeadtrackMaxScanCandidates() {
        let hash: Uint32 = EntityID.GetHash(entity.GetEntityID());
        if !ArrayContains(published, hash) && this.PublishCandidate(entity, hash, cameraSystem, origin, forward) {
          ArrayPush(published, hash);
        }
      }
    }
    this.candidateCount = ArraySize(published);
  }

  // Publishes the entity if its aim point is in front of the camera and on screen.
  private final func PublishCandidate(entity: ref<Entity>, hash: Uint32, cameraSystem: ref<CameraSystem>, origin: Vector4,
    forward: Vector4) -> Bool {
    let point: Vector4 = entity.GetWorldPosition();
    if IsDefined(entity as ScriptedPuppet) {
      point.Z += HeadtrackPuppetAimHeight();
    }
    if Vector4.Dot(point - origin, forward) <= 0.0 {
      return false;
    }
    point.W = 1.0;
    let ndc: Vector4 = cameraSystem.ProjectPoint(point);
    if AbsF(ndc.X) > 1.0 || AbsF(ndc.Y) > 1.0 {
      return false;
    }
    AddHeadtrackScanCandidate(Cast<Uint64>(hash), (ndc.X + 1.0) / 2.0, (1.0 - ndc.Y) / 2.0);
    return true;
  }

  private final func MoveReticles(translation: Vector2) -> Void {
    if HeadtrackSameVector2(translation, this.translation) {
      return;
    }
    this.translation = translation;
    let i: Int32 = ArraySize(this.reticles) - 1;
    while i >= 0 {
      let reticle: wref<inkWidget> = this.reticles[i];
      if IsDefined(reticle) {
        reticle.SetTranslation(translation);
      } else {
        ArrayErase(this.reticles, i);
      }
      i -= 1;
    }
  }

  private final func LogWindowModeOnce(player: ref<PlayerPuppet>) -> Void {
    if this.loggedWindowMode {
      return;
    }
    this.loggedWindowMode = true;
    let mode: ref<ConfigVarListString> = GameInstance.GetSettingsSystem(player.GetGame()).GetVar(n"/video/display", n"WindowMode") as ConfigVarListString;
    let value: String = IsDefined(mode) ? mode.GetValue() : "unknown";
    HeadtrackLog("scan gaze: window mode " + value);
    if Equals(value, "Windowed") {
      HeadtrackLog("scan gaze: windowed mode, gaze is mapped to the whole display");
    }
  }

  // The in-game test rounds are scored from this line: the native redirect works when
  // scanned (UI_Scanner.ScannedObject) matches lockedId.
  private final func ReportPeriodically(player: ref<PlayerPuppet>, scan: Vector4, timeDelta: Float) -> Void {
    this.secondsSinceReport += timeDelta;
    if this.secondsSinceReport < HeadtrackScanReportSeconds() {
      return;
    }
    this.secondsSinceReport = 0.0;
    let blackboards: ref<BlackboardSystem> = GameInstance.GetBlackboardSystem(player.GetGame());
    let scanned: EntityID = blackboards.Get(GetAllBlackboardDefs().UI_Scanner).GetEntityID(GetAllBlackboardDefs().UI_Scanner.ScannedObject);
    let lookAt: EntityID = blackboards.Get(GetAllBlackboardDefs().UI_TargetingInfo).GetEntityID(GetAllBlackboardDefs().UI_TargetingInfo.CurrentVisibleTarget);
    let lockedId: Uint64 = GetHeadtrackScanLockedId();
    HeadtrackLog("scan: point " + FloatToString(scan.X) + "," + FloatToString(scan.Y) + " w=" + FloatToString(scan.Z)
      + " locked=" + (scan.W > 0.5 ? "1" : "0") + " candidates=" + ToString(this.candidateCount)
      + " lockedId=" + (scan.W > 0.5 ? ToString(lockedId) : "none") + " scanned=" + HeadtrackEntityHash(scanned)
      + " lookAt=" + HeadtrackEntityHash(lookAt) + " reticles=" + ToString(ArraySize(this.reticles)));
  }
}

func HeadtrackScanGazeFor(controller: ref<inkGameController>) -> ref<HeadtrackScanGaze> {
  let owner: ref<GameObject> = controller.GetPlayerControlledObject();
  if !IsDefined(owner) {
    return null;
  }
  let system: ref<HeadtrackCameraSystem> = HeadtrackCameraSystem.Get(owner.GetGame());
  return IsDefined(system) ? system.ScanGaze() : null;
}

// The scanner HUD spawns asynchronously, so its reticle only exists from here on.
@wrapMethod(scannerGameController)
protected cb func OnScannerHudSpawned(widget: ref<inkWidget>, userData: ref<IScriptable>) -> Bool {
  let result: Bool = wrappedMethod(widget, userData);
  let scanGaze: ref<HeadtrackScanGaze> = HeadtrackScanGazeFor(this);
  let hud: ref<inkCompoundWidget> = widget as inkCompoundWidget;
  if IsDefined(scanGaze) && IsDefined(hud) {
    let reticle: ref<inkWidget> = hud.GetWidget(n"crosshair");
    if IsDefined(reticle) {
      scanGaze.RegisterReticle(reticle, "scanner hud");
    } else {
      HeadtrackLog("scan gaze: no crosshair in scanner hud; children: " + HeadtrackChildNames(hud));
    }
  }
  return result;
}

@wrapMethod(scannerBorderGameController)
protected cb func OnInitialize() -> Bool {
  let result: Bool = wrappedMethod();
  let scanGaze: ref<HeadtrackScanGaze> = HeadtrackScanGazeFor(this);
  let reticle: ref<inkWidget> = inkWidgetRef.Get(this.m_crosshairContainer);
  if IsDefined(scanGaze) {
    if IsDefined(reticle) {
      scanGaze.RegisterReticle(reticle, "scanner border");
    } else {
      HeadtrackLog("scan gaze: scanner border has no crosshair container");
    }
  }
  return result;
}
