import 'package:flutter/material.dart';

import 'core/design/theme.dart';
import 'state/settings_controller.dart';
import 'state/settings_scope.dart';
import 'ui/home_page.dart';

/// Root widget. Owns the [SettingsController] lifetime and follows the
/// system's light/dark preference — a Windows utility that ignores the OS
/// theme is the first thing that gives it away as third-party.
class WindowEffectApp extends StatefulWidget {
  const WindowEffectApp({super.key});

  @override
  State<WindowEffectApp> createState() => _WindowEffectAppState();
}

class _WindowEffectAppState extends State<WindowEffectApp> {
  late final SettingsController _controller = SettingsController();

  /// Mirrors the controller's backdrop report. Held here rather than read in
  /// build so the themes — and with them the whole app — are rebuilt when it
  /// arrives, and never again on an ordinary settings change.
  bool _translucent = false;

  @override
  void initState() {
    super.initState();
    _controller.addListener(_onControllerChanged);
    _controller.initialize();
  }

  void _onControllerChanged() {
    if (_controller.selfBackdrop == _translucent) return;
    setState(() => _translucent = _controller.selfBackdrop);
  }

  @override
  void dispose() {
    _controller.removeListener(_onControllerChanged);
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return SettingsScope(
      controller: _controller,
      child: MaterialApp(
        title: 'Window Effects',
        debugShowCheckedModeBanner: false,
        theme: AppTheme.light(translucent: _translucent),
        darkTheme: AppTheme.dark(translucent: _translucent),
        themeMode: ThemeMode.system,
        home: const HomePage(),
      ),
    );
  }
}
