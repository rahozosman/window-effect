#ifndef RUNNER_EFFECTS_ENGINE_H_
#define RUNNER_EFFECTS_ENGINE_H_

#include <windows.h>

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "animation.h"
#include "compatibility.h"
#include "config.h"
#include "exclusion_manager.h"
#include "managed_window.h"
#include "shadow_effect.h"
#include "window_classifier.h"
#include "window_detection.h"

namespace effects {

enum class EngineState {
  kStopped,
  // Hooks installed, but the master switch or a fault means nothing is being
  // applied. Also the resting state when every effect is unsupported here.
  kObserving,
  kRunning,
  kPaused,
  kFaulted,
};

struct EngineStatus {
  EngineState state = EngineState::kStopped;
  int managed_windows = 0;
  std::string message;
};

bool operator==(const EngineStatus& left, const EngineStatus& right);
bool operator!=(const EngineStatus& left, const EngineStatus& right);

const char* EngineStateName(EngineState state);

// Owns the window lifecycle, the managed set and the safety latch.
//
// Everything after Start runs on a dedicated thread with its own message loop.
// The public methods are safe from any thread; each one either takes a lock or
// posts to that loop.
class EffectsEngine : public WindowEventSink {
 public:
  // Invoked on the ENGINE thread. The bridge is responsible for hopping to the
  // platform thread before touching a Flutter channel.
  typedef std::function<void(const EngineStatus&)> StatusCallback;

  EffectsEngine();
  ~EffectsEngine() override;

  EffectsEngine(const EffectsEngine&) = delete;
  EffectsEngine& operator=(const EffectsEngine&) = delete;

  // Must be set before Start.
  void SetStatusCallback(StatusCallback callback);

  bool Start();
  void Stop();

  void UpdateConfig(const Config& config);
  void SetEnabled(bool enabled);

  // Suspends every effect without touching the configuration, so resuming
  // gives back exactly what the user had. This is what the tray's "Pause"
  // drives; the master switch is a setting, this is not.
  void SetPaused(bool paused);
  bool IsPaused() const;

  // Restores every managed window now. Returns once the request is queued.
  void RestoreAll();

  EngineStatus GetStatus() const;

 private:
  // WindowEventSink, all on the engine thread.
  void OnWindowAppeared(HWND window) override;
  void OnWindowVanished(HWND window) override;
  void OnWindowDestroyed(HWND window) override;
  void OnForegroundChanged(HWND window) override;
  void OnCloakChanged(HWND window) override;
  void OnMinimizeChanged(HWND window, bool minimized) override;
  void OnMoveSizeChanged(HWND window, bool moving) override;
  void OnLocationChanged(HWND window) override;

  static DWORD WINAPI ThreadEntry(LPVOID parameter);
  void ThreadMain();

  // Engine thread only.
  void ApplyPendingConfig();
  void SetPausedOnThread(bool paused);
  // Starts the entrance animation for a window that has just appeared, if
  // motion is on and this is a window arriving rather than one being
  // re-evaluated.
  void MaybeAnimate(ManagedWindow& window, const EffectPlan& plan);
  // Applies the shadow work that was put off while windows were animating.
  void FlushDeferredShadows();
  // Sends a copy of a window on its way to the taskbar. The window itself is
  // released either way; the animation outlives it by a fraction of a second.
  void MaybeAnimateMinimize(HWND window);
  void OnAnimationFinished(HWND window);
  void StartFrameClock();
  void StopFrameClock();
  void EvaluateWindow(HWND window);
  // Runs the effect managers for one window and handles their failures.
  void ApplyPlan(ManagedWindow& window, const EffectPlan& plan);
  // Recomputes and reapplies one window's plan without re-classifying it.
  void Refresh(HWND window);
  void UpdateLocationTracking();
  void UpdateRestingState();
  // |reason| is recorded in the log so a window that stopped being managed
  // can always be traced back to the rule that released it.
  void ForgetWindow(HWND window, const char* reason);
  void RestoreAllOnThread();
  void PublishStatus();
  void Fault(const std::string& reason);
  bool EffectsAreLive() const;

  ExclusionManager exclusions_;
  WindowClassifier classifier_;
  WindowDetector detector_;
  CompatibilityManager compatibility_;
  ShadowManager shadows_;
  WindowAnimator animator_;

  // True while the engine is holding the system timer resolution down for the
  // animation clock. The clock itself is the loop's own wait timeout; this is
  // only the resolution request, which must be released exactly once.
  bool frame_clock_ = false;

  // Windows whose shadow was skipped because something was animating at the
  // time. Painting a shadow means a blur and a bitmap upload the size of the
  // window, on this thread — which is the same thread the animation frames
  // come from, and it showed: a window opening dropped the animation to about
  // nineteen frames a second.
  std::vector<HWND> deferred_shadows_;

  // False while the engine is walking windows that already existed — at
  // startup, after a config change and on resume. Animating those would mean
  // every window on the desktop jumping at once for no reason the user asked
  // for.
  bool allow_entrance_animation_ = false;

  std::unordered_map<HWND, std::unique_ptr<ManagedWindow>> managed_;
  HWND foreground_ = nullptr;

  // Windows that have given up on us. Enough of them means the problem is
  // ours, not theirs, and the panic latch trips.
  int quarantined_windows_ = 0;

  // Engine-thread copy. Never read from another thread.
  Config config_;

  mutable std::mutex config_mutex_;
  Config pending_config_;
  bool config_pending_ = false;

  mutable std::mutex status_mutex_;
  EngineStatus status_;

  StatusCallback status_callback_;

  HANDLE thread_ = nullptr;
  HANDLE ready_event_ = nullptr;
  DWORD thread_id_ = 0;
};

}  // namespace effects

#endif  // RUNNER_EFFECTS_ENGINE_H_
