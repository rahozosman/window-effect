#ifndef RUNNER_EFFECTS_EFFECTS_BRIDGE_H_
#define RUNNER_EFFECTS_EFFECTS_BRIDGE_H_

#include <flutter/binary_messenger.h>
#include <flutter/encodable_value.h>
#include <flutter/event_channel.h>
#include <flutter/event_sink.h>
#include <flutter/method_call.h>
#include <flutter/method_channel.h>
#include <flutter/method_result.h>

#include <functional>
#include <memory>

#include "dispatcher.h"
#include "engine.h"
#include "tray.h"

namespace effects {

// Connects the Flutter settings UI to the effects engine.
//
// Lives on the platform thread. Everything arriving from the engine thread
// goes through the dispatcher first, because MethodChannel and EventSink may
// only be touched here.
class EffectsBridge {
 public:
  // Everything the bridge needs from the window that hosts the settings UI.
  // Supplied by the window itself so the bridge never has to know what kind of
  // window it is, and so the tray can ask for things the engine has no
  // business doing.
  struct ShellDelegate {
    std::function<void()> show_settings;
    std::function<void()> request_exit;
    // Owner for modal dialogs. May return nullptr once the window is gone.
    std::function<HWND()> settings_window;
  };

  EffectsBridge();
  ~EffectsBridge();

  EffectsBridge(const EffectsBridge&) = delete;
  EffectsBridge& operator=(const EffectsBridge&) = delete;

  // Both must be set before Register, which is when the tray is created.
  void SetShellDelegate(const ShellDelegate& delegate);
  void SetSelfBackdropApplied(bool applied);

  // Wires up both channels, creates the tray and starts the engine. Platform
  // thread only.
  void Register(flutter::BinaryMessenger* messenger);

  // False when Windows refused the notification icon. The window uses this to
  // decide whether closing may hide it — hiding into a tray that is not there
  // would leave the user no way back.
  bool has_tray() const { return has_tray_; }

  // Stops the engine, restoring every managed window, then tears the channels
  // down. Safe to call more than once.
  void Shutdown();

 private:
  void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue>& call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);

  // Called on the ENGINE thread; hops to the platform thread before touching
  // the event sink.
  void OnEngineStatus(const EngineStatus& status);
  void DeliverStatus(const EngineStatus& status);

  void CreateTray();
  void SyncTray();
  // Sends an intent up to the Dart side, which owns the settings model. The
  // tray never writes settings itself; a second writer would drift.
  void NotifyDart(const char* method, const flutter::EncodableMap& arguments);

  Dispatcher dispatcher_;
  EffectsEngine engine_;
  TrayIcon tray_;
  ShellDelegate shell_;

  std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>>
      method_channel_;
  std::unique_ptr<flutter::EventChannel<flutter::EncodableValue>>
      event_channel_;
  std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> event_sink_;

  // Identical statuses are dropped rather than pushed, which is what keeps a
  // burst of window events from becoming a burst of channel traffic.
  EngineStatus last_sent_;
  bool has_last_sent_ = false;

  // What the tray's check marks currently claim. Kept here rather than read
  // back from the engine, because a pause posted a moment ago has not
  // necessarily been applied yet.
  bool tray_enabled_ = true;
  bool tray_paused_ = false;
  bool has_tray_ = false;

  bool self_backdrop_ = false;
  bool registered_ = false;
};

}  // namespace effects

#endif  // RUNNER_EFFECTS_EFFECTS_BRIDGE_H_
