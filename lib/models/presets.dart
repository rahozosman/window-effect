import 'effect_settings.dart';
import 'enums.dart';

/// A named starting point. Presets only carry *visual* settings — they never
/// touch the master switch, startup registration or the exclusion list, so
/// applying one can't silently undo the user's system choices.
class EffectPreset {
  const EffectPreset({
    required this.id,
    required this.summary,
    required this.build,
  });

  final PresetId id;

  /// One line shown under the preset name.
  final String summary;

  /// Applies this preset's visuals onto [base], preserving system settings.
  final EffectSettings Function(EffectSettings base) build;

  String get label => id.label;
}

/// The five built-in presets, in display order.
///
/// Values follow the brief. Where the brief gives a range (macOS "12–14 px")
/// the upper end is used, because that is the one that reads as macOS.
const List<EffectPreset> kPresets = <EffectPreset>[
  EffectPreset(
    id: PresetId.gnome,
    summary: 'Calm and even. Generous corners, a soft grounded shadow.',
    build: _gnome,
  ),
  EffectPreset(
    id: PresetId.macos,
    summary: 'Deeper, softer shadow and a clear focus hierarchy.',
    build: _macos,
  ),
  EffectPreset(
    id: PresetId.fluent,
    summary: 'Tighter corners, Mica backdrop, a hairline active border.',
    build: _fluent,
  ),
  EffectPreset(
    id: PresetId.premium,
    summary: 'The recommended balance. Subtle everywhere, loud nowhere.',
    build: _premium,
  ),
  EffectPreset(
    id: PresetId.minimal,
    summary: 'Corners and almost nothing else. Lowest cost.',
    build: _minimal,
  ),
];

/// The factory default, used on first run and when a config file is unreadable.
EffectSettings get kDefaultSettings => _premium(EffectSettings());

EffectPreset? presetById(PresetId id) {
  for (final preset in kPresets) {
    if (preset.id == id) return preset;
  }
  return null;
}

/// Identifies which preset [settings] currently matches, or [PresetId.custom].
PresetId detectPreset(EffectSettings settings) {
  for (final preset in kPresets) {
    if (settings.matchesVisualsOf(preset.build(settings))) return preset.id;
  }
  return PresetId.custom;
}

// ---------------------------------------------------------------- definitions

EffectSettings _gnome(EffectSettings base) => base.copyWith(
      cornersEnabled: true,
      cornerMode: CornerMode.system,
      cornerRadius: 12,
      shadowEnabled: true,
      shadowStrength: 30,
      shadowBlur: 28,
      shadowOffset: 8,
      transparencyEnabled: false,
      opacity: 95,
      blurEnabled: false,
      blurPreset: BlurPreset.light,
      backdrop: BackdropMode.none,
      activeEmphasis: true,
      activeBorder: false,
      shadowDifference: ShadowDifference.medium,
      dimInactive: false,
      dimAmount: 8,
    );

EffectSettings _macos(EffectSettings base) => base.copyWith(
      cornersEnabled: true,
      cornerMode: CornerMode.system,
      cornerRadius: 14,
      shadowEnabled: true,
      shadowStrength: 42,
      shadowBlur: 34,
      shadowOffset: 10,
      transparencyEnabled: false,
      opacity: 95,
      blurEnabled: false,
      blurPreset: BlurPreset.light,
      backdrop: BackdropMode.none,
      activeEmphasis: true,
      activeBorder: false,
      shadowDifference: ShadowDifference.high,
      dimInactive: false,
      dimAmount: 8,
    );

EffectSettings _fluent(EffectSettings base) => base.copyWith(
      cornersEnabled: true,
      cornerMode: CornerMode.system,
      cornerRadius: 8,
      shadowEnabled: true,
      shadowStrength: 22,
      shadowBlur: 18,
      shadowOffset: 4,
      transparencyEnabled: false,
      opacity: 95,
      blurEnabled: false,
      blurPreset: BlurPreset.none,
      backdrop: BackdropMode.automatic,
      activeEmphasis: true,
      activeBorder: true,
      shadowDifference: ShadowDifference.low,
      dimInactive: false,
      dimAmount: 8,
    );

EffectSettings _premium(EffectSettings base) => base.copyWith(
      cornersEnabled: true,
      cornerMode: CornerMode.system,
      cornerRadius: 12,
      shadowEnabled: true,
      shadowStrength: 35,
      shadowBlur: 24,
      shadowOffset: 6,
      transparencyEnabled: false,
      opacity: 95,
      blurEnabled: false,
      blurPreset: BlurPreset.light,
      backdrop: BackdropMode.automatic,
      activeEmphasis: true,
      activeBorder: false,
      shadowDifference: ShadowDifference.medium,
      dimInactive: false,
      dimAmount: 8,
    );

EffectSettings _minimal(EffectSettings base) => base.copyWith(
      cornersEnabled: true,
      cornerMode: CornerMode.system,
      cornerRadius: 8,
      shadowEnabled: true,
      shadowStrength: 14,
      shadowBlur: 12,
      shadowOffset: 3,
      transparencyEnabled: false,
      opacity: 95,
      blurEnabled: false,
      blurPreset: BlurPreset.none,
      backdrop: BackdropMode.none,
      activeEmphasis: false,
      activeBorder: false,
      shadowDifference: ShadowDifference.low,
      dimInactive: false,
      dimAmount: 8,
    );
