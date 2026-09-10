#include "tray.h"

#include <shellapi.h>
#include <wchar.h>

#include "../resource.h"
#include "log.h"

namespace effects {
namespace {

const wchar_t kTrayClassName[] = L"WindowEffectsTray";
const UINT kTrayCallbackMessage = WM_APP + 3;
const UINT kTrayIconId = 1;

// Menu command identifiers.
const UINT kCommandToggleEnabled = 100;
const UINT kCommandTogglePause = 101;
const UINT kCommandOpenSettings = 102;
const UINT kCommandExit = 103;
const UINT kCommandPresetFirst = 200;

struct PresetEntry {
  const char* id;
  const wchar_t* label;
};

const PresetEntry kPresets[] = {
    {"gnome", L"GNOME"},   {"macos", L"macOS"},     {"fluent", L"Fluent"},
    {"premium", L"Premium"}, {"minimal", L"Minimal"},
};

const size_t kPresetCount = sizeof(kPresets) / sizeof(kPresets[0]);

}  // namespace

TrayIcon::~TrayIcon() {
  Destroy();
}

bool TrayIcon::Create(const Callbacks& callbacks) {
  if (window_ != nullptr) {
    return true;
  }
  callbacks_ = callbacks;

  HINSTANCE instance = ::GetModuleHandleW(nullptr);

  static ATOM class_atom = 0;
  if (class_atom == 0) {
    WNDCLASSEXW window_class = {};
    window_class.cbSize = static_cast<UINT>(sizeof(window_class));
    window_class.lpfnWndProc = &TrayIcon::WndProc;
    window_class.hInstance = instance;
    window_class.lpszClassName = kTrayClassName;
    class_atom = ::RegisterClassExW(&window_class);
    if (class_atom == 0) {
      LogError("could not register the tray window class");
      return false;
    }
  }

  // A real top-level window rather than a message-only one: HWND_MESSAGE
  // windows do not receive broadcasts, and TaskbarCreated is a broadcast.
  // It is never shown.
  window_ = ::CreateWindowExW(WS_EX_TOOLWINDOW, kTrayClassName, L"", WS_POPUP,
                              0, 0, 0, 0, nullptr, nullptr, instance, nullptr);
  if (window_ == nullptr) {
    LogError("could not create the tray window");
    return false;
  }
  ::SetWindowLongPtrW(window_, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(this));

  taskbar_created_ = ::RegisterWindowMessageW(L"TaskbarCreated");

  if (!AddIcon()) {
    LogError("could not add the tray icon");
    return false;
  }
  return true;
}

void TrayIcon::Destroy() {
  if (icon_added_ && window_ != nullptr) {
    NOTIFYICONDATAW data = {};
    data.cbSize = static_cast<DWORD>(sizeof(data));
    data.hWnd = window_;
    data.uID = kTrayIconId;
    ::Shell_NotifyIconW(NIM_DELETE, &data);
    icon_added_ = false;
  }

  if (window_ != nullptr) {
    ::SetWindowLongPtrW(window_, GWLP_USERDATA, 0);
    ::DestroyWindow(window_);
    window_ = nullptr;
  }
}

bool TrayIcon::AddIcon() {
  if (window_ == nullptr) {
    return false;
  }

  HINSTANCE instance = ::GetModuleHandleW(nullptr);
  HICON icon = static_cast<HICON>(::LoadImageW(
      instance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
      ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON),
      LR_DEFAULTCOLOR));
  if (icon == nullptr) {
    icon = ::LoadIconW(nullptr, IDI_APPLICATION);
  }

  NOTIFYICONDATAW data = {};
  data.cbSize = static_cast<DWORD>(sizeof(data));
  data.hWnd = window_;
  data.uID = kTrayIconId;
  data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  data.uCallbackMessage = kTrayCallbackMessage;
  data.hIcon = icon;
  ::wcscpy_s(data.szTip, L"Window Effects");

  icon_added_ = ::Shell_NotifyIconW(NIM_ADD, &data) != FALSE;
  if (icon_added_) {
    UpdateTooltip();
  }
  return icon_added_;
}

void TrayIcon::SetState(bool effects_enabled, bool paused) {
  if (effects_enabled_ == effects_enabled && paused_ == paused) {
    return;
  }
  effects_enabled_ = effects_enabled;
  paused_ = paused;
  UpdateTooltip();
}

void TrayIcon::UpdateTooltip() {
  if (!icon_added_ || window_ == nullptr) {
    return;
  }

  const wchar_t* text = L"Window Effects — active";
  if (!effects_enabled_) {
    text = L"Window Effects — off";
  } else if (paused_) {
    text = L"Window Effects — paused";
  }

  NOTIFYICONDATAW data = {};
  data.cbSize = static_cast<DWORD>(sizeof(data));
  data.hWnd = window_;
  data.uID = kTrayIconId;
  data.uFlags = NIF_TIP;
  ::wcscpy_s(data.szTip, text);
  ::Shell_NotifyIconW(NIM_MODIFY, &data);
}

void TrayIcon::ShowMenu() {
  HMENU menu = ::CreatePopupMenu();
  if (menu == nullptr) {
    return;
  }

  ::AppendMenuW(menu, MF_STRING | MF_DISABLED, 0, L"Window Effects");
  ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  ::AppendMenuW(menu,
                MF_STRING | (effects_enabled_ ? MF_CHECKED : MF_UNCHECKED),
                kCommandToggleEnabled, L"Effects Enabled");
  ::AppendMenuW(menu, MF_STRING | (paused_ ? MF_CHECKED : MF_UNCHECKED),
                kCommandTogglePause, L"Pause Effects");
  ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  ::AppendMenuW(menu, MF_STRING, kCommandOpenSettings, L"Open Settings");

  HMENU presets = ::CreatePopupMenu();
  if (presets != nullptr) {
    for (size_t index = 0; index < kPresetCount; ++index) {
      ::AppendMenuW(presets, MF_STRING,
                    kCommandPresetFirst + static_cast<UINT>(index),
                    kPresets[index].label);
    }
    ::AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(presets),
                  L"Presets");
  }

  ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  ::AppendMenuW(menu, MF_STRING, kCommandExit, L"Exit");

  POINT cursor = {};
  ::GetCursorPos(&cursor);

  // Without this the menu refuses to dismiss when the user clicks elsewhere —
  // the long-standing shell requirement for tray menus.
  ::SetForegroundWindow(window_);

  const int command = ::TrackPopupMenu(
      menu, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, cursor.x, cursor.y,
      0, window_, nullptr);

  ::PostMessageW(window_, WM_NULL, 0, 0);
  ::DestroyMenu(menu);

  if (command > 0) {
    HandleCommand(static_cast<UINT>(command));
  }
}

void TrayIcon::HandleCommand(UINT command) {
  if (command >= kCommandPresetFirst &&
      command < kCommandPresetFirst + static_cast<UINT>(kPresetCount)) {
    const size_t index = static_cast<size_t>(command - kCommandPresetFirst);
    if (callbacks_.apply_preset) {
      callbacks_.apply_preset(std::string(kPresets[index].id));
    }
    return;
  }

  switch (command) {
    case kCommandToggleEnabled:
      if (callbacks_.request_enabled) {
        callbacks_.request_enabled(!effects_enabled_);
      }
      break;
    case kCommandTogglePause:
      paused_ = !paused_;
      UpdateTooltip();
      if (callbacks_.set_paused) {
        callbacks_.set_paused(paused_);
      }
      break;
    case kCommandOpenSettings:
      if (callbacks_.open_settings) {
        callbacks_.open_settings();
      }
      break;
    case kCommandExit:
      if (callbacks_.exit_application) {
        callbacks_.exit_application();
      }
      break;
    default:
      break;
  }
}

LRESULT CALLBACK TrayIcon::WndProc(HWND window, UINT message, WPARAM wparam,
                                   LPARAM lparam) {
  TrayIcon* tray = reinterpret_cast<TrayIcon*>(
      ::GetWindowLongPtrW(window, GWLP_USERDATA));

  if (tray != nullptr) {
    if (message == kTrayCallbackMessage) {
      switch (static_cast<UINT>(lparam)) {
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
          if (tray->callbacks_.open_settings) {
            tray->callbacks_.open_settings();
          }
          break;
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
          tray->ShowMenu();
          break;
        default:
          break;
      }
      return 0;
    }

    if (tray->taskbar_created_ != 0 && message == tray->taskbar_created_) {
      tray->icon_added_ = false;
      tray->AddIcon();
      return 0;
    }
  }

  return ::DefWindowProcW(window, message, wparam, lparam);
}

}  // namespace effects
