#ifndef RUNNER_EFFECTS_TRANSPARENCY_EFFECT_H_
#define RUNNER_EFFECTS_TRANSPARENCY_EFFECT_H_

#include "compatibility.h"
#include "managed_window.h"

namespace effects {

// Owns everything that writes a window's layered alpha: both the Transparency
// setting and inactive dimming. They are the same mechanism, so one manager
// owns it — two writers on the same window's alpha would overwrite each other.
//
// Also applies the blur behind a transparent window, because blur is only
// visible through one and the two must be turned on and off together.
//
// Two failure modes are expected rather than exceptional (FEASIBILITY §2.3):
//
//  * A window the application already made layered is never touched. Forcing
//    LWA_ALPHA onto something that calls UpdateLayeredWindow corrupts it. The
//    compatibility manager filters these out before we see them.
//  * An elevated window rejects SetWindowLongPtr with ERROR_ACCESS_DENIED.
//    That is UIPI working as designed and cannot be worked around from a
//    medium-integrity process; it is reported, once, and the window is left
//    exactly as it was.
class TransparencyEffect {
 public:
  static bool Apply(ManagedWindow& window, const EffectPlan& plan);

 private:
  static bool ApplyAlpha(ManagedWindow& window, const EffectPlan& plan);
  static bool ApplyBlur(ManagedWindow& window, const EffectPlan& plan);
};

}  // namespace effects

#endif  // RUNNER_EFFECTS_TRANSPARENCY_EFFECT_H_
