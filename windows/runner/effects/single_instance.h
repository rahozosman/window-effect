#ifndef RUNNER_EFFECTS_SINGLE_INSTANCE_H_
#define RUNNER_EFFECTS_SINGLE_INSTANCE_H_

#include <windows.h>

namespace effects {

// Keeps exactly one engine running per logged-in session.
//
// Two engines would be worse than a duplicate window: both capture each
// window's "original" state, both restore it, and the second one's idea of the
// original is the first one's modified version. A window could be left rounded
// and layered with nothing left that knows how to undo it.
//
// This matters more here than in an ordinary app, because startup registration
// plus launch-to-tray makes a second launch likely — the utility is already
// running invisibly when the user clicks its shortcut.
class SingleInstance {
 public:
  SingleInstance() = default;
  ~SingleInstance();

  SingleInstance(const SingleInstance&) = delete;
  SingleInstance& operator=(const SingleInstance&) = delete;

  // True when this process is the one that should run.
  bool Claim();

  // Asks the instance that is already running to show its settings window.
  // The caller then exits, so a second launch reads as "bring it back" rather
  // than as nothing happening at all.
  static void SignalExistingInstance();

  // The broadcast the running instance listens for. Registered once per
  // process; zero if Windows refused to register it.
  static UINT ShowSettingsMessage();

 private:
  HANDLE mutex_ = nullptr;
};

}  // namespace effects

#endif  // RUNNER_EFFECTS_SINGLE_INSTANCE_H_
