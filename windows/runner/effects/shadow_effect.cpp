#include "shadow_effect.h"

#include <math.h>

#include "log.h"

namespace effects {
namespace {

const wchar_t kShadowClassName[] = L"WindowEffectsShadow";

// Peak opacity of a full-strength shadow. Black at 100% would be a hole in the
// desktop, not a shadow.
const double kPeakOpacity = 0.55;

// Slack around the blur so the gradient has room to reach zero.
const int kExtraPad = 4;

// Refuse to allocate a bitmap larger than this. A window big enough to need
// one has no visible shadow anyway, and the allocation is the single largest
// the engine ever makes.
const int kMaxShadowPixels = 12 * 1024 * 1024;

// The user's 0-100 strength as the constant alpha the blend multiplies by.
BYTE StrengthAlpha(int strength) {
  int value = strength;
  if (value < 0) {
    value = 0;
  }
  if (value > 100) {
    value = 100;
  }
  return static_cast<BYTE>(255 * value / 100);
}

int Clamp(int value, int low, int high) {
  if (value < low) {
    return low;
  }
  if (value > high) {
    return high;
  }
  return value;
}

// Horizontal span covered by a rounded rectangle of |w| x |h| on row |y|.
void RoundedSpan(int w, int h, int radius, int y, int* x0, int* x1) {
  int inset = 0;
  if (radius > 0) {
    double dy = -1.0;
    if (y < radius) {
      dy = static_cast<double>(radius) - 0.5 - static_cast<double>(y);
    } else if (y >= h - radius) {
      dy = static_cast<double>(y) - static_cast<double>(h - radius) + 0.5;
    }
    if (dy >= 0.0) {
      const double r = static_cast<double>(radius);
      const double inner = r * r - dy * dy;
      const double dx = inner > 0.0 ? sqrt(inner) : 0.0;
      inset = radius - static_cast<int>(dx + 0.5);
      inset = Clamp(inset, 0, w / 2);
    }
  }
  *x0 = inset;
  *x1 = w - inset;
}

// Fills a rounded rectangle into the coverage mask.
void FillRounded(std::vector<unsigned char>* mask, int stride, int height,
                 int x, int y, int w, int h, int radius,
                 unsigned char value) {
  for (int row = 0; row < h; ++row) {
    const int target_y = y + row;
    if (target_y < 0 || target_y >= height) {
      continue;
    }
    int x0 = 0;
    int x1 = 0;
    RoundedSpan(w, h, radius, row, &x0, &x1);

    const int from = Clamp(x + x0, 0, stride);
    const int to = Clamp(x + x1, 0, stride);
    unsigned char* line = mask->data() + static_cast<size_t>(target_y) * stride;
    for (int column = from; column < to; ++column) {
      line[column] = value;
    }
  }
}

// One pass of a box blur. Three passes approximate a gaussian closely enough
// that no one can tell, at a fraction of the cost.
void BoxBlurHorizontal(const std::vector<unsigned char>& source,
                       std::vector<unsigned char>* target, int width,
                       int height, int radius) {
  if (radius <= 0) {
    *target = source;
    return;
  }
  const int window = radius * 2 + 1;

  for (int y = 0; y < height; ++y) {
    const unsigned char* in = source.data() + static_cast<size_t>(y) * width;
    unsigned char* out = target->data() + static_cast<size_t>(y) * width;

    int sum = 0;
    for (int x = -radius; x <= radius; ++x) {
      sum += in[Clamp(x, 0, width - 1)];
    }
    for (int x = 0; x < width; ++x) {
      out[x] = static_cast<unsigned char>(sum / window);
      sum -= in[Clamp(x - radius, 0, width - 1)];
      sum += in[Clamp(x + radius + 1, 0, width - 1)];
    }
  }
}

void BoxBlurVertical(const std::vector<unsigned char>& source,
                     std::vector<unsigned char>* target, int width, int height,
                     int radius) {
  if (radius <= 0) {
    *target = source;
    return;
  }
  const int window = radius * 2 + 1;

  for (int x = 0; x < width; ++x) {
    int sum = 0;
    for (int y = -radius; y <= radius; ++y) {
      sum += source[static_cast<size_t>(Clamp(y, 0, height - 1)) * width + x];
    }
    for (int y = 0; y < height; ++y) {
      (*target)[static_cast<size_t>(y) * width + x] =
          static_cast<unsigned char>(sum / window);
      sum -= source[static_cast<size_t>(Clamp(y - radius, 0, height - 1)) *
                        width +
                    x];
      sum += source[static_cast<size_t>(Clamp(y + radius + 1, 0, height - 1)) *
                        width +
                    x];
    }
  }
}

}  // namespace

void BuildShadowAlpha(int width, int height, int window_width,
                      int window_height, int pad,
                      const ShadowAppearance& look,
                      std::vector<unsigned char>* alpha) {
  const size_t count = static_cast<size_t>(width) * static_cast<size_t>(height);
  alpha->assign(count, 0);

  // The caster: the window silhouette, pushed down by the offset.
  FillRounded(alpha, width, height, pad, pad + look.offset, window_width,
              window_height, look.radius, 255);

  // Three box passes summing to roughly the requested blur.
  const int pass_radius = Clamp(look.blur / 3, 0, 64);
  if (pass_radius > 0) {
    std::vector<unsigned char> scratch(count, 0);
    for (int pass = 0; pass < 3; ++pass) {
      BoxBlurHorizontal(*alpha, &scratch, width, height, pass_radius);
      BoxBlurVertical(scratch, alpha, width, height, pass_radius);
    }
  }

  // Peak opacity only. The user's strength arrives later, as the blend's
  // constant alpha, so that changing it never costs a repaint.
  for (size_t index = 0; index < count; ++index) {
    (*alpha)[index] = static_cast<unsigned char>(
        static_cast<double>((*alpha)[index]) * kPeakOpacity);
  }

  // Knock the window's own silhouette back out, one pixel undersized so the
  // window's edge covers the seam rather than leaving a bright hairline. Without
  // this the shadow would show *through* a transparent window and darken it.
  if (window_width > 2 && window_height > 2) {
    FillRounded(alpha, width, height, pad + 1, pad + 1, window_width - 2,
                window_height - 2, look.radius > 0 ? look.radius - 1 : 0, 0);
  }
}

// ----------------------------------------------------------------- ShadowWindow

ShadowWindow::ShadowWindow() = default;

ShadowWindow::~ShadowWindow() {
  if (window_ != nullptr) {
    ::DestroyWindow(window_);
    window_ = nullptr;
  }
  if (bitmap_ != nullptr) {
    ::DeleteObject(bitmap_);
    bitmap_ = nullptr;
    pixels_ = nullptr;
  }
}

bool ShadowWindow::Create() {
  if (window_ != nullptr) {
    return true;
  }

  HINSTANCE instance = ::GetModuleHandleW(nullptr);

  static ATOM class_atom = 0;
  if (class_atom == 0) {
    WNDCLASSEXW window_class = {};
    window_class.cbSize = static_cast<UINT>(sizeof(window_class));
    window_class.lpfnWndProc = &::DefWindowProcW;
    window_class.hInstance = instance;
    window_class.lpszClassName = kShadowClassName;
    class_atom = ::RegisterClassExW(&window_class);
    if (class_atom == 0) {
      return false;
    }
  }

  // TRANSPARENT keeps it out of hit-testing, NOACTIVATE keeps it from stealing
  // focus, TOOLWINDOW keeps it out of the taskbar and Alt+Tab.
  window_ = ::CreateWindowExW(
      WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
      kShadowClassName, L"", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, instance,
      nullptr);
  if (window_ == nullptr) {
    return false;
  }

  // Windows 11 rounds popup windows by default, which would clip the corners
  // off our own shadow bitmap.
  int preference = kCornerDoNotRound;
  DwmApi::Instance().SetAttribute(window_, kDwmwaWindowCornerPreference,
                                  &preference,
                                  static_cast<DWORD>(sizeof(preference)));
  return true;
}

bool ShadowWindow::Repaint(const RECT& bounds, const ShadowAppearance& look) {
  const int width = static_cast<int>(bounds.right - bounds.left);
  const int height = static_cast<int>(bounds.bottom - bounds.top);
  if (width <= 0 || height <= 0) {
    return false;
  }
  if (static_cast<long long>(width) * static_cast<long long>(height) >
      static_cast<long long>(kMaxShadowPixels)) {
    return false;
  }

  const int pad = look.blur + kExtraPad;
  const int window_width = width - pad * 2;
  const int window_height = height - pad * 2 - look.offset;
  if (window_width <= 0 || window_height <= 0) {
    return false;
  }

  if (bitmap_ != nullptr) {
    ::DeleteObject(bitmap_);
    bitmap_ = nullptr;
    pixels_ = nullptr;
  }

  BITMAPINFO info = {};
  info.bmiHeader.biSize = static_cast<DWORD>(sizeof(BITMAPINFOHEADER));
  info.bmiHeader.biWidth = width;
  info.bmiHeader.biHeight = -height;  // top-down
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;

  bitmap_ = ::CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &pixels_,
                               nullptr, 0);
  if (bitmap_ == nullptr || pixels_ == nullptr) {
    bitmap_ = nullptr;
    pixels_ = nullptr;
    return false;
  }

  std::vector<unsigned char> alpha;
  BuildShadowAlpha(width, height, window_width, window_height, pad, look,
                   &alpha);

  // Premultiplied BGRA. The shadow is pure black, so every colour channel is
  // zero once multiplied by alpha and only the alpha byte carries the shape.
  unsigned char* out = static_cast<unsigned char*>(pixels_);
  const size_t count = static_cast<size_t>(width) * static_cast<size_t>(height);
  for (size_t index = 0; index < count; ++index) {
    const size_t base = index * 4;
    out[base + 0] = 0;
    out[base + 1] = 0;
    out[base + 2] = 0;
    out[base + 3] = alpha[index];
  }

  HDC screen = ::GetDC(nullptr);
  if (screen == nullptr) {
    return false;
  }
  HDC memory = ::CreateCompatibleDC(screen);
  if (memory == nullptr) {
    ::ReleaseDC(nullptr, screen);
    return false;
  }

  HGDIOBJ previous = ::SelectObject(memory, bitmap_);

  POINT source = {0, 0};
  POINT position = {bounds.left, bounds.top};
  SIZE size = {width, height};
  BLENDFUNCTION blend = {};
  blend.BlendOp = static_cast<BYTE>(AC_SRC_OVER);
  blend.BlendFlags = 0;
  blend.SourceConstantAlpha = StrengthAlpha(look.strength);
  blend.AlphaFormat = static_cast<BYTE>(AC_SRC_ALPHA);

  const BOOL updated =
      ::UpdateLayeredWindow(window_, screen, &position, &size, memory, &source,
                            0, &blend, ULW_ALPHA);

  ::SelectObject(memory, previous);
  ::DeleteDC(memory);
  ::ReleaseDC(nullptr, screen);

  if (updated == FALSE) {
    return false;
  }

  painted_size_.cx = width;
  painted_size_.cy = height;
  painted_look_ = look;
  return true;
}

bool ShadowWindow::Reblend(int strength) {
  if (window_ == nullptr || bitmap_ == nullptr) {
    return false;
  }
  BLENDFUNCTION blend = {};
  blend.BlendOp = static_cast<BYTE>(AC_SRC_OVER);
  blend.BlendFlags = 0;
  blend.SourceConstantAlpha = StrengthAlpha(strength);
  blend.AlphaFormat = static_cast<BYTE>(AC_SRC_ALPHA);

  // A null source device context means "keep the pixels you already have", so
  // this changes the blend without touching the bitmap or uploading it again.
  const BOOL updated = ::UpdateLayeredWindow(window_, nullptr, nullptr, nullptr,
                                             nullptr, nullptr, 0, &blend,
                                             ULW_ALPHA);
  if (updated == FALSE) {
    return false;
  }
  painted_look_.strength = strength;
  return true;
}

void ShadowWindow::Follow(HWND target, const RECT& frame,
                          const ShadowAppearance& look) {
  if (!Create()) {
    return;
  }

  const int pad = look.blur + kExtraPad;
  RECT bounds = {};
  bounds.left = frame.left - pad;
  bounds.top = frame.top - pad;
  bounds.right = frame.right + pad;
  bounds.bottom = frame.bottom + pad + look.offset;

  const int width = static_cast<int>(bounds.right - bounds.left);
  const int height = static_cast<int>(bounds.bottom - bounds.top);
  if (width <= 0 || height <= 0) {
    Hide();
    return;
  }

  const bool same_shape = bitmap_ != nullptr && painted_size_.cx == width &&
                          painted_size_.cy == height &&
                          painted_look_.SameShape(look);

  if (!same_shape) {
    if (!Repaint(bounds, look)) {
      Hide();
      return;
    }
  } else if (painted_look_.strength != look.strength) {
    // Focus changed, nothing else. One blend, no blur, no upload.
    Reblend(look.strength);
    ::SetWindowPos(window_, nullptr, bounds.left, bounds.top, width, height,
                   SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOREDRAW);
  } else {
    // Same pixels, new place: UpdateLayeredWindow's content survives a move, so
    // dragging a window costs one SetWindowPos per event and nothing else.
    ::SetWindowPos(window_, nullptr, bounds.left, bounds.top, width, height,
                   SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOREDRAW);
  }

  // Sit directly below the target, every time — the target's Z-order changes
  // whenever it is clicked, and the shadow has to follow it under there.
  ::SetWindowPos(window_, target, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
  visible_ = true;
}

void ShadowWindow::Reposition(HWND target, const RECT& frame) {
  if (window_ == nullptr || bitmap_ == nullptr) {
    return;
  }
  Follow(target, frame, painted_look_);
}

void ShadowWindow::Hide() {
  if (window_ != nullptr && visible_) {
    ::ShowWindow(window_, SW_HIDE);
  }
  visible_ = false;
}

// ---------------------------------------------------------------- ShadowManager

ShadowManager::~ShadowManager() {
  RemoveAll();
}

void ShadowManager::Update(const ManagedWindow& window,
                           const EffectPlan& plan) {
  HWND target = window.hwnd();

  if (!plan.shadow) {
    Remove(target);
    return;
  }

  // A window being dragged outruns anything we can reposition from another
  // process, and a shadow trailing half a second behind is worse than no
  // shadow. It comes back on MOVESIZEEND.
  if (window.moving()) {
    Hide(target);
    return;
  }

  std::unordered_map<HWND, std::unique_ptr<ShadowWindow>>::iterator found =
      shadows_.find(target);
  if (found == shadows_.end()) {
    std::unique_ptr<ShadowWindow> shadow(new ShadowWindow());
    if (!shadow->Create()) {
      LogWarn("could not create a shadow window");
      return;
    }
    found = shadows_.emplace(target, std::move(shadow)).first;
  }

  ShadowAppearance look;
  look.strength = plan.shadow_strength;
  look.blur = plan.shadow_blur;
  look.offset = plan.shadow_offset;
  look.radius = plan.corner_radius;

  found->second->Follow(target, window.info().frame, look);
}

void ShadowManager::Reposition(const ManagedWindow& window) {
  std::unordered_map<HWND, std::unique_ptr<ShadowWindow>>::iterator found =
      shadows_.find(window.hwnd());
  if (found == shadows_.end()) {
    return;
  }
  if (window.moving()) {
    found->second->Hide();
    return;
  }
  found->second->Reposition(window.hwnd(), window.info().frame);
}

void ShadowManager::Hide(HWND target) {
  std::unordered_map<HWND, std::unique_ptr<ShadowWindow>>::iterator found =
      shadows_.find(target);
  if (found != shadows_.end()) {
    found->second->Hide();
  }
}

void ShadowManager::Remove(HWND target) {
  std::unordered_map<HWND, std::unique_ptr<ShadowWindow>>::iterator found =
      shadows_.find(target);
  if (found == shadows_.end()) {
    return;
  }
  shadows_.erase(found);
}

void ShadowManager::RemoveAll() {
  shadows_.clear();
}

}  // namespace effects
