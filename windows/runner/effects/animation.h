#ifndef RUNNER_EFFECTS_ANIMATION_H_
#define RUNNER_EFFECTS_ANIMATION_H_

#include <windows.h>

#include <functional>
#include <memory>
#include <vector>

#include "config.h"
#include "managed_window.h"
#include "proxy_window.h"

namespace effects {

// Why the animation moves a copy of the window rather than the window.
//
// The first version of this moved the real window: SetWindowPos every frame
// from 94% up to 100%. It worked, and it looked wrong. Resizing a window is a
// request to the application to lay its content out again, so every frame the
// target was reflowing text, repositioning controls and repainting — at the
// exact moment it was already busy starting up. That reads as lag, because it
// is lag: the animation was smooth and the content inside it was not.
//
// A DWM thumbnail is the window's own composited pixels, scaled by the
// compositor. Nothing is asked of the application, nothing reflows, and the
// smoothness stops depending on how busy the target is. The real window waits
// underneath, invisible, at its final size, still taking input.
//
// The cost is one hidden window and one thumbnail per animating window, both
// released the moment it finishes.

enum class AnimationTrigger {
  // A window that has just appeared, including one coming back from the
  // taskbar — from here the two are the same thing.
  kEntrance,

  // A window on its way to the taskbar: the same copy, the opposite direction,
  // and it outlives the window it is about.
  kExit,
};

// The resolved shape of one animation, in the units the animator works in.
struct AnimationSpec {
  // Cubic bezier control points, CSS order. Authored in Dart (MotionStyle) and
  // mirrored here; the settings preview and the real window have to move
  // identically or the preview is a lie.
  double x1 = 0.22;
  double y1 = 1.0;
  double x2 = 0.36;
  double y2 = 1.0;

  int duration_ms = 560;
  // Percent of final size the window starts at. 100 disables the zoom.
  int scale_percent = 94;
  // Pixels below the final position the window starts at.
  int lift = 12;
};

// Resolves the style and the user's numbers into one spec.
AnimationSpec SpecFor(const Config& config);

// Milliseconds from the performance counter, as a double.
//
// GetTickCount64 cannot be used for any of this: it advances in steps of about
// 15.6 ms, so a "has 16 ms passed?" test is only ever true every other step and
// the animation runs at half the frame rate it asks for. That is measurable —
// it was the difference between 31 ms frames and 16 ms ones.
double NowMs();

// Evaluates a cubic bezier easing curve at |t| in 0..1. Exposed for the same
// reason the control points are: so there is one definition of the motion.
double EaseAt(const AnimationSpec& spec, double t);

// Runs window animations, entrance and exit.
//
// Lives on the engine thread. Every method must be called from there: the
// frame clock is a thread timer on that thread's queue, the proxy windows are
// owned by it, and none of the state is synchronised.
class WindowAnimator {
 public:
  // Called once a window has finished animating or been cancelled. The engine
  // uses it to apply the effects that were held back — corners and the shadow
  // both need a settled geometry. Never called for an exit: that window was
  // released before its animation started.
  typedef std::function<void(HWND)> FinishedCallback;

  WindowAnimator() = default;
  ~WindowAnimator();

  WindowAnimator(const WindowAnimator&) = delete;
  WindowAnimator& operator=(const WindowAnimator&) = delete;

  void SetFinishedCallback(FinishedCallback callback);

  // Starts an entrance. The real window is hidden immediately and the copy
  // takes over on the next frame — one frame later, because a window that has
  // only just appeared has not painted yet, and a copy of an unpainted window
  // is an empty rectangle.
  //
  // |final_alpha| is the alpha the window must end on, which is whatever the
  // effect plan is about to ask for. Ending anywhere else makes the handoff
  // from the copy back to the window visible.
  bool BeginEntrance(ManagedWindow& window, const AnimationSpec& spec,
                     BYTE final_alpha);

  // Starts an exit. |frame| is where the window was: by the time Windows tells
  // anyone that a window is minimising, it is already iconic and reports a
  // rect off the side of the desktop.
  bool BeginExit(HWND window, const RECT& frame, const AnimationSpec& spec);

  bool IsAnimating(HWND window) const;
  bool active() const { return !running_.empty(); }

  // Advances every running animation by one frame, from the engine's WM_TIMER.
  void Tick();

  // Ends one animation now, putting the window back to full opacity. Safe for
  // a window that is not animating.
  //
  // |notify| is false when the caller is about to restore or release the
  // window anyway: applying the held-back effects one line before handing the
  // window back would be work done only to undo it.
  void Cancel(HWND window, bool notify);
  void CancelAll(bool notify);

 private:
  struct Running {
    HWND window = nullptr;
    // Windows recycles handles, so every frame checks that this one still
    // belongs to the process the animation started on.
    DWORD process_id = 0;
    AnimationTrigger trigger = AnimationTrigger::kEntrance;

    // Entrance: where the window is, re-read every frame so an application
    // that moves itself mid-animation is followed rather than fought.
    // Exit: where the window was.
    RECT target = {};
    // Exit only: the small rect over the taskbar that the copy shrinks toward.
    RECT destination = {};

    AnimationSpec spec;
    double start_ms = 0.0;
    bool started = false;
    BYTE final_alpha = 255;
    // True once we added WS_EX_LAYERED ourselves, which is what lets the real
    // window be held invisible while the copy plays.
    bool added_layered = false;

    std::unique_ptr<ProxyWindow> proxy;
  };

  void Frame(Running& item, double progress);
  // Hands the window back: final alpha, copy destroyed. Every path out of an
  // animation goes through here, which is what makes "a window is never left
  // invisible" true rather than hoped for.
  void Settle(Running& item);
  void Notify(HWND window);

  std::vector<Running> running_;
  FinishedCallback finished_;
};

}  // namespace effects

#endif  // RUNNER_EFFECTS_ANIMATION_H_
