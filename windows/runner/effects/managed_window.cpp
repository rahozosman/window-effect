#include "managed_window.h"

#include "log.h"

namespace effects {
namespace {

// Three consecutive failures against one window is enough to conclude the
// window does not want what we are doing.
const int kQuarantineThreshold = 3;

HRGN CopyRegion(HRGN source) {
  if (source == nullptr) {
    return nullptr;
  }
  HRGN copy = ::CreateRectRgn(0, 0, 0, 0);
  if (copy == nullptr) {
    return nullptr;
  }
  if (::CombineRgn(copy, source, nullptr, RGN_COPY) == ERROR) {
    ::DeleteObject(copy);
    return nullptr;
  }
  return copy;
}

}  // namespace

ManagedWindow::ManagedWindow(const WindowInfo& info)
    : info_(info), last_visible_frame_(info.frame) {}

ManagedWindow::~ManagedWindow() {
  // Last line of defence. Every ordinary path restores explicitly first, and
  // each Restore clears its captured flag as it goes, so this is a no-op in
  // the normal case and a rescue if the record is dropped some other way.
  Restore();

  if (original_region_ != nullptr) {
    ::DeleteObject(original_region_);
    original_region_ = nullptr;
  }
}

bool ManagedWindow::Alive() const {
  return info_.hwnd != nullptr && ::IsWindow(info_.hwnd) != FALSE;
}

namespace {

// A minimised window parks itself far off-screen. Anything out here is a state
// change rather than a position worth remembering.
bool LooksOnScreen(const RECT& frame) {
  return frame.left > -30000 && frame.top > -30000 &&
         frame.right > frame.left && frame.bottom > frame.top;
}

}  // namespace

void ManagedWindow::SetFrame(const RECT& frame) {
  info_.frame = frame;
  if (LooksOnScreen(frame)) {
    last_visible_frame_ = frame;
  }
}

void ManagedWindow::RefreshInfo(const WindowInfo& info) {
  SetFrame(info.frame);
  info_.frame_inset = info.frame_inset;
  info_.minimized = info.minimized;
  info_.maximized = info.maximized;
  info_.owned = info.owned;
  // already_layered is deliberately NOT refreshed. Once transparency adds
  // WS_EX_LAYERED itself, re-reading the bit would make the window look like
  // one the application had made layered, and the compatibility manager would
  // then refuse to touch it ever again.
  //
  // The executable, class name and process id cannot change for a live HWND.
}

// ------------------------------------------------------------ extended style

void ManagedWindow::CaptureExStyle() {
  if (has_ex_style_ || !Alive()) {
    return;
  }
  original_ex_style_ = ::GetWindowLongPtrW(info_.hwnd, GWL_EXSTYLE);
  has_ex_style_ = true;
}

void ManagedWindow::RestoreExStyle() {
  if (!has_ex_style_) {
    return;
  }
  has_ex_style_ = false;
  if (!Alive()) {
    return;
  }

  ::SetWindowLongPtrW(info_.hwnd, GWL_EXSTYLE, original_ex_style_);
  // The frame caches the extended style, so it has to be told to re-read it,
  // and the client area needs a repaint now that it is opaque again.
  ::SetWindowPos(info_.hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                     SWP_FRAMECHANGED);
  ::RedrawWindow(info_.hwnd, nullptr, nullptr,
                 RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
}

// --------------------------------------------------------------------- region

void ManagedWindow::CaptureRegion() {
  if (has_region_ || !Alive()) {
    return;
  }
  has_region_ = true;

  HRGN probe = ::CreateRectRgn(0, 0, 0, 0);
  if (probe == nullptr) {
    had_region_ = false;
    return;
  }

  if (::GetWindowRgn(info_.hwnd, probe) == ERROR) {
    // No region of its own, which is the usual case. Restore clears ours by
    // passing nullptr.
    ::DeleteObject(probe);
    had_region_ = false;
    return;
  }

  original_region_ = probe;
  had_region_ = true;
}

void ManagedWindow::RestoreRegion() {
  if (!has_region_) {
    return;
  }
  has_region_ = false;
  if (!Alive()) {
    return;
  }

  HRGN replacement =
      had_region_ ? CopyRegion(original_region_) : static_cast<HRGN>(nullptr);
  ::SetWindowRgn(info_.hwnd, replacement, TRUE);
}

// ----------------------------------------------------------- corner preference

void ManagedWindow::CaptureCornerPreference() {
  if (has_corner_preference_ || !Alive()) {
    return;
  }
  has_corner_preference_ = true;

  int preference = kCornerDefault;
  const HRESULT result = DwmApi::Instance().GetAttribute(
      info_.hwnd, kDwmwaWindowCornerPreference, &preference,
      static_cast<DWORD>(sizeof(preference)));
  original_corner_preference_ =
      SUCCEEDED(result) ? preference : static_cast<int>(kCornerDefault);
}

void ManagedWindow::RestoreCornerPreference() {
  if (!has_corner_preference_) {
    return;
  }
  has_corner_preference_ = false;
  if (!Alive()) {
    return;
  }
  DwmApi::Instance().SetAttribute(
      info_.hwnd, kDwmwaWindowCornerPreference, &original_corner_preference_,
      static_cast<DWORD>(sizeof(original_corner_preference_)));
}

// --------------------------------------------------------------- border colour

void ManagedWindow::CaptureBorderColor() {
  if (has_border_color_ || !Alive()) {
    return;
  }
  has_border_color_ = true;

  COLORREF color = kDwmColorDefault;
  const HRESULT result = DwmApi::Instance().GetAttribute(
      info_.hwnd, kDwmwaBorderColor, &color, static_cast<DWORD>(sizeof(color)));
  original_border_color_ = SUCCEEDED(result) ? color : kDwmColorDefault;
}

void ManagedWindow::RestoreBorderColor() {
  if (!has_border_color_) {
    return;
  }
  has_border_color_ = false;
  if (!Alive()) {
    return;
  }
  DwmApi::Instance().SetAttribute(
      info_.hwnd, kDwmwaBorderColor, &original_border_color_,
      static_cast<DWORD>(sizeof(original_border_color_)));
}

// ------------------------------------------------------------------- backdrop

void ManagedWindow::CaptureBackdropType() {
  if (has_backdrop_type_ || !Alive()) {
    return;
  }
  has_backdrop_type_ = true;

  int backdrop = kBackdropAuto;
  const HRESULT result = DwmApi::Instance().GetAttribute(
      info_.hwnd, kDwmwaSystemBackdropType, &backdrop,
      static_cast<DWORD>(sizeof(backdrop)));
  original_backdrop_type_ =
      SUCCEEDED(result) ? backdrop : static_cast<int>(kBackdropAuto);
}

void ManagedWindow::RestoreBackdropType() {
  if (!has_backdrop_type_) {
    return;
  }
  has_backdrop_type_ = false;
  if (!Alive()) {
    return;
  }
  DwmApi::Instance().SetAttribute(
      info_.hwnd, kDwmwaSystemBackdropType, &original_backdrop_type_,
      static_cast<DWORD>(sizeof(original_backdrop_type_)));
}

// -------------------------------------------------------------- accent policy

void ManagedWindow::CaptureAccentPolicy() {
  if (has_accent_policy_ || !Alive()) {
    return;
  }
  has_accent_policy_ = true;

  AccentPolicy policy = {};
  WindowCompositionAttributeData data = {};
  data.attribute = kWcaAccentPolicy;
  data.data = &policy;
  data.size = sizeof(policy);

  if (DwmApi::Instance().GetCompositionAttribute(info_.hwnd, &data)) {
    original_accent_policy_ = policy;
    return;
  }

  // The getter is as undocumented as the setter, and may not be exported at
  // all. Assuming "no accent" is the right guess: it is what an ordinary
  // window has, and restoring it is harmless if the window already had none.
  AccentPolicy disabled = {};
  disabled.state = kAccentDisabled;
  original_accent_policy_ = disabled;
}

void ManagedWindow::RestoreAccentPolicy() {
  if (!has_accent_policy_) {
    return;
  }
  has_accent_policy_ = false;
  if (!Alive()) {
    return;
  }

  AccentPolicy policy = original_accent_policy_;
  WindowCompositionAttributeData data = {};
  data.attribute = kWcaAccentPolicy;
  data.data = &policy;
  data.size = sizeof(policy);
  DwmApi::Instance().SetCompositionAttribute(info_.hwnd, &data);
}

// --------------------------------------------------------------- transitions

void ManagedWindow::DisableSystemTransitions() {
  if (has_transitions_ || !Alive()) {
    return;
  }
  has_transitions_ = true;

  BOOL disabled = TRUE;
  DwmApi::Instance().SetAttribute(info_.hwnd, kDwmwaTransitionsForceDisabled,
                                  &disabled,
                                  static_cast<DWORD>(sizeof(disabled)));
}

void ManagedWindow::RestoreSystemTransitions() {
  if (!has_transitions_) {
    return;
  }
  has_transitions_ = false;
  if (!Alive()) {
    return;
  }

  BOOL disabled = FALSE;
  DwmApi::Instance().SetAttribute(info_.hwnd, kDwmwaTransitionsForceDisabled,
                                  &disabled,
                                  static_cast<DWORD>(sizeof(disabled)));
}

// -------------------------------------------------------------------- restore

void ManagedWindow::Restore() {
  // Reverse of the order effects are applied: composition and DWM attributes
  // first, then the region, and the extended style last, because putting that
  // back forces the repaint that makes everything else visible again.
  RestoreSystemTransitions();
  RestoreAccentPolicy();
  RestoreBackdropType();
  RestoreBorderColor();
  RestoreCornerPreference();
  RestoreRegion();
  RestoreExStyle();
}

void ManagedWindow::RecordFailure() {
  ++failures_;
  if (failures_ == kQuarantineThreshold) {
    LogWarn("quarantined " + Narrow(info_.executable) +
            " after repeated failures");
  }
}

void ManagedWindow::ResetFailures() {
  failures_ = 0;
}

bool ManagedWindow::quarantined() const {
  return failures_ >= kQuarantineThreshold;
}

}  // namespace effects
