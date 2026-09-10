import 'package:flutter/material.dart';

/// Spacing scale. Every gap in the UI comes from here so vertical rhythm stays
/// consistent without anyone counting pixels.
abstract final class Space {
  static const double xxs = 4;
  static const double xs = 8;
  static const double sm = 12;
  static const double md = 16;
  static const double lg = 20;
  static const double xl = 24;
  static const double xxl = 32;
  static const double xxxl = 40;
}

/// Corner radii used by the settings UI itself (not the effect radii).
abstract final class Radii {
  static const double sm = 8;
  static const double md = 12;
  static const double lg = 16;
  static const double xl = 20;
  static const double pill = 999;
}

/// Colour roles, carried as a [ThemeExtension] so widgets read semantic names
/// instead of literals and light/dark stay in lockstep.
@immutable
class AppColors extends ThemeExtension<AppColors> {
  const AppColors({
    required this.canvas,
    required this.surface,
    required this.surfaceRaised,
    required this.surfaceSunken,
    required this.stroke,
    required this.strokeStrong,
    required this.textPrimary,
    required this.textSecondary,
    required this.textTertiary,
    required this.accent,
    required this.accentMuted,
    required this.danger,
    required this.success,
    required this.trackOff,
    required this.previewSky,
    required this.previewGlow,
  });

  /// Page background.
  final Color canvas;

  /// Section cards.
  final Color surface;

  /// Controls resting on a card — chips, wells, dialogs.
  final Color surfaceRaised;

  /// Inset areas — the preview stage, slider tracks.
  final Color surfaceSunken;

  final Color stroke;
  final Color strokeStrong;

  final Color textPrimary;
  final Color textSecondary;
  final Color textTertiary;

  /// The single accent. Used for on-state switches, slider fills and the
  /// selected preset ring — nothing else, so it stays meaningful.
  final Color accent;
  final Color accentMuted;

  final Color danger;
  final Color success;

  /// Off-state switch track.
  final Color trackOff;

  /// Base tone of the fake wallpaper behind the live preview.
  final Color previewSky;

  /// Warm highlight painted into the fake wallpaper.
  final Color previewGlow;

  static const AppColors dark = AppColors(
    canvas: Color(0xFF0F1115),
    surface: Color(0xFF171A20),
    surfaceRaised: Color(0xFF1E222A),
    surfaceSunken: Color(0xFF0B0D11),
    stroke: Color(0x12FFFFFF),
    strokeStrong: Color(0x24FFFFFF),
    textPrimary: Color(0xFFECEEF2),
    textSecondary: Color(0xFF98A0AD),
    textTertiary: Color(0xFF6A7280),
    accent: Color(0xFF4F7CFF),
    accentMuted: Color(0x294F7CFF),
    danger: Color(0xFFE5484D),
    success: Color(0xFF3DD68C),
    trackOff: Color(0xFF2A2F38),
    previewSky: Color(0xFF1A2233),
    previewGlow: Color(0xFF3A5C8F),
  );

  static const AppColors light = AppColors(
    canvas: Color(0xFFF4F5F7),
    surface: Color(0xFFFFFFFF),
    surfaceRaised: Color(0xFFF7F8FA),
    surfaceSunken: Color(0xFFEBEDF1),
    stroke: Color(0x140B0D12),
    strokeStrong: Color(0x240B0D12),
    textPrimary: Color(0xFF12141A),
    textSecondary: Color(0xFF5B6270),
    textTertiary: Color(0xFF868D9A),
    accent: Color(0xFF2F63E8),
    accentMuted: Color(0x1F2F63E8),
    danger: Color(0xFFD13438),
    success: Color(0xFF1F9D63),
    trackOff: Color(0xFFD4D8DF),
    previewSky: Color(0xFFBFCDE4),
    previewGlow: Color(0xFFE6D6C2),
  );

  /// Reads the palette for the current theme.
  static AppColors of(BuildContext context) =>
      Theme.of(context).extension<AppColors>() ?? dark;

  @override
  AppColors copyWith({
    Color? canvas,
    Color? surface,
    Color? surfaceRaised,
    Color? surfaceSunken,
    Color? stroke,
    Color? strokeStrong,
    Color? textPrimary,
    Color? textSecondary,
    Color? textTertiary,
    Color? accent,
    Color? accentMuted,
    Color? danger,
    Color? success,
    Color? trackOff,
    Color? previewSky,
    Color? previewGlow,
  }) {
    return AppColors(
      canvas: canvas ?? this.canvas,
      surface: surface ?? this.surface,
      surfaceRaised: surfaceRaised ?? this.surfaceRaised,
      surfaceSunken: surfaceSunken ?? this.surfaceSunken,
      stroke: stroke ?? this.stroke,
      strokeStrong: strokeStrong ?? this.strokeStrong,
      textPrimary: textPrimary ?? this.textPrimary,
      textSecondary: textSecondary ?? this.textSecondary,
      textTertiary: textTertiary ?? this.textTertiary,
      accent: accent ?? this.accent,
      accentMuted: accentMuted ?? this.accentMuted,
      danger: danger ?? this.danger,
      success: success ?? this.success,
      trackOff: trackOff ?? this.trackOff,
      previewSky: previewSky ?? this.previewSky,
      previewGlow: previewGlow ?? this.previewGlow,
    );
  }

  @override
  AppColors lerp(covariant AppColors? other, double t) {
    if (other == null) return this;
    Color mix(Color a, Color b) => Color.lerp(a, b, t)!;
    return AppColors(
      canvas: mix(canvas, other.canvas),
      surface: mix(surface, other.surface),
      surfaceRaised: mix(surfaceRaised, other.surfaceRaised),
      surfaceSunken: mix(surfaceSunken, other.surfaceSunken),
      stroke: mix(stroke, other.stroke),
      strokeStrong: mix(strokeStrong, other.strokeStrong),
      textPrimary: mix(textPrimary, other.textPrimary),
      textSecondary: mix(textSecondary, other.textSecondary),
      textTertiary: mix(textTertiary, other.textTertiary),
      accent: mix(accent, other.accent),
      accentMuted: mix(accentMuted, other.accentMuted),
      danger: mix(danger, other.danger),
      success: mix(success, other.success),
      trackOff: mix(trackOff, other.trackOff),
      previewSky: mix(previewSky, other.previewSky),
      previewGlow: mix(previewGlow, other.previewGlow),
    );
  }
}
