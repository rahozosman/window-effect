import 'package:flutter/material.dart';

import '../../core/design/motion.dart';
import '../../core/design/tokens.dart';

/// One option in a [SegmentedSelector].
class SegmentOption<T> {
  const SegmentOption({
    required this.value,
    required this.label,
    this.unavailableReason,
  });

  final T value;
  final String label;

  /// When set the segment is shown dimmed and cannot be picked. Keeping the
  /// segment visible tells the user the option exists but not on this build.
  final String? unavailableReason;
}

/// A compact inline choice row.
///
/// Used for anything with three to five mutually exclusive options — backdrop
/// material, blur preset, shadow difference. A dropdown would hide the
/// alternatives; radio buttons would cost four rows.
class SegmentedSelector<T> extends StatelessWidget {
  const SegmentedSelector({
    super.key,
    required this.options,
    required this.value,
    required this.onChanged,
    this.enabled = true,
  });

  final List<SegmentOption<T>> options;
  final T value;
  final ValueChanged<T>? onChanged;
  final bool enabled;

  @override
  Widget build(BuildContext context) {
    final AppColors colors = AppColors.of(context);
    return DecoratedBox(
      decoration: BoxDecoration(
        color: colors.surfaceSunken,
        borderRadius: BorderRadius.circular(Radii.sm + 2),
        border: Border.all(color: colors.stroke),
      ),
      child: Padding(
        padding: const EdgeInsets.all(3),
        child: Row(
          children: options
              .map((SegmentOption<T> option) => Expanded(
                    child: _Segment<T>(
                      option: option,
                      selected: option.value == value,
                      enabled: enabled && option.unavailableReason == null,
                      onTap: onChanged == null
                          ? null
                          : () => onChanged!(option.value),
                    ),
                  ))
              .toList(growable: false),
        ),
      ),
    );
  }
}

class _Segment<T> extends StatelessWidget {
  const _Segment({
    required this.option,
    required this.selected,
    required this.enabled,
    required this.onTap,
  });

  final SegmentOption<T> option;
  final bool selected;
  final bool enabled;
  final VoidCallback? onTap;

  @override
  Widget build(BuildContext context) {
    final AppColors colors = AppColors.of(context);
    final Color foreground = selected
        ? colors.textPrimary
        : (enabled ? colors.textSecondary : colors.textTertiary);

    return Tooltip(
      message: option.unavailableReason ?? '',
      waitDuration: const Duration(milliseconds: 400),
      child: MouseRegion(
        cursor: enabled ? SystemMouseCursors.click : SystemMouseCursors.basic,
        child: GestureDetector(
          behavior: HitTestBehavior.opaque,
          onTap: enabled ? onTap : null,
          child: AnimatedContainer(
            duration: Motion.duration(context, Motion.fast),
            curve: Motion.inOut,
            height: 28,
            alignment: Alignment.center,
            decoration: BoxDecoration(
              color: selected ? colors.surfaceRaised : Colors.transparent,
              borderRadius: BorderRadius.circular(Radii.sm - 1),
              border: Border.all(
                color: selected ? colors.strokeStrong : Colors.transparent,
              ),
            ),
            child: AnimatedDefaultTextStyle(
              duration: Motion.duration(context, Motion.fast),
              style: Theme.of(context).textTheme.bodyMedium!.copyWith(
                    color: enabled ? foreground : colors.textTertiary,
                    fontWeight: selected ? FontWeight.w600 : FontWeight.w400,
                  ),
              child: Opacity(
                opacity: enabled ? 1 : 0.5,
                child: Text(option.label, maxLines: 1),
              ),
            ),
          ),
        ),
      ),
    );
  }
}
