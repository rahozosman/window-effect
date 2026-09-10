import 'dart:async';

import 'package:flutter/foundation.dart';

import '../models/capabilities.dart';
import '../models/effect_settings.dart';
import '../models/enums.dart';
import '../models/presets.dart';
import '../platform/effects_channel.dart';
import '../services/os_info.dart';
import '../services/settings_store.dart';

/// The only writer of [EffectSettings].
///
/// Notifies listeners synchronously so the live preview tracks a slider drag
/// exactly, while disk writes and engine pushes are debounced separately: the
/// disk stays quiet, and the engine is never asked to re-evaluate every managed
/// window on each pixel of a drag.
class SettingsController extends ChangeNotifier {
  SettingsController({
    SettingsStore? store,
    EffectsChannel? channel,
  })  : _store = store ?? SettingsStore(),
        _channel = channel ?? EffectsChannel();

  static const Duration _diskDebounce = Duration(milliseconds: 400);
  static const Duration _engineDebounce = Duration(milliseconds: 120);

  final SettingsStore _store;
  final EffectsChannel _channel;

  Timer? _diskTimer;
  Timer? _engineTimer;
  StreamSubscription<EngineStatus>? _statusSubscription;
  bool _disposed = false;

  EffectSettings _settings = kDefaultSettings;
  EngineCapabilities _capabilities = const EngineCapabilities.unknown();
  EngineStatus _status = const EngineStatus(state: EngineState.connecting);
  ShellState _shell = const ShellState();
  bool _loaded = false;
  String? _notice;

  EffectSettings get settings => _settings;
  EngineCapabilities get capabilities => _capabilities;
  EngineStatus get status => _status;

  /// True when the settings window itself is showing Mica, so the page can
  /// stop painting an opaque background over it.
  bool get selfBackdrop => _shell.selfBackdrop;

  /// True when there is a tray icon to go back to, which is what makes hiding
  /// the window a safe thing for the native side to do.
  bool get hasTray => _shell.hasTray;

  /// Suspended from the tray. Not a setting: nothing about it is saved.
  bool get isPaused => _status.state == EngineState.paused;

  /// False until the config file has been read; the UI shows a quiet
  /// placeholder rather than flashing defaults it is about to replace.
  bool get isLoaded => _loaded;

  /// A transient message worth showing once — a failed save, a reset config.
  String? get notice => _notice;

  /// Which preset the current settings match, recomputed on every change.
  PresetId get activePreset => detectPreset(_settings);

  /// Where the config file lives, shown in the System section.
  String get configPath => _store.file.path;

  // ------------------------------------------------------------------ startup

  Future<void> initialize() async {
    _capabilities = OsInfo.detect();

    _settings = await _store.load();
    if (_disposed) return;
    if (_store.lastError != null) _notice = _store.lastError;
    _loaded = true;
    notifyListeners();

    final Map<Object?, Object?>? native = await _channel.getCapabilities();
    if (_disposed) return;
    if (native != null) _capabilities = _capabilities.mergeNative(native);

    _status = await _channel.getStatus();
    if (_disposed) return;

    final ShellState? shell = await _channel.getShellState();
    if (_disposed) return;
    if (shell != null) {
      _shell = shell;
      // The Run key outranks the config file. The user can delete that entry
      // from Windows itself, and a switch that still says "on" afterwards is
      // simply wrong.
      if (shell.startWithWindows != _settings.startWithWindows) {
        _settings =
            _settings.copyWith(startWithWindows: shell.startWithWindows);
        _scheduleDiskWrite();
      }
    }
    notifyListeners();

    // Push the loaded config so a restarted engine picks up where it left off.
    unawaited(_channel.applySettings(_settings));

    if (!_channel.nativeMissing) {
      // The tray sends intents, never settings — they land here, in the one
      // writer, and come back down as an ordinary config push.
      _channel.setTrayListener(
        onEnabledRequested: (bool enabled) {
          if (_disposed) return;
          update((EffectSettings s) => s.copyWith(effectsEnabled: enabled));
        },
        onPresetRequested: (String id) {
          if (_disposed) return;
          applyPreset(PresetId.fromId(id));
        },
      );

      _statusSubscription = _channel.statusStream().listen((EngineStatus next) {
        if (_disposed) return;
        _status = next;
        notifyListeners();
      });
    }
  }

  // ------------------------------------------------------------------ mutation

  /// Applies [change] and schedules persistence. Every setter funnels here so
  /// there is exactly one place that debounces, notifies and pushes.
  void update(EffectSettings Function(EffectSettings current) change) {
    final EffectSettings next = change(_settings);
    if (identical(next, _settings)) return;

    final bool enabledChanged = next.effectsEnabled != _settings.effectsEnabled;
    final bool startupChanged = next.startWithWindows != _settings.startWithWindows;

    _settings = next;
    _notice = null;
    notifyListeners();

    _scheduleDiskWrite();
    _scheduleEnginePush();

    if (enabledChanged) {
      unawaited(_channel.setEnabled(enabled: next.effectsEnabled));
    }
    if (startupChanged) {
      unawaited(_applyStartup(next.startWithWindows));
    }
  }

  void applyPreset(PresetId id) {
    final EffectPreset? preset = presetById(id);
    if (preset == null) return;
    update(preset.build);
  }

  void addExclusion(String rawName) {
    final String name = normalizeExecutableName(rawName);
    if (name.isEmpty) return;
    if (_settings.excludedApps.contains(name)) return;
    update((EffectSettings current) => current.copyWith(
          excludedApps: <String>[...current.excludedApps, name]..sort(),
        ));
  }

  void removeExclusion(String name) {
    if (!_settings.excludedApps.contains(name)) return;
    update((EffectSettings current) => current.copyWith(
          excludedApps: current.excludedApps
              .where((String entry) => entry != name)
              .toList(growable: false),
        ));
  }

  /// Resumes an engine the tray paused. Pause itself lives in the tray; this
  /// exists so a user looking at "Paused" in the window is not sent hunting
  /// for the notification area to undo it.
  Future<void> setPaused(bool paused) async {
    await _channel.setPaused(paused: paused);
    // The engine answers through the status stream rather than here, so the
    // strip reports what actually happened rather than what was asked for.
  }

  /// Opens the native executable picker. Returns the chosen name, already
  /// normalised, or null when the user cancels.
  Future<String?> browseForExecutable() async {
    final String? chosen = await _channel.pickExecutable();
    if (chosen == null) return null;
    final String name = normalizeExecutableName(chosen);
    return name.isEmpty ? null : name;
  }

  /// Panic path: turn everything off and restore managed windows now, skipping
  /// the debounce entirely.
  Future<void> disableEverything() async {
    _settings = _settings.copyWith(effectsEnabled: false);
    notifyListeners();
    _diskTimer?.cancel();
    _engineTimer?.cancel();
    await _channel.setEnabled(enabled: false);
    await _channel.restoreAll();
    await _store.save(_settings);
  }

  /// Turns `chrome.exe`, `Chrome`, or a full path into a comparable key.
  static String normalizeExecutableName(String raw) {
    String name = raw.trim();
    if (name.isEmpty) return '';

    // Accept a pasted full path and keep only the file name.
    final int separator = name.lastIndexOf(RegExp(r'[\\/]'));
    if (separator >= 0) name = name.substring(separator + 1);

    name = name.trim().toLowerCase();
    if (name.isEmpty) return '';
    if (!name.endsWith('.exe')) name = '$name.exe';

    // Reject anything that still looks like a path or a wildcard.
    if (RegExp(r'[<>:"|?*]').hasMatch(name)) return '';
    if (name == '.exe') return '';
    return name;
  }

  // ------------------------------------------------------------------ plumbing

  void _scheduleDiskWrite() {
    _diskTimer?.cancel();
    _diskTimer = Timer(_diskDebounce, () async {
      final String? error = await _store.save(_settings);
      if (_disposed || error == null) return;
      _notice = error;
      notifyListeners();
    });
  }

  void _scheduleEnginePush() {
    if (_channel.nativeMissing) return;
    _engineTimer?.cancel();
    _engineTimer = Timer(_engineDebounce, () {
      unawaited(_channel.applySettings(_settings));
    });
  }

  Future<void> _applyStartup(bool enabled) async {
    final String? error = await _channel.setStartWithWindows(enabled: enabled);
    if (_disposed || error == null) return;
    _notice = error;
    // Reflect the failure rather than showing a switch that lies, and re-arm
    // the disk write so the reverted value is what actually gets persisted.
    _settings = _settings.copyWith(startWithWindows: !enabled);
    notifyListeners();
    _scheduleDiskWrite();
  }

  @override
  void dispose() {
    _disposed = true;
    _diskTimer?.cancel();
    _engineTimer?.cancel();
    _channel.clearTrayListener();
    unawaited(_statusSubscription?.cancel());
    super.dispose();
  }
}
