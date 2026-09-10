#include "dispatcher.h"

#include "log.h"

namespace effects {
namespace {

const wchar_t kWindowClassName[] = L"WindowEffectsDispatcher";
const UINT kMessageDrain = WM_APP + 1;

}  // namespace

Dispatcher::~Dispatcher() {
  Stop();
}

bool Dispatcher::Start() {
  if (window_ != nullptr) {
    return true;
  }

  HINSTANCE instance = ::GetModuleHandleW(nullptr);

  // Registered once for the life of the process. A second registration would
  // fail with ERROR_CLASS_ALREADY_EXISTS, which is harmless but noisy.
  static ATOM window_class_atom = 0;
  if (window_class_atom == 0) {
    WNDCLASSEXW window_class = {};
    window_class.cbSize = static_cast<UINT>(sizeof(window_class));
    window_class.lpfnWndProc = &Dispatcher::WndProc;
    window_class.hInstance = instance;
    window_class.lpszClassName = kWindowClassName;
    window_class_atom = ::RegisterClassExW(&window_class);
    if (window_class_atom == 0) {
      LogError("could not register the dispatcher window class");
      return false;
    }
  }

  // HWND_MESSAGE: never visible, never enumerated, never painted.
  window_ = ::CreateWindowExW(0, kWindowClassName, L"", 0, 0, 0, 0, 0,
                              HWND_MESSAGE, nullptr, instance, nullptr);
  if (window_ == nullptr) {
    LogError("could not create the dispatcher window");
    return false;
  }

  // Set after creation; WndProc treats a null pointer as "not ready yet",
  // which covers the WM_NCCREATE/WM_CREATE pair that arrives before this line.
  ::SetWindowLongPtrW(window_, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(this));

  {
    std::lock_guard<std::mutex> guard(mutex_);
    running_ = true;
  }
  return true;
}

void Dispatcher::Stop() {
  {
    std::lock_guard<std::mutex> guard(mutex_);
    running_ = false;
    tasks_.clear();
  }

  if (window_ != nullptr) {
    ::SetWindowLongPtrW(window_, GWLP_USERDATA, 0);
    ::DestroyWindow(window_);
    window_ = nullptr;
  }
}

void Dispatcher::Post(std::function<void()> task) {
  HWND target = nullptr;
  {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!running_) {
      return;
    }
    tasks_.push_back(std::move(task));
    target = window_;
  }

  if (target != nullptr) {
    ::PostMessageW(target, kMessageDrain, 0, 0);
  }
}

void Dispatcher::Drain() {
  std::vector<std::function<void()>> pending;
  {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!running_) {
      return;
    }
    // Swapped out so a task that posts another task cannot deadlock or grow
    // the batch we are already running.
    pending.swap(tasks_);
  }

  for (size_t index = 0; index < pending.size(); ++index) {
    if (pending[index]) {
      pending[index]();
    }
  }
}

LRESULT CALLBACK Dispatcher::WndProc(HWND window, UINT message, WPARAM wparam,
                                     LPARAM lparam) {
  if (message == kMessageDrain) {
    Dispatcher* dispatcher = reinterpret_cast<Dispatcher*>(
        ::GetWindowLongPtrW(window, GWLP_USERDATA));
    if (dispatcher != nullptr) {
      dispatcher->Drain();
    }
    return 0;
  }
  return ::DefWindowProcW(window, message, wparam, lparam);
}

}  // namespace effects
