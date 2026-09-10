#ifndef RUNNER_EFFECTS_SHADOW_EFFECT_H_
#define RUNNER_EFFECTS_SHADOW_EFFECT_H_

#include <windows.h>

#include <memory>
#include <unordered_map>
#include <vector>

#include "compatibility.h"
#include "managed_window.h"

namespace effects {

// How a shadow looks. Two shadows with equal appearance and equal size share
// the same painted bitmap, so a move never repaints.
struct ShadowAppearance {
  int strength = 0;
  int blur = 0;
  int offset = 0;
  int radius = 0;

  bool operator==(const ShadowAppearance& other) const {
    return strength == other.strength && blur == other.blur &&
           offset == other.offset && radius == other.radius;
  }
  bool operator!=(const ShadowAppearance& other) const {
    return !(*this == other);
  }

  // Everything except strength. Strength is applied when the bitmap is blended
  // rather than when it is built, so a window merely losing focus — which
  // changes only the strength — costs nothing but a re-blend. Painting is the
  // expensive half: a blur and a bitmap upload the size of the window, on the
  // engine thread, which is also where the animation frames come from.
  bool SameShape(const ShadowAppearance& other) const {
    return blur == other.blur && offset == other.offset &&
           radius == other.radius;
  }
};

// One layered window painted with a soft shadow, kept directly beneath its
// target in the Z-order.
//
// DWM's own shadow cannot be tuned at all (docs/FEASIBILITY.md §2.2), so the
// Strength/Blur/Offset settings can only drive a shadow we draw ourselves.
// WS_EX_TRANSPARENT keeps it out of hit-testing, so it cannot interfere with
// clicking, dragging or resizing the window it sits behind.
class ShadowWindow {
 public:
  ShadowWindow();
  ~ShadowWindow();

  ShadowWindow(const ShadowWindow&) = delete;
  ShadowWindow& operator=(const ShadowWindow&) = delete;

  bool Create();

  // Positions the shadow for |frame| and puts it directly below |target|.
  // Repaints only when the size or the appearance actually changed — a plain
  // move is a single SetWindowPos.
  void Follow(HWND target, const RECT& frame, const ShadowAppearance& look);

  // Moves an already-painted shadow, reusing whatever appearance it carries.
  // Passing a fresh ShadowAppearance here instead would repaint the window
  // blank, since a default-constructed one has zero strength.
  void Reposition(HWND target, const RECT& frame);

  bool painted() const { return bitmap_ != nullptr; }

  void Hide();

 private:
  bool Repaint(const RECT& bounds, const ShadowAppearance& look);
  // Changes only how dark the existing bitmap is drawn.
  bool Reblend(int strength);

  HWND window_ = nullptr;
  HBITMAP bitmap_ = nullptr;
  void* pixels_ = nullptr;

  SIZE painted_size_ = {};
  ShadowAppearance painted_look_;
  bool visible_ = false;
};

// Owns one ShadowWindow per target that currently wants a shadow.
//
// Kept separate from ManagedWindow, which records only what a window looked
// like before we touched it — a companion window is something we added, not
// something to put back.
class ShadowManager {
 public:
  ShadowManager() = default;
  ~ShadowManager();

  ShadowManager(const ShadowManager&) = delete;
  ShadowManager& operator=(const ShadowManager&) = delete;

  // Creates, updates or removes the shadow for this window as the plan
  // dictates. A window that is being dragged has its shadow hidden rather than
  // chased — see docs/FEASIBILITY.md §2.2.
  void Update(const ManagedWindow& window, const EffectPlan& plan);

  // Moves an existing shadow without reconsidering anything else. Called from
  // the location-change hot path.
  void Reposition(const ManagedWindow& window);

  void Hide(HWND target);
  void Remove(HWND target);
  void RemoveAll();

 private:
  std::unordered_map<HWND, std::unique_ptr<ShadowWindow>> shadows_;
};

// Fills |alpha| with a |width| x |height| coverage mask: the window silhouette
// blurred and offset downwards, with the silhouette itself knocked back out so
// the shadow never darkens a window that is itself see-through.
//
// Exposed for clarity rather than reuse — it is the whole of the shadow's
// visual character in one function.
void BuildShadowAlpha(int width, int height, int window_width,
                      int window_height, int pad, const ShadowAppearance& look,
                      std::vector<unsigned char>* alpha);

}  // namespace effects

#endif  // RUNNER_EFFECTS_SHADOW_EFFECT_H_
