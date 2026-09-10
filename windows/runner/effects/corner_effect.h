#ifndef RUNNER_EFFECTS_CORNER_EFFECT_H_
#define RUNNER_EFFECTS_CORNER_EFFECT_H_

#include "compatibility.h"
#include "managed_window.h"

namespace effects {

// Rounds window corners by whichever of the two mechanisms the plan chose.
//
// See docs/FEASIBILITY.md §2.1 for why there are two and what each costs. In
// short: DWM gives anti-aliased corners at one of two fixed sizes and keeps
// the system shadow; a window region gives any radius but is aliased and
// removes the shadow.
class CornerEffect {
 public:
  // Applies the plan, including turning the effect off again when the plan no
  // longer asks for it. Returns false only when Windows refused a change we
  // actually attempted.
  static bool Apply(ManagedWindow& window, const EffectPlan& plan);

 private:
  static bool ApplySystem(ManagedWindow& window, const EffectPlan& plan);
  static bool ApplyPrecise(ManagedWindow& window, const EffectPlan& plan);
};

}  // namespace effects

#endif  // RUNNER_EFFECTS_CORNER_EFFECT_H_
