/// What this machine's Windows build can actually do.
///
/// Derived from the build-number table in `docs/FEASIBILITY.md` §3. In Phase 1
/// this is inferred from `Platform.operatingSystemVersion`; from Phase 2 the
/// native engine reports the authoritative set (it can also detect a missing
/// DWM export, which a build number cannot).
class EngineCapabilities {
  const EngineCapabilities({
    required this.buildNumber,
    required this.systemCorners,
    required this.preciseCorners,
    required this.customShadow,
    required this.transparency,
    required this.blurBehind,
    required this.systemBackdrop,
    required this.legacyMica,
    required this.borderColor,
  });

  /// Everything off — the safe assumption when the platform is unknown.
  const EngineCapabilities.unknown()
      : buildNumber = 0,
        systemCorners = false,
        preciseCorners = false,
        customShadow = false,
        transparency = false,
        blurBehind = false,
        systemBackdrop = false,
        legacyMica = false,
        borderColor = false;

  /// Derives the capability set from a Windows build number.
  factory EngineCapabilities.fromBuild(int build) {
    return EngineCapabilities(
      buildNumber: build,
      // Layered windows and window regions predate everything we support.
      preciseCorners: build > 0,
      customShadow: build > 0,
      transparency: build > 0,
      // Accent blur-behind: Windows 10 1809.
      blurBehind: build >= 17763,
      // DWMWA_WINDOW_CORNER_PREFERENCE / _BORDER_COLOR: Windows 11 21H2.
      systemCorners: build >= 22000,
      borderColor: build >= 22000,
      // DWMWA_MICA_EFFECT (undocumented) covers the gap before 22H2.
      legacyMica: build >= 22000 && build < 22621,
      // DWMWA_SYSTEMBACKDROP_TYPE: Windows 11 22H2.
      systemBackdrop: build >= 22621,
    );
  }

  final int buildNumber;

  final bool systemCorners;
  final bool preciseCorners;
  final bool customShadow;
  final bool transparency;
  final bool blurBehind;
  final bool systemBackdrop;
  final bool legacyMica;
  final bool borderColor;

  /// Any backdrop material at all, via either the modern or legacy attribute.
  bool get anyBackdrop => systemBackdrop || legacyMica;

  /// True when only Acrylic is unavailable — the legacy Mica attribute is a
  /// boolean, so it cannot express "Acrylic".
  bool get acrylicBackdrop => systemBackdrop;

  bool get isWindows11 => buildNumber >= 22000;

  /// Human-readable Windows generation, for the status strip.
  String get platformLabel {
    if (buildNumber == 0) return 'Unknown platform';
    if (buildNumber >= 26100) return 'Windows 11 24H2+ (build $buildNumber)';
    if (buildNumber >= 22621) return 'Windows 11 22H2+ (build $buildNumber)';
    if (buildNumber >= 22000) return 'Windows 11 21H2 (build $buildNumber)';
    if (buildNumber >= 17763) return 'Windows 10 1809+ (build $buildNumber)';
    return 'Windows (build $buildNumber)';
  }

  /// Reads a capability map sent by the native engine, falling back to this
  /// instance's value for anything the engine did not report.
  EngineCapabilities mergeNative(Map<Object?, Object?> map) {
    bool read(String key, bool fallback) {
      final Object? value = map[key];
      return value is bool ? value : fallback;
    }

    final Object? build = map['buildNumber'];
    return EngineCapabilities(
      buildNumber: build is int ? build : buildNumber,
      systemCorners: read('systemCorners', systemCorners),
      preciseCorners: read('preciseCorners', preciseCorners),
      customShadow: read('customShadow', customShadow),
      transparency: read('transparency', transparency),
      blurBehind: read('blurBehind', blurBehind),
      systemBackdrop: read('systemBackdrop', systemBackdrop),
      legacyMica: read('legacyMica', legacyMica),
      borderColor: read('borderColor', borderColor),
    );
  }
}
