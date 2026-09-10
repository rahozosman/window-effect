#ifndef RUNNER_EFFECTS_SELF_BACKDROP_H_
#define RUNNER_EFFECTS_SELF_BACKDROP_H_

#include <windows.h>

namespace effects {

// Gives the settings window the same Mica backdrop the utility hands out.
//
// A tool that adds backdrops to other people's windows and paints its own flat
// grey is advertising that it does not trust its own effect. This is the one
// place the utility applies something to itself.
//
// Returns true only when the backdrop is genuinely in effect, which is the
// signal the Dart side needs: it paints a translucent page background only
// when there is something behind it to show through. On anything older than
// build 22621 this does nothing and returns false, and the window stays
// exactly as opaque as it was.
bool ApplySelfBackdrop(HWND window);

}  // namespace effects

#endif  // RUNNER_EFFECTS_SELF_BACKDROP_H_
