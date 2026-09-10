#ifndef RUNNER_EFFECTS_DWM_API_H_
#define RUNNER_EFFECTS_DWM_API_H_

#include <windows.h>

namespace effects {

// DWMWINDOWATTRIBUTE values, declared here rather than taken from the SDK
// header so the build does not depend on which Windows SDK is installed. The
// numbers are stable ABI; see docs/FEASIBILITY.md §3 for the build each one
// needs.
enum DwmWindowAttribute : DWORD {
  // Suppresses DWM's own open, minimise and restore transitions for one
  // window. Set-only: DwmGetWindowAttribute cannot read it back, so the only
  // proof it took is that the window stops playing the system animation.
  kDwmwaTransitionsForceDisabled = 3,
  kDwmwaExtendedFrameBounds = 9,
  kDwmwaCloaked = 14,
  kDwmwaUseImmersiveDarkMode = 20,       // Windows 11 22000
  kDwmwaWindowCornerPreference = 33,     // Windows 11 22000
  kDwmwaBorderColor = 34,                // Windows 11 22000
  kDwmwaCaptionColor = 35,               // Windows 11 22000
  kDwmwaTextColor = 36,                  // Windows 11 22000
  kDwmwaVisibleFrameBorderThickness = 37,
  kDwmwaSystemBackdropType = 38,         // Windows 11 22621
  // Undocumented, and the only Mica switch on builds 22000-22620.
  kDwmwaMicaEffect = 1029,
};

// The complete set of radii DWM offers. There is no attribute for an exact
// pixel radius — see docs/FEASIBILITY.md §2.1.
enum DwmCornerPreference : int {
  kCornerDefault = 0,
  kCornerDoNotRound = 1,
  kCornerRound = 2,       // approximately 8 px
  kCornerRoundSmall = 3,  // approximately 4 px
};

enum DwmSystemBackdrop : int {
  kBackdropAuto = 0,
  kBackdropNone = 1,
  kBackdropMainWindow = 2,       // Mica
  kBackdropTransientWindow = 3,  // Acrylic
  kBackdropTabbedWindow = 4,     // Mica Alt
};

// Suppresses the border entirely.
constexpr COLORREF kDwmColorNone = 0xFFFFFFFE;
// Restores the system's own behaviour.
constexpr COLORREF kDwmColorDefault = 0xFFFFFFFF;

// Accent states for the undocumented user32 composition attribute.
enum AccentState : int {
  kAccentDisabled = 0,
  kAccentEnableGradient = 1,
  kAccentEnableTransparentGradient = 2,
  kAccentEnableBlurBehind = 3,
  kAccentEnableAcrylicBlurBehind = 4,
  kAccentEnableHostBackdrop = 5,
};

struct AccentPolicy {
  int state;
  int flags;
  // ABGR, not RGB.
  unsigned int gradient_color;
  int animation_id;
};

// Layout must match the undocumented WINDOWCOMPOSITIONATTRIBDATA exactly:
// { DWORD, PVOID, SIZE_T }.
struct WindowCompositionAttributeData {
  DWORD attribute;
  void* data;
  SIZE_T size;
};

constexpr DWORD kWcaAccentPolicy = 19;

// Every entry point is resolved at startup with GetProcAddress rather than
// statically linked, so a build that is missing one degrades to "unsupported"
// instead of failing to launch.
class DwmApi {
 public:
  static const DwmApi& Instance();

  bool has_window_attributes() const {
    return set_window_attribute_ != nullptr && get_window_attribute_ != nullptr;
  }

  bool has_composition_attribute() const {
    return set_composition_attribute_ != nullptr;
  }

  // Returns E_NOTIMPL rather than crashing when the export is missing.
  HRESULT SetAttribute(HWND window, DWORD attribute, const void* value,
                       DWORD size) const;
  HRESULT GetAttribute(HWND window, DWORD attribute, void* value,
                       DWORD size) const;

  bool SetCompositionAttribute(HWND window,
                               WindowCompositionAttributeData* data) const;
  bool GetCompositionAttribute(HWND window,
                               WindowCompositionAttributeData* data) const;

  // The system accent colour as a COLORREF, for DWMWA_BORDER_COLOR. This is
  // the same colour Windows itself uses on focused window borders, which is
  // what keeps the active-window hint reading as native rather than added.
  COLORREF AccentBorderColor() const;

  // True when DWM reports the window cloaked — a suspended UWP app, or a
  // window living on another virtual desktop.
  bool IsCloaked(HWND window) const;

  // The window's frame without the invisible resize border, which is what
  // every size decision should be based on. Falls back to GetWindowRect.
  RECT FrameBounds(HWND window) const;

 private:
  DwmApi();
  DwmApi(const DwmApi&) = delete;
  DwmApi& operator=(const DwmApi&) = delete;

  typedef HRESULT(WINAPI* SetWindowAttributeFn)(HWND, DWORD, LPCVOID, DWORD);
  typedef HRESULT(WINAPI* GetWindowAttributeFn)(HWND, DWORD, PVOID, DWORD);
  typedef BOOL(WINAPI* SetWindowCompositionAttributeFn)(
      HWND, WindowCompositionAttributeData*);
  typedef BOOL(WINAPI* GetWindowCompositionAttributeFn)(
      HWND, WindowCompositionAttributeData*);
  typedef HRESULT(WINAPI* GetColorizationColorFn)(DWORD*, BOOL*);

  HMODULE dwm_module_ = nullptr;
  SetWindowAttributeFn set_window_attribute_ = nullptr;
  GetWindowAttributeFn get_window_attribute_ = nullptr;
  SetWindowCompositionAttributeFn set_composition_attribute_ = nullptr;
  GetWindowCompositionAttributeFn get_composition_attribute_ = nullptr;
  GetColorizationColorFn get_colorization_color_ = nullptr;
};

}  // namespace effects

#endif  // RUNNER_EFFECTS_DWM_API_H_
