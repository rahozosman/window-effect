#ifndef RUNNER_EFFECTS_WINDOW_CLASSIFIER_H_
#define RUNNER_EFFECTS_WINDOW_CLASSIFIER_H_

#include <windows.h>

#include <string>
#include <unordered_map>

#include "exclusion_manager.h"

namespace effects {

// Why a window was accepted or turned away. Every rejection is named so the
// log can explain a window that "should" have been styled but wasn't.
enum class WindowVerdict {
  kEligible,
  kNotAWindow,
  kNotTopLevel,
  kChildWindow,
  kInvisible,
  kMinimized,
  kCloaked,
  kToolWindow,
  kNoActivate,
  kBlockedClass,
  kTooSmall,
  kFullscreen,
  kOwnProcess,
  kProtectedProcess,
  kExcludedProcess,
  kInaccessible,
};

const char* VerdictName(WindowVerdict verdict);

// Everything the effect managers need to know about a window, gathered once
// during classification so no manager has to query it again.
struct WindowInfo {
  HWND hwnd = nullptr;
  DWORD process_id = 0;

  // Lowercase base name, e.g. "notepad.exe".
  std::wstring executable;
  std::wstring class_name;

  // Extended frame bounds — the visible frame, without the invisible resize
  // border that GetWindowRect includes.
  RECT frame = {};

  // How far the visible frame sits inside GetWindowRect on each edge. Cached
  // so a window being dragged can be followed with a plain GetWindowRect
  // instead of a DWM round trip on every one of a few hundred move events.
  RECT frame_inset = {};

  // Has an owner window: a dialog or a palette rather than a main window.
  bool owned = false;

  // The application already set WS_EX_LAYERED itself. Corners and backdrop are
  // still safe; transparency and dimming are not, because forcing LWA_ALPHA
  // onto a window that calls UpdateLayeredWindow corrupts its rendering.
  bool already_layered = false;

  bool minimized = false;
  bool maximized = false;
};

// Answers one question: is this a normal top-level application window that we
// may safely touch? The rules are the reject list in docs/FEASIBILITY.md §4.
class WindowClassifier {
 public:
  explicit WindowClassifier(const ExclusionManager* exclusions);

  // Fills |out| as far as it gets before reaching a verdict, so the caller can
  // log which executable was rejected even on a rejection path.
  WindowVerdict Classify(HWND window, WindowInfo* out) const;

  // Drops the PID→executable cache. Called when the engine stops, so a later
  // run cannot inherit a name from a PID that has since been reused.
  void ClearProcessCache();

 private:
  const std::wstring& ExecutableForProcess(DWORD process_id) const;

  const ExclusionManager* exclusions_;
  DWORD own_process_id_;

  // PID→name, to keep OpenProcess off the hot path. Windows reuses PIDs, so
  // the cache is dropped wholesale once it grows past a sane size rather than
  // being trusted indefinitely.
  mutable std::unordered_map<DWORD, std::wstring> process_names_;
};

}  // namespace effects

#endif  // RUNNER_EFFECTS_WINDOW_CLASSIFIER_H_
