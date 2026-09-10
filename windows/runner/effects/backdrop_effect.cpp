#include "backdrop_effect.h"

#include "log.h"
#include "os_version.h"

namespace effects {

bool BackdropEffect::Apply(ManagedWindow& window, const EffectPlan& plan) {
  const Capabilities& capabilities = GetCapabilities();

  if (!plan.backdrop) {
    if (capabilities.system_backdrop) {
      window.RestoreBackdropType();
    } else if (capabilities.legacy_mica) {
      SetLegacyMica(window, false);
    }
    return true;
  }

  if (capabilities.system_backdrop) {
    window.CaptureBackdropType();

    int backdrop = plan.backdrop_type;
    const HRESULT result = DwmApi::Instance().SetAttribute(
        window.hwnd(), kDwmwaSystemBackdropType, &backdrop,
        static_cast<DWORD>(sizeof(backdrop)));

    if (FAILED(result)) {
      LogWarn("backdrop refused by " + Narrow(window.info().executable));
      return false;
    }
    return true;
  }

  if (capabilities.legacy_mica) {
    return SetLegacyMica(window, true);
  }

  return true;
}

bool BackdropEffect::SetLegacyMica(ManagedWindow& window, bool enabled) {
  // The undocumented attribute has no getter worth trusting, so there is
  // nothing to capture: the original state of a window on these builds is
  // always "no Mica", and turning it off restores exactly that.
  BOOL value = enabled ? TRUE : FALSE;
  const HRESULT result = DwmApi::Instance().SetAttribute(
      window.hwnd(), kDwmwaMicaEffect, &value,
      static_cast<DWORD>(sizeof(value)));

  if (FAILED(result)) {
    if (enabled) {
      LogWarn("legacy mica refused by " + Narrow(window.info().executable));
    }
    return false;
  }
  return true;
}

}  // namespace effects
