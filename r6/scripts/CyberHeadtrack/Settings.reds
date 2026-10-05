// CyberHeadtrack: settings bridge for the Cyber Engine Tweaks overlay window
// (bin/x64/plugins/cyber_engine_tweaks/mods/CyberHeadtrack/init.lua). CET calls static
// methods on script classes, so these wrap the plugin's natives. Keys are
// "<profile>.<config.toml name>" with profile on_foot or vehicle, e.g. "vehicle.yaw_scale";
// flags read and write as 1.0 / 0.0. Set and Save return an error message, or an
// empty string on success.

native func GetHeadtrackSetting(key: String) -> Float;
native func SetHeadtrackSetting(key: String, value: Float) -> String;
native func SaveHeadtrackSettings() -> String;

public abstract class HeadtrackSettings {
  public static func Get(key: String) -> Float {
    return GetHeadtrackSetting(key);
  }

  public static func Set(key: String, value: Float) -> String {
    return SetHeadtrackSetting(key, value);
  }

  public static func Save() -> String {
    return SaveHeadtrackSettings();
  }

  public static func IsEnabled() -> Bool {
    return GetHeadtrackEnabled();
  }

  public static func SetEnabled(enabled: Bool) -> Void {
    SetHeadtrackEnabled(enabled);
  }

  public static func ActiveProfile() -> Int32 {
    return GetHeadtrackProfile();
  }

  public static func Recenter() -> Void {
    RecenterHeadtrack();
  }
}
