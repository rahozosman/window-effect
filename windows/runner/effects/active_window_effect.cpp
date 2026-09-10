#include "active_window_effect.h"

#include "log.h"

namespace effects {

bool ActiveWindowEffect::Apply(ManagedWindow& window, const EffectPlan& plan) {
  if (!plan.border) {
    window.RestoreBorderColor();
    return true;
  }

  window.CaptureBorderColor();

  COLORREF color = plan.border_color;
  const HRESULT result = DwmApi::Instance().SetAttribute(
      window.hwnd(), kDwmwaBorderColor, &color,
      static_cast<DWORD>(sizeof(color)));

  if (FAILED(result)) {
    LogWarn("border colour refused by " + Narrow(window.info().executable));
    return false;
  }
  return true;
}

}  // namespace effects
