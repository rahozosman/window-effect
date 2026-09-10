import 'package:flutter/material.dart';

import '../../core/design/tokens.dart';

/// An uppercase group label sitting above a [SectionCard].
class SectionHeader extends StatelessWidget {
  const SectionHeader(this.title, {super.key, this.trailing});

  final String title;
  final Widget? trailing;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(
        left: Space.xxs,
        right: Space.xxs,
        bottom: Space.sm,
      ),
      child: Row(
        children: <Widget>[
          Expanded(
            child: Text(
              title.toUpperCase(),
              style: Theme.of(context).textTheme.titleSmall,
            ),
          ),
          ?trailing,
        ],
      ),
    );
  }
}

/// The single container shape used for every group of settings.
///
/// One card per section, never a card per row — the brief explicitly rules out
/// "excessive cards", and nested surfaces are what make a settings screen feel
/// cluttered.
class SectionCard extends StatelessWidget {
  const SectionCard({super.key, required this.children, this.padding});

  final List<Widget> children;
  final EdgeInsetsGeometry? padding;

  @override
  Widget build(BuildContext context) {
    final AppColors colors = AppColors.of(context);
    return DecoratedBox(
      decoration: BoxDecoration(
        color: colors.surface,
        borderRadius: BorderRadius.circular(Radii.lg),
        border: Border.all(color: colors.stroke),
      ),
      child: Padding(
        padding: padding ??
            const EdgeInsets.symmetric(
              horizontal: Space.lg,
              vertical: Space.xs,
            ),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          mainAxisSize: MainAxisSize.min,
          children: children,
        ),
      ),
    );
  }
}

/// A hairline between rows inside a [SectionCard].
class RowDivider extends StatelessWidget {
  const RowDivider({super.key});

  @override
  Widget build(BuildContext context) {
    return Container(
      height: 1,
      color: AppColors.of(context).stroke,
    );
  }
}
