import 'package:flutter/material.dart';

import '../../core/design/motion.dart';
import '../../core/design/tokens.dart';

/// One line of settings: label on the left, control on the right, with an
/// optional description and an optional inline note.
///
/// [unavailableReason] is the honest path — when Windows cannot do something on
/// this machine, the row stays visible, dims, and says why, instead of
/// vanishing and leaving the user wondering what happened.
class SettingRow extends StatelessWidget {
  const SettingRow({
    super.key,
    required this.label,
    this.description,
    this.note,
    this.trailing,
    this.below,
    this.unavailableReason,
    this.indent = false,
  });

  final String label;
  final String? description;

  /// A short qualifier shown beside the label, e.g. "≈8 px".
  final String? note;

  final Widget? trailing;

  /// Full-width content under the label — sliders, chip rows.
  final Widget? below;

  /// When non-null the row is disabled and this explains why.
  final String? unavailableReason;

  /// Marks a row as dependent on the one above it.
  final bool indent;

  bool get _enabled => unavailableReason == null;

  @override
  Widget build(BuildContext context) {
    final AppColors colors = AppColors.of(context);
    final TextTheme text = Theme.of(context).textTheme;
    final String? subtitle = unavailableReason ?? description;

    return AnimatedOpacity(
      duration: Motion.duration(context, Motion.fast),
      opacity: _enabled ? 1 : 0.55,
      child: Padding(
        padding: EdgeInsets.only(
          left: indent ? Space.md : 0,
          top: Space.sm,
          bottom: Space.sm,
        ),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: <Widget>[
            Row(
              crossAxisAlignment: CrossAxisAlignment.center,
              children: <Widget>[
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: <Widget>[
                      Row(
                        crossAxisAlignment: CrossAxisAlignment.baseline,
                        textBaseline: TextBaseline.alphabetic,
                        children: <Widget>[
                          Flexible(
                            child: Text(label, style: text.bodyLarge),
                          ),
                          if (note != null) ...<Widget>[
                            const SizedBox(width: Space.xs),
                            Text(
                              note!,
                              style: text.bodySmall?.copyWith(
                                color: colors.textTertiary,
                              ),
                            ),
                          ],
                        ],
                      ),
                      if (subtitle != null) ...<Widget>[
                        const SizedBox(height: 3),
                        Text(
                          subtitle,
                          style: text.bodyMedium?.copyWith(
                            color: _enabled
                                ? colors.textSecondary
                                : colors.textTertiary,
                          ),
                        ),
                      ],
                    ],
                  ),
                ),
                if (trailing != null) ...<Widget>[
                  const SizedBox(width: Space.md),
                  trailing!,
                ],
              ],
            ),
            if (below != null) ...<Widget>[
              const SizedBox(height: Space.xs),
              below!,
            ],
          ],
        ),
      ),
    );
  }
}
