#include "compatibility.h"

#include "dwm_api.h"

namespace effects {
namespace {

// DWM already draws a shadow behind every ordinary window. In system-corner
// mode ours is additive, so it is scaled back to this fraction — at full
// strength the two stack into something far heavier than the brief's "subtle".
// In precise mode DWM's shadow is gone and ours carries the whole effect.
const int kAdditiveShadowPercent = 55;

// Fraction of shadow strength removed from an unfocused window, per setting.
int ShadowDifferencePercent(ShadowDifference difference) {
  switch (difference) {
    case ShadowDifference::kLow:
      return 12;
    case ShadowDifference::kMedium:
      return 24;
    case ShadowDifference::kHigh:
      return 38;
  }
  return 24;
}

// Below this the window stops being comfortably readable, whatever combination
// of transparency and dimming asked for it.
const int kMinimumAlphaPercent = 70;

int ClampPercent(int value, int low, int high) {
  if (value < low) {
    return low;
  }
  if (value > high) {
    return high;
  }
  return value;
}

}  // namespace

CompatibilityManager::CompatibilityManager() {
  RefreshSystemTheme();
}

void CompatibilityManager::RefreshSystemTheme() {
  light_theme_ = SystemUsesLightTheme();
}

bool CompatibilityManager::NearlyFillsMonitor(const WindowInfo& info) {
  HMONITOR monitor = ::MonitorFromWindow(info.hwnd, MONITOR_DEFAULTTONEAREST);
  if (monitor == nullptr) {
    return false;
  }
  MONITORINFO monitor_info = {};
  monitor_info.cbSize = static_cast<DWORD>(sizeof(monitor_info));
  if (!::GetMonitorInfoW(monitor, &monitor_info)) {
    return false;
  }

  const LONG monitor_width =
      monitor_info.rcMonitor.right - monitor_info.rcMonitor.left;
  const LONG monitor_height =
      monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top;
  if (monitor_width <= 0 || monitor_height <= 0) {
    return false;
  }

  const LONG width = info.frame.right - info.frame.left;
  const LONG height = info.frame.bottom - info.frame.top;
  return width * 100 >= monitor_width * 85 &&
         height * 100 >= monitor_height * 85;
}

EffectPlan CompatibilityManager::PlanFor(const WindowInfo& info,
                                         const Config& config,
                                         const Capabilities& capabilities,
                                         bool active) const {
  EffectPlan plan;

  // ------------------------------------------------------------------ corners
  const bool wants_precise =
      config.corners_enabled && config.corner_mode == CornerMode::kPrecise;

  if (config.corners_enabled) {
    if (wants_precise && capabilities.precise_corners) {
      plan.corners = true;
      plan.precise = true;
      plan.corner_radius = config.corner_radius;
      // A maximised window has no visible corners to round, and a region on
      // one clips its own edges against the work area.
      if (info.maximized) {
        plan.corner_radius = 0;
      }
    } else if (capabilities.system_corners) {
      plan.corners = true;
      plan.precise = false;
      if (config.corner_radius == 0) {
        plan.dwm_corner = kCornerDoNotRound;
        plan.corner_radius = 0;
      } else if (config.corner_radius <= 6) {
        plan.dwm_corner = kCornerRoundSmall;
        plan.corner_radius = 4;
      } else {
        plan.dwm_corner = kCornerRound;
        plan.corner_radius = 8;
      }
      if (info.maximized) {
        // DWM squares maximised windows itself; matching that keeps the
        // shadow's caster shape honest.
        plan.corner_radius = 0;
      }
    }
  }

  // ------------------------------------------------------------------- shadow
  if (config.shadow_enabled && capabilities.custom_shadow &&
      !info.minimized && !info.maximized && !NearlyFillsMonitor(info)) {
    int strength = config.shadow_strength;
    if (!plan.precise) {
      strength = strength * kAdditiveShadowPercent / 100;
    }
    if (!active && config.active_emphasis) {
      strength -= strength * ShadowDifferencePercent(config.shadow_difference) /
                  100;
    }
    strength = ClampPercent(strength, 0, 100);

    if (strength > 0 && config.shadow_blur > 0) {
      plan.shadow = true;
      plan.shadow_strength = strength;
      plan.shadow_blur = config.shadow_blur;
      plan.shadow_offset = config.shadow_offset;
    }
  }

  // ------------------------------------------------------------------- motion
  // Nothing here is per-window: either this utility animates windows or the
  // system does, and the two cannot share one window without both being
  // visible at once.
  plan.own_transitions = config.animations_enabled;

  // ------------------------------------------------------------------- border
  if (config.active_emphasis && config.active_border &&
      capabilities.border_color) {
    plan.border = true;
    // Only the focused window carries a colour; everything else is handed back
    // to the system default rather than being given a second, duller colour.
    plan.border_color =
        active ? DwmApi::Instance().AccentBorderColor() : kDwmColorDefault;
  }

  // ------------------------------------------------------------ layered alpha
  // A window the application already made layered is off limits: forcing
  // LWA_ALPHA onto something that calls UpdateLayeredWindow corrupts it.
  const bool layered_allowed =
      capabilities.transparency && !info.already_layered;

  if (layered_allowed) {
    int percent = 100;
    if (config.transparency_enabled) {
      percent = config.opacity;
    }
    if (config.dim_inactive && !active) {
      percent -= config.dim_amount;
    }
    percent = ClampPercent(percent, kMinimumAlphaPercent, 100);

    if (percent < 100) {
      plan.layered = true;
      plan.alpha = percent * 255 / 100;
    }
  }

  // -------------------------------------------------------------- blur behind
  // Blur is only visible through a window that is actually see-through, so it
  // is gated on transparency rather than offered on its own.
  if (config.blur_enabled && config.transparency_enabled &&
      config.blur_preset != BlurPreset::kNone && capabilities.blur_behind &&
      plan.layered) {
    plan.blur = true;
    switch (config.blur_preset) {
      case BlurPreset::kLight:
        // The plain gaussian: cheap, and free of Acrylic's drag lag.
        plan.accent_state = kAccentEnableBlurBehind;
        plan.accent_tint = 0;
        break;
      case BlurPreset::kMedium:
        plan.accent_state = kAccentEnableAcrylicBlurBehind;
        plan.accent_tint = light_theme_ ? 0x30FFFFFFu : 0x30000000u;
        break;
      case BlurPreset::kStrong:
        plan.accent_state = kAccentEnableAcrylicBlurBehind;
        plan.accent_tint = light_theme_ ? 0x60FFFFFFu : 0x60000000u;
        break;
      case BlurPreset::kNone:
        plan.blur = false;
        break;
    }
  }

  // ----------------------------------------------------------------- backdrop
  if (config.backdrop != BackdropMode::kNone) {
    if (capabilities.system_backdrop) {
      plan.backdrop = true;
      switch (config.backdrop) {
        case BackdropMode::kMica:
          plan.backdrop_type = kBackdropMainWindow;
          break;
        case BackdropMode::kAcrylic:
          plan.backdrop_type = kBackdropTransientWindow;
          break;
        case BackdropMode::kAutomatic:
          // Owned windows are dialogs and palettes, which is exactly what
          // Acrylic's "transient" material is for.
          plan.backdrop_type =
              info.owned ? kBackdropTransientWindow : kBackdropMainWindow;
          break;
        case BackdropMode::kNone:
          plan.backdrop = false;
          break;
      }
    } else if (capabilities.legacy_mica &&
               config.backdrop != BackdropMode::kAcrylic) {
      // The legacy attribute is a plain boolean, so it can express Mica but
      // never Acrylic. Asking for Acrylic on these builds gets nothing rather
      // than something that is not Acrylic.
      plan.backdrop = true;
      plan.backdrop_type = kBackdropMainWindow;
    }
  }

  return plan;
}

}  // namespace effects
