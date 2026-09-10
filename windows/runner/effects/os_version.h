#ifndef RUNNER_EFFECTS_OS_VERSION_H_
#define RUNNER_EFFECTS_OS_VERSION_H_

#include <cstdint>

namespace effects {

// What this machine can actually do, combining the Windows build number with
// whether the entry points each effect needs actually resolved.
//
// This is the authoritative answer. The Dart side derives the same set from
// Platform.operatingSystemVersion as a fallback, but only this one can tell
// that an export is missing.
struct Capabilities {
  uint32_t build_number = 0;

  bool system_corners = false;   // DWMWA_WINDOW_CORNER_PREFERENCE
  bool precise_corners = false;  // SetWindowRgn
  bool custom_shadow = false;    // layered companion window
  bool transparency = false;     // WS_EX_LAYERED + SetLayeredWindowAttributes
  bool blur_behind = false;      // SetWindowCompositionAttribute
  bool system_backdrop = false;  // DWMWA_SYSTEMBACKDROP_TYPE
  bool legacy_mica = false;      // DWMWA_MICA_EFFECT
  bool border_color = false;     // DWMWA_BORDER_COLOR
};

// Whether apps are currently using the light theme. Read on demand rather than
// cached in Capabilities: API availability never changes, but the user can
// flip the theme while the engine is running.
bool SystemUsesLightTheme();

// Computed once, on first use.
const Capabilities& GetCapabilities();

uint32_t WindowsBuildNumber();

}  // namespace effects

#endif  // RUNNER_EFFECTS_OS_VERSION_H_
