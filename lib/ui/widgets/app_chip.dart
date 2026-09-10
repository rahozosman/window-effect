import 'package:flutter/material.dart';

import '../../core/design/motion.dart';
import '../../core/design/tokens.dart';

/// A removable executable name in the exclusion list.
class AppChip extends StatefulWidget {
  const AppChip({
    super.key,
    required this.label,
    this.onRemove,
    this.locked = false,
  });

  final String label;

  /// Null for entries the user cannot remove.
  final VoidCallback? onRemove;

  /// Built-in protected entries render with a lock instead of a close button.
  final bool locked;

  @override
  State<AppChip> createState() => _AppChipState();
}

class _AppChipState extends State<AppChip> {
  bool _hovered = false;

  @override
  Widget build(BuildContext context) {
    final AppColors colors = AppColors.of(context);
    final bool removable = widget.onRemove != null && !widget.locked;

    return MouseRegion(
      onEnter: (_) => setState(() => _hovered = true),
      onExit: (_) => setState(() => _hovered = false),
      child: AnimatedContainer(
        duration: Motion.duration(context, Motion.fast),
        curve: Motion.inOut,
        padding: EdgeInsets.only(
          left: Space.sm,
          right: removable ? Space.xxs : Space.sm,
          top: 6,
          bottom: 6,
        ),
        decoration: BoxDecoration(
          color: colors.surfaceRaised,
          borderRadius: BorderRadius.circular(Radii.sm),
          border: Border.all(
            color: _hovered && removable ? colors.strokeStrong : colors.stroke,
          ),
        ),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: <Widget>[
            if (widget.locked) ...<Widget>[
              Icon(Icons.lock_outline, size: 12, color: colors.textTertiary),
              const SizedBox(width: 6),
            ],
            Text(
              widget.label,
              style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                    color: widget.locked
                        ? colors.textTertiary
                        : colors.textPrimary,
                  ),
            ),
            if (removable) ...<Widget>[
              const SizedBox(width: Space.xxs),
              _RemoveButton(onTap: widget.onRemove!, visible: _hovered),
            ],
          ],
        ),
      ),
    );
  }
}

class _RemoveButton extends StatelessWidget {
  const _RemoveButton({required this.onTap, required this.visible});

  final VoidCallback onTap;
  final bool visible;

  @override
  Widget build(BuildContext context) {
    final AppColors colors = AppColors.of(context);
    return MouseRegion(
      cursor: SystemMouseCursors.click,
      child: GestureDetector(
        onTap: onTap,
        behavior: HitTestBehavior.opaque,
        child: AnimatedOpacity(
          duration: Motion.duration(context, Motion.fast),
          opacity: visible ? 1 : 0.35,
          child: SizedBox(
            width: 18,
            height: 18,
            child: Icon(
              Icons.close_rounded,
              size: 13,
              color: visible ? colors.danger : colors.textTertiary,
            ),
          ),
        ),
      ),
    );
  }
}

/// The dashed "+ Add Application" affordance that opens the picker dialog.
class AddAppButton extends StatefulWidget {
  const AddAppButton({super.key, required this.onTap});

  final VoidCallback onTap;

  @override
  State<AddAppButton> createState() => _AddAppButtonState();
}

class _AddAppButtonState extends State<AddAppButton> {
  bool _hovered = false;

  @override
  Widget build(BuildContext context) {
    final AppColors colors = AppColors.of(context);
    return MouseRegion(
      cursor: SystemMouseCursors.click,
      onEnter: (_) => setState(() => _hovered = true),
      onExit: (_) => setState(() => _hovered = false),
      child: GestureDetector(
        behavior: HitTestBehavior.opaque,
        onTap: widget.onTap,
        child: AnimatedContainer(
          duration: Motion.duration(context, Motion.fast),
          curve: Motion.inOut,
          padding: const EdgeInsets.symmetric(horizontal: Space.sm, vertical: 6),
          decoration: BoxDecoration(
            color: _hovered ? colors.accentMuted : Colors.transparent,
            borderRadius: BorderRadius.circular(Radii.sm),
            border: Border.all(
              color: _hovered ? colors.accent : colors.strokeStrong,
            ),
          ),
          child: Row(
            mainAxisSize: MainAxisSize.min,
            children: <Widget>[
              Icon(
                Icons.add_rounded,
                size: 14,
                color: _hovered ? colors.accent : colors.textSecondary,
              ),
              const SizedBox(width: 6),
              Text(
                'Add Application',
                style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                      color: _hovered ? colors.accent : colors.textSecondary,
                      fontWeight: FontWeight.w500,
                    ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
