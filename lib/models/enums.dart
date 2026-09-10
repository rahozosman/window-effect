/// Enumerations shared by the settings model, the presets and the engine
/// bridge. Every enum carries a stable [id] used for JSON persistence and for
/// the native channel payload, so renaming a Dart symbol never breaks a saved
/// config.
library;

/// How rounded corners are produced.
///
/// See `docs/FEASIBILITY.md` §2.1 — Windows exposes only three system radii,
/// so an exact pixel radius requires a window region and its three costs.
enum CornerMode {
  /// `DWMWA_WINDOW_CORNER_PREFERENCE`. Anti-aliased, keeps the system shadow,
  /// but snaps to the system's ~4 px / ~8 px buckets. Windows 11 only.
  system('system', 'System corners'),

  /// `SetWindowRgn`. Exact radius, but aliased edges and no system shadow.
  /// The only corner path available on Windows 10.
  precise('precise', 'Precise corners');

  const CornerMode(this.id, this.label);

  final String id;
  final String label;

  static CornerMode fromId(String? id) =>
      values.firstWhere((e) => e.id == id, orElse: () => CornerMode.system);
}

/// System backdrop material requested for managed windows.
enum BackdropMode {
  none('none', 'None'),
  mica('mica', 'Mica'),
  acrylic('acrylic', 'Acrylic'),
  automatic('automatic', 'Automatic');

  const BackdropMode(this.id, this.label);

  final String id;
  final String label;

  static BackdropMode fromId(String? id) =>
      values.firstWhere((e) => e.id == id, orElse: () => BackdropMode.automatic);
}

/// Background blur strength. Maps to an accent state plus a tint alpha rather
/// than to a blur radius — the accent API exposes no radius.
enum BlurPreset {
  none('none', 'None'),
  light('light', 'Light'),
  medium('medium', 'Medium'),
  strong('strong', 'Strong');

  const BlurPreset(this.id, this.label);

  final String id;
  final String label;

  static BlurPreset fromId(String? id) =>
      values.firstWhere((e) => e.id == id, orElse: () => BlurPreset.medium);
}

/// How much stronger the focused window's shadow is than an inactive one's.
enum ShadowDifference {
  low('low', 'Low', 0.12),
  medium('medium', 'Medium', 0.24),
  high('high', 'High', 0.38);

  const ShadowDifference(this.id, this.label, this.factor);

  final String id;
  final String label;

  /// Fraction of the base shadow strength removed from inactive windows.
  final double factor;

  static ShadowDifference fromId(String? id) =>
      values.firstWhere((e) => e.id == id, orElse: () => ShadowDifference.medium);
}

/// How a window animates as it appears, and as it goes to and comes back from
/// the taskbar.
///
/// The four control points are a cubic bezier in the CSS sense. They are the
/// authoritative definition of each style's feel: the settings preview reads
/// them here, and `windows/runner/effects/animation.cpp` carries the same four
/// numbers so the preview and the real window move identically. Change one and
/// the other has to change with it.
enum MotionStyle {
  /// Fast out of the gate and a long settle, scaling from the centre. The
  /// closest thing Windows can do to how a macOS window arrives.
  macos('macos', 'macOS', 0.32, 0.72, 0.0, 1.0, 88, 0, 520),

  /// A restrained zoom over a short distance. GNOME's own window-open is
  /// barely a zoom at all, which is exactly what makes it feel calm.
  gnome('gnome', 'GNOME', 0.16, 1.0, 0.3, 1.0, 93, 0, 460),

  /// No zoom: a rise from below with a hard deceleration, the way Windows 11
  /// moves its own surfaces.
  fluent('fluent', 'Fluent', 0.0, 0.0, 0.0, 1.0, 100, 36, 500),

  /// A little of both — a small zoom and a small rise, held slightly longer.
  premium('premium', 'Premium', 0.22, 1.0, 0.36, 1.0, 91, 14, 600),

  /// Opacity only. The cheapest thing that still reads as intentional.
  minimal('minimal', 'Minimal', 0.4, 0.0, 0.2, 1.0, 100, 0, 340);

  const MotionStyle(
    this.id,
    this.label,
    this.x1,
    this.y1,
    this.x2,
    this.y2,
    this.defaultScale,
    this.defaultLift,
    this.defaultDuration,
  );

  final String id;
  final String label;

  /// Cubic bezier control points, CSS order.
  final double x1;
  final double y1;
  final double x2;
  final double y2;

  /// Percent of final size the window starts at. 100 means no zoom.
  final int defaultScale;

  /// Pixels below its final position the window starts at. 0 means no rise.
  final int defaultLift;

  final int defaultDuration;

  /// One line for the style picker.
  String get summary => switch (this) {
        MotionStyle.macos => 'Scales up from the centre and settles slowly.',
        MotionStyle.gnome => 'A short, calm zoom. Out of the way quickly.',
        MotionStyle.fluent =>
          'Rises from below, no zoom. The way Windows 11 moves its own surfaces.',
        MotionStyle.premium => 'A small zoom and a small rise, held a beat longer.',
        MotionStyle.minimal => 'Fade only. The least motion that still reads.',
      };

  static MotionStyle fromId(String? id) =>
      values.firstWhere((e) => e.id == id, orElse: () => MotionStyle.premium);
}

/// Identifier of a built-in preset, or [custom] once the user deviates.
enum PresetId {
  gnome('gnome', 'GNOME'),
  macos('macos', 'macOS'),
  fluent('fluent', 'Fluent'),
  premium('premium', 'Premium'),
  minimal('minimal', 'Minimal'),
  custom('custom', 'Custom');

  const PresetId(this.id, this.label);

  final String id;
  final String label;

  static PresetId fromId(String? id) =>
      values.firstWhere((e) => e.id == id, orElse: () => PresetId.premium);
}
