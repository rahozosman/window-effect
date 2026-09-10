#include "animation.h"

#include <math.h>
#include <stdlib.h>

#include "dwm_api.h"
#include "log.h"

namespace effects {
namespace {

// Bounds the engine enforces regardless of what arrives from the settings.
const int kMinDurationMs = 120;
const int kMaxDurationMs = 1500;

// A window bigger than this in either direction is not something to be
// compositing a scaled copy of every frame.
const int kMaxAnimatedEdge = 8000;

// The opacity ramp finishes ahead of the movement. A window still visibly
// fading after it has stopped moving looks slow; one that is solid just before
// it settles looks deliberate.
const double kAlphaLead = 1.25;

// How small the copy gets on its way to the taskbar. Not zero: a thumbnail
// scaled to nothing spends its last frames as a shimmering line.
const double kExitEndScale = 0.16;

}  // namespace

double NowMs() {
  static LARGE_INTEGER frequency = {};
  if (frequency.QuadPart == 0) {
    ::QueryPerformanceFrequency(&frequency);
    if (frequency.QuadPart == 0) {
      frequency.QuadPart = 1;
    }
  }
  LARGE_INTEGER counter = {};
  ::QueryPerformanceCounter(&counter);
  return 1000.0 * static_cast<double>(counter.QuadPart) /
         static_cast<double>(frequency.QuadPart);
}

namespace {

double Clamp01(double value) {
  if (value < 0.0) {
    return 0.0;
  }
  return value > 1.0 ? 1.0 : value;
}

int RoundToInt(double value) {
  return static_cast<int>(value < 0.0 ? value - 0.5 : value + 0.5);
}

double BezierComponent(double a, double b, double t) {
  // The standard cubic with p0 = 0 and p3 = 1, expanded so there is no loop.
  const double inv = 1.0 - t;
  return 3.0 * inv * inv * t * a + 3.0 * inv * t * t * b + t * t * t;
}

// Where a minimising window is heading. The taskbar button it belongs to is
// not something another process can find, so the whole taskbar is the target
// and the copy shrinks toward the middle of it — which is where the button is
// on a default Windows 11 desktop anyway.
POINT TaskbarTarget(const RECT& frame) {
  POINT target = {};
  target.x = frame.left + (frame.right - frame.left) / 2;
  target.y = frame.bottom;

  HWND taskbar = ::FindWindowW(L"Shell_TrayWnd", nullptr);
  RECT bar = {};
  if (taskbar != nullptr && ::GetWindowRect(taskbar, &bar) != FALSE) {
    target.x = bar.left + (bar.right - bar.left) / 2;
    target.y = bar.top + (bar.bottom - bar.top) / 2;
  }
  return target;
}

RECT Interpolate(const RECT& from, const RECT& to, double t) {
  RECT result = {};
  result.left = RoundToInt(from.left + (to.left - from.left) * t);
  result.top = RoundToInt(from.top + (to.top - from.top) * t);
  result.right = RoundToInt(from.right + (to.right - from.right) * t);
  result.bottom = RoundToInt(from.bottom + (to.bottom - from.bottom) * t);
  return result;
}

// The rect an entrance starts from: |frame| scaled about its centre and pushed
// down by the lift.
RECT EntranceStart(const RECT& frame, const AnimationSpec& spec) {
  const int width = frame.right - frame.left;
  const int height = frame.bottom - frame.top;
  const double scale = static_cast<double>(spec.scale_percent) / 100.0;

  const int scaled_width = RoundToInt(static_cast<double>(width) * scale);
  const int scaled_height = RoundToInt(static_cast<double>(height) * scale);
  const int centre_x = frame.left + width / 2;
  const int centre_y = frame.top + height / 2;

  RECT start = {};
  start.left = centre_x - scaled_width / 2;
  start.top = centre_y - scaled_height / 2 + spec.lift;
  start.right = start.left + scaled_width;
  start.bottom = start.top + scaled_height;
  return start;
}

}  // namespace

double EaseAt(const AnimationSpec& spec, double t) {
  const double x = Clamp01(t);

  // Solve x = bezier_x(u) for u, then read bezier_y(u). Bisection rather than
  // Newton: twelve iterations is under a microsecond, always converges, and
  // needs no derivative — Newton on a curve with a zero-slope start (Fluent's
  // 0,0,0,1) does not.
  double low = 0.0;
  double high = 1.0;
  double u = x;
  for (int iteration = 0; iteration < 12; ++iteration) {
    const double estimate = BezierComponent(spec.x1, spec.x2, u);
    if (estimate < x) {
      low = u;
    } else {
      high = u;
    }
    u = (low + high) * 0.5;
  }
  return BezierComponent(spec.y1, spec.y2, u);
}

AnimationSpec SpecFor(const Config& config) {
  AnimationSpec spec;

  // These four numbers per style are the same ones MotionStyle carries in
  // lib/models/enums.dart. They are duplicated rather than sent over the
  // channel because the engine has to be able to animate before the Dart side
  // has said anything, and a curve that arrives late is a curve that is wrong
  // for one window.
  switch (config.animation_style) {
    case AnimationStyle::kMacos:
      spec.x1 = 0.32;
      spec.y1 = 0.72;
      spec.x2 = 0.0;
      spec.y2 = 1.0;
      break;
    case AnimationStyle::kGnome:
      spec.x1 = 0.16;
      spec.y1 = 1.0;
      spec.x2 = 0.3;
      spec.y2 = 1.0;
      break;
    case AnimationStyle::kFluent:
      spec.x1 = 0.0;
      spec.y1 = 0.0;
      spec.x2 = 0.0;
      spec.y2 = 1.0;
      break;
    case AnimationStyle::kPremium:
      spec.x1 = 0.22;
      spec.y1 = 1.0;
      spec.x2 = 0.36;
      spec.y2 = 1.0;
      break;
    case AnimationStyle::kMinimal:
      spec.x1 = 0.4;
      spec.y1 = 0.0;
      spec.x2 = 0.2;
      spec.y2 = 1.0;
      break;
  }

  spec.duration_ms = config.animation_duration;
  if (spec.duration_ms < kMinDurationMs) {
    spec.duration_ms = kMinDurationMs;
  }
  if (spec.duration_ms > kMaxDurationMs) {
    spec.duration_ms = kMaxDurationMs;
  }

  spec.scale_percent = config.animation_scale;
  spec.lift = config.animation_lift;
  return spec;
}

// ---------------------------------------------------------------- animator

WindowAnimator::~WindowAnimator() {
  CancelAll(false);
}

void WindowAnimator::SetFinishedCallback(FinishedCallback callback) {
  finished_ = std::move(callback);
}

bool WindowAnimator::IsAnimating(HWND window) const {
  for (size_t index = 0; index < running_.size(); ++index) {
    if (running_[index].window == window) {
      return true;
    }
  }
  return false;
}

bool WindowAnimator::BeginEntrance(ManagedWindow& window,
                                   const AnimationSpec& spec,
                                   BYTE final_alpha) {
  HWND target = window.hwnd();
  if (target == nullptr || ::IsWindow(target) == FALSE) {
    return false;
  }
  if (IsAnimating(target)) {
    return false;
  }
  // Nothing to animate, and hiding a minimised window would hide it for good:
  // it is not going to come back and clear the alpha itself.
  if (::IsWindowVisible(target) == FALSE || ::IsIconic(target) != FALSE) {
    return false;
  }

  RECT frame = {};
  if (::GetWindowRect(target, &frame) == FALSE) {
    return false;
  }
  const int width = frame.right - frame.left;
  const int height = frame.bottom - frame.top;
  if (width <= 0 || height <= 0 || width > kMaxAnimatedEdge ||
      height > kMaxAnimatedEdge) {
    return false;
  }

  // The window has to be able to disappear for this to work at all, and the
  // layered bit has to be ours to set. A window the application already made
  // layered may be driving per-pixel alpha, which LWA_ALPHA destroys.
  const LONG_PTR ex_style = ::GetWindowLongPtrW(target, GWL_EXSTYLE);
  if ((ex_style & WS_EX_LAYERED) != 0) {
    return false;
  }

  // Capture first: the record of what this window looked like has to predate
  // the first thing we change about it.
  window.CaptureExStyle();
  ::SetLastError(0);
  const LONG_PTR previous =
      ::SetWindowLongPtrW(target, GWL_EXSTYLE, ex_style | WS_EX_LAYERED);
  const DWORD error = ::GetLastError();
  if (previous == 0 && error != 0) {
    // Almost always UIPI refusing a higher-integrity window. Nothing changed.
    return false;
  }

  // Invisible from this instant. Everything the user sees for the next few
  // hundred milliseconds is the copy.
  ::SetLayeredWindowAttributes(target, 0, 0, LWA_ALPHA);

  // DWM plays its own transition on a window that has just appeared, at the
  // same moment and with a curve close enough to ours that the two together
  // read as neither. Off it goes, and the effect plan keeps it off for as long
  // as the window stays managed.
  window.DisableSystemTransitions();

  Running item;
  item.window = target;
  item.trigger = AnimationTrigger::kEntrance;
  ::GetWindowThreadProcessId(target, &item.process_id);
  item.target = frame;
  item.spec = spec;
  item.final_alpha = final_alpha;
  item.added_layered = true;
  item.start_ms = NowMs();

  // The copy goes up now, in the same breath as the window went invisible.
  // Anything between the two is a frame of nothing where a window should be.
  item.proxy.reset(new ProxyWindow());
  if (!item.proxy->Create(target, EntranceStart(frame, spec))) {
    // No copy means no animation. Give the window straight back rather than
    // leave it invisible waiting for one.
    ::SetLayeredWindowAttributes(target, 0, final_alpha, LWA_ALPHA);
    return false;
  }
  item.started = true;

  running_.push_back(std::move(item));
  return true;
}

bool WindowAnimator::BeginExit(HWND window, const RECT& frame,
                               const AnimationSpec& spec) {
  const int width = frame.right - frame.left;
  const int height = frame.bottom - frame.top;
  if (window == nullptr || width <= 0 || height <= 0 ||
      width > kMaxAnimatedEdge || height > kMaxAnimatedEdge) {
    return false;
  }

  Running item;
  item.window = window;
  item.trigger = AnimationTrigger::kExit;
  ::GetWindowThreadProcessId(window, &item.process_id);
  item.target = frame;
  item.spec = spec;
  item.start_ms = NowMs();

  const POINT target = TaskbarTarget(frame);
  const int end_width = RoundToInt(static_cast<double>(width) * kExitEndScale);
  const int end_height = RoundToInt(static_cast<double>(height) * kExitEndScale);
  item.destination.left = target.x - end_width / 2;
  item.destination.top = target.y - end_height / 2;
  item.destination.right = item.destination.left + end_width;
  item.destination.bottom = item.destination.top + end_height;

  // The window is already on the taskbar, so there is nothing to wait for: the
  // copy goes up now.
  item.proxy.reset(new ProxyWindow());
  if (!item.proxy->Create(window, frame)) {
    return false;
  }
  item.started = true;

  running_.push_back(std::move(item));
  return true;
}

void WindowAnimator::Frame(Running& item, double progress) {
  if (!item.proxy || !item.proxy->valid()) {
    return;
  }

  const double eased = EaseAt(item.spec, progress);

  if (item.trigger == AnimationTrigger::kEntrance) {
    // Re-read the window every frame. An application that repositions itself
    // while opening — a splash becoming a main window, a restored window
    // settling — is followed rather than fought.
    RECT frame = {};
    if (::GetWindowRect(item.window, &frame) != FALSE &&
        frame.right > frame.left) {
      item.target = frame;
    }
    const RECT start = EntranceStart(item.target, item.spec);
    const RECT current = Interpolate(start, item.target, eased);
    const double opacity = Clamp01(eased * kAlphaLead);
    item.proxy->Update(current, static_cast<BYTE>(RoundToInt(255.0 * opacity)));
    return;
  }

  const RECT current = Interpolate(item.target, item.destination, eased);
  const double opacity = 1.0 - Clamp01(eased * 1.1);
  item.proxy->Update(current, static_cast<BYTE>(RoundToInt(255.0 * opacity)));
}

void WindowAnimator::Settle(Running& item) {
  // Opacity back first, then the copy goes: for one frame both are on screen
  // showing the same thing, which looks like nothing. The other order shows a
  // hole where the window should be.
  if (item.trigger == AnimationTrigger::kEntrance &&
      ::IsWindow(item.window) != FALSE) {
    DWORD process_id = 0;
    ::GetWindowThreadProcessId(item.window, &process_id);
    if (process_id == item.process_id) {
      ::SetLayeredWindowAttributes(item.window, 0, item.final_alpha, LWA_ALPHA);
    }
  }
  if (item.proxy) {
    item.proxy->Destroy();
    item.proxy.reset();
  }
}

void WindowAnimator::Tick() {
  if (running_.empty()) {
    return;
  }

  const double now = NowMs();
  std::vector<HWND> finished;

  for (size_t index = 0; index < running_.size();) {
    Running& item = running_[index];
    const bool entrance = item.trigger == AnimationTrigger::kEntrance;

    if (entrance) {
      if (::IsWindow(item.window) == FALSE) {
        // Gone mid-flight. There is no window left to hand anything back to.
        Settle(item);
        running_.erase(running_.begin() + static_cast<long long>(index));
        continue;
      }
      DWORD process_id = 0;
      ::GetWindowThreadProcessId(item.window, &process_id);
      if (process_id != item.process_id) {
        // Windows recycles handles. This one is somebody else's now, and the
        // only thing left to do is stop writing to it.
        if (item.proxy) {
          item.proxy->Destroy();
          item.proxy.reset();
        }
        running_.erase(running_.begin() + static_cast<long long>(index));
        continue;
      }
      if (::IsIconic(item.window) != FALSE) {
        // Minimised while it was still arriving. Hand the opacity back, or it
        // comes off the taskbar invisible.
        Settle(item);
        finished.push_back(item.window);
        running_.erase(running_.begin() + static_cast<long long>(index));
        continue;
      }
    }

    double progress =
        (now - item.start_ms) / static_cast<double>(item.spec.duration_ms);
    bool done = false;
    if (progress >= 1.0) {
      progress = 1.0;
      done = true;
    }

    Frame(item, progress);

    if (done) {
      Settle(item);
      if (entrance) {
        finished.push_back(item.window);
      }
      running_.erase(running_.begin() + static_cast<long long>(index));
      continue;
    }

    ++index;
  }

  // Notified after the list has settled, because the callback re-enters this
  // object — the engine applies the effects that were waiting on the window.
  for (size_t index = 0; index < finished.size(); ++index) {
    Notify(finished[index]);
  }
}

void WindowAnimator::Cancel(HWND window, bool notify) {
  for (size_t index = 0; index < running_.size(); ++index) {
    if (running_[index].window != window) {
      continue;
    }
    // An exit is deliberately outliving its window: it is started by the same
    // event that releases it, and the release cancels by handle. Cancelling it
    // here would end the minimise animation on the frame it began.
    if (running_[index].trigger == AnimationTrigger::kExit) {
      continue;
    }
    const bool entrance = true;
    Settle(running_[index]);
    running_.erase(running_.begin() + static_cast<long long>(index));
    if (notify && entrance) {
      Notify(window);
    }
    return;
  }
}

void WindowAnimator::CancelAll(bool notify) {
  if (running_.empty()) {
    return;
  }
  std::vector<Running> pending;
  pending.swap(running_);
  for (size_t index = 0; index < pending.size(); ++index) {
    Settle(pending[index]);
  }
  if (!notify) {
    return;
  }
  for (size_t index = 0; index < pending.size(); ++index) {
    if (pending[index].trigger == AnimationTrigger::kEntrance) {
      Notify(pending[index].window);
    }
  }
}

void WindowAnimator::Notify(HWND window) {
  if (finished_) {
    finished_(window);
  }
}

}  // namespace effects
