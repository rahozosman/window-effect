import 'package:flutter/material.dart';

import '../../core/design/tokens.dart';
import '../../models/effect_settings.dart';
import '../../models/enums.dart';

/// Replays the chosen animation on a stand-in window, on a loop.
///
/// It reads the same four bezier control points the engine reads, so what
/// plays here is what a real window will do — the settings screen has no
/// business showing motion the engine would not produce. The only thing it
/// cannot show is the scale in real pixels, so the lift is drawn to the
/// preview's scale rather than the desktop's.
class MotionPreview extends StatefulWidget {
  const MotionPreview({super.key, required this.settings});

  final EffectSettings settings;

  @override
  State<MotionPreview> createState() => _MotionPreviewState();
}

class _MotionPreviewState extends State<MotionPreview>
    with SingleTickerProviderStateMixin, WidgetsBindingObserver {
  late final AnimationController _controller = AnimationController(
    vsync: this,
    duration: Duration(milliseconds: widget.settings.animationDuration),
  );

  // How long the window sits still before the loop starts over. Without a
  // pause the animation reads as a pulse rather than as an arrival.
  static const Duration _hold = Duration(milliseconds: 900);

  bool _disposed = false;
  bool _looping = false;

  /// False once the window is closed to the tray. A loop that keeps running
  /// there costs a repaint every frame for a preview nobody is looking at —
  /// which is exactly the idle cost this whole utility is careful about.
  bool _visible = true;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    _loop();
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    final bool visible = state == AppLifecycleState.resumed ||
        state == AppLifecycleState.inactive;
    if (visible == _visible) return;
    _visible = visible;
    if (visible) _loop();
  }

  Future<void> _loop() async {
    if (_looping) return;
    _looping = true;
    while (!_disposed && _visible && widget.settings.animationsEnabled) {
      _controller
        ..duration = Duration(milliseconds: widget.settings.animationDuration)
        ..reset();
      await _controller.forward();
      if (_disposed || !_visible) break;
      await Future<void>.delayed(_hold);
    }
    _looping = false;
  }

  @override
  void didUpdateWidget(MotionPreview oldWidget) {
    super.didUpdateWidget(oldWidget);
    // A changed setting restarts the run immediately: the point of the preview
    // is to answer "what did that do?" while the slider is still under the
    // user's finger.
    if (oldWidget.settings.animationStyle != widget.settings.animationStyle ||
        oldWidget.settings.animationDuration !=
            widget.settings.animationDuration ||
        oldWidget.settings.animationScale != widget.settings.animationScale ||
        oldWidget.settings.animationLift != widget.settings.animationLift) {
      _controller
        ..duration = Duration(milliseconds: widget.settings.animationDuration)
        ..forward(from: 0);
    }
    // Motion switched back on restarts the loop; switched off, the while
    // condition above ends it on its own.
    if (widget.settings.animationsEnabled) _loop();
  }

  @override
  void dispose() {
    _disposed = true;
    WidgetsBinding.instance.removeObserver(this);
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final AppColors colors = AppColors.of(context);
    final MotionStyle style = widget.settings.animationStyle;
    final Curve curve = Cubic(style.x1, style.y1, style.x2, style.y2);

    final double startScale = widget.settings.animationScale / 100;
    // The lift is in desktop pixels; a preview window a fifth of the size
    // needs a fifth of the movement or it reads as a different animation.
    final double lift = widget.settings.animationLift * 0.35;

    return ClipRRect(
      borderRadius: BorderRadius.circular(Radii.md),
      child: Container(
        height: 170,
        decoration: BoxDecoration(
          color: colors.surfaceSunken,
          border: Border.all(color: colors.stroke),
          borderRadius: BorderRadius.circular(Radii.md),
        ),
        child: Center(
          child: AnimatedBuilder(
            animation: _controller,
            builder: (BuildContext context, Widget? child) {
              final double t = widget.settings.animationsEnabled
                  ? curve.transform(_controller.value)
                  : 1.0;
              final double scale = startScale + (1 - startScale) * t;
              // The engine runs opacity slightly ahead of the movement; so
              // does this.
              final double opacity = (t * 1.35).clamp(0.0, 1.0);

              return Transform.translate(
                offset: Offset(0, lift * (1 - t)),
                child: Transform.scale(
                  scale: scale,
                  child: Opacity(opacity: opacity, child: child),
                ),
              );
            },
            child: _PreviewWindow(colors: colors),
          ),
        ),
      ),
    );
  }
}

class _PreviewWindow extends StatelessWidget {
  const _PreviewWindow({required this.colors});

  final AppColors colors;

  @override
  Widget build(BuildContext context) {
    return Container(
      width: 210,
      height: 118,
      decoration: BoxDecoration(
        color: colors.surface,
        borderRadius: BorderRadius.circular(10),
        border: Border.all(color: colors.strokeStrong),
        boxShadow: <BoxShadow>[
          BoxShadow(
            color: Colors.black.withValues(alpha: 0.35),
            blurRadius: 18,
            offset: const Offset(0, 8),
          ),
        ],
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: <Widget>[
          Container(
            height: 26,
            decoration: BoxDecoration(
              color: colors.surfaceRaised,
              borderRadius: const BorderRadius.vertical(top: Radius.circular(9)),
              border: Border(bottom: BorderSide(color: colors.stroke)),
            ),
            child: Row(
              children: <Widget>[
                const SizedBox(width: Space.sm),
                _Dot(color: colors.textTertiary),
                const SizedBox(width: 5),
                _Dot(color: colors.textTertiary),
                const SizedBox(width: 5),
                _Dot(color: colors.textTertiary),
              ],
            ),
          ),
          Expanded(
            child: Padding(
              padding: const EdgeInsets.all(Space.sm),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: <Widget>[
                  _Line(width: 96, color: colors.stroke),
                  const SizedBox(height: 7),
                  _Line(width: 140, color: colors.stroke),
                  const SizedBox(height: 7),
                  _Line(width: 64, color: colors.stroke),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _Dot extends StatelessWidget {
  const _Dot({required this.color});

  final Color color;

  @override
  Widget build(BuildContext context) {
    return Container(
      width: 6,
      height: 6,
      decoration: BoxDecoration(color: color, shape: BoxShape.circle),
    );
  }
}

class _Line extends StatelessWidget {
  const _Line({required this.width, required this.color});

  final double width;
  final Color color;

  @override
  Widget build(BuildContext context) {
    return Container(
      width: width,
      height: 6,
      decoration: BoxDecoration(
        color: color,
        borderRadius: BorderRadius.circular(3),
      ),
    );
  }
}
