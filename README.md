# cyber-headtrack

A RED4ext + redscript mod for Cyberpunk 2077 that reads head-pose data from a Tobii Eye
Tracker 5 and applies it as a first-person camera glance. No eye/gaze tracking is used.

## Requirements

- Cyberpunk 2077, with [RED4ext](https://github.com/WopsS/RED4ext) and
  [redscript](https://github.com/jac3km4/redscript) installed.
- A Tobii Eye Tracker 5.
- Tobii's own consumer software (Tobii Game Hub or Tobii Experience) installed, so
  Windows recognizes the Eye Tracker 5 hardware in the first place. This mod does not
  replace that driver layer; it only talks to the tracker once it is enumerable.

**On the Stream Engine SDK:** this mod links against and bundles Tobii's Stream Engine
runtime (`tobii_stream_engine.dll`) directly in the plugin folder. Under the Tobii Core
SDK and Tobii Gaming SDK Agreement (v1.2), §3.1 permits distributing Applications built
on the Software Components -- including the Stream Engine -- for "Gaze Interaction Use"
(using tracking data as interactive user input, which is what this mod does), and §3.3
grants the sub-licensing rights needed to redistribute the Stream Engine DLL itself to
end users as part of the Application. So no separate SDK download or install step is
needed for the DLL; you only need the Game Hub/Experience driver described above for the
hardware itself.

## Install

1. Copy `red4ext/plugins/CyberHeadtrack/` into your Cyberpunk 2077 install directory's
   `red4ext/plugins/` folder. This includes the plugin DLL, the bundled
   `tobii_stream_engine.dll`, and `config.toml`.
2. Copy `r6/scripts/CyberHeadtrack/` into your Cyberpunk 2077 install directory's
   `r6/scripts/` folder.
3. Launch the game. Tracking is off by default.
4. Enable tracking. **v1 does not ship a keybind for this.** The native functions
   `SetHeadtrackEnabled(enabled: Bool)` and `RecenterHeadtrack()` are registered and
   callable from redscript (see `r6/scripts/CyberHeadtrack/CameraApply.reds`), but no
   `.reds` script in this release wires them to a key press or an in-game menu -- you
   would need to call them yourself, e.g. from another mod's console/binding, until a
   keybind script is added.

   The mechanism a future keybind script would use is already established elsewhere in
   this codebase: `@wrapMethod(PlayerPuppet) protected cb func OnAction(action:
   ListenerAction, consumer: ListenerActionConsumer) -> Void`, checking
   `ListenerAction.IsAction(action, n"YourBindingName")` together with
   `ListenerAction.IsButtonJustPressed(action)`, paired with a custom binding registered
   in `r6/config/inputUserMappings.xml` (or via a mod-input-loader framework, depending
   on the RED4ext/redscript ecosystem's current convention at the time it's built). That
   script is not part of this task and is not shipped in v1.

## Configuration

Edit `red4ext/plugins/CyberHeadtrack/config.toml`:

- `rot_sensitivity` / `lean_sensitivity`: higher values require more head movement to
  reach the same in-game rotation.
- `rot_deadzone_deg` / `lean_deadzone_cm`: movement below this threshold is ignored.
- `max_yaw_deg` / `max_pitch_deg`: clamps on how far the camera can rotate.

## Scope

v1 applies head tracking as a direct camera rotation -- your aim/weapon direction moves
with the camera, the same way flight-sim TrackIR support works. A separate effort is
investigating whether aim can be kept independent of the camera (glance without changing
where you'd shoot).

Tracking is applied on foot only. The camera hook wraps
`LocomotionEventsTransition.OnUpdate`, the per-frame update for the active on-foot
locomotion state, since vanilla `PlayerPuppet` has no `OnUpdate` of its own to wrap.
Practically this means:

- Head tracking does **not** apply while driving or riding in a vehicle.
- Head tracking briefly stops applying during a few transient player states outside the
  normal locomotion state machine, such as climbing and the superhero-landing animation.
  It resumes automatically once you're back in normal on-foot locomotion.

A pitch-gated body-visibility offset (raising and pulling back the camera as you look
down) is applied on top of the rotation so V's body and clothes stay in frame instead of
clipping through the first-person view; this is tuned by the built-in constants in
`CameraApply.reds`, not by `config.toml`.

## License

MIT.
