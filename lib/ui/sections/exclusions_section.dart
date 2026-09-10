import 'package:flutter/material.dart';

import '../../core/design/motion.dart';
import '../../core/design/tokens.dart';
import '../../models/effect_settings.dart';
import '../../models/protected_apps.dart';
import '../../platform/effects_channel.dart';
import '../../state/settings_controller.dart';
import '../widgets/add_app_dialog.dart';
import '../widgets/app_chip.dart';
import '../widgets/section_card.dart';

/// The user's exclusion list, plus a read-only view of what is always
/// protected.
///
/// Showing the built-in list matters: a utility that reaches into every window
/// on the system should be able to prove which ones it refuses to touch.
class ExclusionsSection extends StatefulWidget {
  const ExclusionsSection({
    super.key,
    required this.settings,
    required this.controller,
  });

  final EffectSettings settings;
  final SettingsController controller;

  @override
  State<ExclusionsSection> createState() => _ExclusionsSectionState();
}

class _ExclusionsSectionState extends State<ExclusionsSection> {
  bool _showProtected = false;

  Future<void> _add() async {
    final String? name = await AddAppDialog.show(
      context,
      widget.settings.excludedApps,
      // Without an engine there is nothing to open the picker, so the button
      // is left off rather than shown doing nothing.
      onBrowse: widget.controller.status.state == EngineState.unavailable
          ? null
          : widget.controller.browseForExecutable,
    );
    if (name == null) return;
    widget.controller.addExclusion(name);
  }

  @override
  Widget build(BuildContext context) {
    final AppColors colors = AppColors.of(context);
    final TextTheme text = Theme.of(context).textTheme;
    final List<String> excluded = widget.settings.excludedApps;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: <Widget>[
        SectionHeader(
          'Excluded Apps',
          trailing: excluded.isEmpty
              ? null
              : Text('${excluded.length}', style: text.bodySmall),
        ),
        SectionCard(
          padding: const EdgeInsets.all(Space.md),
          children: <Widget>[
            if (excluded.isEmpty)
              Padding(
                padding: const EdgeInsets.only(bottom: Space.sm),
                child: Text(
                  'Nothing excluded yet. Add an application if its windows '
                  'behave badly with effects applied.',
                  style: text.bodyMedium,
                ),
              ),
            Wrap(
              spacing: Space.xs,
              runSpacing: Space.xs,
              children: <Widget>[
                ...excluded.map(
                  (String name) => AppChip(
                    label: name,
                    onRemove: () => widget.controller.removeExclusion(name),
                  ),
                ),
                AddAppButton(onTap: _add),
              ],
            ),
            const SizedBox(height: Space.sm),
            const RowDivider(),
            const SizedBox(height: Space.sm),
            _ProtectedToggle(
              expanded: _showProtected,
              count: kProtectedApps.length,
              onTap: () => setState(() => _showProtected = !_showProtected),
            ),
            AnimatedSize(
              duration: Motion.duration(context, Motion.base),
              curve: Motion.enter,
              alignment: Alignment.topCenter,
              child: _showProtected
                  ? Padding(
                      padding: const EdgeInsets.only(top: Space.sm),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: <Widget>[
                          Text(
                            'The shell and security surfaces are never '
                            'modified, whatever the settings say.',
                            style: text.bodyMedium?.copyWith(
                              color: colors.textTertiary,
                            ),
                          ),
                          const SizedBox(height: Space.xs),
                          Wrap(
                            spacing: Space.xs,
                            runSpacing: Space.xs,
                            children: kProtectedApps
                                .map((String name) =>
                                    AppChip(label: name, locked: true))
                                .toList(growable: false),
                          ),
                        ],
                      ),
                    )
                  : const SizedBox(width: double.infinity),
            ),
          ],
        ),
      ],
    );
  }
}

class _ProtectedToggle extends StatelessWidget {
  const _ProtectedToggle({
    required this.expanded,
    required this.count,
    required this.onTap,
  });

  final bool expanded;
  final int count;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final AppColors colors = AppColors.of(context);
    return MouseRegion(
      cursor: SystemMouseCursors.click,
      child: GestureDetector(
        behavior: HitTestBehavior.opaque,
        onTap: onTap,
        child: Row(
          children: <Widget>[
            AnimatedRotation(
              duration: Motion.duration(context, Motion.fast),
              turns: expanded ? 0.25 : 0,
              child: Icon(
                Icons.chevron_right_rounded,
                size: 16,
                color: colors.textTertiary,
              ),
            ),
            const SizedBox(width: Space.xxs),
            Text(
              'Always protected ($count)',
              style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                    color: colors.textSecondary,
                    fontWeight: FontWeight.w500,
                  ),
            ),
          ],
        ),
      ),
    );
  }
}
