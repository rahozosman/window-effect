#ifndef RUNNER_EFFECTS_EXCLUSION_MANAGER_H_
#define RUNNER_EFFECTS_EXCLUSION_MANAGER_H_

#include <string>
#include <vector>

namespace effects {

// Lowercases an ASCII executable name. CharLowerW would bring locale rules
// into a comparison that only ever sees file names.
std::wstring ToLowerAscii(const std::wstring& value);

// Decides whether a process is off limits.
//
// Owned by the engine thread and deliberately unsynchronised: the user list is
// swapped while a new Config is applied, which happens on that same thread.
class ExclusionManager {
 public:
  ExclusionManager() = default;

  // Replaces the user list. Entries are normalised to lowercase.
  void SetUserList(const std::vector<std::wstring>& executables);

  // Shell and security surfaces, enforced whatever the settings say. Checked
  // before the user list so it can never be overridden.
  bool IsProtected(const std::wstring& executable) const;

  // On the user's own exclusion list.
  bool IsExcludedByUser(const std::wstring& executable) const;

  // The built-in list. Mirrored in lib/models/protected_apps.dart, which is
  // what the settings screen displays.
  static const std::vector<std::wstring>& ProtectedApps();

 private:
  std::vector<std::wstring> user_list_;
};

}  // namespace effects

#endif  // RUNNER_EFFECTS_EXCLUSION_MANAGER_H_
