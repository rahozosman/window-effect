#ifndef RUNNER_EFFECTS_MANAGED_WINDOW_H_
#define RUNNER_EFFECTS_MANAGED_WINDOW_H_

#include <windows.h>

#include "dwm_api.h"
#include "window_classifier.h"

namespace effects {

// A window the engine has taken responsibility for, and the record of what it
// looked like before we touched it.
//
// This is what makes "never leave windows permanently corrupted" structural
// rather than aspirational: no effect may change an attribute without first
// calling the matching Capture, and every Restore replays the captured value
// rather than guessing a default. Capture is idempotent, so an effect that
// reapplies never overwrites the true original with our own value.
//
// Each attribute also has its own Restore, because a setting can be switched
// off while the window stays managed — turning corners off must put the corner
// preference back without disturbing the backdrop.
class ManagedWindow {
 public:
  explicit ManagedWindow(const WindowInfo& info);
  ~ManagedWindow();

  ManagedWindow(const ManagedWindow&) = delete;
  ManagedWindow& operator=(const ManagedWindow&) = delete;

  HWND hwnd() const { return info_.hwnd; }
  const WindowInfo& info() const { return info_; }

  // Refreshes the volatile parts of the record after a move, resize or state
  // change. Never touches the captured originals.
  void RefreshInfo(const WindowInfo& info);
  void SetFrame(const RECT& frame);

  // Where the window was the last time it was somewhere real.
  //
  // A minimised window reports a rect at -32000,-32000, and it reports it
  // before anything outside the process is told it is minimising — so by the
  // time the minimise animation is asked where to start, info().frame is
  // already that. This is the answer to that question.
  const RECT& last_visible_frame() const { return last_visible_frame_; }

  bool active() const { return active_; }
  void set_active(bool active) { active_ = active; }

  bool moving() const { return moving_; }
  void set_moving(bool moving) { moving_ = moving; }

  // ------------------------------------------------------- capture / restore

  void CaptureExStyle();
  void RestoreExStyle();
  bool ex_style_modified() const { return has_ex_style_; }

  void CaptureRegion();
  void RestoreRegion();
  bool region_modified() const { return has_region_; }

  void CaptureCornerPreference();
  void RestoreCornerPreference();
  bool corner_preference_modified() const { return has_corner_preference_; }

  void CaptureBorderColor();
  void RestoreBorderColor();
  bool border_color_modified() const { return has_border_color_; }

  void CaptureBackdropType();
  void RestoreBackdropType();
  bool backdrop_type_modified() const { return has_backdrop_type_; }

  void CaptureAccentPolicy();
  void RestoreAccentPolicy();
  bool accent_policy_modified() const { return has_accent_policy_; }

  // Stops DWM playing its own open, minimise and restore transitions for this
  // window, so that ours are the only ones. There is no Capture to match:
  // DWMWA_TRANSITIONS_FORCEDISABLED is set-only, so the restore writes the
  // system default rather than a remembered value — which is the same thing
  // for every window that has never had it set, and every window we touch has
  // never had it set.
  void DisableSystemTransitions();
  void RestoreSystemTransitions();
  bool system_transitions_modified() const { return has_transitions_; }

  // Puts back everything that was captured, in the reverse order it was
  // applied. Infallible by contract: it runs on shutdown paths where there is
  // nothing left to report a failure to, so errors are swallowed.
  void Restore();

  // A window that keeps refusing effects is quarantined rather than retried on
  // every event forever.
  void RecordFailure();
  void ResetFailures();
  bool quarantined() const;

 private:
  // True when the HWND is still alive and worth writing to.
  bool Alive() const;

  WindowInfo info_;

  RECT last_visible_frame_ = {};

  bool active_ = false;
  bool moving_ = false;
  int failures_ = 0;

  bool has_ex_style_ = false;
  LONG_PTR original_ex_style_ = 0;

  // SetWindowRgn takes ownership of the region it is given, so the original is
  // kept as a private copy and handed out only as further copies.
  bool has_region_ = false;
  bool had_region_ = false;
  HRGN original_region_ = nullptr;

  bool has_corner_preference_ = false;
  int original_corner_preference_ = 0;

  bool has_border_color_ = false;
  COLORREF original_border_color_ = 0;

  bool has_backdrop_type_ = false;
  int original_backdrop_type_ = 0;

  bool has_accent_policy_ = false;
  AccentPolicy original_accent_policy_ = {};

  bool has_transitions_ = false;
};

}  // namespace effects

#endif  // RUNNER_EFFECTS_MANAGED_WINDOW_H_
