#include "window_classifier.h"

#include <wchar.h>

#include "dwm_api.h"

namespace effects {
namespace {

// Below this a window is a transient helper, not something worth styling.
const int kMinimumWidth = 160;
const int kMinimumHeight = 120;

// A window may overhang the monitor by this much and still count as covering
// it exactly.
const int kFullscreenSlack = 2;

const size_t kProcessCacheLimit = 512;

// Shell surfaces, menus, tooltips and the various invisible helper windows the
// desktop is built from. Compared case-insensitively against the exact class
// name — never as a prefix, because real application classes such as
// "Chrome_WidgetWin_1" would be caught by one.
const wchar_t* const kBlockedClasses[] = {
    L"Shell_TrayWnd",
    L"Shell_SecondaryTrayWnd",
    L"Shell_ChargeFlyoutWnd",
    L"Shell_Dim",
    L"Shell_InputSwitchTopLevelWindow",
    L"Progman",
    L"WorkerW",
    L"#32768",                 // menus, including every context menu
    L"tooltips_class32",
    L"SysShadow",
    L"ForegroundStaging",
    L"MultitaskingViewFrame",
    L"TaskSwitcherWnd",
    L"TaskSwitcherOverlayWnd",
    L"TaskListThumbnailWnd",
    L"NotifyIconOverflowWindow",
    L"TopLevelWindowForOverflowXamlIsland",
    L"Windows.UI.Core.CoreWindow",
    L"Xaml_WindowedPopupClass",
    L"XamlExplorerHostIslandWindow",
    L"Windows.UI.Composition.DesktopWindowContentBridge",
    L"DV2ControlHost",         // the classic Start menu host
    L"EdgeUiInputTopWndClass",
    L"SearchPane",
    L"ControlCenterWindow",
    L"ApplicationManager_DesktopShellWindow",
    L"Windows.Internal.Shell.TabProxyWindow",
};

bool IsBlockedClass(const std::wstring& class_name) {
  const size_t count = sizeof(kBlockedClasses) / sizeof(kBlockedClasses[0]);
  for (size_t index = 0; index < count; ++index) {
    if (::_wcsicmp(class_name.c_str(), kBlockedClasses[index]) == 0) {
      return true;
    }
  }
  return false;
}

std::wstring ClassNameOf(HWND window) {
  wchar_t buffer[256] = {};
  const int length =
      ::GetClassNameW(window, buffer, static_cast<int>(ARRAYSIZE(buffer)));
  if (length <= 0) {
    return std::wstring();
  }
  return std::wstring(buffer, static_cast<size_t>(length));
}

// Covers its monitor edge to edge and has no frame of its own: a game, or a
// video player in full screen. Maximised windows are excluded by the frame
// test, since they keep their caption and resize border.
bool IsFullscreen(HWND window, const RECT& frame, LONG_PTR style) {
  if ((style & WS_CAPTION) != 0 || (style & WS_THICKFRAME) != 0) {
    return false;
  }

  HMONITOR monitor = ::MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
  if (monitor == nullptr) {
    return false;
  }

  MONITORINFO info = {};
  info.cbSize = static_cast<DWORD>(sizeof(info));
  if (!::GetMonitorInfoW(monitor, &info)) {
    return false;
  }

  return frame.left <= info.rcMonitor.left + kFullscreenSlack &&
         frame.top <= info.rcMonitor.top + kFullscreenSlack &&
         frame.right >= info.rcMonitor.right - kFullscreenSlack &&
         frame.bottom >= info.rcMonitor.bottom - kFullscreenSlack;
}

std::wstring QueryExecutableName(DWORD process_id) {
  // PROCESS_QUERY_LIMITED_INFORMATION is the least privilege that answers the
  // question, and unlike PROCESS_QUERY_INFORMATION it succeeds against
  // higher-integrity processes.
  HANDLE process = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                 process_id);
  if (process == nullptr) {
    return std::wstring();
  }

  wchar_t buffer[MAX_PATH * 2] = {};
  DWORD size = static_cast<DWORD>(ARRAYSIZE(buffer));
  std::wstring result;
  if (::QueryFullProcessImageNameW(process, 0, buffer, &size) && size > 0) {
    const std::wstring full(buffer, size);
    const size_t separator = full.find_last_of(L"\\/");
    result = (separator == std::wstring::npos) ? full
                                               : full.substr(separator + 1);
    result = ToLowerAscii(result);
  }

  ::CloseHandle(process);
  return result;
}

}  // namespace

const char* VerdictName(WindowVerdict verdict) {
  switch (verdict) {
    case WindowVerdict::kEligible:
      return "eligible";
    case WindowVerdict::kNotAWindow:
      return "not-a-window";
    case WindowVerdict::kNotTopLevel:
      return "not-top-level";
    case WindowVerdict::kChildWindow:
      return "child";
    case WindowVerdict::kInvisible:
      return "invisible";
    case WindowVerdict::kMinimized:
      return "minimized";
    case WindowVerdict::kCloaked:
      return "cloaked";
    case WindowVerdict::kToolWindow:
      return "tool-window";
    case WindowVerdict::kNoActivate:
      return "no-activate";
    case WindowVerdict::kBlockedClass:
      return "blocked-class";
    case WindowVerdict::kTooSmall:
      return "too-small";
    case WindowVerdict::kFullscreen:
      return "fullscreen";
    case WindowVerdict::kOwnProcess:
      return "own-process";
    case WindowVerdict::kProtectedProcess:
      return "protected";
    case WindowVerdict::kExcludedProcess:
      return "excluded";
    case WindowVerdict::kInaccessible:
      return "inaccessible";
  }
  return "unknown";
}

WindowClassifier::WindowClassifier(const ExclusionManager* exclusions)
    : exclusions_(exclusions), own_process_id_(::GetCurrentProcessId()) {}

void WindowClassifier::ClearProcessCache() {
  process_names_.clear();
}

const std::wstring& WindowClassifier::ExecutableForProcess(
    DWORD process_id) const {
  std::unordered_map<DWORD, std::wstring>::const_iterator found =
      process_names_.find(process_id);
  if (found != process_names_.end()) {
    return found->second;
  }

  if (process_names_.size() >= kProcessCacheLimit) {
    process_names_.clear();
  }

  return process_names_
      .emplace(process_id, QueryExecutableName(process_id))
      .first->second;
}

WindowVerdict WindowClassifier::Classify(HWND window, WindowInfo* out) const {
  if (out == nullptr) {
    return WindowVerdict::kNotAWindow;
  }
  *out = WindowInfo();

  // Cheapest checks first: everything below is a plain user32 call that never
  // leaves the process.
  if (window == nullptr || ::IsWindow(window) == FALSE) {
    return WindowVerdict::kNotAWindow;
  }
  out->hwnd = window;

  if (::GetAncestor(window, GA_ROOT) != window) {
    return WindowVerdict::kNotTopLevel;
  }

  const LONG_PTR style = ::GetWindowLongPtrW(window, GWL_STYLE);
  if ((style & WS_CHILD) != 0) {
    return WindowVerdict::kChildWindow;
  }

  if (::IsWindowVisible(window) == FALSE) {
    return WindowVerdict::kInvisible;
  }

  const LONG_PTR ex_style = ::GetWindowLongPtrW(window, GWL_EXSTYLE);
  if ((ex_style & WS_EX_TOOLWINDOW) != 0) {
    return WindowVerdict::kToolWindow;
  }
  // Tooltips, IME candidate lists and notification popups all set this; none
  // of them is a window the user thinks of as an application.
  if ((ex_style & WS_EX_NOACTIVATE) != 0) {
    return WindowVerdict::kNoActivate;
  }

  out->already_layered = (ex_style & WS_EX_LAYERED) != 0;
  out->owned = ::GetWindow(window, GW_OWNER) != nullptr;
  out->maximized = ::IsZoomed(window) != FALSE;

  out->class_name = ClassNameOf(window);
  if (out->class_name.empty() || IsBlockedClass(out->class_name)) {
    return WindowVerdict::kBlockedClass;
  }

  out->minimized = ::IsIconic(window) != FALSE;
  if (out->minimized) {
    // Tracked but dormant: re-evaluated on EVENT_SYSTEM_MINIMIZEEND.
    return WindowVerdict::kMinimized;
  }

  // From here the checks cost a cross-process call, so they come last.
  const DwmApi& dwm = DwmApi::Instance();
  if (dwm.IsCloaked(window)) {
    return WindowVerdict::kCloaked;
  }

  out->frame = dwm.FrameBounds(window);

  RECT outer = {};
  if (::GetWindowRect(window, &outer)) {
    out->frame_inset.left = out->frame.left - outer.left;
    out->frame_inset.top = out->frame.top - outer.top;
    out->frame_inset.right = outer.right - out->frame.right;
    out->frame_inset.bottom = outer.bottom - out->frame.bottom;
  }

  const LONG width = out->frame.right - out->frame.left;
  const LONG height = out->frame.bottom - out->frame.top;
  if (width < kMinimumWidth || height < kMinimumHeight) {
    return WindowVerdict::kTooSmall;
  }

  if (IsFullscreen(window, out->frame, style)) {
    return WindowVerdict::kFullscreen;
  }

  ::GetWindowThreadProcessId(window, &out->process_id);
  if (out->process_id == 0) {
    return WindowVerdict::kInaccessible;
  }
  if (out->process_id == own_process_id_) {
    return WindowVerdict::kOwnProcess;
  }

  out->executable = ExecutableForProcess(out->process_id);
  if (out->executable.empty()) {
    // A process we cannot even name is one we should not be restyling.
    return WindowVerdict::kInaccessible;
  }

  if (exclusions_ != nullptr) {
    if (exclusions_->IsProtected(out->executable)) {
      return WindowVerdict::kProtectedProcess;
    }
    if (exclusions_->IsExcludedByUser(out->executable)) {
      return WindowVerdict::kExcludedProcess;
    }
  }

  return WindowVerdict::kEligible;
}

}  // namespace effects
