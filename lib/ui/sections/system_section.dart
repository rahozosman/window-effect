import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../../core/design/tokens.dart';
import '../../models/capabilities.dart';
import '../../models/effect_settings.dart';
import '../../state/settings_controller.dart';
import '../widgets/premium_switch.dart';
import '../widgets/section_card.dart';
import '../widgets/setting_row.dart';

/// Startup, the safety switch and where the config lives.
class SystemSection extends StatelessWidget {
  const SystemSection({
    super.key,
    required this.settings,
    required this.capabilities,
    required this.controller,
  });

  final EffectSettings settings;
  final EngineCapabilities capabilities;
  final SettingsController controller;

  @override
  Widget build(BuildContext context) {
    final AppColors colors = AppColors.of(context);
    final TextTheme text = Theme.of(context).textTheme;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: <Widget>[
        const SectionHeader('System'),
        SectionCard(
          children: <Widget>[
            SettingRow(
              label: 'Start with Windows',
              description: 'Launches quietly into the tray. No window opens.',
              trailing: PremiumSwitch(
                value: settings.startWithWindows,
                onChanged: (bool v) => controller.update(
                  (EffectSettings s) => s.copyWith(startWithWindows: v),
                ),
              ),
            ),
            const RowDivider(),
            SettingRow(
              label: 'Closing this window',
              description: controller.hasTray
                  ? 'Hides it. Effects keep running — the tray icon brings it '
                      'back, and Exit there stops everything.'
                  : 'Quits. Windows would not accept a tray icon, so there is '
                      'nothing to hide into.',
              trailing: Icon(
                controller.hasTray
                    ? Icons.expand_more_rounded
                    : Icons.power_settings_new_rounded,
                size: 16,
                color: colors.textTertiary,
              ),
            ),
            const RowDivider(),
            SettingRow(
              label: 'Turn off all effects',
              description:
                  'Restores every window Windows Effects has touched, right '
                  'now. Use this if anything looks wrong.',
              trailing: _DangerButton(
                label: 'Restore all',
                onTap: controller.disableEverything,
              ),
            ),
            const RowDivider(),
            SettingRow(
              label: 'Settings file',
              description: controller.configPath,
              trailing: _CopyButton(value: controller.configPath),
            ),
            const RowDivider(),
            Padding(
              padding: const EdgeInsets.symmetric(vertical: Space.sm),
              child: Row(
                children: <Widget>[
                  Icon(
                    Icons.desktop_windows_outlined,
                    size: 14,
                    color: colors.textTertiary,
                  ),
                  const SizedBox(width: Space.xs),
                  Expanded(
                    child: Text(
                      capabilities.platformLabel,
                      style: text.bodyMedium?.copyWith(
                        color: colors.textTertiary,
                      ),
                    ),
                  ),
                ],
              ),
            ),
          ],
        ),
      ],
    );
  }
}

class _DangerButton extends StatefulWidget {
  const _DangerButton({required this.label, required this.onTap});

  final String label;
  final Future<void> Function() onTap;

  @override
  State<_DangerButton> createState() => _DangerButtonState();
}

class _DangerButtonState extends State<_DangerButton> {
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
        onTap: () => widget.onTap(),
        child: Container(
          padding: const EdgeInsets.symmetric(
            horizontal: Space.sm,
            vertical: 6,
          ),
          decoration: BoxDecoration(
            color: _hovered
                ? colors.danger.withValues(alpha: 0.12)
                : Colors.transparent,
            borderRadius: BorderRadius.circular(Radii.sm),
            border: Border.all(
              color: _hovered ? colors.danger : colors.strokeStrong,
            ),
          ),
          child: Text(
            widget.label,
            style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                  color: _hovered ? colors.danger : colors.textSecondary,
                  fontWeight: FontWeight.w500,
                ),
          ),
        ),
      ),
    );
  }
}

class _CopyButton extends StatefulWidget {
  const _CopyButton({required this.value});

  final String value;

  @override
  State<_CopyButton> createState() => _CopyButtonState();
}

class _CopyButtonState extends State<_CopyButton> {
  bool _copied = false;

  Future<void> _copy() async {
    await Clipboard.setData(ClipboardData(text: widget.value));
    if (!mounted) return;
    setState(() => _copied = true);
    await Future<void>.delayed(const Duration(seconds: 2));
    if (!mounted) return;
    setState(() => _copied = false);
  }

  @override
  Widget build(BuildContext context) {
    final AppColors colors = AppColors.of(context);
    return Tooltip(
      message: _copied ? 'Copied' : 'Copy path',
      child: MouseRegion(
        cursor: SystemMouseCursors.click,
        child: GestureDetector(
          behavior: HitTestBehavior.opaque,
          onTap: _copy,
          child: SizedBox(
            width: 30,
            height: 26,
            child: Icon(
              _copied ? Icons.check_rounded : Icons.copy_rounded,
              size: 14,
              color: _copied ? colors.success : colors.textTertiary,
            ),
          ),
        ),
      ),
    );
  }
}
