#include "startup.h"

#include <windows.h>

namespace effects {
namespace {

const wchar_t kRunKeyPath[] =
    LR"(Software\Microsoft\Windows\CurrentVersion\Run)";
const wchar_t kValueName[] = L"WindowEffects";

std::string DescribeError(LSTATUS status) {
  return "Windows refused the startup entry (error " +
         std::to_string(static_cast<long>(status)) + ").";
}

}  // namespace

std::wstring Startup::LaunchCommand() {
  wchar_t path[MAX_PATH] = {};
  const DWORD length =
      ::GetModuleFileNameW(nullptr, path, static_cast<DWORD>(MAX_PATH));
  if (length == 0 || length >= static_cast<DWORD>(MAX_PATH)) {
    return std::wstring();
  }

  // Quoted because Program Files has a space in it, and --tray so a login
  // starts into the background rather than opening the settings window.
  std::wstring command;
  command.append(L"\"");
  command.append(path, length);
  command.append(L"\" --tray");
  return command;
}

bool Startup::IsEnabled() {
  HKEY key = nullptr;
  if (::RegOpenKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, KEY_QUERY_VALUE,
                      &key) != ERROR_SUCCESS) {
    return false;
  }

  DWORD type = 0;
  DWORD size = 0;
  const LSTATUS status =
      ::RegQueryValueExW(key, kValueName, nullptr, &type, nullptr, &size);
  ::RegCloseKey(key);

  return status == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ);
}

std::string Startup::SetEnabled(bool enabled) {
  HKEY key = nullptr;
  const LSTATUS opened = ::RegCreateKeyExW(
      HKEY_CURRENT_USER, kRunKeyPath, 0, nullptr, REG_OPTION_NON_VOLATILE,
      KEY_SET_VALUE, nullptr, &key, nullptr);
  if (opened != ERROR_SUCCESS) {
    return DescribeError(opened);
  }

  LSTATUS status = ERROR_SUCCESS;
  if (enabled) {
    const std::wstring command = LaunchCommand();
    if (command.empty()) {
      ::RegCloseKey(key);
      return "Could not work out where this application is installed.";
    }
    // Byte count including the terminator, which RegSetValueEx requires for
    // string values.
    const DWORD bytes =
        static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t));
    status = ::RegSetValueExW(
        key, kValueName, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(command.c_str()), bytes);
  } else {
    status = ::RegDeleteValueW(key, kValueName);
    if (status == ERROR_FILE_NOT_FOUND) {
      // Already absent, which is exactly what was asked for.
      status = ERROR_SUCCESS;
    }
  }

  ::RegCloseKey(key);
  return status == ERROR_SUCCESS ? std::string() : DescribeError(status);
}

}  // namespace effects
