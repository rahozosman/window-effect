import 'dart:ui' as ui;

import 'package:flutter/material.dart';

import '../../core/design/motion.dart';
import '../../core/design/tokens.dart';
import '../../models/effect_settings.dart';
import '../../models/enums.dart';
import 'wallpaper_painter.dart';

/// A visual demonstration of the current settings.
///
/// This is **only** a mock-up. It shares no code path with the native engine
/// and never touches a real window — it exists so a slider has an immediate
/// answer, not to implement anything. Where Windows would fall short of the
/// requested value (the ~8 px corner ceiling, blur needing transparency) the
/// preview shows the *achieved* result, so it can't set expectations the
/// engine will then break.
class LivePreview extends StatefulWidget {
  const LivePreview({super.key, required this.settings});

  final EffectSettings settings;

  @override
  State<LivePreview> createState() => _LivePreviewState();
}

class _LivePreviewState extends State<LivePreview> {
  bool _showActive = true;

  @override
  Widget build(BuildContext context) {
    final AppColors colors = AppColors.of(context);

    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: <Widget>[
        ClipRRect(
          borderRadius: BorderRadius.circular(Radii.lg),
          child: DecoratedBox(
            decoration: BoxDecoration(border: Border.all(color: colors.stroke)),
            child: SizedBox(
              height: 236,
              child: Stack(
                fit: StackFit.expand,
                children: <Widget>[
                  CustomPaint(
                    painter: WallpaperPainter(
                      sky: colors.previewSky,
                      glow: colors.previewGlow,
                    ),
                  ),
                  Center(
                    child: _PreviewWindow(
                      settings: widget.settings,
                      active: _showActive,
                    ),
                  ),
                ],
              ),
            ),
          ),
        ),
        const SizedBox(height: Space.sm),
        Row(
          children: <Widget>[
            Expanded(
              child: Text(
                'Visual demonstration only — no windows are affected here.',
                style: Theme.of(context).textTheme.bodySmall,
              ),
            ),
            _StateToggle(
              active: _showActive,
              onChanged: (bool value) => setState(() => _showActive = value),
            ),
          ],
        ),
      ],
    );
  }
}

/// Switches the preview between focused and unfocused, so active-window
/// emphasis and inactive dimming are inspectable rather than theoretical.
class _StateToggle extends StatelessWidget {
  const _StateToggle({required this.active, required this.onChanged});

  final bool active;
  final ValueChanged<bool> onChanged;

  @override
  Widget build(BuildContext context) {
    final AppColors colors = AppColors.of(context);
    return MouseRegion(
      cursor: SystemMouseCursors.click,
      child: GestureDetector(
        behavior: HitTestBehavior.opaque,
        onTap: () => onChanged(!active),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: <Widget>[
            Icon(
              active ? Icons.center_focus_strong_outlined : Icons.blur_on_outlined,
              size: 13,
              color: colors.textTertiary,
            ),
            const SizedBox(width: 6),
            Text(
              active ? 'Active window' : 'Inactive window',
              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                    color: colors.textSecondary,
                    fontWeight: FontWeight.w500,
                  ),
            ),
          ],
        ),
      ),
    );
  }
}

class _PreviewWindow extends StatelessWidget {
  const _PreviewWindow({required this.settings, required this.active});

  final EffectSettings settings;
  final bool active;

  static const double _width = 300;
  static const double _height = 168;

  @override
  Widget build(BuildContext context) {
    final AppColors colors = AppColors.of(context);
    final Duration duration = Motion.duration(context, Motion.base);

    // Preview scale: the mock window is roughly a third of a real one, so the
    // radius is scaled to keep the proportion honest rather than exaggerated.
    final double radius = settings.effectiveCornerRadius * 0.85;

    return TweenAnimationBuilder<double>(
      duration: duration,
      curve: Motion.enter,
      // Only `end` is set: TweenAnimationBuilder lerps from whatever the
      // radius currently is whenever the target changes.
      tween: Tween<double>(end: radius),
      builder: (BuildContext context, double animatedRadius, Widget? child) {
        return AnimatedContainer(
          duration: duration,
          curve: Motion.enter,
          width: _width,
          height: _height,
          decoration: BoxDecoration(
            borderRadius: BorderRadius.circular(animatedRadius),
            boxShadow: _shadows(),
          ),
          child: ClipRRect(
            borderRadius: BorderRadius.circular(animatedRadius),
            child: Stack(
              fit: StackFit.expand,
              children: <Widget>[
                if (settings.blurIsActive)
                  BackdropFilter(
                    filter: ui.ImageFilter.blur(
                      sigmaX: _blurSigma,
                      sigmaY: _blurSigma,
                    ),
                    child: const SizedBox.expand(),
                  ),
                AnimatedContainer(
                  duration: duration,
                  curve: Motion.enter,
                  color: _surfaceColor(colors),
                  child: _WindowChrome(
                    settings: settings,
                    active: active,
                    colors: colors,
                  ),
                ),
                // Border drawn last so it sits above the surface fill.
                IgnorePointer(
                  child: AnimatedContainer(
                    duration: duration,
                    curve: Motion.enter,
                    decoration: BoxDecoration(
                      borderRadius: BorderRadius.circular(animatedRadius),
                      border: Border.all(
                        color: _borderColor(colors),
                        width: 1,
                      ),
                    ),
                  ),
                ),
              ],
            ),
          ),
        );
      },
    );
  }

  double get _blurSigma => switch (settings.blurPreset) {
        BlurPreset.none => 0,
        BlurPreset.light => 6,
        BlurPreset.medium => 12,
        BlurPreset.strong => 20,
      };

  /// The window's own surface, carrying transparency, backdrop tint and the
  /// inactive dim in one colour.
  Color _surfaceColor(AppColors colors) {
    Color base = colors.surface;

    // Mica and Acrylic read as the wallpaper tinting the window, so blend a
    // little of the desktop's tone into the surface.
    final double tint = switch (settings.backdrop) {
      BackdropMode.none => 0,
      BackdropMode.mica => 0.18,
      BackdropMode.acrylic => 0.26,
      BackdropMode.automatic => 0.18,
    };
    if (tint > 0) base = Color.lerp(base, colors.previewSky, tint)!;

    double alpha = settings.transparencyEnabled ? settings.opacity / 100 : 1.0;

    // Blur reads as frosted glass, which is never fully opaque.
    if (settings.blurIsActive) alpha = (alpha - 0.06).clamp(0.4, 1.0);

    if (!active && settings.dimInactive) {
      base = Color.lerp(base, Colors.black, settings.dimAmount / 100)!;
    }

    return base.withValues(alpha: alpha);
  }

  Color _borderColor(AppColors colors) {
    if (active && settings.activeEmphasis && settings.activeBorder) {
      // Restrained on purpose: the brief rules out anything that reads as a
      // glowing neon ring.
      return colors.accent.withValues(alpha: 0.55);
    }
    return colors.strokeStrong;
  }

  List<BoxShadow> _shadows() {
    if (!settings.shadowEnabled || settings.shadowStrength == 0) {
      return const <BoxShadow>[];
    }

    double strength = settings.shadowStrength / 100;
    if (!active && settings.activeEmphasis) {
      strength *= (1 - settings.shadowDifference.factor);
    }

    // Two layers: a tight contact shadow that anchors the window to the
    // surface, and a wide ambient one that gives it height. A single blur
    // reads as a smudge; this reads as a window resting on something.
    final double blur = settings.shadowBlur.toDouble();
    final double offset = settings.shadowOffset.toDouble();

    return <BoxShadow>[
      BoxShadow(
        color: Colors.black.withValues(alpha: strength * 0.30),
        blurRadius: (blur * 0.35).clamp(1.0, 20.0),
        offset: Offset(0, (offset * 0.35).clamp(0.0, 8.0)),
      ),
      BoxShadow(
        color: Colors.black.withValues(alpha: strength * 0.45),
        blurRadius: blur.clamp(1.0, 50.0),
        offset: Offset(0, offset),
        spreadRadius: -blur * 0.08,
      ),
    ];
  }
}

/// The title bar and body inside the mock window.
class _WindowChrome extends StatelessWidget {
  const _WindowChrome({
    required this.settings,
    required this.active,
    required this.colors,
  });

  final EffectSettings settings;
  final bool active;
  final AppColors colors;

  @override
  Widget build(BuildContext context) {
    final TextTheme text = Theme.of(context).textTheme;
    final Color title = active ? colors.textPrimary : colors.textTertiary;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: <Widget>[
        SizedBox(
          height: 34,
          child: Row(
            children: <Widget>[
              const SizedBox(width: Space.sm),
              Container(
                width: 12,
                height: 12,
                decoration: BoxDecoration(
                  color: colors.strokeStrong,
                  borderRadius: BorderRadius.circular(3),
                ),
              ),
              const SizedBox(width: Space.xs),
              Expanded(
                child: Text(
                  'Example Application',
                  style: text.bodyMedium?.copyWith(
                    color: title,
                    fontWeight: FontWeight.w500,
                  ),
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                ),
              ),
              const _CaptionButtons(),
            ],
          ),
        ),
        Container(height: 1, color: colors.stroke),
        Expanded(
          child: Padding(
            padding: const EdgeInsets.all(Space.md),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                Text(
                  'Window Preview',
                  style: text.bodyLarge?.copyWith(
                    color: active ? colors.textPrimary : colors.textSecondary,
                  ),
                ),
                const SizedBox(height: Space.sm),
                _SkeletonLine(width: 0.86, color: colors.stroke),
                const SizedBox(height: Space.xs),
                _SkeletonLine(width: 0.64, color: colors.stroke),
                const SizedBox(height: Space.xs),
                _SkeletonLine(width: 0.42, color: colors.stroke),
              ],
            ),
          ),
        ),
      ],
    );
  }
}

class _CaptionButtons extends StatelessWidget {
  const _CaptionButtons();

  @override
  Widget build(BuildContext context) {
    final Color color = AppColors.of(context).textTertiary;
    return Row(
      children: <IconData>[
        Icons.remove,
        Icons.crop_square,
        Icons.close,
      ]
          .map((IconData icon) => SizedBox(
                width: 28,
                height: 34,
                child: Icon(icon, size: 11, color: color),
              ))
          .toList(growable: false),
    );
  }
}

class _SkeletonLine extends StatelessWidget {
  const _SkeletonLine({required this.width, required this.color});

  final double width;
  final Color color;

  @override
  Widget build(BuildContext context) {
    return FractionallySizedBox(
      alignment: Alignment.centerLeft,
      widthFactor: width,
      child: Container(
        height: 7,
        decoration: BoxDecoration(
          color: color,
          borderRadius: BorderRadius.circular(3.5),
        ),
      ),
    );
  }
}
