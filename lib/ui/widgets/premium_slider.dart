import 'package:flutter/material.dart';

import '../../core/design/tokens.dart';

/// A thin, quiet slider with a fixed-width numeric readout.
///
/// The readout is right-aligned in a reserved column so the track never
/// reflows as the number changes width — a small thing that stops the whole
/// row from twitching during a drag.
class PremiumSlider extends StatelessWidget {
  const PremiumSlider({
    super.key,
    required this.value,
    required this.min,
    required this.max,
    required this.onChanged,
    required this.formatValue,
    this.divisions,
    this.enabled = true,
  });

  final int value;
  final int min;
  final int max;
  final ValueChanged<int>? onChanged;

  /// Renders the readout, e.g. `(v) => '$v px'`.
  final String Function(int value) formatValue;

  final int? divisions;
  final bool enabled;

  static const double _readoutWidth = 52;

  @override
  Widget build(BuildContext context) {
    final AppColors colors = AppColors.of(context);
    final bool interactive = enabled && onChanged != null && max > min;
    final double clamped = value.clamp(min, max).toDouble();

    return Row(
      children: <Widget>[
        Expanded(
          child: SliderTheme(
            data: SliderThemeData(
              trackHeight: 3,
              activeTrackColor: interactive ? colors.accent : colors.trackOff,
              inactiveTrackColor: colors.trackOff,
              thumbColor: Colors.white,
              overlayColor: colors.accentMuted,
              thumbShape: const RoundSliderThumbShape(
                enabledThumbRadius: 7,
                disabledThumbRadius: 6,
                elevation: 1.5,
                pressedElevation: 2.5,
              ),
              overlayShape: const RoundSliderOverlayShape(overlayRadius: 14),
              tickMarkShape: SliderTickMarkShape.noTickMark,
              showValueIndicator: ShowValueIndicator.never,
              trackShape: const RoundedRectSliderTrackShape(),
            ),
            child: Slider(
              value: clamped,
              min: min.toDouble(),
              max: max.toDouble(),
              divisions: divisions ?? (max - min),
              onChanged: interactive
                  ? (double next) => onChanged!(next.round())
                  : null,
            ),
          ),
        ),
        const SizedBox(width: Space.xs),
        SizedBox(
          width: _readoutWidth,
          child: Text(
            formatValue(value),
            textAlign: TextAlign.right,
            style: Theme.of(context).textTheme.labelLarge?.copyWith(
                  color: interactive ? colors.textSecondary : colors.textTertiary,
                  fontFeatures: const <FontFeature>[FontFeature.tabularFigures()],
                ),
          ),
        ),
      ],
    );
  }
}
