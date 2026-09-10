#ifndef RUNNER_EFFECTS_PROXY_WINDOW_H_
#define RUNNER_EFFECTS_PROXY_WINDOW_H_

#include <windows.h>

namespace effects {

// A stand-in for another application's window, drawn by DWM.
//
// This is what makes the animation smooth. Moving and resizing the real window
// every frame asks the application to lay its content out again every frame —
// which is what "the window looks laggy while it opens" actually is. A DWM
// thumbnail is the window's own composited pixels, scaled by the compositor on
// the GPU, so the application is not involved at all and nothing reflows.
//
// The overlay is click-through and never activates, so the real window
// underneath keeps taking input for the whole animation.
class ProxyWindow {
 public:
  ProxyWindow() = default;
  ~ProxyWindow();

  ProxyWindow(const ProxyWindow&) = delete;
  ProxyWindow& operator=(const ProxyWindow&) = delete;

  // Puts a copy of |source| on screen at |frame|. Returns false when DWM has
  // nothing to draw, in which case nothing is shown at all — an empty
  // rectangle where a window should be is worse than no animation.
  bool Create(HWND source, const RECT& frame);

  // Moves the copy and sets its opacity, 0-255. Cheap enough to call every
  // frame: one SetWindowPos on a window we own, and one DWM property update.
  void Update(const RECT& frame, BYTE opacity);

  void Destroy();

  bool valid() const { return overlay_ != nullptr; }

 private:
  static bool EnsureClass();

  HWND overlay_ = nullptr;
  // HTHUMBNAIL, kept as a HANDLE so this header does not drag dwmapi.h in.
  HANDLE thumbnail_ = nullptr;
};

}  // namespace effects

#endif  // RUNNER_EFFECTS_PROXY_WINDOW_H_
