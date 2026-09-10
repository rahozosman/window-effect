import 'package:flutter/material.dart';

import '../../core/design/motion.dart';
import '../../core/design/tokens.dart';
import '../../models/effect_settings.dart';

/// A preset button carrying a miniature of what it does.
///
/// The glyph is drawn from the preset's own values, so the difference between
/// Minimal and macOS is visible before you click either one — which is the
/// whole point of having presets.
class PresetTile extends StatefulWidget {
  const PresetTile({
    super.key,
    required this.label,
    required this.summary,
    required this.settings,
    required this.selected,
    required this.onTap,
  });

  final String label;
  final String summary;

  /// The settings this preset would produce, used to draw the glyph.
  final EffectSettings settings;

  final bool selected;
  final VoidCallback onTap;

  @override
  State<PresetTile> createState() => _PresetTileState();
}

class _PresetTileState extends State<PresetTile> {
  bool _hovered = false;

  @override
  Widget build(BuildContext context) {
    final AppColors colors = AppColors.of(context);
    final Duration duration = Motion.duration(context, Motion.fast);

    final Color border = widget.selected
        ? colors.accent
        : (_hovered ? colors.strokeStrong : colors.stroke);

    return Tooltip(
      message: widget.summary,
      waitDuration: const Duration(milliseconds: 500),
      child: MouseRegion(
        cursor: SystemMouseCursors.click,
        onEnter: (_) => setState(() => _hovered = true),
        onExit: (_) => setState(() => _hovered = false),
        child: GestureDetector(
          behavior: HitTestBehavior.opaque,
          onTap: widget.onTap,
          child: AnimatedContainer(
            duration: duration,
            curve: Motion.inOut,
            width: 128,
            padding: const EdgeInsets.fromLTRB(Space.sm, Space.sm, Space.sm, Space.xs),
            decoration: BoxDecoration(
              color: widget.selected ? colors.accentMuted : colors.surfaceSunken,
              borderRadius: BorderRadius.circular(Radii.md),
              border: Border.all(color: border, width: widget.selected ? 1.5 : 1),
            ),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                SizedBox(
                  height: 44,
                  width: double.infinity,
                  child: CustomPaint(
                    painter: _PresetGlyphPainter(
                      settings: widget.settings,
                      surface: colors.surfaceRaised,
                      stroke: colors.strokeStrong,
                      accent: colors.accent,
                    ),
                  ),
                ),
                const SizedBox(height: Space.xs),
                Text(
                  widget.label,
                  style: Theme.of(context).textTheme.bodyLarge?.copyWith(
                        color: widget.selected
                            ? colors.textPrimary
                            : colors.textSecondary,
                        fontWeight:
                            widget.selected ? FontWeight.w600 : FontWeight.w500,
                      ),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

/// Draws a small window carrying the preset's radius, shadow and border.
class _PresetGlyphPainter extends CustomPainter {
  _PresetGlyphPainter({
    required this.settings,
    required this.surface,
    required this.stroke,
    required this.accent,
  });

  final EffectSettings settings;
  final Color surface;
  final Color stroke;
  final Color accent;

  @override
  void paint(Canvas canvas, Size size) {
    if (size.isEmpty) return;

    final Rect window = Rect.fromLTWH(
      size.width * 0.16,
      size.height * 0.18,
      size.width * 0.68,
      size.height * 0.62,
    );

    // Scale the real radius into the glyph's much smaller window.
    final double radius =
        (settings.effectiveCornerRadius * 0.42).clamp(0.0, window.height / 2);
    final RRect shape = RRect.fromRectAndRadius(window, Radius.circular(radius));

    if (settings.shadowEnabled && settings.shadowStrength > 0) {
      final double alpha = (settings.shadowStrength / 100) * 0.55;
      final double blur = 1 + settings.shadowBlur * 0.10;
      final double dy = settings.shadowOffset * 0.18;
      canvas.drawRRect(
        shape.shift(Offset(0, dy)),
        Paint()
          ..color = Colors.black.withValues(alpha: alpha)
          ..maskFilter = MaskFilter.blur(BlurStyle.normal, blur),
      );
    }

    canvas.drawRRect(
      shape,
      Paint()..color = surface.withValues(alpha: settings.opacity / 100),
    );

    canvas.drawRRect(
      shape,
      Paint()
        ..style = PaintingStyle.stroke
        ..strokeWidth = 1
        ..color = settings.activeBorder ? accent.withValues(alpha: 0.7) : stroke,
    );

    // Two content lines, enough to read as a window rather than a rectangle.
    final Paint line = Paint()..color = stroke;
    final double left = window.left + 6;
    final double width = window.width - 12;
    for (int i = 0; i < 2; i++) {
      canvas.drawRRect(
        RRect.fromRectAndRadius(
          Rect.fromLTWH(left, window.top + 11 + i * 7, width * (i == 0 ? 0.7 : 0.45), 2.5),
          const Radius.circular(1.5),
        ),
        line,
      );
    }
  }

  @override
  bool shouldRepaint(_PresetGlyphPainter oldDelegate) {
    return !oldDelegate.settings.matchesVisualsOf(settings) ||
        oldDelegate.surface != surface ||
        oldDelegate.stroke != stroke ||
        oldDelegate.accent != accent;
  }
}
