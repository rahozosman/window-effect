#ifndef RUNNER_EFFECTS_TRAY_H_
#define RUNNER_EFFECTS_TRAY_H_

#include <windows.h>

#include <functional>
#include <string>

namespace effects {

// The notification-area icon and its menu.
//
// Lives on the platform thread, because it owns a window and shows a menu.
//
// The tray deliberately owns no settings of its own. Choosing a preset or
// toggling effects here sends an intent to the Dart side, which applies it to
// the one settings model, persists it and pushes it back down. Letting the
// tray write settings directly would create a second writer and the two would
// drift apart.
//
// "Pause" is the exception, and is not a setting at all: it suspends the engine
// without touching the saved configuration, so resuming restores exactly what
// the user had.
class TrayIcon {
 public:
  struct Callbacks {
    std::function<void()> open_settings;
    // Requests a change to the persisted master switch.
    std::function<void(bool)> request_enabled;
    // Suspends or resumes without touching the saved configuration.
    std::function<void(bool)> set_paused;
    // Preset ids match the Dart PresetId values: gnome, macos, fluent,
    // premium, minimal.
    std::function<void(const std::string&)> apply_preset;
    std::function<void()> exit_application;
  };

  TrayIcon() = default;
  ~TrayIcon();

  TrayIcon(const TrayIcon&) = delete;
  TrayIcon& operator=(const TrayIcon&) = delete;

  bool Create(const Callbacks& callbacks);
  void Destroy();

  // Keeps the menu's check marks honest. Called whenever settings arrive from
  // the Dart side, so the tray always reflects the real state.
  void SetState(bool effects_enabled, bool paused);

  bool paused() const { return paused_; }

 private:
  static LRESULT CALLBACK WndProc(HWND window, UINT message, WPARAM wparam,
                                  LPARAM lparam);

  bool AddIcon();
  void ShowMenu();
  void HandleCommand(UINT command);
  void UpdateTooltip();

  HWND window_ = nullptr;
  Callbacks callbacks_;

  bool effects_enabled_ = true;
  bool paused_ = false;
  bool icon_added_ = false;

  // Broadcast by the shell when Explorer restarts; the icon has to be added
  // again or it silently disappears for the rest of the session.
  UINT taskbar_created_ = 0;
};

}  // namespace effects

#endif  // RUNNER_EFFECTS_TRAY_H_
