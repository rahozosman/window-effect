# Roadmap

Five phases. Each ends with `flutter analyze` clean. No test files, no test
runs, no automatic app launches.

---

## Phase 1 — Architecture, settings UI, persistence ✅

Pure Dart. The native engine is deliberately absent so the UI, the model and the
persistence layer can be finished and verified without a C++ compile in the loop.

- [x] `EffectSettings` model — immutable, `copyWith`, JSON round-trip, clamped
- [x] Enums: corner mode, backdrop, blur preset, shadow difference, preset id
- [x] Five presets + automatic "Custom" detection
- [x] `SettingsStore` — atomic JSON write to `%APPDATA%\WindowEffects\`
- [x] `OsInfo` — build-number parsing and the capability matrix from FEASIBILITY §3
- [x] `EffectsChannel` — Method/Event channel wrapper that degrades cleanly to
      `EngineState.unavailable` when the native half doesn't exist yet
- [x] `SettingsController` — single writer, split debounce (disk 400 ms / engine 120 ms)
- [x] Design system — tokens, motion, light + dark themes
- [x] Settings page — appearance, presets, exclusions, system
- [x] Live preview — visual demonstration only, never the real implementation
- [x] Capability gating — unsupported effects disabled with the reason shown

**Not in Phase 1:** any window is touched. Nothing leaves the process.

---

## Phase 2 — Detection, classification, exclusions ✅

- [x] `log` — rolling file log capped at 512 KB, in `%APPDATA%\WindowEffects\`
- [x] `dwm_api` — `GetProcAddress` resolution for every DWM/accent entry point
- [x] `os_version` — authoritative capability probe via `RtlGetVersion`,
      combined with whether each export actually resolved
- [x] `config` — native mirror of `EffectSettings`, clamped on arrival
- [x] `window_detection` — five grouped WinEvent hooks on a dedicated thread
- [x] `window_classifier` — the reject rules from FEASIBILITY §4, cheapest
      checks first, with a PID→executable cache
- [x] `exclusion_manager` — built-in protected list + user list
- [x] `managed_window` — original-state capture, restore contract, quarantine
- [x] `dispatcher` — message-only window marshalling engine → platform thread
- [x] `effects_bridge` — both channels, capability report, status stream
- [x] UI: `EngineState.observing` with a live managed-window count

Deliverable met: the engine sees every window correctly and applies nothing.
`EVENT_OBJECT_LOCATIONCHANGE` is never subscribed, so the idle profile is flat.

---

## Phase 3 — Corners, shadow, active window ✅

- [x] `compatibility` — resolves every version gate, window quirk and setting
      interaction into one `EffectPlan` per window
- [x] `corner_effect` — DWM preference path, plus a `SetWindowRgn` precise path
      that subtracts only the four corner wedges so the invisible resize margin
      survives
- [x] `shadow_effect` — companion layered window, 3-pass box blur, silhouette
      knocked back out, repaint only on size change, hidden during drags
- [x] `active_window_effect` — `DWMWA_BORDER_COLOR` in the system accent
- [x] Shadow differential and the additive-shadow scale-down for system corners
- [x] `EVENT_OBJECT_LOCATIONCHANGE` now installed on demand, with a hot path
      that makes no DWM round trips

---

## Phase 4 — Transparency, blur, backdrop ✅

- [x] `transparency_effect` — owns *all* layered alpha, so transparency and
      inactive dimming can never fight over the same window
- [x] Pre-existing-layered detection and UIPI (`ERROR_ACCESS_DENIED`) handling
- [x] Blur presets mapped to accent state plus a theme-aware tint
- [x] `backdrop_effect` — `DWMWA_SYSTEMBACKDROP_TYPE` → `DWMWA_MICA_EFFECT` →
      nothing, with Acrylic refused rather than faked on builds that lack it
- [x] Accent-policy capture and restore, alongside the other originals
- [x] Panic latch wired: eight quarantined windows restores everything and stops

---

## Phase 5 — Presets, tray, startup, polish ✅

- [x] `tray` — `Shell_NotifyIcon`, a menu of master switch / pause / presets /
      settings / exit, and a `TaskbarCreated` handler so the icon survives an
      Explorer restart
- [x] Launch-to-tray — `--tray` starts the engine with no window; closing the
      settings window hides it, and only the tray's Exit quits
- [x] `startup` — `HKCU\...\Run` registration writing `"<path>" --tray`, with
      the registry's answer reconciled against the config file on every launch
- [x] `single_instance` — one engine per session, and a second launch asks the
      running one to show its window instead of starting a rival
- [x] Engine pause — suspends and restores without touching the saved config,
      and drops the `LOCATIONCHANGE` subscription while suspended
- [x] Native `IFileOpenDialog` for picking an executable to exclude
- [x] `self_backdrop` — the settings window applies Mica to itself on 22621+,
      and the UI drops its opaque page background only when that succeeded
- [x] Idle profiling — see the note below; the honest answer is not the one the
      original bullet assumed

**The tray owns no settings.** Choosing a preset or flipping the master switch
there sends an intent to the Dart side, which applies it to the one settings
model, saves it and pushes it back down. Pause is the exception, and is not a
setting at all.

### Idle profiling, honestly

The original bullet expected zero `EVENT_OBJECT_LOCATIONCHANGE` traffic on
default settings. That is wrong, and changing the defaults to make it true
would have meant shipping without a shadow.

`Config::NeedsLocationTracking()` subscribes when the companion shadow is on,
or when corners are in precise mode — both have to follow a window pixel by
pixel. The shipped default (Premium) has the shadow on, so the subscription
*is* installed. It is not installed when:

- the master switch is off, or the engine is paused;
- the shadow is off and corners are in system mode (the Minimal preset with
  the shadow switched off);
- nothing is being applied at all.

The hot path was built for exactly this: the handler makes no DWM round trips,
repaints only when the window's size changes, and hides the shadow outright
during an interactive drag.

---

## Motion — added after Phase 5

Not in the original five phases; asked for afterwards, and built on the same
rules as everything else.

- [x] `animation` — entrance motion on the real window, five styles as cubic
      beziers, with duration, zoom and rise adjustable under each
- [x] `minimize_animation` — exit motion on a DWM thumbnail, because the real
      window is gone before we are told
- [x] `DWMWA_TRANSITIONS_FORCEDISABLED` held for as long as a window is managed,
      so the system's transitions and ours are never both playing
- [x] Corners, shadow and transparency deferred until the geometry settles
- [x] A settings preview that replays the same curve the engine will use
- [x] Frame clock stops when nothing is moving

**Measured on build 26200**, with the probe traces that proved each one:

| Behaviour | Evidence |
|---|---|
| Premium entrance | 538x504 → 572x536, alpha 0 → 242, ends on the exact rect |
| macOS entrance | starts at 90% and no rise, per that style's numbers |
| Restore from taskbar | same path as an open, 538x504 → 572x536 |
| Minimise | overlay 485x460 → 92x87 travelling to the taskbar, then gone |
| Style change from the UI | click → config.json → engine → next window |
| Effects after motion | `rgn=3`, alpha 242, shadow back — all reapplied |
| Idle | clock on at 47.010, off at 47.242; nothing between animations |

**Two things Windows does not allow, found by measuring rather than guessing:**

- A window is iconic about three milliseconds after the click, before any
  out-of-process hook runs. There is no window left to animate on minimise —
  hence the thumbnail.
- `SetWindowPos` on a minimised window rewrites the rect it restores to, and on
  a maximised one silently un-maximises it. The animator refuses both.

---

## Verification note

`flutter analyze` covers the Dart half. `flutter build windows --release`
builds all 22 effects modules and links, warning-free under `/W4 /WX` with
`_HAS_EXCEPTIONS=0` and no C++/WinRT.

The first build surfaced exactly one error, and it was header hygiene:
`compatibility.h` used DWM constants (`kCornerDoNotRound`, `kDwmColorDefault`,
`kAccentDisabled`, `kBackdropAuto`) as `EffectPlan` defaults without including
`dwm_api.h`, and no translation unit happened to include the two in the right
order. It now includes what it uses.

### First run — what was actually exercised

On Windows 11 build 26200 (`corners=1 backdrop=1 legacyMica=0 blur=1`), against
live windows, measured rather than assumed:

| Behaviour | Evidence |
|---|---|
| Precise corners | `GetWindowRgn` = COMPLEXREGION; the corner is visibly cut |
| System corners | `DWMWA_WINDOW_CORNER_PREFERENCE` = 2 after the GNOME preset |
| Transparency | `WS_EX_LAYERED` set, `GetLayeredWindowAttributes` alpha 224 = 88% |
| Companion shadow | pixel ramp 226 → 144 over ~30 px outside the frame |
| Tray menu | opens, checks state, and every command round-trips |
| Master switch from the tray | tray → Dart → config file → engine → UI switch |
| Preset from the tray | GNOME rewrote the config and re-applied every window |
| Pause / Resume | `restoring 3 window(s)` + `location tracking off`, and back |
| Startup registration | `HKCU\...\Run` = `"<exe>" --tray` |
| Executable picker | `IFileOpenDialog` → `excludedApps: ["calc.exe"]` |
| Close to tray | window hidden, process alive, engine still managing |
| Single instance | second launch raised the first one, no second process |
| `--tray` launch | engine running, settings window never shown |
| Restore on exit | corner preference back to 0, layered and region gone |

Two things the run itself changed:

- `LogOpen` now runs before the capability probe. The build line and the
  self-backdrop line were being written before the log file existed, which made
  "did Mica apply?" unanswerable from the log — the exact question the log is
  for.
- `self_backdrop` says why it declined, instead of failing silently.

One quirk worth knowing rather than fixing: the menu's disabled title item is
still reachable with the arrow keys, so keyboard users pass through it on the
way to the first real command. Mouse users never see it.
