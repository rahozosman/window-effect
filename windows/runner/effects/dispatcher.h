#ifndef RUNNER_EFFECTS_DISPATCHER_H_
#define RUNNER_EFFECTS_DISPATCHER_H_

#include <windows.h>

#include <functional>
#include <mutex>
#include <vector>

namespace effects {

// Moves work from the engine thread onto the Flutter platform thread.
//
// MethodChannel and EventSink may only be touched on the platform thread, and
// the engine thread must never block on it. A message-only window owned by the
// platform thread solves both: Post appends to a queue and wakes the window,
// and the queue is drained inside the platform thread's own message loop.
class Dispatcher {
 public:
  Dispatcher() = default;
  ~Dispatcher();

  Dispatcher(const Dispatcher&) = delete;
  Dispatcher& operator=(const Dispatcher&) = delete;

  // Creates the message-only window. Must be called on the platform thread.
  bool Start();

  // Drops any queued work and destroys the window. Platform thread only.
  void Stop();

  // Safe from any thread. Does nothing once Stop has run, so a late callback
  // from the engine thread during shutdown cannot reach a dead channel.
  void Post(std::function<void()> task);

 private:
  static LRESULT CALLBACK WndProc(HWND window, UINT message, WPARAM wparam,
                                  LPARAM lparam);
  void Drain();

  HWND window_ = nullptr;
  std::mutex mutex_;
  std::vector<std::function<void()>> tasks_;
  bool running_ = false;
};

}  // namespace effects

#endif  // RUNNER_EFFECTS_DISPATCHER_H_
