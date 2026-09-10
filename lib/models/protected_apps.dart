/// Processes the engine will never touch, regardless of user settings.
///
/// These are enforced natively as well — this list exists so the settings
/// screen can *show* the user what is already protected, rather than leaving
/// them to guess whether the taskbar is at risk.
///
/// Two categories, kept separate because they are protected for different
/// reasons: shell surfaces would visibly break, and security surfaces must
/// never be modified at all.
const List<String> kProtectedShellApps = <String>[
  'explorer.exe',
  'searchhost.exe',
  'startmenuexperiencehost.exe',
  'shellexperiencehost.exe',
  'textinputhost.exe',
  'sihost.exe',
  'dwm.exe',
];

const List<String> kProtectedSystemApps = <String>[
  'consent.exe',
  'lsaiso.exe',
  'logonui.exe',
  'winlogon.exe',
  'csrss.exe',
];

/// Everything protected, for membership checks in the UI.
List<String> get kProtectedApps =>
    <String>[...kProtectedShellApps, ...kProtectedSystemApps];

/// Applications commonly worth excluding, offered as one-tap suggestions.
///
/// Each renders its own window with GPU compositing, which is exactly the case
/// where forcing layered transparency tends to flicker (see FEASIBILITY §2.3).
const List<String> kSuggestedExclusions = <String>[
  'chrome.exe',
  'msedge.exe',
  'firefox.exe',
  'code.exe',
  'discord.exe',
  'steam.exe',
  'obs64.exe',
  'vlc.exe',
];
