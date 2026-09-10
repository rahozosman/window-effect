#include <flutter/dart_project.h>
#include <flutter/flutter_view_controller.h>
#include <windows.h>

#include "effects/single_instance.h"
#include "flutter_window.h"
#include "utils.h"

namespace {

// Written into the Run key by the startup switch, so a login starts the engine
// quietly instead of opening the settings window in the user's face.
const char kTrayArgument[] = "--tray";

bool WantsTrayStart(const std::vector<std::string>& arguments) {
  for (size_t index = 0; index < arguments.size(); ++index) {
    if (arguments[index] == kTrayArgument) {
      return true;
    }
  }
  return false;
}

}  // namespace

int APIENTRY wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE prev,
                      _In_ wchar_t *command_line, _In_ int show_command) {
  // Attach to console when present (e.g., 'flutter run') or create a
  // new console when running with a debugger.
  if (!::AttachConsole(ATTACH_PARENT_PROCESS) && ::IsDebuggerPresent()) {
    CreateAndAttachConsole();
  }

  // One engine per session, and before anything else is created. Two engines
  // would each capture the other's modified window as "original", and the
  // second one to restore would put the first one's changes back.
  effects::SingleInstance single_instance;
  if (!single_instance.Claim()) {
    effects::SingleInstance::SignalExistingInstance();
    return EXIT_SUCCESS;
  }

  // Initialize COM, so that it is available for use in the library and/or
  // plugins.
  ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

  flutter::DartProject project(L"data");

  std::vector<std::string> command_line_arguments =
      GetCommandLineArguments();

  const bool start_hidden = WantsTrayStart(command_line_arguments);

  project.set_dart_entrypoint_arguments(std::move(command_line_arguments));

  FlutterWindow window(project, start_hidden);
  Win32Window::Point origin(10, 10);
  Win32Window::Size size(760, 880);
  if (!window.Create(L"Window Effects", origin, size)) {
    return EXIT_FAILURE;
  }
  window.SetQuitOnClose(true);

  ::MSG msg;
  while (::GetMessage(&msg, nullptr, 0, 0)) {
    ::TranslateMessage(&msg);
    ::DispatchMessage(&msg);
  }

  // The tray's Exit only ends the loop: the window is still standing, and with
  // it every window the engine has modified. Destroying it here is what runs
  // FlutterWindow::OnDestroy, which restores them. Doing it from the tray
  // callback instead would destroy the tray while its own menu handler was
  // still running.
  window.Destroy();

  ::CoUninitialize();
  return EXIT_SUCCESS;
}
