// CyberHeadtrack: key handling for the HeadtrackToggle / HeadtrackRecenter actions
// registered by r6/input/CyberHeadtrack.xml (merged by Input Loader). Custom actions are
// only delivered to listeners registered for them by name via
// PlayerPuppet.RegisterInputListener, so HeadtrackCameraSystem registers this listener
// when the player attaches and unregisters it on detach.

public class HeadtrackInputListener {
  public func Register(player: ref<PlayerPuppet>) -> Void {
    player.RegisterInputListener(this, n"HeadtrackToggle");
    player.RegisterInputListener(this, n"HeadtrackRecenter");
  }

  public func Unregister(player: ref<PlayerPuppet>) -> Void {
    player.UnregisterInputListener(this);
  }

  protected cb func OnAction(action: ListenerAction, consumer: ListenerActionConsumer) -> Bool {
    if !ListenerAction.IsButtonJustPressed(action) {
      return false;
    }
    if ListenerAction.IsAction(action, n"HeadtrackToggle") {
      HeadtrackLog("toggle key pressed");
      SetHeadtrackEnabled(!GetHeadtrackEnabled());
    } else {
      if ListenerAction.IsAction(action, n"HeadtrackRecenter") {
        HeadtrackLog("recenter key pressed");
        RecenterHeadtrack();
      }
    }
    return false;
  }
}
