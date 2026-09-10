#include "exe_picker.h"

#include <shobjidl.h>

#include "exclusion_manager.h"

namespace effects {
namespace {

std::wstring BaseName(const std::wstring& path) {
  const size_t separator = path.find_last_of(L"\\/");
  if (separator == std::wstring::npos) {
    return path;
  }
  return path.substr(separator + 1);
}

}  // namespace

std::wstring PickExecutable(HWND owner) {
  // COM is already initialised on the platform thread by wWinMain, which is
  // the only thread this runs on.
  IFileOpenDialog* dialog = nullptr;
  HRESULT result =
      ::CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                         IID_PPV_ARGS(&dialog));
  if (FAILED(result) || dialog == nullptr) {
    return std::wstring();
  }

  COMDLG_FILTERSPEC filters[1] = {};
  filters[0].pszName = L"Applications";
  filters[0].pszSpec = L"*.exe";
  dialog->SetFileTypes(1, filters);
  dialog->SetTitle(L"Choose an application to exclude");

  DWORD options = 0;
  if (SUCCEEDED(dialog->GetOptions(&options))) {
    dialog->SetOptions(options | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST |
                       FOS_FORCEFILESYSTEM);
  }

  std::wstring name;

  result = dialog->Show(owner);
  if (SUCCEEDED(result)) {
    IShellItem* item = nullptr;
    if (SUCCEEDED(dialog->GetResult(&item)) && item != nullptr) {
      PWSTR path = nullptr;
      if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) &&
          path != nullptr) {
        name = ToLowerAscii(BaseName(std::wstring(path)));
        ::CoTaskMemFree(path);
      }
      item->Release();
    }
  }
  // A cancelled dialog returns HRESULT_FROM_WIN32(ERROR_CANCELLED), which is a
  // normal outcome and not worth reporting.

  dialog->Release();
  return name;
}

}  // namespace effects
