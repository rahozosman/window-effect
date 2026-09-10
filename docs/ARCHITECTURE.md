# Architecture

## Process model

One executable. The Flutter engine hosts the settings UI; the effects engine is
native C++ compiled into the same runner and driven by its own thread.

```
window_effect.exe
├── Platform thread ── Flutter engine ── settings UI (idle when hidden)
├── Engine thread ──── message loop + WinEvent hooks + effect managers
└── Tray/window thread (= platform thread) ── Shell_NotifyIcon, show/hide
```

**Why the engine gets its own thread.** `SetWinEventHook` with
`WINEVENT_OUTOFCONTEXT` delivers callbacks on the *installing thread's* message
queue. `EVENT_OBJECT_LOCATIONCHANGE` fires hundreds of times per second while a
window is dragged. Installing those hooks on Flutter's platform thread would
push that traffic through the same queue that services the UI. A dedicated
thread with its own `GetMessage` loop keeps the two completely isolated.

**Why not a separate background daemon.** A pure Win32 daemon idles at ~4 MB
versus ~100 MB for a Flutter host, but costs a second binary, an IPC transport,
independent startup registration and two lifecycles to keep in sync. The brief's
own diagram specifies `Flutter → Platform Channel → Engine`. Single process it
is; the memory cost is recorded honestly in the README rather than hidden.

Idle cost is dominated by the *window*, not the process: Flutter stops producing
frames when its window is hidden, so tray‑resident CPU is effectively zero.

**Window lifecycle.** The settings window is not the application. Closing it
hides it and the engine keeps working; the tray icon brings it back; only the
tray's "Exit" quits. A `--tray` launch — what the Run key registers — starts
with no window at all. If Windows refuses the notification icon, closing the
window quits instead, because hiding into a tray that is not there would leave
the user with a running utility and no way to reach it.

Exit is deliberately indirect: the tray callback only posts `WM_QUIT`. Tearing
down from inside a menu handler would destroy the tray icon that is still
executing that handler. `wWinMain` unwinds after the loop instead, and that
unwind is what restores every managed window.

**One engine per session.** A named mutex claims the session; a second launch
broadcasts a request for the running instance to show its window and exits.
Two engines would be worse than a duplicate window: each would capture the
other's *modified* window as the original, and the last one to restore would
put the other's changes back permanently.

## Layers

```
  Flutter UI (lib/ui)
        │  reads/writes
  SettingsController (lib/state)          ChangeNotifier, debounced
        │                    ╲
  SettingsStore (lib/services)  EffectsChannel (lib/platform)
        │  %APPDATA% JSON            │  MethodChannel + EventChannel
        ▼                            ▼
                          EffectsBridge (windows/runner/effects)
                                     │  posts to engine thread
                                     ▼
                                EffectsEngine
   ┌────────────┬──────────────┬─────────────┬──────────────┐
   │ Detection  │ Classifier   │ Exclusions  │ Compatibility│
   └────────────┴──────────────┴─────────────┴──────────────┘
   ┌────────┬────────┬──────────────┬──────────┬────────────┐
   │ Corner │ Shadow │ Transparency │ Backdrop │ ActiveWin  │
   └────────┴────────┴──────────────┴──────────┴────────────┘
                                     │
                          DwmApi · Win32 · Composition
```

Each effect manager owns one concern and exposes exactly one entry point:

```cpp
static bool Apply(ManagedWindow& w, const EffectPlan& plan);
```

The plan already encodes every version gate, window quirk and interaction
between settings, so a manager never re-derives them and two managers can never
disagree. `Apply` also handles being switched *off*: a plan that no longer asks
for an effect is how the effect gets unwound, which means there is no separate
teardown path that could be forgotten.

Undo lives on `ManagedWindow` rather than in the managers, because the thing
being restored is the window's original state, not the manager's. `ShadowManager`
is the one exception — a companion window is something we added, not something
to put back — so it owns its own map and is torn down explicitly.

## The per-window pipeline

```
WinEvent  →  Classify  →  Exclusions  →  Compatibility  →  Apply
                 │             │              │              │
              reject        reject         downgrade      ManagedWindow
                                                              │
                                          lifecycle events ───┤
                                                              ▼
                                                     Restore on destroy,
                                                     shutdown, or panic
```

`ManagedWindow` stores the **original** state of every attribute it changes
(ex‑style bits, window region, border colour, backdrop type). Restore replays
those values. This is what makes "never leave windows permanently corrupted"
structural rather than aspirational.

## Safety model

Four independent guarantees:

1. **Recorded originals.** Nothing is changed without first capturing what it
   was. `Restore` writes the captured value back, not a guessed default.
2. **Restore on every exit path.** Engine stop, tray "Pause", tray "Exit",
   master toggle off, window destroy, and `WM_ENDSESSION` — the last of which
   is the only chance a logoff or shutdown gives before the process is
   terminated outright.
3. **Failure is data, not an exception.** Every Win32/DWM call is checked;
   a failure marks that effect unsupported *for that window* and the pipeline
   continues with the rest.
4. **Panic latch.** Repeated failures against the same window quarantine it;
   repeated failures overall trip a global latch that stops applying new
   effects, restores everything managed, and reports the reason to the UI.
   Windows stays usable; the app stays alive.

## Motion

One animator, and it never moves a window.

`ProxyWindow` is a borderless, click-through, no-activate overlay with a DWM
thumbnail of the target drawn into it. `WindowAnimator` hides the real window
(layered alpha 0, the bit captured first like every other change), puts the
copy up in the same breath, and animates the copy's rect and opacity. At the
end the real window's alpha goes back and the copy is destroyed, in that order,
so there is never a frame with a hole in it.

**Why not move the real window.** That was the first version. `SetWindowPos`
every frame from 94% up to 100% animates fine and looks bad: resizing a window
asks the application to lay its content out again, so the target reflows and
repaints on every frame, while it is already busy starting up. Smooth motion
wrapped around stuttering content reads as lag. The compositor scales a
thumbnail on the GPU and asks the application nothing.

**Why minimising had no choice.** Windows marks a window iconic about three
milliseconds after the click — before `EVENT_SYSTEM_MINIMIZESTART` reaches
another process — and `SetWindowPos` on a minimised window rewrites the rect it
restores to. The copy is all that is left, and DWM still renders a thumbnail of
a window it has just minimised, because that is what taskbar previews are.

**What made it smooth**, none of which was the animation code:

1. DWM plays its own transition on the same window at the same moment.
   `DWMWA_TRANSITIONS_FORCEDISABLED` goes on at the first frame and stays on
   for as long as the window is managed.
2. `GetTickCount64` advances in ~15.6 ms steps, so "have 16 ms passed?" was
   only ever true every *other* step — a 60 fps animation running at 32. The
   clock is `QueryPerformanceCounter` now (`NowMs` in animation.h).
3. `WM_TIMER` is the lowest-priority message Windows has: it is delivered only
   when the queue is empty, and this queue receives every WinEvent in the
   system. The frame clock is a wait timeout instead, and the loop also ticks
   *inside* the message drain so a burst of events cannot hold frames back.
4. The companion shadow's blur and bitmap upload run on this same thread. While
   any window is animating, every other window's plan is deferred and applied
   afterwards, and a shadow whose only change is strength is now re-blended
   rather than repainted.

Measured on build 26200 after all four: 53 frames in 900 ms for a minimise,
0.6 ms of work per frame.

Corners, the shadow and transparency are held back for the animating window
too — a corner region is in window coordinates and would be wrong at every size
but the last. The animator's finished callback applies them in one pass once
the geometry has settled.

## Threading and channel discipline

Flutter's `MethodChannel` and `EventSink` may only be touched on the platform
thread. The engine thread never calls them directly. It posts to a hidden
message window owned by the platform thread, whose `WndProc` drains a
mutex‑guarded queue and forwards to the channel.

Settings travel the other way as an immutable snapshot: the controller pushes a
whole `Config` struct, the engine swaps it under a lock and re‑evaluates managed
windows. No partial states, no torn reads.

## Event subscriptions

Registered once, on the engine thread:

| Event | Purpose | Cost |
|---|---|---|
| `EVENT_OBJECT_SHOW` / `HIDE` | window appears / disappears | low |
| `EVENT_OBJECT_DESTROY` | drop from the managed set | low |
| `EVENT_SYSTEM_FOREGROUND` | active‑window emphasis | low |
| `EVENT_OBJECT_CLOAKED` / `UNCLOAKED` | UWP + virtual desktops | low |
| `EVENT_SYSTEM_MINIMIZESTART` / `END` | suspend effects | low |
| `EVENT_SYSTEM_MOVESIZESTART` / `END` | hide/restore companion shadow | low |
| `EVENT_OBJECT_LOCATIONCHANGE` | shadow + precise‑corner tracking | **high** |

`LOCATIONCHANGE` is registered **only** when the companion shadow or precise
corners are enabled — `Config::NeedsLocationTracking()` decides, and the engine
re-evaluates it on every config change. Turning the shadow off and leaving
corners on the system path means the hook is never installed at all.

When it is installed, the handler is built to be cheap rather than throttled:

- The frame is derived from `GetWindowRect` plus an inset cached at
  classification time, so the hot path never makes a DWM round trip.
- A pure move is one `SetWindowPos` on the shadow window and nothing else.
- Only an actual size change triggers the expensive path (rebuild the window
  region, repaint the shadow bitmap).
- Interactive drags do not reach it at all: the shadow is hidden between
  `MOVESIZESTART` and `MOVESIZEEND`.

## Dart-side structure

| Path | Responsibility |
|---|---|
| `lib/core/design/` | tokens, motion curves, light/dark themes |
| `lib/models/` | `EffectSettings`, enums, presets, capability model |
| `lib/services/` | JSON persistence, OS build detection |
| `lib/platform/` | `MethodChannel`/`EventChannel` wrapper, degrades when absent |
| `lib/state/` | `SettingsController` + `InheritedNotifier` scope |
| `lib/ui/` | page, sections, reusable controls, live preview |

`SettingsController` is the only writer. It debounces disk writes (400 ms) and
engine pushes (120 ms) separately: the preview stays instant, the disk stays
quiet, and the engine is never asked to re‑evaluate on every slider tick.

## Configuration

`%APPDATA%\WindowEffects\config.json`, written atomically (temp file + rename)
with a `schemaVersion` field. A corrupt or unreadable file falls back to the
Premium preset rather than refusing to start.

## Native file layout (Phase 2+)

```
windows/runner/effects/
  effects_bridge.{h,cpp}        MethodChannel ↔ engine, platform-thread side
  dispatcher.{h,cpp}            engine thread → platform thread marshalling
  engine.{h,cpp}                lifecycle, config swap, panic latch
  config.{h,cpp}                native mirror of EffectSettings
  os_version.{h,cpp}            build number, capability probe
  dwm_api.{h,cpp}               GetProcAddress-resolved DWM/accent entry points
  window_detection.{h,cpp}      WinEvent hooks + enumeration
  window_classifier.{h,cpp}     "is this a normal app window?"
  exclusion_manager.{h,cpp}     built-in + user exclusion lists
  compatibility.{h,cpp}         per-window downgrade decisions
  managed_window.{h,cpp}        original-state capture and restore
  animation.{h,cpp}             entrance and exit motion, curves, frame clock
  proxy_window.{h,cpp}          the DWM thumbnail copy that actually moves
  corner_effect.{h,cpp}
  shadow_effect.{h,cpp}
  transparency_effect.{h,cpp}
  backdrop_effect.{h,cpp}
  active_window_effect.{h,cpp}
  tray.{h,cpp}                  Shell_NotifyIcon menu
  startup.{h,cpp}               Run-key registration
  exe_picker.{h,cpp}            IFileOpenDialog, returns a bare exe name
  self_backdrop.{h,cpp}         Mica on our own settings window
  single_instance.{h,cpp}       one engine per session
  log.{h,cpp}                   rolling file log
```

One concern per file, matching the brief's module list.
