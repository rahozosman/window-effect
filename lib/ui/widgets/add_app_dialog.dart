import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../../core/design/motion.dart';
import '../../core/design/tokens.dart';
import '../../models/protected_apps.dart';
import '../../state/settings_controller.dart';

/// Asks for an executable name to exclude.
///
/// Accepts a bare name, a name without `.exe`, or a pasted full path — all
/// normalised by [SettingsController.normalizeExecutableName]. Typing stays
/// the fastest route for a name the user already knows; Browse is for the one
/// they would have to go and look up.
class AddAppDialog extends StatefulWidget {
  const AddAppDialog({super.key, required this.existing, this.onBrowse});

  /// Names already excluded, so duplicates can be caught before submitting.
  final List<String> existing;

  /// Opens the native file picker. Null when there is no engine to ask, in
  /// which case the button is not offered rather than offered and inert.
  final Future<String?> Function()? onBrowse;

  static Future<String?> show(
    BuildContext context,
    List<String> existing, {
    Future<String?> Function()? onBrowse,
  }) {
    return showDialog<String>(
      context: context,
      barrierColor: Colors.black.withValues(alpha: 0.45),
      builder: (BuildContext context) =>
          AddAppDialog(existing: existing, onBrowse: onBrowse),
    );
  }

  @override
  State<AddAppDialog> createState() => _AddAppDialogState();
}

class _AddAppDialogState extends State<AddAppDialog> {
  final TextEditingController _field = TextEditingController();
  final FocusNode _focus = FocusNode();
  String? _error;
  bool _browsing = false;

  @override
  void initState() {
    super.initState();
    _focus.requestFocus();
  }

  @override
  void dispose() {
    _field.dispose();
    _focus.dispose();
    super.dispose();
  }

  void _submit() {
    final String name = SettingsController.normalizeExecutableName(_field.text);
    if (name.isEmpty) {
      setState(() => _error = 'Enter an executable name, such as chrome.exe.');
      return;
    }
    if (widget.existing.contains(name)) {
      setState(() => _error = '$name is already excluded.');
      return;
    }
    Navigator.of(context).pop(name);
  }

  Future<void> _browse() async {
    final Future<String?> Function()? browse = widget.onBrowse;
    if (browse == null || _browsing) return;

    setState(() => _browsing = true);
    final String? name = await browse();
    if (!mounted) return;
    setState(() => _browsing = false);

    // Null is a cancelled dialog, which is not an error and needs no message.
    if (name == null) return;
    if (widget.existing.contains(name)) {
      setState(() => _error = '$name is already excluded.');
      return;
    }
    Navigator.of(context).pop(name);
  }

  void _pickSuggestion(String name) {
    _field.text = name;
    _field.selection = TextSelection.collapsed(offset: name.length);
    setState(() => _error = null);
  }

  @override
  Widget build(BuildContext context) {
    final AppColors colors = AppColors.of(context);
    final TextTheme text = Theme.of(context).textTheme;
    final List<String> suggestions = kSuggestedExclusions
        .where((String name) => !widget.existing.contains(name))
        .take(6)
        .toList(growable: false);

    return Dialog(
      backgroundColor: Colors.transparent,
      elevation: 0,
      child: ConstrainedBox(
        constraints: const BoxConstraints(maxWidth: 420),
        child: DecoratedBox(
          decoration: BoxDecoration(
            color: colors.surface,
            borderRadius: BorderRadius.circular(Radii.lg),
            border: Border.all(color: colors.strokeStrong),
            boxShadow: <BoxShadow>[
              BoxShadow(
                color: Colors.black.withValues(alpha: 0.35),
                blurRadius: 32,
                offset: const Offset(0, 12),
              ),
            ],
          ),
          child: Padding(
            padding: const EdgeInsets.all(Space.lg),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: <Widget>[
                Text('Exclude an application', style: text.headlineSmall),
                const SizedBox(height: Space.xxs),
                Text(
                  'Windows belonging to this executable are left completely '
                  'untouched.',
                  style: text.bodyMedium,
                ),
                const SizedBox(height: Space.md),
                _Field(
                  controller: _field,
                  focusNode: _focus,
                  hasError: _error != null,
                  onSubmitted: (_) => _submit(),
                  onChanged: (_) {
                    if (_error != null) setState(() => _error = null);
                  },
                ),
                if (_error != null) ...<Widget>[
                  const SizedBox(height: Space.xs),
                  Text(
                    _error!,
                    style: text.bodyMedium?.copyWith(color: colors.danger),
                  ),
                ],
                if (suggestions.isNotEmpty) ...<Widget>[
                  const SizedBox(height: Space.md),
                  Text('Common choices', style: text.titleSmall),
                  const SizedBox(height: Space.xs),
                  Wrap(
                    spacing: Space.xs,
                    runSpacing: Space.xs,
                    children: suggestions
                        .map((String name) => _Suggestion(
                              label: name,
                              onTap: () => _pickSuggestion(name),
                            ))
                        .toList(growable: false),
                  ),
                ],
                const SizedBox(height: Space.lg),
                Row(
                  children: <Widget>[
                    if (widget.onBrowse != null)
                      _DialogButton(
                        label: _browsing ? 'Choosing…' : 'Browse…',
                        onTap: _browse,
                      ),
                    const Spacer(),
                    _DialogButton(
                      label: 'Cancel',
                      onTap: () => Navigator.of(context).pop(),
                    ),
                    const SizedBox(width: Space.xs),
                    _DialogButton(
                      label: 'Exclude',
                      primary: true,
                      onTap: _submit,
                    ),
                  ],
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

class _Field extends StatelessWidget {
  const _Field({
    required this.controller,
    required this.focusNode,
    required this.hasError,
    required this.onSubmitted,
    required this.onChanged,
  });

  final TextEditingController controller;
  final FocusNode focusNode;
  final bool hasError;
  final ValueChanged<String> onSubmitted;
  final ValueChanged<String> onChanged;

  @override
  Widget build(BuildContext context) {
    final AppColors colors = AppColors.of(context);
    return DecoratedBox(
      decoration: BoxDecoration(
        color: colors.surfaceSunken,
        borderRadius: BorderRadius.circular(Radii.sm),
        border: Border.all(color: hasError ? colors.danger : colors.stroke),
      ),
      child: Padding(
        padding: const EdgeInsets.symmetric(
          horizontal: Space.sm,
          vertical: Space.xs,
        ),
        child: TextField(
          controller: controller,
          focusNode: focusNode,
          autocorrect: false,
          enableSuggestions: false,
          inputFormatters: <TextInputFormatter>[
            LengthLimitingTextInputFormatter(260),
          ],
          style: Theme.of(context).textTheme.bodyLarge,
          cursorColor: colors.accent,
          cursorWidth: 1.5,
          decoration: InputDecoration(
            isDense: true,
            border: InputBorder.none,
            hintText: 'chrome.exe',
            hintStyle: Theme.of(context).textTheme.bodyLarge?.copyWith(
                  color: colors.textTertiary,
                  fontWeight: FontWeight.w400,
                ),
          ),
          onSubmitted: onSubmitted,
          onChanged: onChanged,
        ),
      ),
    );
  }
}

class _Suggestion extends StatelessWidget {
  const _Suggestion({required this.label, required this.onTap});

  final String label;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final AppColors colors = AppColors.of(context);
    return MouseRegion(
      cursor: SystemMouseCursors.click,
      child: GestureDetector(
        behavior: HitTestBehavior.opaque,
        onTap: onTap,
        child: Container(
          padding: const EdgeInsets.symmetric(horizontal: Space.sm, vertical: 5),
          decoration: BoxDecoration(
            color: colors.surfaceRaised,
            borderRadius: BorderRadius.circular(Radii.sm),
            border: Border.all(color: colors.stroke),
          ),
          child: Text(
            label,
            style: Theme.of(context)
                .textTheme
                .bodyMedium
                ?.copyWith(color: colors.textSecondary),
          ),
        ),
      ),
    );
  }
}

class _DialogButton extends StatefulWidget {
  const _DialogButton({
    required this.label,
    required this.onTap,
    this.primary = false,
  });

  final String label;
  final VoidCallback onTap;
  final bool primary;

  @override
  State<_DialogButton> createState() => _DialogButtonState();
}

class _DialogButtonState extends State<_DialogButton> {
  bool _hovered = false;

  @override
  Widget build(BuildContext context) {
    final AppColors colors = AppColors.of(context);
    final Color background = widget.primary
        ? (_hovered ? colors.accent : colors.accent.withValues(alpha: 0.88))
        : (_hovered ? colors.surfaceRaised : Colors.transparent);

    return MouseRegion(
      cursor: SystemMouseCursors.click,
      onEnter: (_) => setState(() => _hovered = true),
      onExit: (_) => setState(() => _hovered = false),
      child: GestureDetector(
        behavior: HitTestBehavior.opaque,
        onTap: widget.onTap,
        child: AnimatedContainer(
          duration: Motion.duration(context, Motion.fast),
          padding: const EdgeInsets.symmetric(
            horizontal: Space.md,
            vertical: Space.xs,
          ),
          decoration: BoxDecoration(
            color: background,
            borderRadius: BorderRadius.circular(Radii.sm),
            border: Border.all(
              color: widget.primary ? Colors.transparent : colors.stroke,
            ),
          ),
          child: Text(
            widget.label,
            style: Theme.of(context).textTheme.bodyLarge?.copyWith(
                  color: widget.primary ? Colors.white : colors.textSecondary,
                  fontWeight: FontWeight.w600,
                ),
          ),
        ),
      ),
    );
  }
}
