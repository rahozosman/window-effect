# Technical Feasibility — What Windows Actually Allows

This document is the honest foundation of the project. Every effect below was
checked against the public Win32/DWM surface **and** against what real shipping
tools (Mica For Everyone, Glass2k, RoundedTB) actually achieve on third‑party
windows.

The guiding rule from the brief:

> Do not pretend that Windows allows arbitrary modification of every
> application's rendering.

## 0. The one fact that makes this product possible

`DwmSetWindowAttribute` is an **RPC into the DWM system service**, not a call
into the target process. It therefore works on windows owned by *other*
processes. This is exactly how *Mica For Everyone* applies backdrops to Notepad,
Regedit and Explorer without injecting code.

That gives us a legitimate, documented, crash‑safe channel for corners,
backdrop materials, border colour and caption colour. It is the backbone of the
engine.

What it does **not** give us: arbitrary rendering control. Everything outside
the DWM attribute set is either a compromise or impossible.

## 1. Capability matrix

| Effect | Mechanism | Verdict | Real limitation |
|---|---|---|---|
| Rounded corners (system radius) | `DWMWA_WINDOW_CORNER_PREFERENCE` (33) | ✅ Reliable | Win 11 22000+; **only 3 radii exist** |
| Rounded corners (exact px) | `SetWindowRgn` + `CreateRoundRectRgn` | ⚠️ Compromised | Aliased corners, **kills the DWM shadow**, must re‑apply on every resize |
| Custom shadow | Companion layered window + `UpdateLayeredWindow` | ⚠️ Works, costs care | Adds to DWM's shadow; needs Z‑order + lifecycle tracking |
| Transparency | `WS_EX_LAYERED` + `SetLayeredWindowAttributes` | ✅ Reliable | Breaks apps that already use `UpdateLayeredWindow`; blocked by UIPI on elevated windows |
| Background blur | `SetWindowCompositionAttribute` (undocumented) | ⚠️ Use sparingly | Acrylic variant causes **window‑drag lag**; API is undocumented |
| Mica / Acrylic backdrop | `DWMWA_SYSTEMBACKDROP_TYPE` (38) | ✅ Reliable | Win 11 22621+; **only visible where the app doesn't paint opaquely** |
| Mica (legacy) | `DWMWA_MICA_EFFECT` (1029, undocumented) | ⚠️ Fallback only | Builds 22000–22620 only |
| Active border | `DWMWA_BORDER_COLOR` (34) | ✅ Reliable | Win 11 22000+ |
| Dark window frame | `DWMWA_USE_IMMERSIVE_DARK_MODE` (20) | ✅ Reliable | Win 11 22000+ |
| Dim inactive | Layered alpha on non‑foreground windows | ⚠️ Works | Same constraints as transparency |
| Window lifecycle | `SetWinEventHook` (out‑of‑context) | ✅ Reliable | `LOCATIONCHANGE` is extremely noisy — must be opt‑in + coalesced |

Attribute IDs and minimum builds are taken from the current `DWMWINDOWATTRIBUTE`
reference (see Sources).

## 2. Effect‑by‑effect analysis

### 2.1 Rounded corners — the brief's radius list is not achievable as written

`DWM_WINDOW_CORNER_PREFERENCE` exposes exactly four values:

```
DWMWCP_DEFAULT     = 0   // let the system decide
DWMWCP_DONOTROUND  = 1   // square
DWMWCP_ROUND       = 2   // ~8 px, anti-aliased, shadow preserved
DWMWCP_ROUNDSMALL  = 3   // ~4 px, anti-aliased, shadow preserved
```

The radii are **baked into dwm.exe**. There is no `DWMWA_WINDOW_CORNER_RADIUS`
in the public enum, and no documented way to request 12 px.

The only way to get an exact radius is the pre‑Vista technique:

```cpp
HRGN rgn = CreateRoundRectRgn(0, 0, w + 1, h + 1, r * 2, r * 2);
SetWindowRgn(hwnd, rgn, TRUE);
```

This genuinely produces a 12 px corner — and costs three things:

1. **The DWM shadow disappears.** Windows does not shadow region‑clipped
   windows. Microsoft's own guidance notes that apps using window regions are
   excluded from automatic rounding.
2. **No anti‑aliasing.** `SetWindowRgn` clips to whole pixels, so the corner is
   visibly stair‑stepped. This is the single most "a weird program is
   manipulating my windows" artifact in the whole project.
3. **It must be re‑applied on every resize**, which means subscribing to
   `EVENT_OBJECT_LOCATIONCHANGE` for every managed window.

**Decision.** Two modes, native as the default:

- **System corners (default).** Radius slider maps onto the DWM buckets:
  `0 → DONOTROUND`, `1–6 → ROUNDSMALL (≈4 px)`, `7+ → ROUND (≈8 px)`.
  The UI states the *achieved* radius, never the requested one. Zero risk,
  anti‑aliased, shadow intact.
- **Precise corners (advanced, opt‑in).** `SetWindowRgn` with the exact radius,
  clearly labelled with its three costs, and automatically paired with the
  companion shadow (since DWM's is gone). Also the only corner path that works
  on Windows 10.

Refusing to fake the radius list is the correct call: showing "12 px" while
silently rendering 8 px would be lying to the user in their own settings screen.

### 2.2 Shadow — DWM's shadow is not tunable

There is no API to change DWM's shadow colour, blur, spread or offset. The
brief's Strength/Blur/Offset sliders can only drive a shadow **we draw
ourselves**:

A per‑target companion window, `WS_EX_LAYERED | WS_EX_TRANSPARENT |
WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW`, sized slightly larger than the target,
filled via `UpdateLayeredWindow` with a premultiplied‑ARGB gaussian, and kept
directly **below** the target in the Z‑order with
`SetWindowPos(shadow, target, …)`.

`WS_EX_TRANSPARENT` makes it invisible to hit‑testing, so the brief's "must not
interfere with mouse input or window movement" requirement is satisfied
structurally rather than by tuning.

Three hazards and how the implementation actually handles them:

| Hazard | Mitigation as built |
|---|---|
| Shadow lags behind during a drag | Hidden on `EVENT_SYSTEM_MOVESIZESTART`, restored on `MOVESIZEEND`. Nothing in another process can keep up with a drag, and a trailing shadow is worse than none. |
| Double shadow (ours + DWM's) | In system-corner mode ours is scaled to 55% and is purely additive; in precise mode DWM's is gone and ours runs at full strength. |
| Re-blurring on every move event | The bitmap is repainted **only when the size or appearance changes**. A pure move is one `SetWindowPos` — `UpdateLayeredWindow` content survives a move — so the expensive path never runs during ordinary window motion. |

The blur itself is three box passes over an 8-bit coverage mask, which is
indistinguishable from a gaussian at these radii and a fraction of the cost.
The window's own silhouette is knocked back out of the mask afterwards, one
pixel undersized: without that, the shadow would show *through* a window that
transparency has made see-through and darken it from behind.

The honest cost is memory. `UpdateLayeredWindow` needs a full-size premultiplied
bitmap, so a large window's shadow is a few megabytes. Two rules keep that
bounded: windows that are maximised or already cover 85% of their monitor get no
shadow at all (there is nothing to see behind them anyway), and any bitmap over
12 megapixels is refused outright.

Maximized, minimized, cloaked and fullscreen windows get no companion shadow —
which is also what GNOME and macOS do.

### 2.3 Transparency — reliable, with two hard blockers

`SetWindowLongPtr(GWL_EXSTYLE, … | WS_EX_LAYERED)` followed by
`SetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA)` works cross‑process and
is what every "make windows transparent" utility has used for twenty years.

Blockers we must detect rather than fight:

- **Windows already using `UpdateLayeredWindow`.** Forcing `LWA_ALPHA` onto them
  corrupts their rendering. Detect the pre‑existing `WS_EX_LAYERED` bit and skip.
- **UIPI / elevation.** A medium‑integrity process cannot call
  `SetWindowLongPtr` on an elevated window; the call fails with
  `ERROR_ACCESS_DENIED`. This is by design and cannot be worked around without
  running the utility elevated. We fail gracefully and report it.
- **GPU‑composited surfaces** (Chrome, Electron, DirectX) can flicker or go
  black. These belong on the default exclusion list, not in a workaround.

Clamping the floor to 80 % opacity, as the brief requires, keeps text legible.

### 2.4 Background blur — real, but the API is undocumented and slow

`SetWindowCompositionAttribute` is an unexported‑by‑header `user32` function
resolved with `GetProcAddress`. Two accent states matter:

- `ACCENT_ENABLE_BLURBEHIND` (3) — cheap gaussian, well behaved.
- `ACCENT_ENABLE_ACRYLICBLURBEHIND` (4) — adds a tint colour, and is the known
  cause of **window‑drag lag** since Windows 10 1903. It is still reported as
  problematic on Windows 11.

Neither exposes a blur radius, so the brief's None/Light/Medium/Strong presets
map to *material and tint*, not to a radius:

| Preset | Accent state | Tint alpha |
|---|---|---|
| None | `ACCENT_DISABLED` | — |
| Light | `ACCENT_ENABLE_BLURBEHIND` | — |
| Medium | `ACCENT_ENABLE_ACRYLICBLURBEHIND` | low |
| Strong | `ACCENT_ENABLE_ACRYLICBLURBEHIND` | high |

Blur is also only *visible* where the window is actually see‑through, so it is
correctly gated behind Transparency — exactly as the brief specifies.

### 2.5 Mica / Acrylic — works, but understand what you get

`DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, …)` with:

```
DWMSBT_AUTO             = 0
DWMSBT_NONE             = 1
DWMSBT_MAINWINDOW       = 2   // Mica
DWMSBT_TRANSIENTWINDOW  = 3   // Acrylic
DWMSBT_TABBEDWINDOW     = 4   // Mica Alt
```

The material is drawn **behind the window's content**. An ordinary Win32 app
paints its client area opaquely, so the material survives only where the app
does not paint — in practice, **the title bar**. Applying Mica to Notepad gives
you a Mica title bar, not a Mica document area.

That is not a defect to engineer around. A consistently Mica‑tinted title bar
across every app is precisely the subtle, native, Fluent look the brief asks
for, and it is what Mica For Everyone actually delivers.

Version fallback chain:

```
build ≥ 22621  →  DWMWA_SYSTEMBACKDROP_TYPE
build ≥ 22000  →  DWMWA_MICA_EFFECT (1029, undocumented BOOL)
build <  22000 →  no backdrop; fall back to blur-behind if transparency is on
```

`Automatic` resolves to Mica for main windows and Acrylic for transient ones,
degrading down the chain when unavailable.

### 2.6 Active window emphasis

`EVENT_SYSTEM_FOREGROUND` gives us focus changes for free. Emphasis is applied
through the *cheapest* mechanism that produces the effect:

- **Border** — `DWMWA_BORDER_COLOR`. Native, flicker‑free, no extra windows.
  `DWMWA_COLOR_DEFAULT` (0xFFFFFFFF) restores; `DWMWA_COLOR_NONE` (0xFFFFFFFE)
  removes the border entirely.
- **Shadow difference** — companion shadow alpha only. No extra API cost.
- **Dimming** — layered alpha, inheriting §2.3's constraints.

The brief's "never look like a glowing neon border" is enforced in the design
tokens: the active border is the system accent at low alpha, ~1 px, never a glow.

### 2.7 What we deliberately will not do

- **No code injection, no DLL hooks, no `SetWindowsHookEx` into other
  processes.** It is the only way to truly control another app's rendering, and
  it is also how you get flagged as malware and crash the host.
- **No screen capture / re‑compositing.** Would satisfy every effect and violate
  every performance requirement.
- **No arbitrary anti‑aliased radius.** Not possible through public APIs.
  Claimed by no shipping tool.

## 3. Windows version gating

| Build | Name | What is available |
|---|---|---|
| < 17763 | Win 10 pre‑1809 | Transparency + precise corners + custom shadow only |
| 17763+ | Win 10 1809+ | …plus blur‑behind / acrylic accent |
| 22000 | Win 11 21H2 | …plus system corners, border colour, legacy Mica |
| 22621 | Win 11 22H2 | …plus `DWMWA_SYSTEMBACKDROP_TYPE` (Mica / Acrylic / Tabbed) |
| 26100+ | Win 11 24H2+ | …plus `DWMWA_BORDER_MARGINS`, redirection‑bitmap alpha |

Every DWM and accent entry point is resolved with `GetProcAddress` at startup,
never statically linked, so a missing export degrades instead of failing to load.

## 3.5 Animating other applications' windows

Measured on build 26200 rather than assumed.

| Question | Answer |
|---|---|
| Can another process's window be scaled and faded? | Yes. `SetWindowPos` plus `SetLayeredWindowAttributes`, both cheap. Use `SWP_ASYNCWINDOWPOS` or a hung target stalls the caller, and `SWP_NOSENDCHANGING` or an app with a minimum size clamps the frames. |
| Does DWM animate the same window at the same time? | Yes, and with a close enough curve that the two together read as neither. `DWMWA_TRANSITIONS_FORCEDISABLED` (attribute 3) switches the system's off. It is set-only — `DwmGetWindowAttribute` cannot read it back. |
| Can a minimise be animated? | Not on the real window. It is iconic within ~3 ms of the click, before `EVENT_SYSTEM_MINIMIZESTART` reaches another process, and `SetWindowPos` on it then rewrites the rect it restores to. |
| Is there anything left to animate after that? | Yes. `DwmRegisterThumbnail` still composes a full thumbnail of a just-minimised window — the taskbar preview path — so a copy can be animated when the window cannot. Verified: `DwmQueryThumbnailSourceSize` returned 556x528 for a minimised window and the thumbnail rendered its content in full. |
| What about a maximised window? | `SetWindowPos` silently drops it out of the maximised state. Scale is skipped for those; the fade still runs. |
| Should the real window be moved? | No. `SetWindowPos` per frame makes the target lay out its content again per frame; the animation is smooth and the content inside it is not. Animate a `DwmRegisterThumbnail` copy and hold the real window invisible underneath. |
| Frame clock | Not `WM_TIMER` — lowest priority, delivered only when the queue is empty, and this queue takes every WinEvent in the system. Not `GetTickCount64` either: it advances in ~15.6 ms steps, so a 16 ms test passes every other step and 60 fps becomes 32. Use `QueryPerformanceCounter` and a wait timeout. |
| What else steals frames | Anything heavy on the same thread. The companion shadow's blur plus its bitmap upload was costing ~100 ms; deferring other windows' effects during an animation, and re-blending rather than repainting a shadow whose strength alone changed, fixed it. |

## 4. Windows we must never touch

Enforced in `window_classifier` before any effect is considered:

- Not a root window (`GetAncestor(GA_ROOT) != hwnd`), or `WS_CHILD`
- `WS_EX_TOOLWINDOW` or `WS_EX_NOACTIVATE` (tooltips, popups, IME candidates)
- Cloaked (`DWMWA_CLOAKED` ≠ 0) — suspended UWP, other virtual desktops
- Class blacklist: `Shell_TrayWnd`, `Progman`, `WorkerW`, `#32768` (menus),
  `tooltips_class32`, `Windows.UI.Core.CoreWindow`, `XamlExplorerHostIslandWindow`,
  `ForegroundStaging`, `MultitaskingViewFrame`, `TaskSwitcherWnd`, …
- Smaller than 160 × 120 (transient helpers)
- Covering its monitor edge to edge with no caption or resize border
  (fullscreen apps and games; a maximised window keeps its frame and so is not
  caught by this)
- A process we cannot even name — if `QueryFullProcessImageNameW` fails, we
  have no business restyling it
- Our own process
- Any process on the user or built-in exclusion list

Two cases are tracked rather than rejected outright:

- **Minimised** windows get the `kMinimized` verdict and are dropped from the
  managed set, then re-evaluated on `EVENT_SYSTEM_MINIMIZEEND`. There is
  nothing to style while a window sits on the taskbar.
- **Windows the app already made layered** (`WS_EX_LAYERED` set by its owner)
  are recorded with an `already_layered` flag rather than refused. Corners and
  backdrop are perfectly safe on them; only transparency and dimming are not,
  because forcing `LWA_ALPHA` onto a window that calls `UpdateLayeredWindow`
  corrupts its rendering. The flag is what Phase 4's compatibility manager
  checks.

## Sources

- [DWMWINDOWATTRIBUTE enumeration — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute)
- [DWM_WINDOW_CORNER_PREFERENCE — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwm_window_corner_preference)
- [Apply rounded corners in desktop apps — Microsoft Learn](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/ui/apply-rounded-corners)
- [DwmSetWindowAttribute — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/nf-dwmapi-dwmsetwindowattribute)
- [Mica For Everyone — cross-process DWM backdrops](https://github.com/MicaForEveryone/MicaForEveryone)
- [Acrylic drag-lag regression report](https://github.com/Seo-Rii/electron-acrylic-window/issues/40)
- [Rounded corners in Win32 windows — SetWindowRgn tradeoffs](https://www.aloneguid.uk/posts/2022/12/rounded-corners-win32/)
