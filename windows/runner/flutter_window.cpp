#include "flutter_window.h"

#include <optional>

#include "effects/log.h"
#include "effects/self_backdrop.h"
#include "effects/single_instance.h"
#include "flutter/generated_plugin_registrant.h"

FlutterWindow::FlutterWindow(const flutter::DartProject& project,
                             bool start_hidden)
    : project_(project), start_hidden_(start_hidden) {}

FlutterWindow::~FlutterWindow() {}

bool FlutterWindow::OnCreate() {
  if (!Win32Window::OnCreate()) {
    return false;
  }

  show_settings_message_ = effects::SingleInstance::ShowSettingsMessage();

  // Before the first thing that logs, not with the bridge: the capability
  // probe and the backdrop attempt below both happen out here, and their two
  // lines are exactly what a "why is this effect missing?" question needs.
  // Opening twice is a no-op.
  effects::LogOpen();

  // Before the view exists, so the first frame is already composited against
  // the backdrop rather than flashing an opaque window first.
  const bool self_backdrop = effects::ApplySelfBackdrop(GetHandle());

  RECT frame = GetClientArea();

  // The size here must match the window dimensions to avoid unnecessary surface
  // creation / destruction in the startup path.
  flutter_controller_ = std::make_unique<flutter::FlutterViewController>(
      frame.right - frame.left, frame.bottom - frame.top, project_);
  // Ensure that basic setup of the controller was successful.
  if (!flutter_controller_->engine() || !flutter_controller_->view()) {
    return false;
  }
  RegisterPlugins(flutter_controller_->engine());

  effects_bridge_ = std::make_unique<effects::EffectsBridge>();

  effects::EffectsBridge::ShellDelegate delegate;
  delegate.show_settings = [this]() { ShowSettings(); };
  delegate.request_exit = [this]() { RequestExit(); };
  delegate.settings_window = [this]() { return GetHandle(); };
  effects_bridge_->SetShellDelegate(delegate);
  effects_bridge_->SetSelfBackdropApplied(self_backdrop);

  // Delegate first: Register creates the tray, and a tray menu is clickable
  // the instant it exists.
  effects_bridge_->Register(flutter_controller_->engine()->messenger());

  SetChildContent(flutter_controller_->view()->GetNativeWindow());

  flutter_controller_->engine()->SetNextFrameCallback([this]() {
    // A --tray launch has a window, a running engine and nothing on screen.
    // The tray icon is the only thing that brings it up.
    if (!start_hidden_) {
      this->Show();
    }
  });

  // Flutter can complete the first frame before the "show window" callback is
  // registered. The following call ensures a frame is pending to ensure the
  // window is shown. It is a no-op if the first frame hasn't completed yet.
  flutter_controller_->ForceRedraw();

  return true;
}

void FlutterWindow::ShowSettings() {
  HWND handle = GetHandle();
  if (handle == nullptr) {
    return;
  }

  if (::IsIconic(handle) != FALSE) {
    ::ShowWindow(handle, SW_RESTORE);
  } else {
    ::ShowWindow(handle, SW_SHOW);
  }
  ::SetForegroundWindow(handle);

  // The window may have spent a long time hidden, in which case Flutter has
  // not produced a frame for it in just as long.
  if (flutter_controller_) {
    flutter_controller_->ForceRedraw();
  }
}

void FlutterWindow::RequestExit() {
  if (exiting_) {
    return;
  }
  exiting_ = true;
  ::PostQuitMessage(0);
}

void FlutterWindow::OnDestroy() {
  // Restores every managed window before the engine goes away. Nothing below
  // this line may fail in a way that leaves a window modified.
  if (effects_bridge_) {
    effects_bridge_->Shutdown();
    effects_bridge_ = nullptr;
  }

  if (flutter_controller_) {
    flutter_controller_ = nullptr;
  }

  Win32Window::OnDestroy();
}

LRESULT
FlutterWindow::MessageHandler(HWND hwnd, UINT const message,
                              WPARAM const wparam,
                              LPARAM const lparam) noexcept {
  // Give Flutter, including plugins, an opportunity to handle window messages.
  if (flutter_controller_) {
    std::optional<LRESULT> result =
        flutter_controller_->HandleTopLevelWindowProc(hwnd, message, wparam,
                                                      lparam);
    if (result) {
      return *result;
    }
  }

  // A second launch broadcasts this instead of starting a second engine.
  if (show_settings_message_ != 0 && message == show_settings_message_) {
    ShowSettings();
    return 0;
  }

  switch (message) {
    case WM_CLOSE:
      // Closing puts the utility back in the tray with the engine still
      // running. Quitting is a deliberate choice in the tray menu, not a side
      // effect of the X — but only while there is a tray to go back to.
      if (!exiting_ && effects_bridge_ && effects_bridge_->has_tray()) {
        ::ShowWindow(hwnd, SW_HIDE);
        return 0;
      }
      break;

    case WM_ENDSESSION:
      // The session is ending and the process is about to be terminated
      // whether it cooperates or not. This is the last moment at which every
      // modified window can be handed back, so it happens here rather than in
      // a destructor that may never run.
      if (wparam != 0 && effects_bridge_) {
        effects_bridge_->Shutdown();
        effects_bridge_ = nullptr;
      }
      break;

    case WM_FONTCHANGE:
      flutter_controller_->engine()->ReloadSystemFonts();
      break;
  }

  return Win32Window::MessageHandler(hwnd, message, wparam, lparam);
}
