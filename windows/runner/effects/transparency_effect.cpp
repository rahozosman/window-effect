#include "transparency_effect.h"

#include "log.h"

namespace effects {

bool TransparencyEffect::Apply(ManagedWindow& window, const EffectPlan& plan) {
  // Blur first when turning off, so the window is opaque again only after the
  // accent is gone — the other order flashes a frosted opaque window.
  bool ok = true;
  if (!plan.blur) {
    window.RestoreAccentPolicy();
  }
  if (!ApplyAlpha(window, plan)) {
    ok = false;
  }
  if (plan.blur && !ApplyBlur(window, plan)) {
    ok = false;
  }
  return ok;
}

bool TransparencyEffect::ApplyAlpha(ManagedWindow& window,
                                    const EffectPlan& plan) {
  if (!plan.layered) {
    window.RestoreExStyle();
    return true;
  }

  HWND target = window.hwnd();
  window.CaptureExStyle();

  const LONG_PTR current = ::GetWindowLongPtrW(target, GWL_EXSTYLE);
  if ((current & WS_EX_LAYERED) == 0) {
    // SetWindowLongPtr returns 0 both on failure and when the previous value
    // happened to be 0, so the error code is the only way to tell them apart.
    ::SetLastError(0);
    const LONG_PTR previous =
        ::SetWindowLongPtrW(target, GWL_EXSTYLE, current | WS_EX_LAYERED);
    const DWORD error = ::GetLastError();
    if (previous == 0 && error != 0) {
      // Almost always UIPI refusing a higher-integrity window. Nothing was
      // changed, so there is nothing to undo.
      LogWarn("layered style refused by " + Narrow(window.info().executable) +
              " (error " + std::to_string(error) + ")");
      return false;
    }
  }

  if (::SetLayeredWindowAttributes(target, 0,
                                   static_cast<BYTE>(plan.alpha),
                                   LWA_ALPHA) == FALSE) {
    LogWarn("layered alpha refused by " + Narrow(window.info().executable));
    return false;
  }

  // A window that has just become layered will not repaint on its own.
  ::RedrawWindow(target, nullptr, nullptr,
                 RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
  return true;
}

bool TransparencyEffect::ApplyBlur(ManagedWindow& window,
                                   const EffectPlan& plan) {
  const DwmApi& dwm = DwmApi::Instance();
  if (!dwm.has_composition_attribute()) {
    return true;
  }

  window.CaptureAccentPolicy();

  AccentPolicy policy = {};
  policy.state = plan.accent_state;
  policy.flags = 0;
  policy.gradient_color = plan.accent_tint;
  policy.animation_id = 0;

  WindowCompositionAttributeData data = {};
  data.attribute = kWcaAccentPolicy;
  data.data = &policy;
  data.size = sizeof(policy);

  if (!dwm.SetCompositionAttribute(window.hwnd(), &data)) {
    LogWarn("blur refused by " + Narrow(window.info().executable));
    return false;
  }
  return true;
}

}  // namespace effects
