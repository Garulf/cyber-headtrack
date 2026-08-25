// CyberHeadtrack: key handling for the HeadtrackToggle / HeadtrackRecenter actions
// registered by r6/input/CyberHeadtrack.xml (merged by Input Loader). Uses the
// standard custom-keybind hook: PlayerPuppet.OnAction receives every input action
// routed to the player, no explicit RegisterInputListener needed (verified against
// vanilla scripts and mods wrapping it, e.g. psiberx/cp2077-cyberware-ex
// scripts/Overrides/PlayerPuppet.reds; the callback returns Bool, not Void).
//
// The native funcs are declared once in CameraApply.reds; redscript globals are
// shared across files, so they are callable here without redeclaration.

@wrapMethod(PlayerPuppet)
protected cb func OnAction(action: ListenerAction, consumer: ListenerActionConsumer) -> Bool {
  if ListenerAction.IsButtonJustPressed(action) {
    if ListenerAction.IsAction(action, n"HeadtrackToggle") {
      SetHeadtrackEnabled(!GetHeadtrackEnabled());
    } else {
      if ListenerAction.IsAction(action, n"HeadtrackRecenter") {
        RecenterHeadtrack();
      }
    }
  }
  return wrappedMethod(action, consumer);
}
