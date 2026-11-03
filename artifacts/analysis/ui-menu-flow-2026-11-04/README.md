# Aurora menu flow, firmwaredecoded (2026-11-04, work in progress)

Purpose: the navigable screen graph for a bug-for-bug Aurora replacement.
Every edge below names its firmware evidence. Hypotheses are marked as such;
do not implement from them.

## Entry: launcher rail select

- `MainLauncherMenuDialog::onSelectPress` (0xe48405) only calls
  `focusLauncher(false)` (0xe4831c): bookkeeping (last-selected index,
  menu-active flags) plus virtual `LauncherDialog::[+0x10](1)`.
- Key map (dialog, verified live 2026-11-04 via DIAL + fb1 after every key):
  Menu opens portal; Down/Up move rail focus; Select on rail 4 (wrench)
  opens the settings tabs + shortcut tiles; Right/Left move within tiles
  when tiles focused; Up enters tiles; Select on a tile opens in-place
  value edit (`< Maximaal >`); Exit cancels edit without change.
- OPEN: which `+0x10` target rail-4 Select reaches, and whether the
  observed tabs+tiles are one dialog or two stacked.

## Central router: DialogNavigator::handleNavigationMessage (0xd1aa79)

Machine-extracted message-code → action table (see analysis note
`nav-routing.txt` in this directory for the full list). Settings-relevant:
`0x12/0x13/0x14` → `doNavigateTabDialog(false,false)/(true,false)/(false,true)`;
`0x15/0x16` → source/other settings via TabDialog;
`0x17/0x18` → advanced picture/sound dialogs;
`0x19/0x1a` → PIP/PC-position dialogs. Tuner/DVB/EPG/PVR/broadcast entries
are out of scope for the HDMI-only set but listed for completeness.

## SubLauncherDialog: category shortcuts, not the full list

- `LauncherDialog::showSubLauncher` builds it; called from
  `onUpDownPress` and `onSubLauncherTimer` (debounced key path, not a
  navigation message).
- `initialiseFields` (0xee3614) switches on category id +0x4b8:
  2=picture shortcuts, 3=sound, 7=system, 0=channels, else recommendations/
  ads/AI-menu. Observed live: 3 tiles (Modus/Natuurlijk,
  Energiebesparing/Maximaal, Beeld zoomen/16:9) = exactly the static
  `initialisePictureSettingsShortcuts` rows (type-2, selection-disabled).
- Row templates here are 463/464 (`..._launcher.svg`/`..._sublauncher.svg`),
  step 140 — NOT the settings list (460, step 70). Live heap scan found no
  PictureSettingsDialog, no font-18 title, no type-9 rows in this state.

## TabDialog: tabs + per-tab content + title (decoded, not yet reached live)

- Ctor `setSVGName(191)` fixes resource `DialogSettings_tab`; tab buttons
  created in `init` from `picture/sound/network/installation/system`
  descriptions with computed widths ((769-11-2*(n-1))/n; n=2 here → 378).
- `updateTitles`/`updateGlyphAndTitle` (0xd6faed): per-tab keys
  (PICTURE[_MENU]/SOUND[_MENU]/NETWORK/INSTALLATION_STR/SETUP_MENU/SYSTEM_STR,
  PANASONIC-gated) resolved via `UIResourceManager::lookupString` at
  `setText` time; glyph ids 0x335/0x336/0x337/0x32f/0x338.
- `showSubMenu` (0xd6f141): `hideSubMenu` first, then content per focused
  tab — 0=PictureSettingsDialog, 1=Sound, 2=IPMultiple (wifi-gated),
  3=Installation, 4=SettingsListMenu. Content must pass vtable+0x8/+0x18
  or it is hidden again immediately.
- Title `+0x140` built by `createComponents` (0xd6da90, via init chain) from
  `dialogTitle`/`dialogTitleShadow` (record 191 has title only, no shadow);
  id 419, lines 1, type 0 → font 18 after rescale; live-confirmed geometry
  (108,5,691,95). Visible title needs `create()` (container population via
  `updateSVGData`) BEFORE `init`-chain `createComponents`.
- Key routing (decoded): Left/Right with submenu flag → tab switch
  (hide → UIGroup 0x20c/0x20d → updateGlyphAndTitle → showSubMenu);
  Up/Down with flag → focus submenu list; Select opens per-type sub-dialogs
  (0x6d advanced, 7 PCP, 0x120 menu-result, 8 message/prompt, 10 demo).
- Left/Right on main-list slider rows does NOT adjust in place: ids 0-5 go
  through `updateViaPictureElementSettingDialog` which OPENS the per-setting
  sub-dialog. Shortcut-tile rows DO adjust in place (observed live).
- OPEN: which flow reaches TabDialog-with-content (vs sublauncher); the
  `create()` caller and container/window ordering; the dynamic `update*`
  trigger that populates slider rows (provider bounds are runtime data).

## Value/tab interaction (blocking rule)

No in-place value path may be built as vendor parity: main-list Left/Right
opens sub-dialogs, bounds come from runtime picture-pipeline providers
(native ownership, out of scope). The lab `ui.edit=1` in-place digits stay
an explicitly marked placeholder. Tab switching replaces the content dialog;
`setSelectedItem` only stores the index.

Evidence: `nav-routing.txt` (machine-extracted switch map),
`../settings-nav-2026-11-04/` (live DIAL navigation + sublauncher scan),
`../ui-text-setup-2026-11-01/` (row/label/title decodes),
`../ui-tab-titles-2026-11-02/` (title keys). No TV writes; read-only
decompile plus owner-authorized DIAL keys with per-step fb1 verification.
