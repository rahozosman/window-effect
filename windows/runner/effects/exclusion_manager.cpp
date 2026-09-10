#include "exclusion_manager.h"

namespace effects {

std::wstring ToLowerAscii(const std::wstring& value) {
  std::wstring result = value;
  for (size_t index = 0; index < result.size(); ++index) {
    const wchar_t character = result[index];
    if (character >= L'A' && character <= L'Z') {
      result[index] =
          static_cast<wchar_t>(character - L'A' + L'a');
    }
  }
  return result;
}

void ExclusionManager::SetUserList(
    const std::vector<std::wstring>& executables) {
  user_list_.clear();
  user_list_.reserve(executables.size());
  for (size_t index = 0; index < executables.size(); ++index) {
    const std::wstring normalised = ToLowerAscii(executables[index]);
    if (!normalised.empty()) {
      user_list_.push_back(normalised);
    }
  }
}

bool ExclusionManager::IsProtected(const std::wstring& executable) const {
  if (executable.empty()) {
    return true;
  }
  const std::vector<std::wstring>& list = ProtectedApps();
  for (size_t index = 0; index < list.size(); ++index) {
    if (list[index] == executable) {
      return true;
    }
  }
  return false;
}

bool ExclusionManager::IsExcludedByUser(const std::wstring& executable) const {
  for (size_t index = 0; index < user_list_.size(); ++index) {
    if (user_list_[index] == executable) {
      return true;
    }
  }
  return false;
}

const std::vector<std::wstring>& ExclusionManager::ProtectedApps() {
  static const std::vector<std::wstring> apps = {
      // Shell surfaces. Restyling these visibly breaks the desktop.
      L"explorer.exe",
      L"searchhost.exe",
      L"startmenuexperiencehost.exe",
      L"shellexperiencehost.exe",
      L"textinputhost.exe",
      L"sihost.exe",
      L"dwm.exe",
      // Security surfaces. These must never be modified at all.
      L"consent.exe",
      L"lsaiso.exe",
      L"logonui.exe",
      L"winlogon.exe",
      L"csrss.exe",
  };
  return apps;
}

}  // namespace effects
