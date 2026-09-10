#ifndef RUNNER_EFFECTS_ACTIVE_WINDOW_EFFECT_H_
#define RUNNER_EFFECTS_ACTIVE_WINDOW_EFFECT_H_

#include "compatibility.h"
#include "managed_window.h"

namespace effects {

// Marks the focused window with a one-pixel border in the system accent
// colour, and hands every other window's border back to the system default.
//
// This is the whole of the emphasis effect that lives here. The other two
// halves are applied elsewhere by design: the shadow differential is a shadow
// concern, and inactive dimming is layered alpha, which transparency owns —
// two managers writing the same window's alpha would fight.
//
// DWMWA_BORDER_COLOR is the cheapest possible mechanism: one DWM attribute, no
// extra windows, no flicker, and it is the same colour Windows itself uses on
// focused borders, which is what keeps the effect from reading as an add-on.
class ActiveWindowEffect {
 public:
  static bool Apply(ManagedWindow& window, const EffectPlan& plan);
};

}  // namespace effects

#endif  // RUNNER_EFFECTS_ACTIVE_WINDOW_EFFECT_H_
