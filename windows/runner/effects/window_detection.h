#ifndef RUNNER_EFFECTS_WINDOW_DETECTION_H_
#define RUNNER_EFFECTS_WINDOW_DETECTION_H_

#include <windows.h>

namespace effects {

// Callbacks arrive on the engine thread, inside its message loop.
class WindowEventSink {
 public:
  virtual ~WindowEventSink() = default;

  // A window became visible, or was created already visible.
  virtual void OnWindowAppeared(HWND window) = 0;
  // A window was hidden but still exists.
  virtual void OnWindowVanished(HWND window) = 0;
  virtual void OnWindowDestroyed(HWND window) = 0;

  virtual void OnForegroundChanged(HWND window) = 0;
  virtual void OnCloakChanged(HWND window) = 0;
  virtual void OnMinimizeChanged(HWND window, bool minimized) = 0;
  virtual void OnMoveSizeChanged(HWND window, bool moving) = 0;

  // Only ever called while location tracking is switched on.
  virtual void OnLocationChanged(HWND window) = 0;
};

// Installs the WinEvent hooks the engine needs and turns raw events into the
// sink's vocabulary.
//
// Install and Uninstall must both run on the thread that owns the message
// loop: WINEVENT_OUTOFCONTEXT delivers callbacks through the installing
// thread's queue, and UnhookWinEvent is only valid from that thread.
class WindowDetector {
 public:
  WindowDetector() = default;
  ~WindowDetector();

  WindowDetector(const WindowDetector&) = delete;
  WindowDetector& operator=(const WindowDetector&) = delete;

  bool Install(WindowEventSink* sink);
  void Uninstall();

  // EVENT_OBJECT_LOCATIONCHANGE fires hundreds of times a second during a
  // drag, so it is a separate, opt-in subscription rather than part of the
  // standard set. Only the companion shadow and precise corners need it.
  void SetLocationTracking(bool enabled);
  bool location_tracking() const { return location_hook_ != nullptr; }

  // Walks the windows that already exist, so a window opened before the engine
  // started is picked up too.
  void EnumerateExisting();

 private:
  static void CALLBACK EventProc(HWINEVENTHOOK hook, DWORD event, HWND window,
                                 LONG object_id, LONG child_id,
                                 DWORD event_thread, DWORD event_time);
  static BOOL CALLBACK EnumProc(HWND window, LPARAM parameter);

  void Dispatch(DWORD event, HWND window);

  WindowEventSink* sink_ = nullptr;

  HWINEVENTHOOK foreground_hook_ = nullptr;
  HWINEVENTHOOK movesize_hook_ = nullptr;
  HWINEVENTHOOK minimize_hook_ = nullptr;
  HWINEVENTHOOK object_hook_ = nullptr;
  HWINEVENTHOOK cloak_hook_ = nullptr;
  HWINEVENTHOOK location_hook_ = nullptr;
};

}  // namespace effects

#endif  // RUNNER_EFFECTS_WINDOW_DETECTION_H_
