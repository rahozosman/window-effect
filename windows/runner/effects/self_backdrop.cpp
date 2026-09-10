#include "self_backdrop.h"

#include <uxtheme.h>
// After uxtheme.h: MARGINS is declared there, and dwmapi.h uses it without
// pulling it in itself.
#include <dwmapi.h>

#include "dwm_api.h"
#include "log.h"
#include "os_version.h"

namespace effects {

bool ApplySelfBackdrop(HWND window) {
  if (window == nullptr) {
    return false;
  }

  const Capabilities& capabilities = GetCapabilities();
  const DwmApi& dwm = DwmApi::Instance();

  // Deliberately the documented attribute only. DWMWA_MICA_EFFECT works on
  // 22000-22620, but it is undocumented, and guessing on our own window in
  // front of the user is a worse trade than a plain opaque window.
  if (!capabilities.system_backdrop || !dwm.has_window_attributes()) {
    LogInfo("no self backdrop: this build has no DWMWA_SYSTEMBACKDROP_TYPE");
    return false;
  }

  // The backdrop is drawn behind the whole window but only shows where nothing
  // has painted over it. Extending the frame across the client area is what
  // makes that area part of the same sheet of glass.
  MARGINS margins = {-1, -1, -1, -1};
  if (FAILED(::DwmExtendFrameIntoClientArea(window, &margins))) {
    LogWarn("no self backdrop: the frame could not be extended");
    return false;
  }

  int backdrop = static_cast<int>(kBackdropMainWindow);
  const HRESULT applied = dwm.SetAttribute(
      window, static_cast<DWORD>(kDwmwaSystemBackdropType), &backdrop,
      static_cast<DWORD>(sizeof(backdrop)));
  if (FAILED(applied)) {
    // Undo the frame extension rather than leaving the window half-configured
    // with a transparent client area and nothing behind it.
    MARGINS none = {0, 0, 0, 0};
    ::DwmExtendFrameIntoClientArea(window, &none);
    LogWarn("no self backdrop: DWM refused the backdrop type");
    return false;
  }

  LogInfo("settings window is using its own Mica backdrop");
  return true;
}

}  // namespace effects
