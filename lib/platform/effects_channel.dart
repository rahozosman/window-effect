import 'dart:async';

import 'package:flutter/services.dart';

import '../models/effect_settings.dart';

/// Whether the native effects engine is reachable.
enum EngineState {
  /// Not queried yet.
  connecting,

  /// Hooks installed and windows being tracked, but nothing is being applied
  /// — usually because the master switch is off or nothing is eligible yet.
  observing,

  /// Engine responded and is applying effects.
  running,

  /// Engine responded but effects are paused (master switch or tray).
  paused,

  /// No native half is registered. Expected until Phase 2 lands; afterwards it
  /// means the engine failed to start.
  unavailable,

  /// Engine reported a fault and stopped applying effects.
  faulted,
}

/// A snapshot of what the engine is doing, for the status strip.
class EngineStatus {
  const EngineStatus({
    required this.state,
    this.managedWindows = 0,
    this.message,
  });

  const EngineStatus.unavailable(this.message)
      : state = EngineState.unavailable,
        managedWindows = 0;

  final EngineState state;

  /// How many windows currently have effects applied.
  final int managedWindows;

  /// Failure detail, or a note about why the engine is idle.
  final String? message;

  static EngineStatus fromMap(Map<Object?, Object?> map) {
    final Object? rawState = map['state'];
    final EngineState state = switch (rawState) {
      'observing' => EngineState.observing,
      'running' => EngineState.running,
      'paused' => EngineState.paused,
      'faulted' => EngineState.faulted,
      _ => EngineState.unavailable,
    };
    final Object? count = map['managedWindows'];
    final Object? message = map['message'];
    return EngineStatus(
      state: state,
      managedWindows: count is int ? count : 0,
      message: message is String && message.isNotEmpty ? message : null,
    );
  }
}

/// What the native shell reports about itself once, at startup.
///
/// None of this is a setting. It is the state of things the Dart side cannot
/// see — the registry, the notification area, and whether our own window ended
/// up with a backdrop behind it.
class ShellState {
  const ShellState({
    this.startWithWindows = false,
    this.paused = false,
    this.hasTray = false,
    this.selfBackdrop = false,
  });

  /// What the `HKCU\...\Run` key actually says, which outranks whatever the
  /// config file remembers.
  final bool startWithWindows;

  final bool paused;

  /// False when Windows refused the notification icon, in which case closing
  /// the window quits instead of hiding.
  final bool hasTray;

  /// True when the settings window really is showing Mica, so the UI can drop
  /// its opaque page background and let it through.
  final bool selfBackdrop;

  static ShellState fromMap(Map<Object?, Object?> map) {
    return ShellState(
      startWithWindows: map['startWithWindows'] == true,
      paused: map['paused'] == true,
      hasTray: map['tray'] == true,
      selfBackdrop: map['selfBackdrop'] == true,
    );
  }
}

/// Typed wrapper over the platform channels used to talk to the native engine.
///
/// Every call degrades instead of throwing. Before the native half exists the
/// channel raises [MissingPluginException]; the settings UI must stay fully
/// usable in that state, so it is translated into [EngineState.unavailable]
/// rather than propagated.
class EffectsChannel {
  EffectsChannel({
    MethodChannel? method,
    EventChannel? events,
  })  : _method = method ?? const MethodChannel(_methodChannelName),
        _events = events ?? const EventChannel(_eventChannelName);

  static const String _methodChannelName = 'window_effect/engine';
  static const String _eventChannelName = 'window_effect/engine_events';

  final MethodChannel _method;
  final EventChannel _events;

  bool _nativeMissing = false;

  /// True once a call has proven the native half is not registered. Further
  /// calls short-circuit rather than paying for a failed channel round trip on
  /// every slider tick.
  bool get nativeMissing => _nativeMissing;

  /// Asks the engine what this machine supports. Returns null when the native
  /// half is absent, leaving the caller on its build-number heuristic.
  Future<Map<Object?, Object?>?> getCapabilities() =>
      _invokeMap('getCapabilities');

  Future<EngineStatus> getStatus() async {
    final Map<Object?, Object?>? map = await _invokeMap('getStatus');
    if (map == null) {
      return const EngineStatus.unavailable('Effects engine is not running.');
    }
    return EngineStatus.fromMap(map);
  }

  /// Pushes a complete settings snapshot. Partial updates are deliberately not
  /// supported — the engine swaps whole configs so it can never observe a torn
  /// state mid-change.
  Future<void> applySettings(EffectSettings settings) =>
      _invokeVoid('applySettings', settings.toJson());

  /// Master switch. Turning this off must restore every managed window.
  Future<void> setEnabled({required bool enabled}) =>
      _invokeVoid('setEnabled', <String, Object?>{'enabled': enabled});

  /// Suspends the engine without touching the saved configuration, so resuming
  /// gives back exactly what the user had. Not a setting, and never persisted.
  Future<void> setPaused({required bool paused}) =>
      _invokeVoid('setPaused', <String, Object?>{'paused': paused});

  /// Reads the registry, the tray and the backdrop in one round trip. Null
  /// when the native half is absent.
  Future<ShellState?> getShellState() async {
    final Map<Object?, Object?>? map = await _invokeMap('getShellState');
    return map == null ? null : ShellState.fromMap(map);
  }

  /// Opens the system file dialog and returns the chosen executable's name,
  /// lowercased. Null when the user cancels — which is why this cannot simply
  /// return an empty string.
  Future<String?> pickExecutable() async {
    if (_nativeMissing) return null;
    try {
      return await _method.invokeMethod<String>('pickExecutable');
    } on MissingPluginException {
      _nativeMissing = true;
      return null;
    } on PlatformException {
      return null;
    }
  }

  /// Registers handlers for the intents the tray sends up.
  ///
  /// The tray owns no settings. Choosing a preset or flipping the master
  /// switch there arrives here, is applied to the one settings model, saved,
  /// and pushed back down — so the menu and the window can never disagree
  /// about what is on.
  void setTrayListener({
    required void Function(bool enabled) onEnabledRequested,
    required void Function(String presetId) onPresetRequested,
  }) {
    _method.setMethodCallHandler((MethodCall call) async {
      final Object? arguments = call.arguments;
      if (arguments is! Map) return null;
      if (call.method == 'trayEnabled') {
        final Object? enabled = arguments['enabled'];
        if (enabled is bool) onEnabledRequested(enabled);
      } else if (call.method == 'trayPreset') {
        final Object? preset = arguments['preset'];
        if (preset is String) onPresetRequested(preset);
      }
      return null;
    });
  }

  /// Stops listening for tray intents. Called when the controller is disposed,
  /// so a late intent cannot arrive at a dead listener.
  void clearTrayListener() => _method.setMethodCallHandler(null);

  /// Registers or removes the `HKCU\...\Run` entry. Returns null on success or
  /// a message describing why it failed.
  Future<String?> setStartWithWindows({required bool enabled}) async {
    try {
      if (_nativeMissing) return null;
      final Object? result = await _method.invokeMethod<Object?>(
        'setStartWithWindows',
        <String, Object?>{'enabled': enabled},
      );
      if (result is Map && result['error'] is String) {
        return result['error'] as String;
      }
      return null;
    } on MissingPluginException {
      _nativeMissing = true;
      return null;
    } on PlatformException catch (error) {
      return error.message ?? 'Could not change the startup setting.';
    }
  }

  /// Restores every managed window immediately. Used by the safety path.
  Future<void> restoreAll() => _invokeVoid('restoreAll', null);

  /// Status updates pushed from the engine thread. Falls back to an empty
  /// stream when the native half is absent so listeners need no special case.
  Stream<EngineStatus> statusStream() {
    return _events
        .receiveBroadcastStream()
        .where((Object? event) => event is Map)
        .map((Object? event) => EngineStatus.fromMap(event! as Map<Object?, Object?>))
        .handleError((Object _) {
      _nativeMissing = true;
    });
  }

  // ----------------------------------------------------------------- plumbing

  Future<Map<Object?, Object?>?> _invokeMap(String method) async {
    if (_nativeMissing) return null;
    try {
      return await _method.invokeMethod<Map<Object?, Object?>>(method);
    } on MissingPluginException {
      _nativeMissing = true;
      return null;
    } on PlatformException {
      return null;
    }
  }

  Future<void> _invokeVoid(String method, Object? arguments) async {
    if (_nativeMissing) return;
    try {
      await _method.invokeMethod<void>(method, arguments);
    } on MissingPluginException {
      _nativeMissing = true;
    } on PlatformException {
      // Engine-side failures surface through the status stream; a settings
      // write should never throw into the UI.
    }
  }
}
