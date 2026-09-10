import 'package:flutter/material.dart';

import '../../models/capabilities.dart';
import '../../models/effect_settings.dart';
import '../../models/enums.dart';
import '../../state/settings_controller.dart';
import '../widgets/premium_slider.dart';
import '../widgets/premium_switch.dart';
import '../widgets/radius_picker.dart';
import '../widgets/section_card.dart';
import '../widgets/segmented_selector.dart';
import '../widgets/setting_row.dart';

/// Corners, shadow, transparency, blur, backdrop and focus behaviour.
///
/// Rows that Windows cannot honour on this machine stay in place, dimmed, with
/// the reason attached — `unavailableReason` on [SettingRow].
class AppearanceSection extends StatelessWidget {
  const AppearanceSection({
    super.key,
    required this.settings,
    required this.capabilities,
    required this.controller,
  });

  final EffectSettings settings;
  final EngineCapabilities capabilities;
  final SettingsController controller;

  void _set(EffectSettings Function(EffectSettings s) change) =>
      controller.update(change);

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: <Widget>[
        const SectionHeader('Appearance'),
        SectionCard(
          children: <Widget>[
            ..._corners(context),
            const RowDivider(),
            ..._shadow(context),
            const RowDivider(),
            ..._transparency(context),
            const RowDivider(),
            ..._backdrop(context),
            const RowDivider(),
            ..._focus(context),
          ],
        ),
      ],
    );
  }

  // ---------------------------------------------------------------- corners

  List<Widget> _corners(BuildContext context) {
    final bool systemAvailable = capabilities.systemCorners;
    final bool preciseSelected = settings.cornerMode == CornerMode.precise;

    // On Windows 10 the DWM path does not exist, so the region path is the only
    // way to round anything at all.
    final String? cornersUnavailable = (!systemAvailable && !capabilities.preciseCorners)
        ? 'Not available on this Windows build.'
        : null;

    return <Widget>[
      SettingRow(
        label: 'Rounded Corners',
        description: 'Softens the corners of managed application windows.',
        unavailableReason: cornersUnavailable,
        trailing: PremiumSwitch(
          value: settings.cornersEnabled,
          onChanged: cornersUnavailable != null
              ? null
              : (bool v) => _set((EffectSettings s) => s.copyWith(cornersEnabled: v)),
        ),
      ),
      if (settings.cornersEnabled && cornersUnavailable == null) ...<Widget>[
        SettingRow(
          indent: true,
          label: 'Corner Radius',
          note: settings.cornerRadiusIsApproximate
              ? 'renders at ${settings.effectiveCornerRadius} px'
              : null,
          description: settings.cornerRadiusIsApproximate
              ? 'Windows offers only two rounded sizes, so the closest one is used.'
              : null,
          below: RadiusPicker(
            value: settings.cornerRadius,
            onChanged: (int v) =>
                _set((EffectSettings s) => s.copyWith(cornerRadius: v)),
          ),
        ),
        SettingRow(
          indent: true,
          label: 'Corner Method',
          description: preciseSelected
              ? 'Exact radius, but corners are not anti-aliased and Windows '
                  'removes its own shadow — keep Window Shadow on.'
              : 'Uses the built-in Windows corner, which keeps the system '
                  'shadow and smooth edges.',
          below: SegmentedSelector<CornerMode>(
            value: settings.cornerMode,
            options: <SegmentOption<CornerMode>>[
              SegmentOption<CornerMode>(
                value: CornerMode.system,
                label: 'System',
                unavailableReason: systemAvailable
                    ? null
                    : 'Requires Windows 11 (build 22000 or newer).',
              ),
              const SegmentOption<CornerMode>(
                value: CornerMode.precise,
                label: 'Precise',
              ),
            ],
            onChanged: (CornerMode v) =>
                _set((EffectSettings s) => s.copyWith(cornerMode: v)),
          ),
        ),
      ],
    ];
  }

  // ----------------------------------------------------------------- shadow

  List<Widget> _shadow(BuildContext context) {
    return <Widget>[
      SettingRow(
        label: 'Window Shadow',
        description: settings.shadowIsLoadBearing
            ? 'Required while Precise corners are on — Windows draws no shadow '
                'behind a shaped window.'
            : 'Adds a soft shadow so windows lift off the desktop.',
        trailing: PremiumSwitch(
          value: settings.shadowEnabled,
          onChanged: (bool v) =>
              _set((EffectSettings s) => s.copyWith(shadowEnabled: v)),
        ),
      ),
      if (settings.shadowEnabled) ...<Widget>[
        SettingRow(
          indent: true,
          label: 'Strength',
          below: PremiumSlider(
            value: settings.shadowStrength,
            min: 0,
            max: 100,
            formatValue: (int v) => '$v%',
            onChanged: (int v) =>
                _set((EffectSettings s) => s.copyWith(shadowStrength: v)),
          ),
        ),
        SettingRow(
          indent: true,
          label: 'Blur',
          below: PremiumSlider(
            value: settings.shadowBlur,
            min: 0,
            max: 50,
            formatValue: (int v) => '$v px',
            onChanged: (int v) =>
                _set((EffectSettings s) => s.copyWith(shadowBlur: v)),
          ),
        ),
        SettingRow(
          indent: true,
          label: 'Offset',
          below: PremiumSlider(
            value: settings.shadowOffset,
            min: 0,
            max: 20,
            formatValue: (int v) => '$v px',
            onChanged: (int v) =>
                _set((EffectSettings s) => s.copyWith(shadowOffset: v)),
          ),
        ),
      ],
    ];
  }

  // ----------------------------------------------------------- transparency

  List<Widget> _transparency(BuildContext context) {
    final String? blurUnavailable = capabilities.blurBehind
        ? null
        : 'Requires Windows 10 version 1809 or newer.';

    return <Widget>[
      SettingRow(
        label: 'Transparency',
        description: 'Applied only to windows that can safely support it.',
        trailing: PremiumSwitch(
          value: settings.transparencyEnabled,
          onChanged: (bool v) =>
              _set((EffectSettings s) => s.copyWith(transparencyEnabled: v)),
        ),
      ),
      if (settings.transparencyEnabled) ...<Widget>[
        SettingRow(
          indent: true,
          label: 'Opacity',
          description: settings.opacity <= 85
              ? 'Lower values start to cost readability.'
              : null,
          below: PremiumSlider(
            value: settings.opacity,
            min: EffectSettings.minOpacity,
            max: 100,
            formatValue: (int v) => '$v%',
            onChanged: (int v) =>
                _set((EffectSettings s) => s.copyWith(opacity: v)),
          ),
        ),
        SettingRow(
          indent: true,
          label: 'Background Blur',
          description: 'Frosts the desktop behind a transparent window.',
          unavailableReason: blurUnavailable,
          trailing: PremiumSwitch(
            value: settings.blurEnabled,
            onChanged: blurUnavailable != null
                ? null
                : (bool v) =>
                    _set((EffectSettings s) => s.copyWith(blurEnabled: v)),
          ),
        ),
        if (settings.blurEnabled && blurUnavailable == null)
          SettingRow(
            indent: true,
            label: 'Blur Amount',
            description: settings.blurPreset == BlurPreset.strong
                ? 'Strong uses Acrylic, which can lag while dragging a window.'
                : null,
            below: SegmentedSelector<BlurPreset>(
              value: settings.blurPreset,
              options: const <SegmentOption<BlurPreset>>[
                SegmentOption<BlurPreset>(value: BlurPreset.none, label: 'None'),
                SegmentOption<BlurPreset>(value: BlurPreset.light, label: 'Light'),
                SegmentOption<BlurPreset>(value: BlurPreset.medium, label: 'Medium'),
                SegmentOption<BlurPreset>(value: BlurPreset.strong, label: 'Strong'),
              ],
              onChanged: (BlurPreset v) =>
                  _set((EffectSettings s) => s.copyWith(blurPreset: v)),
            ),
          ),
      ] else
        SettingRow(
          indent: true,
          label: 'Background Blur',
          unavailableReason: 'Turn on Transparency to use blur.',
          trailing: const PremiumSwitch(value: false, onChanged: null),
        ),
    ];
  }

  // --------------------------------------------------------------- backdrop

  List<Widget> _backdrop(BuildContext context) {
    final String? unavailable = capabilities.anyBackdrop
        ? null
        : 'Requires Windows 11. Mica and Acrylic do not exist on this build.';

    return <Widget>[
      SettingRow(
        label: 'Backdrop Material',
        description: unavailable != null
            ? null
            : 'Most apps paint their own background, so this usually shows in '
                'the title bar.',
        unavailableReason: unavailable,
        below: SegmentedSelector<BackdropMode>(
          value: settings.backdrop,
          enabled: unavailable == null,
          options: <SegmentOption<BackdropMode>>[
            const SegmentOption<BackdropMode>(
              value: BackdropMode.none,
              label: 'None',
            ),
            const SegmentOption<BackdropMode>(
              value: BackdropMode.mica,
              label: 'Mica',
            ),
            SegmentOption<BackdropMode>(
              value: BackdropMode.acrylic,
              label: 'Acrylic',
              unavailableReason: capabilities.acrylicBackdrop
                  ? null
                  : 'Requires Windows 11 22H2 (build 22621 or newer).',
            ),
            const SegmentOption<BackdropMode>(
              value: BackdropMode.automatic,
              label: 'Automatic',
            ),
          ],
          onChanged: unavailable != null
              ? null
              : (BackdropMode v) =>
                  _set((EffectSettings s) => s.copyWith(backdrop: v)),
        ),
      ),
    ];
  }

  // ------------------------------------------------------------------ focus

  List<Widget> _focus(BuildContext context) {
    return <Widget>[
      SettingRow(
        label: 'Active Window Emphasis',
        description: 'Gives the focused window a little more presence.',
        trailing: PremiumSwitch(
          value: settings.activeEmphasis,
          onChanged: (bool v) =>
              _set((EffectSettings s) => s.copyWith(activeEmphasis: v)),
        ),
      ),
      if (settings.activeEmphasis) ...<Widget>[
        SettingRow(
          indent: true,
          label: 'Active Border',
          description: 'A one-pixel accent line around the focused window.',
          unavailableReason: capabilities.borderColor
              ? null
              : 'Requires Windows 11 (build 22000 or newer).',
          trailing: PremiumSwitch(
            value: settings.activeBorder,
            onChanged: capabilities.borderColor
                ? (bool v) =>
                    _set((EffectSettings s) => s.copyWith(activeBorder: v))
                : null,
          ),
        ),
        SettingRow(
          indent: true,
          label: 'Shadow Difference',
          description: 'How much lighter an unfocused window\'s shadow becomes.',
          below: SegmentedSelector<ShadowDifference>(
            value: settings.shadowDifference,
            enabled: settings.shadowEnabled,
            options: const <SegmentOption<ShadowDifference>>[
              SegmentOption<ShadowDifference>(
                value: ShadowDifference.low,
                label: 'Low',
              ),
              SegmentOption<ShadowDifference>(
                value: ShadowDifference.medium,
                label: 'Medium',
              ),
              SegmentOption<ShadowDifference>(
                value: ShadowDifference.high,
                label: 'High',
              ),
            ],
            onChanged: settings.shadowEnabled
                ? (ShadowDifference v) =>
                    _set((EffectSettings s) => s.copyWith(shadowDifference: v))
                : null,
          ),
        ),
      ],
      SettingRow(
        label: 'Dim Inactive Windows',
        description: 'Fades unfocused windows very slightly.',
        trailing: PremiumSwitch(
          value: settings.dimInactive,
          onChanged: (bool v) =>
              _set((EffectSettings s) => s.copyWith(dimInactive: v)),
        ),
      ),
      if (settings.dimInactive)
        SettingRow(
          indent: true,
          label: 'Dim Amount',
          below: PremiumSlider(
            value: settings.dimAmount,
            min: 0,
            max: EffectSettings.maxDimAmount,
            formatValue: (int v) => '$v%',
            onChanged: (int v) =>
                _set((EffectSettings s) => s.copyWith(dimAmount: v)),
          ),
        ),
    ];
  }
}
