#ifndef RUNNER_EFFECTS_BACKDROP_EFFECT_H_
#define RUNNER_EFFECTS_BACKDROP_EFFECT_H_

#include "compatibility.h"
#include "managed_window.h"

namespace effects {

// Applies Mica or Acrylic through DWM's system backdrop.
//
// Worth being clear about what this actually produces: the material is drawn
// *behind* the window's content, and an ordinary Win32 application paints its
// client area opaquely. So on most apps the material survives only where the
// app does not paint — the title bar. A consistently Mica-tinted title bar
// across every window is the effect, not a shortfall of one; it is exactly
// what Mica For Everyone delivers. Apps that extend their frame into the
// client area get the material throughout.
//
// Fallback chain, per FEASIBILITY §2.5:
//
//   build >= 22621  ->  DWMWA_SYSTEMBACKDROP_TYPE (Mica, Acrylic, Tabbed)
//   build >= 22000  ->  DWMWA_MICA_EFFECT, an undocumented boolean that can
//                       express Mica but never Acrylic
//   otherwise       ->  nothing; the material does not exist on that build
//
// One thing this deliberately does not touch is DWMWA_USE_IMMERSIVE_DARK_MODE.
// Forcing an app's title bar dark when the app did not ask for it can leave it
// drawing dark text on a dark caption, and an unreadable title bar is a much
// worse outcome than a light one.
class BackdropEffect {
 public:
  static bool Apply(ManagedWindow& window, const EffectPlan& plan);

 private:
  static bool SetLegacyMica(ManagedWindow& window, bool enabled);
};

}  // namespace effects

#endif  // RUNNER_EFFECTS_BACKDROP_EFFECT_H_
