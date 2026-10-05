#pragma once

// Writes one printf-style line to this plugin's RED4ext log (red4ext/logs/CyberHeadtrack-*.log).
// Safe to call from any thread; a no-op until the plugin has been loaded.
void PluginLog(const char* format, ...);
