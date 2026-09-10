import 'dart:math' as math;

import 'package:flutter/material.dart';

/// The fake desktop behind the live preview.
///
/// Painted rather than shipped as an asset: it costs no binary size, adapts to
/// the theme, and gives the blur and transparency controls something with real
/// structure to show through. Blur is only legible against varied content — a
/// flat fill would make every blur preset look identical.
class WallpaperPainter extends CustomPainter {
  const WallpaperPainter({required this.sky, required this.glow});

  final Color sky;
  final Color glow;

  @override
  void paint(Canvas canvas, Size size) {
    if (size.isEmpty) return;
    final Rect bounds = Offset.zero & size;

    canvas.drawRect(
      bounds,
      Paint()
        ..shader = LinearGradient(
          begin: Alignment.topLeft,
          end: Alignment.bottomRight,
          colors: <Color>[
            sky,
            Color.lerp(sky, glow, 0.45)!,
            Color.lerp(sky, Colors.black, 0.18)!,
          ],
          stops: const <double>[0.0, 0.55, 1.0],
        ).createShader(bounds),
    );

    // Three soft light pools. Radial gradients, not blurs — same look, and no
    // filter cost on a surface that repaints whenever a slider moves.
    _pool(canvas, bounds, const Alignment(-0.62, -0.55), 0.58, glow, 0.34);
    _pool(canvas, bounds, const Alignment(0.75, 0.15), 0.46, glow, 0.20);
    _pool(canvas, bounds, const Alignment(0.05, 0.95), 0.62, sky, 0.30);

    _bands(canvas, bounds);
  }

  void _pool(
    Canvas canvas,
    Rect bounds,
    Alignment at,
    double scale,
    Color color,
    double alpha,
  ) {
    final Offset center = at.withinRect(bounds);
    final double radius = math.max(bounds.width, bounds.height) * scale;
    canvas.drawCircle(
      center,
      radius,
      Paint()
        ..shader = RadialGradient(
          colors: <Color>[
            color.withValues(alpha: alpha),
            color.withValues(alpha: 0),
          ],
        ).createShader(Rect.fromCircle(center: center, radius: radius)),
    );
  }

  /// Faint diagonal banding. Gives the blur something fine-grained to dissolve,
  /// which is what makes the difference between Light and Strong readable.
  void _bands(Canvas canvas, Rect bounds) {
    final Paint paint = Paint()
      ..color = Colors.white.withValues(alpha: 0.035)
      ..strokeWidth = 1.4;
    const double step = 26;
    canvas.save();
    canvas.clipRect(bounds);
    for (double x = -bounds.height; x < bounds.width; x += step) {
      canvas.drawLine(
        Offset(x, bounds.bottom),
        Offset(x + bounds.height, bounds.top),
        paint,
      );
    }
    canvas.restore();
  }

  @override
  bool shouldRepaint(WallpaperPainter oldDelegate) =>
      oldDelegate.sky != sky || oldDelegate.glow != glow;
}
