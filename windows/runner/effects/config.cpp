#include "config.h"

namespace effects {
namespace {

int Clamp(int value, int low, int high) {
  if (value < low) {
    return low;
  }
  if (value > high) {
    return high;
  }
  return value;
}

}  // namespace

bool Config::NeedsLocationTracking() const {
  if (!effects_enabled) {
    return false;
  }
  // The companion shadow has to be repositioned with the window, and a window
  // region has to be rebuilt whenever the window resizes.
  if (shadow_enabled) {
    return true;
  }
  return corners_enabled && corner_mode == CornerMode::kPrecise;
}

CornerMode CornerModeFromId(const std::string& id) {
  if (id == "precise") {
    return CornerMode::kPrecise;
  }
  return CornerMode::kSystem;
}

BackdropMode BackdropModeFromId(const std::string& id) {
  if (id == "none") {
    return BackdropMode::kNone;
  }
  if (id == "mica") {
    return BackdropMode::kMica;
  }
  if (id == "acrylic") {
    return BackdropMode::kAcrylic;
  }
  return BackdropMode::kAutomatic;
}

BlurPreset BlurPresetFromId(const std::string& id) {
  if (id == "none") {
    return BlurPreset::kNone;
  }
  if (id == "light") {
    return BlurPreset::kLight;
  }
  if (id == "strong") {
    return BlurPreset::kStrong;
  }
  return BlurPreset::kMedium;
}

ShadowDifference ShadowDifferenceFromId(const std::string& id) {
  if (id == "low") {
    return ShadowDifference::kLow;
  }
  if (id == "high") {
    return ShadowDifference::kHigh;
  }
  return ShadowDifference::kMedium;
}

AnimationStyle AnimationStyleFromId(const std::string& id) {
  if (id == "macos") {
    return AnimationStyle::kMacos;
  }
  if (id == "gnome") {
    return AnimationStyle::kGnome;
  }
  if (id == "fluent") {
    return AnimationStyle::kFluent;
  }
  if (id == "minimal") {
    return AnimationStyle::kMinimal;
  }
  return AnimationStyle::kPremium;
}

void ClampConfig(Config* config) {
  if (config == nullptr) {
    return;
  }
  config->corner_radius = Clamp(config->corner_radius, 0, 24);
  config->shadow_strength = Clamp(config->shadow_strength, 0, 100);
  config->shadow_blur = Clamp(config->shadow_blur, 0, 50);
  config->shadow_offset = Clamp(config->shadow_offset, 0, 20);
  // 80 is the floor the settings screen advertises for readability.
  config->opacity = Clamp(config->opacity, 80, 100);
  config->dim_amount = Clamp(config->dim_amount, 0, 20);
  // The same bounds the settings screen advertises, applied again here
  // because a hand-edited config file never went through the UI.
  config->animation_duration = Clamp(config->animation_duration, 150, 1500);
  config->animation_scale = Clamp(config->animation_scale, 80, 100);
  config->animation_lift = Clamp(config->animation_lift, 0, 60);
}

}  // namespace effects
