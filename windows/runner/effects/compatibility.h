#ifndef RUNNER_EFFECTS_COMPATIBILITY_H_
#define RUNNER_EFFECTS_COMPATIBILITY_H_

#include <windows.h>

#include "config.h"
// EffectPlan's defaults are DWM's own constants, so the header that declares
// them has to come with it rather than be assumed present at each use site.
#include "dwm_api.h"
#include "os_version.h"
#include "window_classifier.h"

namespace effects {

// The resolved, per-window answer to "what may we actually do here".
//
// Every effect manager reads this and nothing else. Version gates, window
// quirks, focus state and the interaction between settings are all decided in
// one place, so no manager has to re-derive them and they cannot disagree.
struct EffectPlan {
  // ------------------------------------------------------------------ corners
  bool corners = false;
  // Use SetWindowRgn for an exact radius instead of the DWM preference.
  bool precise = false;
  // The radius that will actually render, in pixels. The shadow uses this so
  // its caster shape matches the window's real silhouette.
  int corner_radius = 0;
  int dwm_corner = kCornerDoNotRound;

  // ------------------------------------------------------------------- shadow
  bool shadow = false;
  // 0-100, already scaled for corner mode and focus state.
  int shadow_strength = 0;
  int shadow_blur = 0;
  int shadow_offset = 0;

  // ------------------------------------------------------------------- border
  bool border = false;
  COLORREF border_color = kDwmColorDefault;

  // ------------------------------------------------- layered alpha (Phase 4)
  // Covers both transparency and inactive dimming; they are the same mechanism
  // and must never fight over the same window.
  bool layered = false;
  int alpha = 255;

  // --------------------------------------------------- blur behind (Phase 4)
  bool blur = false;
  int accent_state = kAccentDisabled;
  unsigned int accent_tint = 0;  // ABGR

  // ------------------------------------------------------ backdrop (Phase 4)
  bool backdrop = false;
  int backdrop_type = kBackdropAuto;

  // ------------------------------------------------------------------ motion
  // True when this utility is the one animating the window, in which case
  // DWM's own transitions stay off for as long as it is managed. Off again
  // the moment motion is switched off, or the window is released.
  bool own_transitions = false;
};

// Decides the plan for one window.
//
// Deliberately the only place that knows about version gates, window quirks and
// the interactions between settings — that blur needs transparency, that
// dimming and transparency share one alpha, that a maximised window has no
// corners to round. Keeping it here is what stops two effect managers from
// reaching different conclusions about the same window.
class CompatibilityManager {
 public:
  CompatibilityManager();

  // |active| is whether this window currently has focus.
  EffectPlan PlanFor(const WindowInfo& info, const Config& config,
                     const Capabilities& capabilities, bool active) const;

  // Re-reads the system light/dark preference. The engine calls this whenever
  // it re-evaluates everything, so an Acrylic tint chosen for a dark desktop
  // does not survive a switch to light.
  void RefreshSystemTheme();

 private:
  // A window this close to filling its monitor gets no companion shadow: the
  // shadow would be invisible behind it, and its bitmap is the largest one we
  // would ever allocate.
  static bool NearlyFillsMonitor(const WindowInfo& info);

  bool light_theme_ = false;
};

}  // namespace effects

#endif  // RUNNER_EFFECTS_COMPATIBILITY_H_
