import 'package:flutter/material.dart';

import '../core/design/motion.dart';
import '../core/design/tokens.dart';
import '../models/effect_settings.dart';
import '../state/settings_controller.dart';
import '../state/settings_scope.dart';
import 'preview/live_preview.dart';
import 'sections/appearance_section.dart';
import 'sections/exclusions_section.dart';
import 'sections/motion_section.dart';
import 'sections/presets_section.dart';
import 'sections/system_section.dart';
import 'widgets/notice_bar.dart';
import 'widgets/page_header.dart';

/// The one and only screen.
///
/// A single scrolling column, capped at a readable width. The brief rules out
/// complicated navigation, and there is not enough here to justify a sidebar —
/// everything fits in four sections.
class HomePage extends StatelessWidget {
  const HomePage({super.key});

  static const double _maxContentWidth = 660;

  @override
  Widget build(BuildContext context) {
    final SettingsController controller = SettingsScope.of(context);

    if (!controller.isLoaded) return const _LoadingState();

    final EffectSettings settings = controller.settings;

    return Scaffold(
      body: SafeArea(
        child: Center(
          child: ConstrainedBox(
            constraints: const BoxConstraints(maxWidth: _maxContentWidth),
            child: Scrollbar(
              child: ListView(
                padding: const EdgeInsets.fromLTRB(
                  Space.xl,
                  Space.xl,
                  Space.xl,
                  Space.xxxl,
                ),
                children: <Widget>[
                  PageHeader(
                    status: controller.status,
                    effectsEnabled: settings.effectsEnabled,
                    onEnabledChanged: (bool v) => controller.update(
                      (EffectSettings s) => s.copyWith(effectsEnabled: v),
                    ),
                    onResume: () => controller.setPaused(false),
                  ),
                  if (controller.notice != null) ...<Widget>[
                    const SizedBox(height: Space.md),
                    NoticeBar(message: controller.notice!),
                  ],
                  const SizedBox(height: Space.lg),
                  LivePreview(settings: settings),
                  const SizedBox(height: Space.xl),
                  _Dimmed(
                    dimmed: !settings.effectsEnabled,
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.stretch,
                      children: <Widget>[
                        AppearanceSection(
                          settings: settings,
                          capabilities: controller.capabilities,
                          controller: controller,
                        ),
                        const SizedBox(height: Space.xl),
                        MotionSection(
                          settings: settings,
                          controller: controller,
                        ),
                        const SizedBox(height: Space.xl),
                        PresetsSection(
                          settings: settings,
                          controller: controller,
                        ),
                        const SizedBox(height: Space.xl),
                        ExclusionsSection(
                          settings: settings,
                          controller: controller,
                        ),
                      ],
                    ),
                  ),
                  const SizedBox(height: Space.xl),
                  SystemSection(
                    settings: settings,
                    capabilities: controller.capabilities,
                    controller: controller,
                  ),
                ],
              ),
            ),
          ),
        ),
      ),
    );
  }
}

/// Fades the effect settings while the master switch is off — they still read,
/// and stay editable, but clearly are not doing anything right now.
class _Dimmed extends StatelessWidget {
  const _Dimmed({required this.dimmed, required this.child});

  final bool dimmed;
  final Widget child;

  @override
  Widget build(BuildContext context) {
    return AnimatedOpacity(
      duration: Motion.duration(context, Motion.base),
      curve: Motion.inOut,
      opacity: dimmed ? 0.45 : 1,
      child: child,
    );
  }
}

class _LoadingState extends StatelessWidget {
  const _LoadingState();

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: Center(
        child: SizedBox(
          width: 18,
          height: 18,
          child: CircularProgressIndicator(
            strokeWidth: 2,
            color: AppColors.of(context).textTertiary,
          ),
        ),
      ),
    );
  }
}
