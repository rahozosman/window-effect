#ifndef RUNNER_FLUTTER_WINDOW_H_
#define RUNNER_FLUTTER_WINDOW_H_

#include <flutter/dart_project.h>
#include <flutter/flutter_view_controller.h>

#include <memory>

#include "effects/effects_bridge.h"
#include "win32_window.h"

// A window that does nothing but host a Flutter view.
//
// It is also the only visible part of a utility that is meant to be invisible:
// the engine keeps running while this window is hidden, so closing it hides it
// and only the tray's Exit actually quits.
class FlutterWindow : public Win32Window {
 public:
  // Creates a new FlutterWindow hosting a Flutter view running |project|.
  // |start_hidden| is the --tray launch: the engine starts, the window does
  // not appear until it is asked for.
  FlutterWindow(const flutter::DartProject& project, bool start_hidden);
  virtual ~FlutterWindow();

  // Brings the settings window back from the tray, restoring it first if the
  // user minimised it rather than closing it.
  void ShowSettings();

  // Ends the message loop. Deliberately does not tear anything down: this can
  // run from inside a tray menu callback, and the tray is owned by the bridge
  // that teardown would destroy. wWinMain unwinds instead, in an order that is
  // safe.
  void RequestExit();

 protected:
  // Win32Window:
  bool OnCreate() override;
  void OnDestroy() override;
  LRESULT MessageHandler(HWND window, UINT const message, WPARAM const wparam,
                         LPARAM const lparam) noexcept override;

 private:
  // The project to run.
  flutter::DartProject project_;

  // The Flutter instance hosted by this window.
  std::unique_ptr<flutter::FlutterViewController> flutter_controller_;

  // The window effects engine and its channels. Torn down before the Flutter
  // controller so no managed window is left modified after the UI is gone.
  std::unique_ptr<effects::EffectsBridge> effects_bridge_;

  // Suppresses the first-frame Show for a --tray launch.
  bool start_hidden_ = false;

  // True once Exit has been chosen, which is what turns the next WM_CLOSE from
  // "hide" back into "close".
  bool exiting_ = false;

  // The broadcast a second launch sends, cached so the message handler is not
  // registering a window message on every message it sees.
  UINT show_settings_message_ = 0;
};

#endif  // RUNNER_FLUTTER_WINDOW_H_
