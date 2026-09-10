import 'package:flutter/material.dart';

import 'tokens.dart';

/// Light and dark themes for the settings window.
///
/// The type scale uses the platform's own UI font (Segoe UI on Windows) with
/// tightened tracking on headings — native enough to disappear, deliberate
/// enough to read as designed.
abstract final class AppTheme {
  /// [translucent] is set only when the native side reports that the window
  /// really did get a Mica backdrop. The page then stops painting its own
  /// canvas so that backdrop is what shows through; every card keeps its own
  /// opaque fill, so text contrast never depends on the wallpaper behind it.
  static ThemeData dark({bool translucent = false}) =>
      _build(Brightness.dark, AppColors.dark, translucent);

  static ThemeData light({bool translucent = false}) =>
      _build(Brightness.light, AppColors.light, translucent);

  static ThemeData _build(
    Brightness brightness,
    AppColors colors,
    bool translucent,
  ) {
    final TextTheme text = _textTheme(colors);
    return ThemeData(
      useMaterial3: true,
      brightness: brightness,
      colorScheme: ColorScheme.fromSeed(
        seedColor: colors.accent,
        brightness: brightness,
        primary: colors.accent,
        surface: colors.surface,
        error: colors.danger,
      ),
      scaffoldBackgroundColor: translucent ? Colors.transparent : colors.canvas,
      canvasColor: colors.canvas,
      dividerColor: colors.stroke,
      textTheme: text,
      iconTheme: IconThemeData(color: colors.textSecondary, size: 18),
      splashFactory: NoSplash.splashFactory,
      highlightColor: Colors.transparent,
      extensions: <ThemeExtension<dynamic>>[colors],
    );
  }

  static TextTheme _textTheme(AppColors colors) {
    TextStyle style({
      required double size,
      required FontWeight weight,
      required Color color,
      double tracking = 0,
      double height = 1.4,
    }) {
      return TextStyle(
        fontSize: size,
        fontWeight: weight,
        color: color,
        letterSpacing: tracking,
        height: height,
      );
    }

    return TextTheme(
      // Page title.
      headlineSmall: style(
        size: 22,
        weight: FontWeight.w600,
        color: colors.textPrimary,
        tracking: -0.4,
        height: 1.25,
      ),
      // Section headers.
      titleSmall: style(
        size: 11,
        weight: FontWeight.w600,
        color: colors.textTertiary,
        tracking: 1.0,
      ),
      // Setting row labels.
      bodyLarge: style(
        size: 13.5,
        weight: FontWeight.w500,
        color: colors.textPrimary,
        tracking: -0.1,
      ),
      // Setting row descriptions.
      bodyMedium: style(
        size: 12,
        weight: FontWeight.w400,
        color: colors.textSecondary,
        height: 1.45,
      ),
      // Values, hints, footnotes.
      bodySmall: style(
        size: 11.5,
        weight: FontWeight.w400,
        color: colors.textTertiary,
      ),
      // Numeric readouts beside sliders.
      labelLarge: style(
        size: 12.5,
        weight: FontWeight.w600,
        color: colors.textSecondary,
        tracking: 0,
      ),
    );
  }
}
