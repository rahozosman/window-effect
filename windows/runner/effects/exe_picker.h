#ifndef RUNNER_EFFECTS_EXE_PICKER_H_
#define RUNNER_EFFECTS_EXE_PICKER_H_

#include <windows.h>

#include <string>

namespace effects {

// Opens the system file dialog filtered to executables and returns the chosen
// file's base name, lowercased — "chrome.exe", not the full path, because the
// exclusion list matches on the name a window's process reports.
//
// Returns an empty string when the user cancels or the dialog cannot be
// created. Blocks the calling thread while the dialog is up, which is fine:
// it runs on the platform thread, and the effects engine has its own.
std::wstring PickExecutable(HWND owner);

}  // namespace effects

#endif  // RUNNER_EFFECTS_EXE_PICKER_H_
