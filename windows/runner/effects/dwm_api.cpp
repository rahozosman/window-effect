#include "dwm_api.h"

#include "log.h"

namespace effects {

DwmApi::DwmApi() {
  // LOAD_LIBRARY_SEARCH_SYSTEM32 keeps a planted dwmapi.dll beside the
  // executable from being picked up.
  dwm_module_ = ::LoadLibraryExW(L"dwmapi.dll", nullptr,
                                 LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (dwm_module_ != nullptr) {
    set_window_attribute_ = reinterpret_cast<SetWindowAttributeFn>(
        ::GetProcAddress(dwm_module_, "DwmSetWindowAttribute"));
    get_window_attribute_ = reinterpret_cast<GetWindowAttributeFn>(
        ::GetProcAddress(dwm_module_, "DwmGetWindowAttribute"));
  }

  // user32 is already loaded into every process; no reference is taken.
  HMODULE user32 = ::GetModuleHandleW(L"user32.dll");
  if (user32 != nullptr) {
    set_composition_attribute_ =
        reinterpret_cast<SetWindowCompositionAttributeFn>(
            ::GetProcAddress(user32, "SetWindowCompositionAttribute"));
    get_composition_attribute_ =
        reinterpret_cast<GetWindowCompositionAttributeFn>(
            ::GetProcAddress(user32, "GetWindowCompositionAttribute"));
  }

  if (dwm_module_ != nullptr) {
    get_colorization_color_ = reinterpret_cast<GetColorizationColorFn>(
        ::GetProcAddress(dwm_module_, "DwmGetColorizationColor"));
  }

  if (!has_window_attributes()) {
    LogWarn("dwmapi window attributes unavailable; corners and backdrop are off");
  }
  if (!has_composition_attribute()) {
    LogWarn("SetWindowCompositionAttribute unavailable; blur is off");
  }
}

const DwmApi& DwmApi::Instance() {
  static const DwmApi instance;
  return instance;
}

HRESULT DwmApi::SetAttribute(HWND window, DWORD attribute, const void* value,
                             DWORD size) const {
  if (set_window_attribute_ == nullptr) {
    return E_NOTIMPL;
  }
  return set_window_attribute_(window, attribute, value, size);
}

HRESULT DwmApi::GetAttribute(HWND window, DWORD attribute, void* value,
                             DWORD size) const {
  if (get_window_attribute_ == nullptr) {
    return E_NOTIMPL;
  }
  return get_window_attribute_(window, attribute, value, size);
}

bool DwmApi::SetCompositionAttribute(
    HWND window, WindowCompositionAttributeData* data) const {
  if (set_composition_attribute_ == nullptr) {
    return false;
  }
  return set_composition_attribute_(window, data) != FALSE;
}

bool DwmApi::GetCompositionAttribute(
    HWND window, WindowCompositionAttributeData* data) const {
  if (get_composition_attribute_ == nullptr) {
    return false;
  }
  return get_composition_attribute_(window, data) != FALSE;
}

COLORREF DwmApi::AccentBorderColor() const {
  if (get_colorization_color_ == nullptr) {
    return kDwmColorDefault;
  }

  DWORD colorization = 0;
  BOOL opaque = FALSE;
  if (FAILED(get_colorization_color_(&colorization, &opaque))) {
    return kDwmColorDefault;
  }

  // DwmGetColorizationColor answers in ARGB; COLORREF wants 0x00BBGGRR.
  const DWORD red = (colorization >> 16) & 0xFF;
  const DWORD green = (colorization >> 8) & 0xFF;
  const DWORD blue = colorization & 0xFF;
  return RGB(red, green, blue);
}

bool DwmApi::IsCloaked(HWND window) const {
  DWORD cloaked = 0;
  const HRESULT result = GetAttribute(window, kDwmwaCloaked, &cloaked,
                                      static_cast<DWORD>(sizeof(cloaked)));
  if (FAILED(result)) {
    return false;
  }
  return cloaked != 0;
}

RECT DwmApi::FrameBounds(HWND window) const {
  RECT bounds = {};
  const HRESULT result =
      GetAttribute(window, kDwmwaExtendedFrameBounds, &bounds,
                   static_cast<DWORD>(sizeof(bounds)));
  if (SUCCEEDED(result) && bounds.right > bounds.left &&
      bounds.bottom > bounds.top) {
    return bounds;
  }

  RECT fallback = {};
  ::GetWindowRect(window, &fallback);
  return fallback;
}

}  // namespace effects
