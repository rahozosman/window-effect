import 'dart:io';

import '../models/capabilities.dart';

/// Windows build-number detection from the Dart side.
///
/// Used until the native engine comes online in Phase 2 and reports the
/// authoritative capability set. `Platform.operatingSystemVersion` on Windows
/// looks like:
///
///     "Windows 11 Pro" 10.0 (Build 26200)
class OsInfo {
  const OsInfo._();

  static final RegExp _buildPattern = RegExp(r'Build\s+(\d+)', caseSensitive: false);

  /// Parses the build number out of a version string, or 0 if absent.
  static int parseBuild(String version) {
    final RegExpMatch? match = _buildPattern.firstMatch(version);
    if (match == null) return 0;
    return int.tryParse(match.group(1)!) ?? 0;
  }

  /// The capability set implied by this machine's Windows build.
  ///
  /// Returns [EngineCapabilities.unknown] off Windows, so a developer running
  /// the settings UI on another platform sees every effect correctly disabled
  /// rather than a UI that pretends to work.
  static EngineCapabilities detect() {
    if (!Platform.isWindows) return const EngineCapabilities.unknown();
    final int build = parseBuild(Platform.operatingSystemVersion);
    if (build == 0) return const EngineCapabilities.unknown();
    return EngineCapabilities.fromBuild(build);
  }
}
