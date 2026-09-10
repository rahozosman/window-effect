#include "single_instance.h"

namespace effects {
namespace {

// Local\ rather than Global\: this is a per-user utility, and two users signed
// in at once are entitled to one engine each.
const wchar_t kMutexName[] = L"Local\\WindowEffects.SingleInstance";
const wchar_t kShowSettingsMessageName[] = L"WindowEffects.ShowSettings";

}  // namespace

SingleInstance::~SingleInstance() {
  if (mutex_ != nullptr) {
    ::ReleaseMutex(mutex_);
    ::CloseHandle(mutex_);
    mutex_ = nullptr;
  }
}

bool SingleInstance::Claim() {
  mutex_ = ::CreateMutexW(nullptr, TRUE, kMutexName);
  if (mutex_ == nullptr) {
    // No mutex means no way to tell, and refusing to start on a failure of the
    // guard itself would be worse than the duplicate it guards against.
    return true;
  }
  if (::GetLastError() == ERROR_ALREADY_EXISTS) {
    ::CloseHandle(mutex_);
    mutex_ = nullptr;
    return false;
  }
  return true;
}

UINT SingleInstance::ShowSettingsMessage() {
  static const UINT message = ::RegisterWindowMessageW(kShowSettingsMessageName);
  return message;
}

void SingleInstance::SignalExistingInstance() {
  const UINT message = ShowSettingsMessage();
  if (message == 0) {
    return;
  }
  // Broadcast rather than FindWindow: the running instance's window class is
  // Flutter's own, which every Flutter application on the machine shares.
  // A registered message id is unique across the system, so only the instance
  // that registered the same name acts on it.
  ::PostMessageW(HWND_BROADCAST, message, 0, 0);
}

}  // namespace effects
