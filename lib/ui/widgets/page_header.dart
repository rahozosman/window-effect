import 'package:flutter/material.dart';

import '../../core/design/motion.dart';
import '../../core/design/tokens.dart';
import '../../platform/effects_channel.dart';
import '../widgets/premium_switch.dart';

/// Title, engine status and the master switch.
///
/// The status line is deliberately literal. A utility that reaches into other
/// applications' windows should say plainly whether it is currently doing so.
class PageHeader extends StatelessWidget {
  const PageHeader({
    super.key,
    required this.status,
    required this.effectsEnabled,
    required this.onEnabledChanged,
    required this.onResume,
  });

  final EngineStatus status;
  final bool effectsEnabled;
  final ValueChanged<bool> onEnabledChanged;

  /// Undoes a pause started from the tray. Pause is deliberately not a setting
  /// and has no switch here, but a user reading "Paused" in this window should
  /// not have to go hunting through the notification area to lift it.
  final VoidCallback onResume;

  @override
  Widget build(BuildContext context) {
    final TextTheme text = Theme.of(context).textTheme;

    return Row(
      crossAxisAlignment: CrossAxisAlignment.center,
      children: <Widget>[
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: <Widget>[
              Text('Window Effects', style: text.headlineSmall),
              const SizedBox(height: Space.xxs),
              _StatusLine(status: status, effectsEnabled: effectsEnabled),
            ],
          ),
        ),
        const SizedBox(width: Space.md),
        if (status.state == EngineState.paused) ...<Widget>[
          _ResumeButton(onTap: onResume),
          const SizedBox(width: Space.sm),
        ],
        PremiumSwitch(
          value: effectsEnabled,
          onChanged: onEnabledChanged,
          semanticLabel: 'Enable all window effects',
        ),
      ],
    );
  }
}

class _ResumeButton extends StatefulWidget {
  const _ResumeButton({required this.onTap});

  final VoidCallback onTap;

  @override
  State<_ResumeButton> createState() => _ResumeButtonState();
}

class _ResumeButtonState extends State<_ResumeButton> {
  bool _hovered = false;

  @override
  Widget build(BuildContext context) {
    final AppColors colors = AppColors.of(context);
    return MouseRegion(
      cursor: SystemMouseCursors.click,
      onEnter: (_) => setState(() => _hovered = true),
      onExit: (_) => setState(() => _hovered = false),
      child: GestureDetector(
        behavior: HitTestBehavior.opaque,
        onTap: widget.onTap,
        child: AnimatedContainer(
          duration: Motion.duration(context, Motion.fast),
          padding: const EdgeInsets.symmetric(
            horizontal: Space.sm,
            vertical: 6,
          ),
          decoration: BoxDecoration(
            color: _hovered
                ? colors.accent.withValues(alpha: 0.12)
                : Colors.transparent,
            borderRadius: BorderRadius.circular(Radii.sm),
            border: Border.all(
              color: _hovered ? colors.accent : colors.strokeStrong,
            ),
          ),
          child: Text(
            'Resume',
            style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                  color: _hovered ? colors.accent : colors.textSecondary,
                  fontWeight: FontWeight.w500,
                ),
          ),
        ),
      ),
    );
  }
}

class _StatusLine extends StatelessWidget {
  const _StatusLine({required this.status, required this.effectsEnabled});

  final EngineStatus status;
  final bool effectsEnabled;

  @override
  Widget build(BuildContext context) {
    final AppColors colors = AppColors.of(context);

    final (Color dot, String label) = _describe(colors);

    return Row(
      children: <Widget>[
        AnimatedContainer(
          duration: Motion.duration(context, Motion.base),
          width: 7,
          height: 7,
          decoration: BoxDecoration(color: dot, shape: BoxShape.circle),
        ),
        const SizedBox(width: Space.xs),
        Flexible(
          child: AnimatedDefaultTextStyle(
            duration: Motion.duration(context, Motion.base),
            style: Theme.of(context).textTheme.bodyMedium!,
            child: Text(label, maxLines: 1, overflow: TextOverflow.ellipsis),
          ),
        ),
      ],
    );
  }

  (Color, String) _describe(AppColors colors) {
    if (!effectsEnabled) {
      return (colors.textTertiary, 'Effects are off. No windows are modified.');
    }
    return switch (status.state) {
      EngineState.connecting => (colors.textTertiary, 'Starting…'),
      EngineState.observing => (
          colors.textSecondary,
          status.managedWindows == 0
              ? 'Watching for application windows'
              : 'Watching ${status.managedWindows} windows',
        ),
      EngineState.running => (
          colors.success,
          status.managedWindows == 1
              ? 'Active — 1 window enhanced'
              : 'Active — ${status.managedWindows} windows enhanced',
        ),
      EngineState.paused => (colors.textTertiary, 'Paused from the tray.'),
      EngineState.faulted => (
          colors.danger,
          status.message ?? 'Engine stopped after an error. Windows restored.',
        ),
      EngineState.unavailable => (
          colors.textTertiary,
          status.message ?? 'Effects engine is not running.',
        ),
    };
  }
}
