#include "effects_bridge.h"

#include <flutter/event_stream_handler_functions.h>
#include <flutter/standard_method_codec.h>
#include <windows.h>

#include <string>
#include <variant>
#include <vector>

#include "exe_picker.h"
#include "log.h"
#include "os_version.h"
#include "startup.h"

namespace effects {
namespace {

const char kMethodChannelName[] = "window_effect/engine";
const char kEventChannelName[] = "window_effect/engine_events";

using flutter::EncodableMap;
using flutter::EncodableValue;

std::wstring Widen(const std::string& value) {
  if (value.empty()) {
    return std::wstring();
  }
  const int length = ::MultiByteToWideChar(
      CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
  if (length <= 0) {
    return std::wstring();
  }
  std::wstring result(static_cast<size_t>(length), L'\0');
  ::MultiByteToWideChar(CP_UTF8, 0, value.c_str(),
                        static_cast<int>(value.size()), &result[0], length);
  return result;
}

// The mirror of Widen. Narrow() in log.h flattens anything outside ASCII to
// '?', which is fine for a log line and not fine for a name that has to match
// a running process.
std::string Utf8From(const std::wstring& value) {
  if (value.empty()) {
    return std::string();
  }
  const int length = ::WideCharToMultiByte(
      CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0,
      nullptr, nullptr);
  if (length <= 0) {
    return std::string();
  }
  std::string result(static_cast<size_t>(length), '\0');
  ::WideCharToMultiByte(CP_UTF8, 0, value.c_str(),
                        static_cast<int>(value.size()), &result[0], length,
                        nullptr, nullptr);
  return result;
}

const EncodableValue* Find(const EncodableMap& map, const char* key) {
  EncodableMap::const_iterator found = map.find(EncodableValue(key));
  if (found == map.end()) {
    return nullptr;
  }
  return &found->second;
}

bool ReadBool(const EncodableMap& map, const char* key, bool fallback) {
  const EncodableValue* value = Find(map, key);
  if (value == nullptr) {
    return fallback;
  }
  const bool* result = std::get_if<bool>(value);
  return result != nullptr ? *result : fallback;
}

// Dart sends small integers as int32 and large ones as int64, so both have to
// be accepted for the same field.
int ReadInt(const EncodableMap& map, const char* key, int fallback) {
  const EncodableValue* value = Find(map, key);
  if (value == nullptr) {
    return fallback;
  }
  if (const int32_t* narrow = std::get_if<int32_t>(value)) {
    return static_cast<int>(*narrow);
  }
  if (const int64_t* wide = std::get_if<int64_t>(value)) {
    return static_cast<int>(*wide);
  }
  return fallback;
}

std::string ReadString(const EncodableMap& map, const char* key) {
  const EncodableValue* value = Find(map, key);
  if (value == nullptr) {
    return std::string();
  }
  const std::string* result = std::get_if<std::string>(value);
  return result != nullptr ? *result : std::string();
}

std::vector<std::wstring> ReadStringList(const EncodableMap& map,
                                         const char* key) {
  std::vector<std::wstring> result;
  const EncodableValue* value = Find(map, key);
  if (value == nullptr) {
    return result;
  }
  const flutter::EncodableList* list = std::get_if<flutter::EncodableList>(value);
  if (list == nullptr) {
    return result;
  }
  result.reserve(list->size());
  for (size_t index = 0; index < list->size(); ++index) {
    const std::string* entry = std::get_if<std::string>(&(*list)[index]);
    if (entry != nullptr && !entry->empty()) {
      result.push_back(Widen(*entry));
    }
  }
  return result;
}

Config ConfigFromMap(const EncodableMap& map) {
  Config config;
  config.effects_enabled = ReadBool(map, "effectsEnabled", true);

  config.corners_enabled = ReadBool(map, "cornersEnabled", true);
  config.corner_mode = CornerModeFromId(ReadString(map, "cornerMode"));
  config.corner_radius = ReadInt(map, "cornerRadius", 12);

  config.shadow_enabled = ReadBool(map, "shadowEnabled", true);
  config.shadow_strength = ReadInt(map, "shadowStrength", 35);
  config.shadow_blur = ReadInt(map, "shadowBlur", 24);
  config.shadow_offset = ReadInt(map, "shadowOffset", 6);

  config.transparency_enabled = ReadBool(map, "transparencyEnabled", false);
  config.opacity = ReadInt(map, "opacity", 95);

  config.blur_enabled = ReadBool(map, "blurEnabled", false);
  config.blur_preset = BlurPresetFromId(ReadString(map, "blurPreset"));

  config.backdrop = BackdropModeFromId(ReadString(map, "backdrop"));

  config.active_emphasis = ReadBool(map, "activeEmphasis", true);
  config.active_border = ReadBool(map, "activeBorder", false);
  config.shadow_difference =
      ShadowDifferenceFromId(ReadString(map, "shadowDifference"));

  config.dim_inactive = ReadBool(map, "dimInactive", false);
  config.dim_amount = ReadInt(map, "dimAmount", 8);

  config.animations_enabled = ReadBool(map, "animationsEnabled", true);
  config.animation_style = AnimationStyleFromId(ReadString(map, "animationStyle"));
  config.animation_duration = ReadInt(map, "animationDuration", 600);
  config.animation_scale = ReadInt(map, "animationScale", 91);
  config.animation_lift = ReadInt(map, "animationLift", 14);
  config.animate_minimize = ReadBool(map, "animateMinimize", true);

  config.excluded_apps = ReadStringList(map, "excludedApps");

  ClampConfig(&config);
  return config;
}

EncodableMap StatusToMap(const EngineStatus& status) {
  return EncodableMap{
      {EncodableValue("state"), EncodableValue(EngineStateName(status.state))},
      {EncodableValue("managedWindows"),
       EncodableValue(status.managed_windows)},
      {EncodableValue("message"), EncodableValue(status.message)},
  };
}

EncodableMap CapabilitiesToMap() {
  const Capabilities& c = GetCapabilities();
  return EncodableMap{
      {EncodableValue("buildNumber"),
       EncodableValue(static_cast<int32_t>(c.build_number))},
      {EncodableValue("systemCorners"), EncodableValue(c.system_corners)},
      {EncodableValue("preciseCorners"), EncodableValue(c.precise_corners)},
      {EncodableValue("customShadow"), EncodableValue(c.custom_shadow)},
      {EncodableValue("transparency"), EncodableValue(c.transparency)},
      {EncodableValue("blurBehind"), EncodableValue(c.blur_behind)},
      {EncodableValue("systemBackdrop"), EncodableValue(c.system_backdrop)},
      {EncodableValue("legacyMica"), EncodableValue(c.legacy_mica)},
      {EncodableValue("borderColor"), EncodableValue(c.border_color)},
  };
}

}  // namespace

EffectsBridge::EffectsBridge() = default;

EffectsBridge::~EffectsBridge() {
  Shutdown();
}

void EffectsBridge::SetShellDelegate(const ShellDelegate& delegate) {
  shell_ = delegate;
}

void EffectsBridge::SetSelfBackdropApplied(bool applied) {
  self_backdrop_ = applied;
}

void EffectsBridge::Register(flutter::BinaryMessenger* messenger) {
  if (registered_ || messenger == nullptr) {
    return;
  }
  registered_ = true;

  LogOpen();
  LogInfo("bridge registering");

  method_channel_ =
      std::make_unique<flutter::MethodChannel<EncodableValue>>(
          messenger, kMethodChannelName,
          &flutter::StandardMethodCodec::GetInstance());
  method_channel_->SetMethodCallHandler(
      [this](const flutter::MethodCall<EncodableValue>& call,
             std::unique_ptr<flutter::MethodResult<EncodableValue>> result) {
        HandleMethodCall(call, std::move(result));
      });

  event_channel_ = std::make_unique<flutter::EventChannel<EncodableValue>>(
      messenger, kEventChannelName,
      &flutter::StandardMethodCodec::GetInstance());
  event_channel_->SetStreamHandler(
      std::make_unique<flutter::StreamHandlerFunctions<EncodableValue>>(
          [this](const EncodableValue* arguments,
                 std::unique_ptr<flutter::EventSink<EncodableValue>>&& events)
              -> std::unique_ptr<flutter::StreamHandlerError<EncodableValue>> {
            (void)arguments;
            event_sink_ = std::move(events);
            has_last_sent_ = false;
            // Hand the listener the current state immediately rather than
            // leaving the UI blank until the next window event.
            DeliverStatus(engine_.GetStatus());
            return nullptr;
          },
          [this](const EncodableValue* arguments)
              -> std::unique_ptr<flutter::StreamHandlerError<EncodableValue>> {
            (void)arguments;
            event_sink_.reset();
            return nullptr;
          }));

  // The dispatcher has to be able to accept work before the engine can produce
  // any, so it starts first.
  dispatcher_.Start();

  engine_.SetStatusCallback(
      [this](const EngineStatus& status) { OnEngineStatus(status); });
  engine_.Start();

  // Last, because every menu item it offers needs something that is now
  // running.
  CreateTray();
}

void EffectsBridge::CreateTray() {
  TrayIcon::Callbacks callbacks;
  callbacks.open_settings = [this]() {
    if (shell_.show_settings) {
      shell_.show_settings();
    }
  };
  // The master switch is a persisted setting, so it goes up to the one writer
  // and comes back down as a normal config push.
  callbacks.request_enabled = [this](bool enabled) {
    NotifyDart("trayEnabled",
               EncodableMap{{EncodableValue("enabled"),
                             EncodableValue(enabled)}});
  };
  // Pause is not a setting, so it goes straight to the engine and never
  // touches the config file.
  callbacks.set_paused = [this](bool paused) {
    tray_paused_ = paused;
    engine_.SetPaused(paused);
  };
  callbacks.apply_preset = [this](const std::string& id) {
    NotifyDart("trayPreset",
               EncodableMap{{EncodableValue("preset"), EncodableValue(id)}});
  };
  callbacks.exit_application = [this]() {
    if (shell_.request_exit) {
      shell_.request_exit();
    }
  };

  has_tray_ = tray_.Create(callbacks);
  if (!has_tray_) {
    // Not fatal: the settings window is still there, and closing it will quit
    // rather than hide, because has_tray() says there is nowhere to hide to.
    LogWarn("running without a notification icon");
    return;
  }
  SyncTray();
}

void EffectsBridge::SyncTray() {
  if (!has_tray_) {
    return;
  }
  tray_.SetState(tray_enabled_, tray_paused_);
}

void EffectsBridge::NotifyDart(const char* method,
                               const EncodableMap& arguments) {
  if (!method_channel_) {
    return;
  }
  method_channel_->InvokeMethod(
      method, std::make_unique<EncodableValue>(arguments));
}

void EffectsBridge::Shutdown() {
  if (!registered_) {
    return;
  }
  registered_ = false;

  LogInfo("bridge shutting down");

  // The icon goes first so it disappears the moment the user chooses Exit,
  // rather than sitting there through the restore pass.
  tray_.Destroy();
  has_tray_ = false;

  // Order matters: stopping the engine restores every managed window and joins
  // its thread, so no further status can be produced. Only then is it safe to
  // drop the dispatcher's queue and the channels.
  engine_.Stop();
  dispatcher_.Stop();

  if (method_channel_) {
    method_channel_->SetMethodCallHandler(nullptr);
    method_channel_.reset();
  }
  if (event_channel_) {
    event_channel_->SetStreamHandler(nullptr);
    event_channel_.reset();
  }
  event_sink_.reset();

  LogClose();
}

void EffectsBridge::OnEngineStatus(const EngineStatus& status) {
  // Called on the engine thread. Copy the status into the task so the engine
  // thread never has to outlive the hop.
  EngineStatus copy = status;
  dispatcher_.Post([this, copy]() { DeliverStatus(copy); });
}

void EffectsBridge::DeliverStatus(const EngineStatus& status) {
  if (!event_sink_) {
    return;
  }
  if (has_last_sent_ && last_sent_ == status) {
    return;
  }
  last_sent_ = status;
  has_last_sent_ = true;
  event_sink_->Success(EncodableValue(StatusToMap(status)));
}

void EffectsBridge::HandleMethodCall(
    const flutter::MethodCall<EncodableValue>& call,
    std::unique_ptr<flutter::MethodResult<EncodableValue>> result) {
  const std::string& method = call.method_name();

  if (method == "getCapabilities") {
    result->Success(EncodableValue(CapabilitiesToMap()));
    return;
  }

  if (method == "getStatus") {
    result->Success(EncodableValue(StatusToMap(engine_.GetStatus())));
    return;
  }

  if (method == "applySettings") {
    const EncodableMap* arguments =
        std::get_if<EncodableMap>(call.arguments());
    if (arguments == nullptr) {
      result->Error("bad-arguments", "applySettings expects a settings map.");
      return;
    }
    const Config config = ConfigFromMap(*arguments);
    engine_.UpdateConfig(config);
    // Every settings push is also the tray's chance to stop lying about the
    // master switch.
    tray_enabled_ = config.effects_enabled;
    SyncTray();
    result->Success();
    return;
  }

  if (method == "setEnabled") {
    const EncodableMap* arguments =
        std::get_if<EncodableMap>(call.arguments());
    const bool enabled =
        arguments != nullptr ? ReadBool(*arguments, "enabled", true) : true;
    engine_.SetEnabled(enabled);
    tray_enabled_ = enabled;
    SyncTray();
    result->Success();
    return;
  }

  if (method == "setPaused") {
    const EncodableMap* arguments =
        std::get_if<EncodableMap>(call.arguments());
    const bool paused =
        arguments != nullptr ? ReadBool(*arguments, "paused", false) : false;
    engine_.SetPaused(paused);
    tray_paused_ = paused;
    SyncTray();
    result->Success();
    return;
  }

  if (method == "restoreAll") {
    engine_.RestoreAll();
    result->Success();
    return;
  }

  if (method == "setStartWithWindows") {
    const EncodableMap* arguments =
        std::get_if<EncodableMap>(call.arguments());
    const bool enabled =
        arguments != nullptr ? ReadBool(*arguments, "enabled", false) : false;
    // The registry's answer, not ours. A switch that says "on" when the Run
    // key says otherwise is worse than an error.
    const std::string error = Startup::SetEnabled(enabled);
    if (error.empty()) {
      result->Success(EncodableValue(EncodableMap{}));
    } else {
      result->Success(EncodableValue(EncodableMap{
          {EncodableValue("error"), EncodableValue(error)}}));
    }
    return;
  }

  if (method == "getShellState") {
    result->Success(EncodableValue(EncodableMap{
        {EncodableValue("startWithWindows"),
         EncodableValue(Startup::IsEnabled())},
        // The engine's own answer rather than the tray's, because the tray's
        // is what was asked for and this is what actually happened.
        {EncodableValue("paused"), EncodableValue(engine_.IsPaused())},
        {EncodableValue("tray"), EncodableValue(has_tray_)},
        {EncodableValue("selfBackdrop"), EncodableValue(self_backdrop_)},
    }));
    return;
  }

  if (method == "pickExecutable") {
    HWND owner = shell_.settings_window ? shell_.settings_window() : nullptr;
    const std::wstring chosen = PickExecutable(owner);
    if (chosen.empty()) {
      // Cancelled. Null rather than an empty string, so the caller can tell
      // "no choice" from "chose something unusable".
      result->Success();
      return;
    }
    result->Success(EncodableValue(Utf8From(chosen)));
    return;
  }

  result->NotImplemented();
}

}  // namespace effects
