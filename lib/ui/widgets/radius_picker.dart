import 'package:flutter/material.dart';

import '../../core/design/motion.dart';
import '../../core/design/tokens.dart';
import '../../models/effect_settings.dart';
import 'premium_slider.dart';

/// The corner-radius control: quick preset chips plus a Custom escape hatch.
///
/// The chips cover the values people actually want; Custom reveals a slider
/// for anything else, rather than making everyone drag for 12 px.
class RadiusPicker extends StatefulWidget {
  const RadiusPicker({
    super.key,
    required this.value,
    required this.onChanged,
  });

  final int value;
  final ValueChanged<int> onChanged;

  @override
  State<RadiusPicker> createState() => _RadiusPickerState();
}

class _RadiusPickerState extends State<RadiusPicker> {
  late bool _custom = !EffectSettings.cornerRadiusChoices.contains(widget.value);

  /// Set while a change originates from this widget's own slider. Without it,
  /// dragging the custom slider onto a value that happens to be a chip (8, 12)
  /// would collapse the slider out from under the cursor mid-drag.
  bool _selfChange = false;

  @override
  void didUpdateWidget(RadiusPicker oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (widget.value == oldWidget.value) return;

    if (_selfChange) {
      _selfChange = false;
      return;
    }

    // A preset applied from elsewhere should collapse the custom slider again.
    if (EffectSettings.cornerRadiusChoices.contains(widget.value)) {
      _custom = false;
    }
  }

  void _onSliderChanged(int value) {
    _selfChange = true;
    widget.onChanged(value);
  }

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: <Widget>[
        Wrap(
          spacing: Space.xs,
          runSpacing: Space.xs,
          children: <Widget>[
            ...EffectSettings.cornerRadiusChoices.map(
              (int radius) => _RadiusChip(
                label: '$radius',
                selected: !_custom && widget.value == radius,
                onTap: () {
                  setState(() => _custom = false);
                  widget.onChanged(radius);
                },
              ),
            ),
            _RadiusChip(
              label: 'Custom',
              selected: _custom,
              onTap: () => setState(() => _custom = true),
            ),
          ],
        ),
        AnimatedSize(
          duration: Motion.duration(context, Motion.base),
          curve: Motion.enter,
          alignment: Alignment.topCenter,
          child: _custom
              ? Padding(
                  padding: const EdgeInsets.only(top: Space.xs),
                  child: PremiumSlider(
                    value: widget.value,
                    min: EffectSettings.minCornerRadius,
                    max: EffectSettings.maxCornerRadius,
                    formatValue: (int v) => '$v px',
                    onChanged: _onSliderChanged,
                  ),
                )
              : const SizedBox(width: double.infinity),
        ),
      ],
    );
  }
}

class _RadiusChip extends StatelessWidget {
  const _RadiusChip({
    required this.label,
    required this.selected,
    required this.onTap,
  });

  final String label;
  final bool selected;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final AppColors colors = AppColors.of(context);
    return MouseRegion(
      cursor: SystemMouseCursors.click,
      child: GestureDetector(
        behavior: HitTestBehavior.opaque,
        onTap: onTap,
        child: AnimatedContainer(
          duration: Motion.duration(context, Motion.fast),
          curve: Motion.inOut,
          padding: const EdgeInsets.symmetric(horizontal: Space.sm, vertical: 6),
          decoration: BoxDecoration(
            color: selected ? colors.accentMuted : colors.surfaceSunken,
            borderRadius: BorderRadius.circular(Radii.sm),
            border: Border.all(
              color: selected ? colors.accent : colors.stroke,
            ),
          ),
          child: Text(
            label,
            style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                  color: selected ? colors.textPrimary : colors.textSecondary,
                  fontWeight: selected ? FontWeight.w600 : FontWeight.w400,
                  fontFeatures: const <FontFeature>[FontFeature.tabularFigures()],
                ),
          ),
        ),
      ),
    );
  }
}
