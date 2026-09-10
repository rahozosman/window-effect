#include "os_version.h"

#include <windows.h>

#include <string>

#include "dwm_api.h"
#include "log.h"

namespace effects {
namespace {

// Matches RTL_OSVERSIONINFOW. Declared locally so the build does not need
// winternl.h, whose contents are officially subject to change.
struct VersionInfo {
  ULONG size;
  ULONG major;
  ULONG minor;
  ULONG build;
  ULONG platform_id;
  WCHAR service_pack[128];
};

typedef LONG(WINAPI* RtlGetVersionFn)(VersionInfo*);

// GetVersionEx lies to unmanifested applications. RtlGetVersion does not.
uint32_t QueryBuildNumber() {
  HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
  if (ntdll == nullptr) {
    return 0;
  }
  RtlGetVersionFn rtl_get_version = reinterpret_cast<RtlGetVersionFn>(
      ::GetProcAddress(ntdll, "RtlGetVersion"));
  if (rtl_get_version == nullptr) {
    return 0;
  }

  VersionInfo info = {};
  info.size = static_cast<ULONG>(sizeof(info));
  if (rtl_get_version(&info) != 0) {
    return 0;
  }
  return static_cast<uint32_t>(info.build);
}

}  // namespace

bool SystemUsesLightTheme() {
  HKEY key = nullptr;
  if (::RegOpenKeyExW(
          HKEY_CURRENT_USER,
          LR"(Software\Microsoft\Windows\CurrentVersion\Themes\Personalize)",
          0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
    return false;
  }

  DWORD value = 0;
  DWORD size = static_cast<DWORD>(sizeof(value));
  DWORD type = 0;
  const LSTATUS status = ::RegQueryValueExW(key, L"AppsUseLightTheme", nullptr,
                                            &type, reinterpret_cast<LPBYTE>(&value),
                                            &size);
  ::RegCloseKey(key);

  if (status != ERROR_SUCCESS || type != REG_DWORD) {
    return false;
  }
  return value != 0;
}

namespace {

Capabilities ComputeCapabilities() {
  Capabilities capabilities;
  capabilities.build_number = QueryBuildNumber();

  const DwmApi& dwm = DwmApi::Instance();
  const uint32_t build = capabilities.build_number;

  // Window regions and layered windows predate every build we support, so
  // these need no version gate at all.
  capabilities.precise_corners = true;
  capabilities.custom_shadow = true;
  capabilities.transparency = true;

  capabilities.blur_behind = dwm.has_composition_attribute() && build >= 17763;
  capabilities.system_corners = dwm.has_window_attributes() && build >= 22000;
  capabilities.border_color = capabilities.system_corners;
  capabilities.legacy_mica =
      dwm.has_window_attributes() && build >= 22000 && build < 22621;
  capabilities.system_backdrop = dwm.has_window_attributes() && build >= 22621;

  LogInfo("windows build " + std::to_string(build) +
          "; corners=" + (capabilities.system_corners ? "1" : "0") +
          " backdrop=" + (capabilities.system_backdrop ? "1" : "0") +
          " legacyMica=" + (capabilities.legacy_mica ? "1" : "0") +
          " blur=" + (capabilities.blur_behind ? "1" : "0"));

  return capabilities;
}

}  // namespace

const Capabilities& GetCapabilities() {
  static const Capabilities capabilities = ComputeCapabilities();
  return capabilities;
}

uint32_t WindowsBuildNumber() {
  return GetCapabilities().build_number;
}

}  // namespace effects
