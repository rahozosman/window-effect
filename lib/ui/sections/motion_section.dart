import 'package:flutter/material.dart';

import '../../core/design/tokens.dart';
import '../../models/effect_settings.dart';
import '../../models/enums.dart';
import '../../state/settings_controller.dart';
import '../widgets/motion_preview.dart';
import '../widgets/premium_slider.dart';
import '../widgets/premium_switch.dart';
import '../widgets/section_card.dart';
import '../widgets/setting_row.dart';

/// How windows arrive, and how they leave for the taskbar.
///
/// The style sets the curve; the three numbers under it are the same ones the
/// engine reads. Picking a style seeds them with that style's own values, so
/// the pickers are a starting point rather than a lock.
class MotionSection extends StatelessWidget {
  const MotionSection({
    super.key,
    required this.settings,
    required this.controller,
  });

  final EffectSettings settings;
  final SettingsController controller;

  void _set(EffectSettings Function(EffectSettings s) change) =>
      controller.update(change);

  @override
  Widget build(BuildContext context) {
    final bool on = settings.animationsEnabled;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: <Widget>[
        const SectionHeader('Motion'),
        SectionCard(
          children: <Widget>[
            SettingRow(
              label: 'Window Animations',
              description:
                  'Windows scale and fade in as they open, and again when they '
                  'come back from the taskbar.',
              trailing: PremiumSwitch(
                value: on,
                onChanged: (bool v) =>
                    _set((EffectSettings s) => s.copyWith(animationsEnabled: v)),
                semanticLabel: 'Animate windows',
              ),
            ),
            const RowDivider(),
            Padding(
              padding: const EdgeInsets.only(bottom: Space.sm),
              child: MotionPreview(settings: settings),
            ),
            const RowDivider(),
            ..._styles(context, on),
            const RowDivider(),
            SettingRow(
              label: 'Duration',
              description:
                  'How long a window takes to arrive. Longer is easier to '
                  'watch; shorter stays out of the way.',
              indent: true,
              below: PremiumSlider(
                value: settings.animationDuration,
                min: EffectSettings.minAnimationDuration,
                max: EffectSettings.maxAnimationDuration,
                divisions: 45,
                enabled: on,
                onChanged: on
                    ? (int v) => _set(
                        (EffectSettings s) => s.copyWith(animationDuration: v))
                    : null,
                formatValue: (int v) => '$v ms',
              ),
            ),
            SettingRow(
              label: 'Zoom',
              description:
                  'The size a window starts at. 100% opens it at full size and '
                  'fades only.',
              indent: true,
              below: PremiumSlider(
                value: settings.animationScale,
                min: EffectSettings.minAnimationScale,
                max: 100,
                divisions: 20,
                enabled: on,
                onChanged: on
                    ? (int v) =>
                        _set((EffectSettings s) => s.copyWith(animationScale: v))
                    : null,
                formatValue: (int v) => v == 100 ? 'None' : '$v%',
              ),
            ),
            SettingRow(
              label: 'Rise',
              description: 'How far below its final place a window starts.',
              indent: true,
              below: PremiumSlider(
                value: settings.animationLift,
                min: 0,
                max: EffectSettings.maxAnimationLift,
                divisions: 12,
                enabled: on,
                onChanged: on
                    ? (int v) =>
                        _set((EffectSettings s) => s.copyWith(animationLift: v))
                    : null,
                formatValue: (int v) => v == 0 ? 'None' : '$v px',
              ),
            ),
            const RowDivider(),
            SettingRow(
              label: 'Animate minimising',
              description:
                  'A copy of the window shrinks toward the taskbar. Windows '
                  'hides the real one before anyone outside it can be told, so '
                  'this is the copy Windows itself draws for previews.',
              indent: true,
              trailing: PremiumSwitch(
                value: settings.animateMinimize,
                onChanged: on
                    ? (bool v) => _set(
                        (EffectSettings s) => s.copyWith(animateMinimize: v))
                    : null,
                semanticLabel: 'Animate minimising',
              ),
            ),
          ],
        ),
      ],
    );
  }

  // ----------------------------------------------------------------- styles

  List<Widget> _styles(BuildContext context, bool enabled) {
    return <Widget>[
      SettingRow(
        label: 'Style',
        description: settings.animationStyle.summary,
        indent: true,
        below: Padding(
          padding: const EdgeInsets.only(top: Space.xs),
          child: Wrap(
            spacing: Space.xs,
            runSpacing: Space.xs,
            children: MotionStyle.values
                .map((MotionStyle style) => _StyleChip(
                      style: style,
                      selected: settings.animationStyle == style,
                      enabled: enabled,
                      onTap: () => _set((EffectSettings s) => s.copyWith(
                            animationStyle: style,
                            // Choosing a style means choosing its feel, so its
                            // numbers come with it. They stay editable.
                            animationDuration: style.defaultDuration,
                            animationScale: style.defaultScale,
                            animationLift: style.defaultLift,
                          )),
                    ))
                .toList(growable: false),
          ),
        ),
      ),
    ];
  }
}

class _StyleChip extends StatefulWidget {
  const _StyleChip({
    required this.style,
    required this.selected,
    required this.enabled,
    required this.onTap,
  });

  final MotionStyle style;
  final bool selected;
  final bool enabled;
  final VoidCallback onTap;

  @override
  State<_StyleChip> createState() => _StyleChipState();
}

class _StyleChipState extends State<_StyleChip> {
  bool _hovered = false;

  @override
  Widget build(BuildContext context) {
    final AppColors colors = AppColors.of(context);
    final bool active = widget.selected;

    final Color border = active
        ? colors.accent
        : (_hovered ? colors.strokeStrong : colors.stroke);
    final Color background = active
        ? colors.accent.withValues(alpha: 0.12)
        : (_hovered ? colors.surfaceRaised : Colors.transparent);

    return Opacity(
      opacity: widget.enabled ? 1 : 0.45,
      child: MouseRegion(
        cursor: widget.enabled
            ? SystemMouseCursors.click
            : SystemMouseCursors.basic,
        onEnter: (_) => setState(() => _hovered = true),
        onExit: (_) => setState(() => _hovered = false),
        child: GestureDetector(
          behavior: HitTestBehavior.opaque,
          onTap: widget.enabled ? widget.onTap : null,
          child: AnimatedContainer(
            duration: const Duration(milliseconds: 140),
            padding: const EdgeInsets.symmetric(
              horizontal: Space.sm,
              vertical: 7,
            ),
            decoration: BoxDecoration(
              color: background,
              borderRadius: BorderRadius.circular(Radii.sm),
              border: Border.all(color: border),
            ),
            child: Text(
              widget.style.label,
              style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                    color: active ? colors.accent : colors.textSecondary,
                    fontWeight: active ? FontWeight.w600 : FontWeight.w500,
                  ),
            ),
          ),
        ),
      ),
    );
  }
}
