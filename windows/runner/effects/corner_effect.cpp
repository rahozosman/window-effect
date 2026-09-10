#include "corner_effect.h"

#include "log.h"

namespace effects {
namespace {

// Builds the region for precise corners.
//
// The naive approach — a single rounded rectangle over the visible frame —
// works visually but clips away the invisible resize border, shrinking the
// window's grab area from about eight pixels to almost nothing. So instead the
// region keeps the *whole* window rect and subtracts only the four corner
// wedges: the visible corners round, every edge keeps its full resize margin.
//
// Returns nullptr if the geometry is degenerate; the caller then leaves the
// window alone rather than applying a broken shape.
HRGN BuildCornerRegion(const RECT& outer, const RECT& inset, int radius) {
  const int width = static_cast<int>(outer.right - outer.left);
  const int height = static_cast<int>(outer.bottom - outer.top);
  if (width <= 0 || height <= 0) {
    return nullptr;
  }

  // Frame rectangle in window-relative coordinates.
  const int left = static_cast<int>(inset.left);
  const int top = static_cast<int>(inset.top);
  const int right = width - static_cast<int>(inset.right);
  const int bottom = height - static_cast<int>(inset.bottom);
  if (right - left <= radius * 2 || bottom - top <= radius * 2) {
    return nullptr;
  }

  HRGN full = ::CreateRectRgn(0, 0, width, height);
  if (full == nullptr) {
    return nullptr;
  }
  if (radius <= 0) {
    return full;
  }

  // CreateRoundRectRgn is exclusive on the right and bottom edges.
  HRGN rounded =
      ::CreateRoundRectRgn(left, top, right + 1, bottom + 1, radius * 2,
                           radius * 2);
  HRGN square = ::CreateRectRgn(left, top, right, bottom);
  HRGN wedges = ::CreateRectRgn(0, 0, 0, 0);

  if (rounded == nullptr || square == nullptr || wedges == nullptr) {
    if (rounded != nullptr) {
      ::DeleteObject(rounded);
    }
    if (square != nullptr) {
      ::DeleteObject(square);
    }
    if (wedges != nullptr) {
      ::DeleteObject(wedges);
    }
    ::DeleteObject(full);
    return nullptr;
  }

  // wedges = the corner slivers the rounding removes.
  ::CombineRgn(wedges, square, rounded, RGN_DIFF);
  ::CombineRgn(full, full, wedges, RGN_DIFF);

  ::DeleteObject(rounded);
  ::DeleteObject(square);
  ::DeleteObject(wedges);
  return full;
}

}  // namespace

bool CornerEffect::Apply(ManagedWindow& window, const EffectPlan& plan) {
  if (!plan.corners) {
    // Switched off, or unsupported on this build: hand back whatever we found.
    window.RestoreCornerPreference();
    window.RestoreRegion();
    return true;
  }

  if (plan.precise) {
    // The two mechanisms must never be layered. Leaving a DWM preference set
    // under a region would round the corners twice at different radii.
    window.RestoreCornerPreference();
    return ApplyPrecise(window, plan);
  }

  window.RestoreRegion();
  return ApplySystem(window, plan);
}

bool CornerEffect::ApplySystem(ManagedWindow& window, const EffectPlan& plan) {
  window.CaptureCornerPreference();

  int preference = plan.dwm_corner;
  const HRESULT result = DwmApi::Instance().SetAttribute(
      window.hwnd(), kDwmwaWindowCornerPreference, &preference,
      static_cast<DWORD>(sizeof(preference)));

  if (FAILED(result)) {
    LogWarn("corner preference refused by " + Narrow(window.info().executable));
    return false;
  }
  return true;
}

bool CornerEffect::ApplyPrecise(ManagedWindow& window,
                                const EffectPlan& plan) {
  RECT outer = {};
  if (!::GetWindowRect(window.hwnd(), &outer)) {
    return false;
  }

  window.CaptureRegion();

  HRGN region =
      BuildCornerRegion(outer, window.info().frame_inset, plan.corner_radius);
  if (region == nullptr) {
    // Too small to round without eating the window. Leave it square.
    window.RestoreRegion();
    return true;
  }

  // SetWindowRgn takes ownership of the region whether it succeeds or fails.
  if (::SetWindowRgn(window.hwnd(), region, TRUE) == 0) {
    LogWarn("window region refused by " + Narrow(window.info().executable));
    return false;
  }
  return true;
}

}  // namespace effects
