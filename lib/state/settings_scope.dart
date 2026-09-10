import 'package:flutter/widgets.dart';

import 'settings_controller.dart';

/// Provides the [SettingsController] to the widget tree and rebuilds
/// dependants when it notifies.
class SettingsScope extends InheritedNotifier<SettingsController> {
  const SettingsScope({
    super.key,
    required SettingsController controller,
    required super.child,
  }) : super(notifier: controller);

  static SettingsController of(BuildContext context) {
    final SettingsScope? scope =
        context.dependOnInheritedWidgetOfExactType<SettingsScope>();
    assert(scope != null, 'No SettingsScope found in the widget tree.');
    return scope!.notifier!;
  }

  /// Reads the controller without subscribing — for event handlers, which run
  /// outside build and must not create a dependency.
  static SettingsController read(BuildContext context) {
    final SettingsScope? scope =
        context.getInheritedWidgetOfExactType<SettingsScope>();
    assert(scope != null, 'No SettingsScope found in the widget tree.');
    return scope!.notifier!;
  }
}
