import 'package:flutter/material.dart';

import '../../core/design/motion.dart';
import '../../core/design/tokens.dart';

/// A restrained track-and-thumb switch.
///
/// Deliberately not [Switch]: Material's version brings a ripple, an overlay
/// and an icon that all read as louder than this UI wants.
class PremiumSwitch extends StatelessWidget {
  const PremiumSwitch({
    super.key,
    required this.value,
    required this.onChanged,
    this.semanticLabel,
  });

  final bool value;
  final ValueChanged<bool>? onChanged;
  final String? semanticLabel;

  static const double _width = 38;
  static const double _height = 21;
  static const double _thumb = 15;
  static const double _inset = 3;

  bool get _enabled => onChanged != null;

  @override
  Widget build(BuildContext context) {
    final AppColors colors = AppColors.of(context);
    final Duration duration = Motion.duration(context, Motion.fast);

    return Semantics(
      label: semanticLabel,
      toggled: value,
      enabled: _enabled,
      child: MouseRegion(
        cursor: _enabled ? SystemMouseCursors.click : SystemMouseCursors.basic,
        child: GestureDetector(
          behavior: HitTestBehavior.opaque,
          onTap: _enabled ? () => onChanged!(!value) : null,
          child: AnimatedOpacity(
            duration: duration,
            opacity: _enabled ? 1 : 0.4,
            child: AnimatedContainer(
              duration: duration,
              curve: Motion.inOut,
              width: _width,
              height: _height,
              decoration: BoxDecoration(
                color: value ? colors.accent : colors.trackOff,
                borderRadius: BorderRadius.circular(Radii.pill),
              ),
              child: AnimatedAlign(
                duration: duration,
                curve: Motion.spring,
                alignment: value ? Alignment.centerRight : Alignment.centerLeft,
                child: Padding(
                  padding: const EdgeInsets.symmetric(horizontal: _inset),
                  child: Container(
                    width: _thumb,
                    height: _thumb,
                    decoration: const BoxDecoration(
                      color: Colors.white,
                      shape: BoxShape.circle,
                      boxShadow: <BoxShadow>[
                        BoxShadow(
                          color: Color(0x33000000),
                          blurRadius: 3,
                          offset: Offset(0, 1),
                        ),
                      ],
                    ),
                  ),
                ),
              ),
            ),
          ),
        ),
      ),
    );
  }
}
