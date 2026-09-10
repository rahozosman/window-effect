#include "engine.h"

#include <timeapi.h>

#include <string>
#include <vector>

#include "active_window_effect.h"
#include "backdrop_effect.h"
#include "corner_effect.h"
#include "log.h"
#include "os_version.h"
#include "transparency_effect.h"

namespace effects {
namespace {

const UINT kMessageConfigChanged = WM_APP + 1;
const UINT kMessageRestoreAll = WM_APP + 2;
// wParam carries the requested state: non-zero pauses, zero resumes.
const UINT kMessagePauseChanged = WM_APP + 3;

// One window refusing effects is that window's business. This many refusing
// means the fault is ours, and the engine stops rather than keeps poking.
const int kQuarantineLatchThreshold = 8;

// Asks for more than 60 a second on purpose. Each frame costs a SetWindowPos
// and a DWM thumbnail update, both of which are round trips to another
// process, so a 16 ms budget was landing nearer 25. Asking for 9 ms lands at
// about 16 — and a frame that arrives early simply waits.
const double kFrameIntervalMs = 9.0;

const DWORD kStartTimeoutMs = 5000;
const DWORD kStopTimeoutMs = 5000;

}  // namespace

bool operator==(const EngineStatus& left, const EngineStatus& right) {
  return left.state == right.state &&
         left.managed_windows == right.managed_windows &&
         left.message == right.message;
}

bool operator!=(const EngineStatus& left, const EngineStatus& right) {
  return !(left == right);
}

const char* EngineStateName(EngineState state) {
  switch (state) {
    case EngineState::kStopped:
      return "stopped";
    case EngineState::kObserving:
      return "observing";
    case EngineState::kRunning:
      return "running";
    case EngineState::kPaused:
      return "paused";
    case EngineState::kFaulted:
      return "faulted";
  }
  return "stopped";
}

EffectsEngine::EffectsEngine() : classifier_(&exclusions_) {
  // The animator holds corners, the shadow and transparency back until a
  // window has stopped moving; this is how they are told it has.
  animator_.SetFinishedCallback(
      [this](HWND window) { OnAnimationFinished(window); });
}

EffectsEngine::~EffectsEngine() {
  Stop();
  if (ready_event_ != nullptr) {
    ::CloseHandle(ready_event_);
    ready_event_ = nullptr;
  }
}

void EffectsEngine::SetStatusCallback(StatusCallback callback) {
  status_callback_ = std::move(callback);
}

bool EffectsEngine::Start() {
  if (thread_ != nullptr) {
    return true;
  }

  ready_event_ = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (ready_event_ == nullptr) {
    LogError("could not create the engine ready event");
    return false;
  }

  thread_ = ::CreateThread(nullptr, 0, &EffectsEngine::ThreadEntry, this, 0,
                           &thread_id_);
  if (thread_ == nullptr) {
    LogError("could not start the engine thread");
    ::CloseHandle(ready_event_);
    ready_event_ = nullptr;
    return false;
  }

  // Waits until the thread has a message queue and its hooks installed, so a
  // config pushed immediately after Start cannot be posted into the void.
  ::WaitForSingleObject(ready_event_, kStartTimeoutMs);
  return true;
}

void EffectsEngine::Stop() {
  if (thread_ == nullptr) {
    return;
  }

  ::PostThreadMessageW(thread_id_, WM_QUIT, 0, 0);
  if (::WaitForSingleObject(thread_, kStopTimeoutMs) != WAIT_OBJECT_0) {
    // Leaving the thread running is strictly better than terminating it while
    // it holds a window in a modified state.
    LogError("engine thread did not exit in time");
  }

  ::CloseHandle(thread_);
  thread_ = nullptr;
  thread_id_ = 0;
}

DWORD WINAPI EffectsEngine::ThreadEntry(LPVOID parameter) {
  EffectsEngine* engine = static_cast<EffectsEngine*>(parameter);
  if (engine != nullptr) {
    engine->ThreadMain();
  }
  return 0;
}

void EffectsEngine::ThreadMain() {
  // Forces the message queue into existence before anyone may PostThreadMessage
  // to this thread.
  MSG message = {};
  ::PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);

  {
    std::lock_guard<std::mutex> guard(config_mutex_);
    if (config_pending_) {
      config_ = pending_config_;
      config_pending_ = false;
    }
  }
  exclusions_.SetUserList(config_.excluded_apps);

  const bool installed = detector_.Install(this);
  if (installed) {
    // Before the scan, not after: a window enumerated while foreground_ is
    // still null would be built inactive and stay that way until the user
    // clicked something else.
    foreground_ = ::GetForegroundWindow();
    // Windows that were already open are not arriving, so they are not
    // animated: the alternative is every window on the desktop jumping at
    // once the moment the engine starts.
    allow_entrance_animation_ = false;
    detector_.EnumerateExisting();
    allow_entrance_animation_ = true;
  }

  {
    std::lock_guard<std::mutex> guard(status_mutex_);
    status_.state =
        installed ? EngineState::kObserving : EngineState::kFaulted;
    if (!installed) {
      status_.message = "Windows refused the window event hooks.";
    }
  }
  UpdateLocationTracking();
  UpdateRestingState();
  PublishStatus();

  if (ready_event_ != nullptr) {
    ::SetEvent(ready_event_);
  }

  // The loop waits on a timeout rather than on a WM_TIMER, because WM_TIMER
  // is the lowest-priority message Windows has: it is only generated when the
  // queue is empty, and this queue is where every WinEvent in the system
  // lands. A window opening produces a burst of them, so the one moment the
  // frame clock has to be reliable is the one moment WM_TIMER is starved —
  // which is what made the animation stutter at about 19 frames a second.
  //
  // MsgWaitForMultipleObjectsEx returns on either a message or the timeout,
  // and the timeout is not a message, so nothing can crowd it out.
  bool quit = false;
  double last_frame = 0.0;

  while (!quit) {
    while (::PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != FALSE) {
      if (message.message == WM_QUIT) {
        quit = true;
        break;
      }
      switch (message.message) {
        case kMessageConfigChanged:
          ApplyPendingConfig();
          break;
        case kMessageRestoreAll:
          RestoreAllOnThread();
          PublishStatus();
          break;
        case kMessagePauseChanged:
          SetPausedOnThread(message.wParam != 0);
          break;
        default:
          break;
      }
      ::TranslateMessage(&message);
      ::DispatchMessageW(&message);

      // Inside the drain, not after it. A window opening arrives as a burst of
      // WinEvents, and each one can cost a handful of DWM round trips; waiting
      // for the burst to finish before looking at the clock is what turned a
      // 16 ms frame into an 80 ms one.
      if (animator_.active()) {
        const double burst_now = NowMs();
        if (burst_now - last_frame >= kFrameIntervalMs) {
          last_frame = burst_now;
          animator_.Tick();
        }
      }
    }
    if (quit) {
      break;
    }

    if (!animator_.active()) {
      // Nothing moving: sleep until something happens, at no cost at all.
      ::MsgWaitForMultipleObjectsEx(0, nullptr, INFINITE, QS_ALLINPUT,
                                    MWMO_INPUTAVAILABLE);
      continue;
    }

    const double now = NowMs();
    if (now - last_frame >= kFrameIntervalMs) {
      last_frame = now;
      animator_.Tick();
      if (!animator_.active()) {
        StopFrameClock();
        // Nothing is moving any more, so the work that was making room for it
        // can happen now.
        FlushDeferredShadows();
      }
      continue;
    }

    // At least a millisecond, or a wait of zero turns the loop into a spin.
    double remaining_ms = kFrameIntervalMs - (now - last_frame);
    if (remaining_ms < 1.0) {
      remaining_ms = 1.0;
    }
    ::MsgWaitForMultipleObjectsEx(0, nullptr,
                                  static_cast<DWORD>(remaining_ms), QS_ALLINPUT,
                                  MWMO_INPUTAVAILABLE);
  }

  // Every exit path unwinds. This one covers a normal shutdown; the faulted
  // path unwinds in Fault before it gets here.
  StopFrameClock();
  detector_.Uninstall();
  RestoreAllOnThread();
  classifier_.ClearProcessCache();

  {
    std::lock_guard<std::mutex> guard(status_mutex_);
    status_.state = EngineState::kStopped;
    status_.managed_windows = 0;
  }
  PublishStatus();

  LogInfo("engine thread stopped");
}

// ---------------------------------------------------------------- public API

void EffectsEngine::UpdateConfig(const Config& config) {
  {
    std::lock_guard<std::mutex> guard(config_mutex_);
    pending_config_ = config;
    ClampConfig(&pending_config_);
    config_pending_ = true;
  }
  if (thread_id_ != 0) {
    ::PostThreadMessageW(thread_id_, kMessageConfigChanged, 0, 0);
  }
}

void EffectsEngine::SetEnabled(bool enabled) {
  {
    std::lock_guard<std::mutex> guard(config_mutex_);
    if (!config_pending_) {
      pending_config_ = config_;
    }
    pending_config_.effects_enabled = enabled;
    config_pending_ = true;
  }
  if (thread_id_ != 0) {
    ::PostThreadMessageW(thread_id_, kMessageConfigChanged, 0, 0);
  }
}

void EffectsEngine::SetPaused(bool paused) {
  if (thread_id_ != 0) {
    ::PostThreadMessageW(thread_id_, kMessagePauseChanged,
                         paused ? 1u : 0u, 0);
  }
}

bool EffectsEngine::IsPaused() const {
  std::lock_guard<std::mutex> guard(status_mutex_);
  return status_.state == EngineState::kPaused;
}

void EffectsEngine::RestoreAll() {
  if (thread_id_ != 0) {
    ::PostThreadMessageW(thread_id_, kMessageRestoreAll, 0, 0);
  }
}

EngineStatus EffectsEngine::GetStatus() const {
  std::lock_guard<std::mutex> guard(status_mutex_);
  return status_;
}

// ------------------------------------------------------------- engine thread

bool EffectsEngine::EffectsAreLive() const {
  if (!config_.effects_enabled) {
    return false;
  }
  std::lock_guard<std::mutex> guard(status_mutex_);
  return status_.state != EngineState::kFaulted &&
         status_.state != EngineState::kPaused;
}

void EffectsEngine::ApplyPendingConfig() {
  {
    std::lock_guard<std::mutex> guard(config_mutex_);
    if (!config_pending_) {
      return;
    }
    config_ = pending_config_;
    config_pending_ = false;
  }

  exclusions_.SetUserList(config_.excluded_apps);
  compatibility_.RefreshSystemTheme();
  // A window excluded by the new list has to give its original state back
  // before it leaves the managed set, so the whole set is re-evaluated.
  classifier_.ClearProcessCache();

  if (!config_.effects_enabled) {
    RestoreAllOnThread();
    UpdateLocationTracking();
    UpdateRestingState();
    PublishStatus();
    return;
  }

  UpdateLocationTracking();

  // Re-classify everything currently managed, then rescan for windows the new
  // settings have just made eligible. None of this is a window arriving, so
  // none of it animates — changing a setting must not make the desktop move.
  allow_entrance_animation_ = false;

  std::vector<HWND> current;
  current.reserve(managed_.size());
  for (std::unordered_map<HWND, std::unique_ptr<ManagedWindow>>::const_iterator
           iterator = managed_.begin();
       iterator != managed_.end(); ++iterator) {
    current.push_back(iterator->first);
  }
  for (size_t index = 0; index < current.size(); ++index) {
    EvaluateWindow(current[index]);
  }
  detector_.EnumerateExisting();
  allow_entrance_animation_ = true;

  UpdateRestingState();
  PublishStatus();
}

void EffectsEngine::SetPausedOnThread(bool paused) {
  {
    std::lock_guard<std::mutex> guard(status_mutex_);
    // A faulted or stopped engine has already handed every window back;
    // pausing it would only paper over the reason it did.
    if (status_.state == EngineState::kFaulted ||
        status_.state == EngineState::kStopped) {
      return;
    }
    if ((status_.state == EngineState::kPaused) == paused) {
      return;
    }
    // UpdateRestingState settles this into running or observing below; going
    // through kObserving first is what lets it run at all.
    status_.state = paused ? EngineState::kPaused : EngineState::kObserving;
  }

  if (paused) {
    LogInfo("paused");
    RestoreAllOnThread();
    // Nothing is following a window any more, so the most expensive
    // subscription goes with it rather than idling on.
    UpdateLocationTracking();
  } else {
    LogInfo("resumed");
    UpdateLocationTracking();
    allow_entrance_animation_ = false;
    detector_.EnumerateExisting();
    allow_entrance_animation_ = true;
  }

  UpdateRestingState();
  PublishStatus();
}

void EffectsEngine::UpdateLocationTracking() {
  // The single most expensive subscription in the system, so it is installed
  // only while an effect actually has to follow a window pixel by pixel. With
  // the shadow off and system corners selected, it is never installed at all.
  //
  // The pause check belongs here rather than at the call sites: a settings
  // change arriving while the engine is paused goes through the ordinary
  // config path, and without this it would quietly reinstate the hook that
  // pausing had just dropped.
  detector_.SetLocationTracking(!IsPaused() && config_.NeedsLocationTracking());
}

void EffectsEngine::UpdateRestingState() {
  std::lock_guard<std::mutex> guard(status_mutex_);
  if (status_.state == EngineState::kFaulted ||
      status_.state == EngineState::kStopped ||
      status_.state == EngineState::kPaused) {
    return;
  }
  // "Running" means windows are actually being changed. With the master switch
  // off, or nothing managed, the honest word is "observing".
  status_.state = (config_.effects_enabled && !managed_.empty())
                      ? EngineState::kRunning
                      : EngineState::kObserving;
}

void EffectsEngine::EvaluateWindow(HWND window) {
  if (!EffectsAreLive()) {
    ForgetWindow(window, "effects off");
    return;
  }

  WindowInfo info;
  const WindowVerdict verdict = classifier_.Classify(window, &info);

  if (verdict != WindowVerdict::kEligible) {
    ForgetWindow(window, VerdictName(verdict));
    return;
  }

  std::unordered_map<HWND, std::unique_ptr<ManagedWindow>>::iterator existing =
      managed_.find(window);
  if (existing != managed_.end()) {
    existing->second->RefreshInfo(info);
    ApplyPlan(*existing->second,
              compatibility_.PlanFor(existing->second->info(), config_,
                                     GetCapabilities(),
                                     existing->second->active()));
    return;
  }

  std::unique_ptr<ManagedWindow> managed(new ManagedWindow(info));
  managed->set_active(window == foreground_);
  ManagedWindow* entry = managed.get();
  managed_.emplace(window, std::move(managed));

  // The plan is computed once and used twice: the animation has to end on the
  // same alpha the plan is about to ask for, or the handoff between them is a
  // visible step.
  const EffectPlan plan = compatibility_.PlanFor(
      entry->info(), config_, GetCapabilities(), entry->active());

  // Before the log line, deliberately. Writing to a file takes long enough to
  // be visible here: until this returns, the window is on screen at full size
  // with no animation on it.
  MaybeAnimate(*entry, plan);

  LogInfo("managing " + Narrow(info.executable) + " [" +
          Narrow(info.class_name) + "]");

  ApplyPlan(*entry, plan);
  UpdateRestingState();
  PublishStatus();
}

void EffectsEngine::ApplyPlan(ManagedWindow& window, const EffectPlan& plan) {
  if (window.quarantined()) {
    return;
  }

  // While a window is animating, the animator owns its layered alpha, and the
  // effects that depend on a settled geometry have nothing stable to work
  // from. Both wait, and OnAnimationFinished applies them.
  const bool animating = animator_.IsAnimating(window.hwnd());

  // And while *any* window is animating, every other window is left alone.
  // Restyling one costs a region rebuild, a shadow blur and a repaint of
  // somebody else's window, all on this thread — which is the thread the
  // animation frames come from. A window that keeps the look it already has
  // for another few hundred milliseconds is invisible; a stuttering animation
  // is not.
  if (!animating && animator_.active()) {
    deferred_shadows_.push_back(window.hwnd());
    return;
  }

  // Whose animation this window gets is not a per-frame decision, so it is
  // applied on every pass rather than only while something is moving.
  if (plan.own_transitions) {
    window.DisableSystemTransitions();
  } else {
    window.RestoreSystemTransitions();
  }

  bool ok = true;
  if (!animating) {
    ok = CornerEffect::Apply(window, plan) && ok;
  }
  ok = BackdropEffect::Apply(window, plan) && ok;
  if (!animating) {
    ok = TransparencyEffect::Apply(window, plan) && ok;
  }
  ok = ActiveWindowEffect::Apply(window, plan) && ok;
  if (animating) {
    shadows_.Hide(window.hwnd());
  } else {
    shadows_.Update(window, plan);
  }

  if (ok) {
    window.ResetFailures();
    return;
  }

  window.RecordFailure();
  if (!window.quarantined()) {
    return;
  }

  // Three strikes: this window does not want what we are doing. Hand back
  // everything and stop retrying it on every event for the rest of its life.
  shadows_.Remove(window.hwnd());
  window.Restore();

  ++quarantined_windows_;
  if (quarantined_windows_ >= kQuarantineLatchThreshold) {
    Fault(
        "Too many windows rejected these effects, so everything has been "
        "restored. Turn effects back on to try again.");
  }
}

void EffectsEngine::MaybeAnimate(ManagedWindow& window,
                                 const EffectPlan& plan) {
  if (!config_.animations_enabled || !allow_entrance_animation_) {
    return;
  }
  if (!config_.effects_enabled || IsPaused()) {
    return;
  }

  // The alpha the plan is about to ask for is where the animation has to end.
  const BYTE final_alpha =
      plan.layered ? static_cast<BYTE>(plan.alpha) : static_cast<BYTE>(255);

  if (!animator_.BeginEntrance(window, SpecFor(config_), final_alpha)) {
    return;
  }
  StartFrameClock();
}

void EffectsEngine::MaybeAnimateMinimize(HWND window) {
  if (!config_.animations_enabled || !config_.animate_minimize) {
    return;
  }
  if (!config_.effects_enabled || IsPaused()) {
    return;
  }

  std::unordered_map<HWND, std::unique_ptr<ManagedWindow>>::iterator found =
      managed_.find(window);
  if (found == managed_.end()) {
    return;
  }

  // The window's own rect is already the shell's by now; the last frame it
  // had while it was on screen is the only honest starting point left.
  if (!animator_.BeginExit(window, found->second->last_visible_frame(),
                           SpecFor(config_))) {
    return;
  }
  StartFrameClock();
}

void EffectsEngine::OnAnimationFinished(HWND window) {
  // The window has settled. Everything that was waiting on a stable geometry
  // goes on now, in one pass.
  Refresh(window);
}

void EffectsEngine::FlushDeferredShadows() {
  if (deferred_shadows_.empty()) {
    return;
  }
  std::vector<HWND> pending;
  pending.swap(deferred_shadows_);
  for (size_t index = 0; index < pending.size(); ++index) {
    std::unordered_map<HWND, std::unique_ptr<ManagedWindow>>::iterator found =
        managed_.find(pending[index]);
    if (found == managed_.end()) {
      continue;
    }
    ManagedWindow& entry = *found->second;
    ApplyPlan(entry, compatibility_.PlanFor(entry.info(), config_,
                                            GetCapabilities(),
                                            entry.active()));
  }
}

void EffectsEngine::StartFrameClock() {
  if (frame_clock_) {
    return;
  }
  frame_clock_ = true;
  // Windows' default scheduler resolution is 15.6 ms, so a 16 ms wait lands
  // at 15.6 or at 31.2 — the difference between an animation and a stutter.
  // The request is dropped again the moment the clock stops.
  ::timeBeginPeriod(1);
}

void EffectsEngine::StopFrameClock() {
  if (!frame_clock_) {
    return;
  }
  frame_clock_ = false;
  ::timeEndPeriod(1);
}

void EffectsEngine::Refresh(HWND window) {
  std::unordered_map<HWND, std::unique_ptr<ManagedWindow>>::iterator found =
      managed_.find(window);
  if (found == managed_.end()) {
    return;
  }
  ManagedWindow& entry = *found->second;
  ApplyPlan(entry, compatibility_.PlanFor(entry.info(), config_,
                                          GetCapabilities(), entry.active()));
}

void EffectsEngine::ForgetWindow(HWND window, const char* reason) {
  std::unordered_map<HWND, std::unique_ptr<ManagedWindow>>::iterator found =
      managed_.find(window);
  if (found == managed_.end()) {
    // Not ours. This is the common case — most window events in the system
    // belong to windows the classifier already turned away.
    return;
  }

  // Quietly: the window is being handed back on the next line, so applying
  // the effects the animation was holding would be work done only to undo it.
  animator_.Cancel(window, false);
  shadows_.Remove(window);
  found->second->Restore();
  LogInfo("released " + Narrow(found->second->info().executable) + " (" +
          (reason != nullptr ? reason : "unspecified") + ")");
  managed_.erase(found);
  UpdateRestingState();
  PublishStatus();
}

void EffectsEngine::RestoreAllOnThread() {
  // Animations first, and quietly: each one puts its window back where it
  // belongs, which has to happen before the restore pass reads that geometry.
  animator_.CancelAll(false);
  StopFrameClock();
  deferred_shadows_.clear();

  // Unconditional: shadow windows are created on this thread and DestroyWindow
  // from any other one silently does nothing, so they must be torn down here
  // even when there is nothing else left to restore.
  shadows_.RemoveAll();

  if (managed_.empty()) {
    return;
  }
  LogInfo("restoring " + std::to_string(managed_.size()) + " window(s)");
  for (std::unordered_map<HWND, std::unique_ptr<ManagedWindow>>::iterator
           iterator = managed_.begin();
       iterator != managed_.end(); ++iterator) {
    iterator->second->Restore();
  }
  managed_.clear();
}

void EffectsEngine::PublishStatus() {
  EngineStatus snapshot;
  {
    std::lock_guard<std::mutex> guard(status_mutex_);
    status_.managed_windows = static_cast<int>(managed_.size());
    snapshot = status_;
  }
  if (status_callback_) {
    status_callback_(snapshot);
  }
}

void EffectsEngine::Fault(const std::string& reason) {
  {
    std::lock_guard<std::mutex> guard(status_mutex_);
    if (status_.state == EngineState::kFaulted) {
      return;
    }
    status_.state = EngineState::kFaulted;
    status_.message = reason;
  }

  LogError("fault: " + reason);

  // Stop observing, hand every window back, and stay alive so the UI can still
  // report what happened and the user can still turn things off.
  detector_.Uninstall();
  RestoreAllOnThread();
  PublishStatus();
}

// ---------------------------------------------------------- window callbacks

void EffectsEngine::OnWindowAppeared(HWND window) {
  EvaluateWindow(window);
}

void EffectsEngine::OnWindowVanished(HWND window) {
  ForgetWindow(window, "hidden");
}

void EffectsEngine::OnWindowDestroyed(HWND window) {
  // The HWND is already invalid, so ManagedWindow::Restore will find IsWindow
  // false and do nothing. Dropping the record is all that is left.
  animator_.Cancel(window, false);
  std::unordered_map<HWND, std::unique_ptr<ManagedWindow>>::iterator found =
      managed_.find(window);
  if (found == managed_.end()) {
    return;
  }
  shadows_.Remove(window);
  managed_.erase(found);
  UpdateRestingState();
  PublishStatus();
}

void EffectsEngine::OnForegroundChanged(HWND window) {
  HWND previous = foreground_;
  foreground_ = window;

  for (std::unordered_map<HWND, std::unique_ptr<ManagedWindow>>::iterator
           iterator = managed_.begin();
       iterator != managed_.end(); ++iterator) {
    iterator->second->set_active(iterator->first == window);
  }

  // Exactly two windows change state, so only those two are reapplied. Every
  // other window was already inactive and still is.
  if (previous != nullptr && previous != window) {
    Refresh(previous);
  }

  // A window can become the foreground before its EVENT_OBJECT_SHOW is seen,
  // so this has to be the full path rather than a refresh.
  EvaluateWindow(window);
}

void EffectsEngine::OnCloakChanged(HWND window) {
  EvaluateWindow(window);
}

void EffectsEngine::OnMinimizeChanged(HWND window, bool minimized) {
  if (minimized) {
    // Order matters: the copy is made from the frame this window still
    // remembers, and releasing it is what clears that record.
    MaybeAnimateMinimize(window);
    ForgetWindow(window, "minimized");
    return;
  }
  // Coming back from the taskbar is a window arriving, and gets the same
  // entrance as one that has just opened.
  EvaluateWindow(window);
}

void EffectsEngine::OnMoveSizeChanged(HWND window, bool moving) {
  std::unordered_map<HWND, std::unique_ptr<ManagedWindow>>::iterator found =
      managed_.find(window);
  if (found == managed_.end()) {
    return;
  }
  found->second->set_moving(moving);

  if (moving) {
    // Nothing in another process can keep up with a drag, and a shadow
    // trailing behind the window is worse than none at all.
    shadows_.Hide(window);
    return;
  }

  // Settled: re-classify, because the size may have changed, then bring the
  // shadow back at the new geometry.
  EvaluateWindow(window);
}

void EffectsEngine::OnLocationChanged(HWND window) {
  std::unordered_map<HWND, std::unique_ptr<ManagedWindow>>::iterator found =
      managed_.find(window);
  if (found == managed_.end()) {
    return;
  }
  ManagedWindow& entry = *found->second;

  // The hot path. Deliberately avoids DwmGetWindowAttribute: the frame is
  // derived from GetWindowRect and the inset cached at classification time,
  // which turns a cross-process round trip into a local call.
  RECT outer = {};
  if (!::GetWindowRect(window, &outer)) {
    return;
  }
  const RECT& inset = entry.info().frame_inset;
  RECT frame = {};
  frame.left = outer.left + inset.left;
  frame.top = outer.top + inset.top;
  frame.right = outer.right - inset.right;
  frame.bottom = outer.bottom - inset.bottom;

  const RECT& previous = entry.info().frame;
  const bool resized =
      (frame.right - frame.left) != (previous.right - previous.left) ||
      (frame.bottom - frame.top) != (previous.bottom - previous.top);

  entry.SetFrame(frame);

  if (resized) {
    // A window region has to be rebuilt for the new size, and the shadow
    // bitmap has to be repainted. Both happen inside the full reapply.
    Refresh(window);
    return;
  }

  // A pure move costs one SetWindowPos and no repaint at all.
  shadows_.Reposition(entry);
}

}  // namespace effects
