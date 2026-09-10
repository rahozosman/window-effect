#include "window_detection.h"

#include "log.h"

// Present since the Windows 8 SDK, but guarded so an older SDK still builds.
#ifndef EVENT_OBJECT_CLOAKED
#define EVENT_OBJECT_CLOAKED 0x8017
#endif
#ifndef EVENT_OBJECT_UNCLOAKED
#define EVENT_OBJECT_UNCLOAKED 0x8018
#endif
#ifndef OBJID_WINDOW
#define OBJID_WINDOW ((LONG)0x00000000)
#endif

namespace effects {
namespace {

// SetWinEventHook's callback carries no user pointer, so the active detector
// is reached through a file-scope pointer. Only one engine ever exists, and
// both the assignment and every callback happen on the engine thread.
WindowDetector* g_detector = nullptr;

// SKIPOWNPROCESS keeps our own settings window out of the stream entirely,
// which is cheaper and safer than filtering it later.
const DWORD kHookFlags = WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS;

// CHILDID_SELF is declared in oleacc.h. The value is fixed ABI, and pulling
// the whole accessibility header in for one zero is not worth it.
const LONG kChildIdSelf = 0;

}  // namespace

void CALLBACK WindowDetector::EventProc(HWINEVENTHOOK hook, DWORD event,
                                        HWND window, LONG object_id,
                                        LONG child_id, DWORD event_thread,
                                        DWORD event_time) {
  (void)hook;
  (void)event_thread;
  (void)event_time;

  // Everything we care about is the window itself. Filtering here keeps the
  // per-control accessibility traffic — which is the overwhelming majority of
  // these events — from reaching the classifier at all.
  if (object_id != OBJID_WINDOW || child_id != kChildIdSelf) {
    return;
  }
  if (window == nullptr) {
    return;
  }
  if (g_detector == nullptr) {
    return;
  }
  g_detector->Dispatch(event, window);
}

void WindowDetector::Dispatch(DWORD event, HWND window) {
  if (sink_ == nullptr) {
    return;
  }

  switch (event) {
    case EVENT_OBJECT_SHOW:
      sink_->OnWindowAppeared(window);
      break;
    case EVENT_OBJECT_HIDE:
      sink_->OnWindowVanished(window);
      break;
    case EVENT_OBJECT_DESTROY:
      sink_->OnWindowDestroyed(window);
      break;
    case EVENT_SYSTEM_FOREGROUND:
      sink_->OnForegroundChanged(window);
      break;
    case EVENT_OBJECT_CLOAKED:
    case EVENT_OBJECT_UNCLOAKED:
      sink_->OnCloakChanged(window);
      break;
    case EVENT_SYSTEM_MINIMIZESTART:
      sink_->OnMinimizeChanged(window, true);
      break;
    case EVENT_SYSTEM_MINIMIZEEND:
      sink_->OnMinimizeChanged(window, false);
      break;
    case EVENT_SYSTEM_MOVESIZESTART:
      sink_->OnMoveSizeChanged(window, true);
      break;
    case EVENT_SYSTEM_MOVESIZEEND:
      sink_->OnMoveSizeChanged(window, false);
      break;
    case EVENT_OBJECT_LOCATIONCHANGE:
      sink_->OnLocationChanged(window);
      break;
    default:
      break;
  }
}

WindowDetector::~WindowDetector() {
  Uninstall();
}

bool WindowDetector::Install(WindowEventSink* sink) {
  if (sink == nullptr) {
    return false;
  }
  if (sink_ != nullptr) {
    return true;
  }

  sink_ = sink;
  g_detector = this;

  // Contiguous event ranges are grouped so the system installs as few hooks as
  // possible. LOCATIONCHANGE is deliberately absent — see SetLocationTracking.
  foreground_hook_ =
      ::SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
                        nullptr, &WindowDetector::EventProc, 0, 0, kHookFlags);
  movesize_hook_ =
      ::SetWinEventHook(EVENT_SYSTEM_MOVESIZESTART, EVENT_SYSTEM_MOVESIZEEND,
                        nullptr, &WindowDetector::EventProc, 0, 0, kHookFlags);
  minimize_hook_ =
      ::SetWinEventHook(EVENT_SYSTEM_MINIMIZESTART, EVENT_SYSTEM_MINIMIZEEND,
                        nullptr, &WindowDetector::EventProc, 0, 0, kHookFlags);
  object_hook_ =
      ::SetWinEventHook(EVENT_OBJECT_DESTROY, EVENT_OBJECT_HIDE, nullptr,
                        &WindowDetector::EventProc, 0, 0, kHookFlags);
  cloak_hook_ =
      ::SetWinEventHook(EVENT_OBJECT_CLOAKED, EVENT_OBJECT_UNCLOAKED, nullptr,
                        &WindowDetector::EventProc, 0, 0, kHookFlags);

  const bool installed = foreground_hook_ != nullptr && object_hook_ != nullptr;
  if (!installed) {
    LogError("failed to install the window event hooks");
    Uninstall();
    return false;
  }

  LogInfo("window event hooks installed");
  return true;
}

void WindowDetector::Uninstall() {
  SetLocationTracking(false);

  HWINEVENTHOOK* const hooks[] = {&foreground_hook_, &movesize_hook_,
                                  &minimize_hook_, &object_hook_,
                                  &cloak_hook_};
  const size_t count = sizeof(hooks) / sizeof(hooks[0]);
  for (size_t index = 0; index < count; ++index) {
    if (*hooks[index] != nullptr) {
      ::UnhookWinEvent(*hooks[index]);
      *hooks[index] = nullptr;
    }
  }

  sink_ = nullptr;
  if (g_detector == this) {
    g_detector = nullptr;
  }
}

void WindowDetector::SetLocationTracking(bool enabled) {
  if (enabled == (location_hook_ != nullptr)) {
    return;
  }

  if (enabled) {
    location_hook_ = ::SetWinEventHook(
        EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, nullptr,
        &WindowDetector::EventProc, 0, 0, kHookFlags);
    LogInfo("location tracking on");
    return;
  }

  ::UnhookWinEvent(location_hook_);
  location_hook_ = nullptr;
  LogInfo("location tracking off");
}

BOOL CALLBACK WindowDetector::EnumProc(HWND window, LPARAM parameter) {
  WindowDetector* detector = reinterpret_cast<WindowDetector*>(parameter);
  if (detector != nullptr && detector->sink_ != nullptr) {
    detector->sink_->OnWindowAppeared(window);
  }
  return TRUE;
}

void WindowDetector::EnumerateExisting() {
  if (sink_ == nullptr) {
    return;
  }
  ::EnumWindows(&WindowDetector::EnumProc, reinterpret_cast<LPARAM>(this));
}

}  // namespace effects
