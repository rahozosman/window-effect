import 'package:flutter/widgets.dart';

/// Motion vocabulary for the settings UI.
///
/// Every animation here answers one of: feedback, orientation, continuity,
/// focus. Anything that would be decoration only is left out — the brief asks
/// for a utility that feels calm, not one that performs.
abstract final class Motion {
  /// Switch thumbs, chip presses, hover states. Feedback must land inside the
  /// window where an interaction still feels instantaneous.
  static const Duration fast = Duration(milliseconds: 140);

  /// Value changes propagating into the live preview.
  static const Duration base = Duration(milliseconds: 240);

  /// Section reveals, preset changes touching many properties at once.
  static const Duration slow = Duration(milliseconds: 420);

  /// Entering the page.
  static const Duration entrance = Duration(milliseconds: 560);

  /// Decelerating — the default for anything arriving or settling.
  static const Curve enter = Curves.easeOutCubic;

  /// Symmetric — for values that move between two states in place.
  static const Curve inOut = Curves.easeInOutCubic;

  /// A restrained overshoot for the switch thumb only.
  static const Curve spring = Curves.easeOutBack;

  /// Honours the OS "reduce motion" setting. When set, durations collapse to
  /// zero and the UI jumps between states instead of animating.
  static Duration duration(BuildContext context, Duration value) {
    return MediaQuery.disableAnimationsOf(context) ? Duration.zero : value;
  }
}
