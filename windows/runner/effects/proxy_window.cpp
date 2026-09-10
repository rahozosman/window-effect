#include "proxy_window.h"

#include <dwmapi.h>

namespace effects {
namespace {

const wchar_t kProxyClassName[] = L"WindowEffectsProxy";

// DWM_THUMBNAIL_PROPERTIES flags, spelled out rather than depending on which
// Windows SDK this machine happens to have.
const DWORD kTnpRectDestination = 0x00000001;
const DWORD kTnpOpacity = 0x00000004;
const DWORD kTnpVisible = 0x00000008;

LRESULT CALLBACK ProxyProc(HWND window, UINT message, WPARAM wparam,
                           LPARAM lparam) {
  if (message == WM_ERASEBKGND) {
    // Every pixel comes from the thumbnail. Erasing would only be a chance to
    // flash a background nobody asked for.
    return 1;
  }
  return ::DefWindowProcW(window, message, wparam, lparam);
}

}  // namespace

ProxyWindow::~ProxyWindow() {
  Destroy();
}

bool ProxyWindow::EnsureClass() {
  static ATOM class_atom = 0;
  if (class_atom != 0) {
    return true;
  }
  WNDCLASSEXW window_class = {};
  window_class.cbSize = static_cast<UINT>(sizeof(window_class));
  window_class.lpfnWndProc = &ProxyProc;
  window_class.hInstance = ::GetModuleHandleW(nullptr);
  window_class.lpszClassName = kProxyClassName;
  window_class.hbrBackground = nullptr;
  class_atom = ::RegisterClassExW(&window_class);
  return class_atom != 0;
}

bool ProxyWindow::Create(HWND source, const RECT& frame) {
  Destroy();

  const int width = frame.right - frame.left;
  const int height = frame.bottom - frame.top;
  if (source == nullptr || width <= 0 || height <= 0 || !EnsureClass()) {
    return false;
  }

  // TRANSPARENT so clicks fall through to the real window, NOACTIVATE so the
  // copy never takes focus, TOPMOST because during a minimise the real
  // window's place in the z-order has already gone.
  overlay_ = ::CreateWindowExW(
      WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOPMOST,
      kProxyClassName, L"", WS_POPUP, frame.left, frame.top, width, height,
      nullptr, nullptr, ::GetModuleHandleW(nullptr), nullptr);
  if (overlay_ == nullptr) {
    return false;
  }

  // The proxy is a popup, and Windows 11 rounds those by default, which would
  // clip the corners off the copy.
  int preference = 1;  // DWMWCP_DONOTROUND
  ::DwmSetWindowAttribute(overlay_, 33, &preference,
                          static_cast<DWORD>(sizeof(preference)));

  HANDLE thumbnail = nullptr;
  const HRESULT registered = ::DwmRegisterThumbnail(
      overlay_, source, reinterpret_cast<PHTHUMBNAIL>(&thumbnail));
  if (FAILED(registered) || thumbnail == nullptr) {
    ::DestroyWindow(overlay_);
    overlay_ = nullptr;
    return false;
  }
  thumbnail_ = thumbnail;

  // Sized and filled before it is shown, so it never appears for one frame at
  // the wrong size or with nothing in it.
  Update(frame, 255);
  ::ShowWindow(overlay_, SW_SHOWNA);
  return true;
}

void ProxyWindow::Update(const RECT& frame, BYTE opacity) {
  if (overlay_ == nullptr) {
    return;
  }
  const int width = frame.right - frame.left;
  const int height = frame.bottom - frame.top;
  if (width <= 0 || height <= 0) {
    return;
  }

  ::SetWindowPos(overlay_, HWND_TOPMOST, frame.left, frame.top, width, height,
                 SWP_NOACTIVATE | SWP_NOSENDCHANGING);

  if (thumbnail_ == nullptr) {
    return;
  }
  DWM_THUMBNAIL_PROPERTIES properties = {};
  properties.dwFlags = kTnpRectDestination | kTnpOpacity | kTnpVisible;
  properties.rcDestination.left = 0;
  properties.rcDestination.top = 0;
  properties.rcDestination.right = width;
  properties.rcDestination.bottom = height;
  properties.opacity = opacity;
  properties.fVisible = TRUE;
  ::DwmUpdateThumbnailProperties(
      reinterpret_cast<HTHUMBNAIL>(thumbnail_), &properties);
}

void ProxyWindow::Destroy() {
  if (thumbnail_ != nullptr) {
    ::DwmUnregisterThumbnail(reinterpret_cast<HTHUMBNAIL>(thumbnail_));
    thumbnail_ = nullptr;
  }
  if (overlay_ != nullptr) {
    ::DestroyWindow(overlay_);
    overlay_ = nullptr;
  }
}

}  // namespace effects
