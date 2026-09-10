import 'package:flutter/material.dart';

import '../../core/design/tokens.dart';
import '../../models/effect_settings.dart';
import '../../models/enums.dart';
import '../../models/presets.dart';
import '../../state/settings_controller.dart';
import '../widgets/preset_tile.dart';
import '../widgets/section_card.dart';

/// The five starting points, shown as miniatures of what they do.
class PresetsSection extends StatelessWidget {
  const PresetsSection({
    super.key,
    required this.settings,
    required this.controller,
  });

  final EffectSettings settings;
  final SettingsController controller;

  @override
  Widget build(BuildContext context) {
    final AppColors colors = AppColors.of(context);
    final PresetId active = controller.activePreset;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: <Widget>[
        SectionHeader(
          'Presets',
          trailing: active == PresetId.custom
              ? Text(
                  'Custom',
                  style: Theme.of(context).textTheme.bodySmall?.copyWith(
                        color: colors.accent,
                        fontWeight: FontWeight.w600,
                      ),
                )
              : null,
        ),
        SectionCard(
          padding: const EdgeInsets.all(Space.md),
          children: <Widget>[
            Wrap(
              spacing: Space.sm,
              runSpacing: Space.sm,
              children: kPresets
                  .map((EffectPreset preset) => PresetTile(
                        label: preset.label,
                        summary: preset.summary,
                        settings: preset.build(settings),
                        selected: active == preset.id,
                        onTap: () => controller.applyPreset(preset.id),
                      ))
                  .toList(growable: false),
            ),
            const SizedBox(height: Space.sm),
            Text(
              active == PresetId.custom
                  ? 'Your own combination. Pick a preset to start over.'
                  : (presetById(active)?.summary ?? ''),
              style: Theme.of(context).textTheme.bodyMedium,
            ),
          ],
        ),
      ],
    );
  }
}
