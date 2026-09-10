import 'enums.dart';

/// Immutable snapshot of every user-facing setting.
///
/// This is the single payload that travels to disk and to the native engine.
/// All numeric fields are clamped in the constructor, so an out-of-range value
/// from a hand-edited config file cannot reach the engine.
class EffectSettings {
  EffectSettings({
    this.effectsEnabled = true,
    this.cornersEnabled = true,
    this.cornerMode = CornerMode.system,
    int cornerRadius = 12,
    this.shadowEnabled = true,
    int shadowStrength = 35,
    int shadowBlur = 24,
    int shadowOffset = 6,
    this.transparencyEnabled = false,
    int opacity = 95,
    this.blurEnabled = false,
    this.blurPreset = BlurPreset.light,
    this.backdrop = BackdropMode.automatic,
    this.activeEmphasis = true,
    this.activeBorder = false,
    this.shadowDifference = ShadowDifference.medium,
    this.dimInactive = false,
    int dimAmount = 8,
    this.animationsEnabled = true,
    this.animationStyle = MotionStyle.premium,
    int animationDuration = 600,
    int animationScale = 91,
    int animationLift = 14,
    this.animateMinimize = true,
    this.startWithWindows = false,
    List<String> excludedApps = const <String>[],
  })  : cornerRadius = _clampInt(cornerRadius, minCornerRadius, maxCornerRadius),
        shadowStrength = _clampInt(shadowStrength, 0, 100),
        shadowBlur = _clampInt(shadowBlur, 0, 50),
        shadowOffset = _clampInt(shadowOffset, 0, 20),
        opacity = _clampInt(opacity, minOpacity, 100),
        dimAmount = _clampInt(dimAmount, 0, maxDimAmount),
        animationDuration =
            _clampInt(animationDuration, minAnimationDuration, maxAnimationDuration),
        animationScale =
            _clampInt(animationScale, minAnimationScale, 100),
        animationLift = _clampInt(animationLift, 0, maxAnimationLift),
        excludedApps = List<String>.unmodifiable(excludedApps);

  // ---------------------------------------------------------------- bounds

  static const int minCornerRadius = 0;
  static const int maxCornerRadius = 24;

  /// Below this the brief's readability requirement stops holding.
  static const int minOpacity = 80;

  static const int maxDimAmount = 20;

  /// Under 150 ms nothing reads as motion at all. The top end is generous on
  /// purpose: a window arriving over a second and a half is slower than most
  /// people want, and some people want exactly that.
  static const int minAnimationDuration = 150;
  static const int maxAnimationDuration = 1500;

  /// Starting below 80% of final size stops looking like the window arriving
  /// and starts looking like it being thrown at you.
  static const int minAnimationScale = 80;

  static const int maxAnimationLift = 60;

  /// The radius values offered as quick choices. Any value in
  /// [minCornerRadius]..[maxCornerRadius] is still valid via "Custom".
  static const List<int> cornerRadiusChoices = <int>[0, 6, 8, 10, 12, 14, 16, 20];

  // ------------------------------------------------------------------ master

  /// Master switch. When false the engine restores every managed window and
  /// stops observing.
  final bool effectsEnabled;

  // ----------------------------------------------------------------- corners

  final bool cornersEnabled;
  final CornerMode cornerMode;
  final int cornerRadius;

  // ------------------------------------------------------------------ shadow

  final bool shadowEnabled;

  /// 0–100. Opacity of the companion shadow.
  final int shadowStrength;

  /// 0–50 px gaussian radius.
  final int shadowBlur;

  /// 0–20 px downward offset.
  final int shadowOffset;

  // ------------------------------------------------------------ transparency

  final bool transparencyEnabled;

  /// [minOpacity]–100.
  final int opacity;

  // -------------------------------------------------------------------- blur

  final bool blurEnabled;
  final BlurPreset blurPreset;

  // ---------------------------------------------------------------- backdrop

  final BackdropMode backdrop;

  // ------------------------------------------------------------ active window

  final bool activeEmphasis;
  final bool activeBorder;
  final ShadowDifference shadowDifference;

  final bool dimInactive;

  /// 0–[maxDimAmount] percent.
  final int dimAmount;

  // ------------------------------------------------------------------ motion

  /// Master switch for window open, restore and minimise animation.
  final bool animationsEnabled;

  final MotionStyle animationStyle;

  /// Milliseconds, [minAnimationDuration]..[maxAnimationDuration].
  final int animationDuration;

  /// Percent of final size the window starts at, [minAnimationScale]..100.
  final int animationScale;

  /// Pixels below its final position the window starts at, 0..[maxAnimationLift].
  final int animationLift;

  /// Whether going to and coming back from the taskbar is animated too.
  final bool animateMinimize;

  // ------------------------------------------------------------------ system

  final bool startWithWindows;

  /// Lowercase executable names, e.g. `chrome.exe`.
  final List<String> excludedApps;

  // ------------------------------------------------------------------ derived

  /// The radius the system will actually render, given [cornerMode].
  ///
  /// In [CornerMode.system] the DWM buckets are ~4 px and ~8 px; reporting the
  /// requested radius instead of this one would be lying in the user's own
  /// settings screen.
  int get effectiveCornerRadius {
    if (!cornersEnabled || cornerRadius == 0) return 0;
    if (cornerMode == CornerMode.precise) return cornerRadius;
    if (cornerRadius <= 6) return 4;
    return 8;
  }

  /// True when the requested radius cannot be rendered exactly.
  bool get cornerRadiusIsApproximate =>
      cornersEnabled &&
      cornerMode == CornerMode.system &&
      cornerRadius != effectiveCornerRadius;

  /// Blur only has a visible effect where the window is actually see-through.
  bool get blurIsActive =>
      blurEnabled && transparencyEnabled && blurPreset != BlurPreset.none;

  /// Precise corners remove the DWM shadow, so the companion shadow is the
  /// only thing separating the window from the desktop.
  bool get shadowIsLoadBearing =>
      cornersEnabled && cornerMode == CornerMode.precise;

  // ---------------------------------------------------------------- copyWith

  EffectSettings copyWith({
    bool? effectsEnabled,
    bool? cornersEnabled,
    CornerMode? cornerMode,
    int? cornerRadius,
    bool? shadowEnabled,
    int? shadowStrength,
    int? shadowBlur,
    int? shadowOffset,
    bool? transparencyEnabled,
    int? opacity,
    bool? blurEnabled,
    BlurPreset? blurPreset,
    BackdropMode? backdrop,
    bool? activeEmphasis,
    bool? activeBorder,
    ShadowDifference? shadowDifference,
    bool? dimInactive,
    int? dimAmount,
    bool? animationsEnabled,
    MotionStyle? animationStyle,
    int? animationDuration,
    int? animationScale,
    int? animationLift,
    bool? animateMinimize,
    bool? startWithWindows,
    List<String>? excludedApps,
  }) {
    return EffectSettings(
      effectsEnabled: effectsEnabled ?? this.effectsEnabled,
      cornersEnabled: cornersEnabled ?? this.cornersEnabled,
      cornerMode: cornerMode ?? this.cornerMode,
      cornerRadius: cornerRadius ?? this.cornerRadius,
      shadowEnabled: shadowEnabled ?? this.shadowEnabled,
      shadowStrength: shadowStrength ?? this.shadowStrength,
      shadowBlur: shadowBlur ?? this.shadowBlur,
      shadowOffset: shadowOffset ?? this.shadowOffset,
      transparencyEnabled: transparencyEnabled ?? this.transparencyEnabled,
      opacity: opacity ?? this.opacity,
      blurEnabled: blurEnabled ?? this.blurEnabled,
      blurPreset: blurPreset ?? this.blurPreset,
      backdrop: backdrop ?? this.backdrop,
      activeEmphasis: activeEmphasis ?? this.activeEmphasis,
      activeBorder: activeBorder ?? this.activeBorder,
      shadowDifference: shadowDifference ?? this.shadowDifference,
      dimInactive: dimInactive ?? this.dimInactive,
      dimAmount: dimAmount ?? this.dimAmount,
      animationsEnabled: animationsEnabled ?? this.animationsEnabled,
      animationStyle: animationStyle ?? this.animationStyle,
      animationDuration: animationDuration ?? this.animationDuration,
      animationScale: animationScale ?? this.animationScale,
      animationLift: animationLift ?? this.animationLift,
      animateMinimize: animateMinimize ?? this.animateMinimize,
      startWithWindows: startWithWindows ?? this.startWithWindows,
      excludedApps: excludedApps ?? this.excludedApps,
    );
  }

  // -------------------------------------------------------------------- JSON

  Map<String, Object?> toJson() => <String, Object?>{
        'effectsEnabled': effectsEnabled,
        'cornersEnabled': cornersEnabled,
        'cornerMode': cornerMode.id,
        'cornerRadius': cornerRadius,
        'shadowEnabled': shadowEnabled,
        'shadowStrength': shadowStrength,
        'shadowBlur': shadowBlur,
        'shadowOffset': shadowOffset,
        'transparencyEnabled': transparencyEnabled,
        'opacity': opacity,
        'blurEnabled': blurEnabled,
        'blurPreset': blurPreset.id,
        'backdrop': backdrop.id,
        'activeEmphasis': activeEmphasis,
        'activeBorder': activeBorder,
        'shadowDifference': shadowDifference.id,
        'dimInactive': dimInactive,
        'dimAmount': dimAmount,
        'animationsEnabled': animationsEnabled,
        'animationStyle': animationStyle.id,
        'animationDuration': animationDuration,
        'animationScale': animationScale,
        'animationLift': animationLift,
        'animateMinimize': animateMinimize,
        'startWithWindows': startWithWindows,
        'excludedApps': excludedApps,
      };

  factory EffectSettings.fromJson(Map<String, Object?> json) {
    return EffectSettings(
      effectsEnabled: _bool(json['effectsEnabled'], true),
      cornersEnabled: _bool(json['cornersEnabled'], true),
      cornerMode: CornerMode.fromId(json['cornerMode'] as String?),
      cornerRadius: _int(json['cornerRadius'], 12),
      shadowEnabled: _bool(json['shadowEnabled'], true),
      shadowStrength: _int(json['shadowStrength'], 35),
      shadowBlur: _int(json['shadowBlur'], 24),
      shadowOffset: _int(json['shadowOffset'], 6),
      transparencyEnabled: _bool(json['transparencyEnabled'], false),
      opacity: _int(json['opacity'], 95),
      blurEnabled: _bool(json['blurEnabled'], false),
      blurPreset: BlurPreset.fromId(json['blurPreset'] as String?),
      backdrop: BackdropMode.fromId(json['backdrop'] as String?),
      activeEmphasis: _bool(json['activeEmphasis'], true),
      activeBorder: _bool(json['activeBorder'], false),
      shadowDifference: ShadowDifference.fromId(json['shadowDifference'] as String?),
      dimInactive: _bool(json['dimInactive'], false),
      dimAmount: _int(json['dimAmount'], 8),
      animationsEnabled: _bool(json['animationsEnabled'], true),
      animationStyle: MotionStyle.fromId(json['animationStyle'] as String?),
      animationDuration: _int(json['animationDuration'], 600),
      animationScale: _int(json['animationScale'], 91),
      animationLift: _int(json['animationLift'], 14),
      animateMinimize: _bool(json['animateMinimize'], true),
      startWithWindows: _bool(json['startWithWindows'], false),
      excludedApps: _stringList(json['excludedApps']),
    );
  }

  // ------------------------------------------------------------------ equality

  /// Compares everything the *engine* cares about. [startWithWindows] and
  /// [effectsEnabled] are excluded so that toggling them never makes a preset
  /// read as "Custom".
  bool matchesVisualsOf(EffectSettings other) {
    return cornersEnabled == other.cornersEnabled &&
        cornerMode == other.cornerMode &&
        cornerRadius == other.cornerRadius &&
        shadowEnabled == other.shadowEnabled &&
        shadowStrength == other.shadowStrength &&
        shadowBlur == other.shadowBlur &&
        shadowOffset == other.shadowOffset &&
        transparencyEnabled == other.transparencyEnabled &&
        opacity == other.opacity &&
        blurEnabled == other.blurEnabled &&
        blurPreset == other.blurPreset &&
        backdrop == other.backdrop &&
        activeEmphasis == other.activeEmphasis &&
        activeBorder == other.activeBorder &&
        shadowDifference == other.shadowDifference &&
        dimInactive == other.dimInactive &&
        dimAmount == other.dimAmount;
  }

  // ------------------------------------------------------------------ helpers

  static int _clampInt(int value, int min, int max) =>
      value < min ? min : (value > max ? max : value);

  static bool _bool(Object? value, bool fallback) =>
      value is bool ? value : fallback;

  static int _int(Object? value, int fallback) {
    if (value is int) return value;
    if (value is num) return value.round();
    return fallback;
  }

  static List<String> _stringList(Object? value) {
    if (value is! List) return const <String>[];
    return value.whereType<String>().toList(growable: false);
  }
}
