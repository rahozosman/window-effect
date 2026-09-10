#ifndef RUNNER_EFFECTS_STARTUP_H_
#define RUNNER_EFFECTS_STARTUP_H_

#include <string>

namespace effects {

// Registers the utility under HKCU\...\Run so it comes back after a reboot.
//
// Per-user rather than machine-wide on purpose: HKLM would need elevation to
// write, and this is a per-user preference, not a system service.
//
// The registered command carries --tray, so a login starts the engine quietly
// in the background instead of opening the settings window in the user's face.
class Startup {
 public:
  static bool IsEnabled();

  // Returns an empty string on success, or a message describing why it failed.
  // Failures here are reported rather than swallowed: a switch that says "on"
  // when the registry says otherwise is worse than an error.
  static std::string SetEnabled(bool enabled);

 private:
  static std::wstring LaunchCommand();
};

}  // namespace effects

#endif  // RUNNER_EFFECTS_STARTUP_H_
