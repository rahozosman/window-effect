#ifndef RUNNER_EFFECTS_CONFIG_H_
#define RUNNER_EFFECTS_CONFIG_H_

#include <string>
#include <vector>

namespace effects {

enum class CornerMode { kSystem, kPrecise };
enum class BackdropMode { kNone, kMica, kAcrylic, kAutomatic };
enum class BlurPreset { kNone, kLight, kMedium, kStrong };
enum class ShadowDifference { kLow, kMedium, kHigh };
enum class AnimationStyle { kMacos, kGnome, kFluent, kPremium, kMinimal };

// Native mirror of the Dart EffectSettings.
//
// The engine swaps this as a whole rather than accepting field updates, so it
// can never observe a half-applied change mid-evaluation.
struct Config {
  bool effects_enabled = true;

  bool corners_enabled = true;
  CornerMode corner_mode = CornerMode::kSystem;
  int corner_radius = 12;

  bool shadow_enabled = true;
  int shadow_strength = 35;
  int shadow_blur = 24;
  int shadow_offset = 6;

  bool transparency_enabled = false;
  int opacity = 95;

  bool blur_enabled = false;
  BlurPreset blur_preset = BlurPreset::kLight;

  BackdropMode backdrop = BackdropMode::kAutomatic;

  bool active_emphasis = true;
  bool active_border = false;
  ShadowDifference shadow_difference = ShadowDifference::kMedium;

  bool dim_inactive = false;
  int dim_amount = 8;

  // Motion. The curve itself lives in animation.cpp, keyed by the style; what
  // travels here is the style and the three numbers the user can move.
  bool animations_enabled = true;
  AnimationStyle animation_style = AnimationStyle::kPremium;
  int animation_duration = 600;
  int animation_scale = 91;
  int animation_lift = 14;
  bool animate_minimize = true;

  // Lowercase executable names from the user's exclusion list.
  std::vector<std::wstring> excluded_apps;

  // True when an enabled effect has to follow the window pixel by pixel, which
  // is the only reason to subscribe to EVENT_OBJECT_LOCATIONCHANGE. Both
  // callers arrive in Phase 3; until then the hook is never installed, which
  // is what keeps the idle profile flat.
  bool NeedsLocationTracking() const;
};

CornerMode CornerModeFromId(const std::string& id);
AnimationStyle AnimationStyleFromId(const std::string& id);
BackdropMode BackdropModeFromId(const std::string& id);
BlurPreset BlurPresetFromId(const std::string& id);
ShadowDifference ShadowDifferenceFromId(const std::string& id);

// Clamps every numeric field into the range the UI advertises, so a
// hand-edited config file cannot push an out-of-range value into an effect.
void ClampConfig(Config* config);

}  // namespace effects

#endif  // RUNNER_EFFECTS_CONFIG_H_
