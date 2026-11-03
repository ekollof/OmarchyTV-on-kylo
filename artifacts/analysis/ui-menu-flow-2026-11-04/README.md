# Aurora menu navigation, v2 — two settings paths

Status: live-verified portal→sublauncher path + read-only heap confirmation;
TabDialog path firmware-decoded, NOT yet driven live. Do not mix the two
paths' widgets when implementing parity.

## Path A — launcher rail → SubLauncherDialog (shortcuts + tab strip). LIVE.

Verified 2026-11-04, one DIAL key per step with fb1 check after each:

| step | key | code | observed result |
|---|---|---|---|
| 1 | Menu | 1048 | portal `Startpagina`, 5-rail icon bar |
| 2 | Down | 1019 | rail focus 3rd(home) → 4th(TV row) |
| 3 | Down | 1019 | wrench rail (4th→5th?) focused, title `Instellingen` appears (top-left), NO tabs/tiles yet |
| 4 | Select | 1053 | tab strip + shortcut tiles appear (Beeld focused, orange underline) |
| 5 | Select | 1053 | toggles back to tabs-only view |
| 6 | Up | 1020 | tiles+tabs again, Modus tile focused |
| 7 | Right | 1022 | tile focus Modus → Energiebesparing (Left presumed symmetric, NOT explicitly tested) |
| 8 | Select | 1053 | in-place edit on tile (`< Maximaal >`, orange). NO Left/Right sent (would change the value) |
| 9 | Exit | 1010 | edit cancelled, value unchanged |

- Left/Right on the tile row moves tile focus; Up enters tiles; Select
  toggles tile view vs tabs-only view (observed; mechanism undecoded).
- Firmware side: Up/Down also drive `LauncherDialog::showSubLauncher`
  (0xed9888, direct call + timer), which builds `SubLauncherDialog` whose
  `initialiseFields` (0xee3614) switches on category id +0x4b8
  (2=picture shortcuts, 3=sound, 7=system, 0=channels). Row templates
  463/464 (`...RowItems_launcher/sublauncher`), step 140 — NOT the
  settings list (460, step 70).
- Live heap scan of this state: no PictureSettingsDialog, no font-18
  title textbox, no type-9 rows. The tab strip here is SubLauncher-owned
  static category display (record 189 has NO tab widgets and NO dialogTitle;
  only background/main_rectangle/listBox). Do NOT attribute these tabs to
  TabDialog::updateGlyphAndTitle without a live widget-identity read.

## Path B — QuickMenu key → TabDialog (title + full slider list). FIRMWARE.

- `doQuickMenuDialog` posts navigation message **0x12** → router
  `doNavigateTabDialog(false,false)`. (Sibling codes: 0x13/0x14 variants,
  0x15/0x16 tab-wrapped source/other settings, 0x17/0x18 advanced dialogs;
  full router map in nav-routing.txt, 154 cases machine-extracted.)
- `TabDialog` (resource 191): `init` builds 5 tab buttons from
  `picture/sound/network/installation/system` descriptions (computed
  widths; n=2 available → 378 px), `updateGlyphAndTitle` sets per-tab keys
  (PICTURE/SOUND/NETWORK/INSTALLATION_STR/SYSTEM_STR, PANASONIC-gated) and
  glyphs 0x335/0x336/0x337/0x32f/0x338, `showSubMenu` creates tab 0's
  `PictureSettingsDialog` (resource 179) — and destroys it immediately if
  its +0x8/+0x1c validity check fails.
- Title +0x140 built by `createComponents` ONLY if the dialog container
  already carries a `dialogTitle` description (`SVGDialog::create` populates
  it via `updateSVGData`; `doModal` runs init, not create — the ordering
  that yields a titled dialog is still open).
- Tab keys once open (from `TabDialog::preHandleKeyPressMessage` 0xd6faec):
  Left/Right with submenu flag → switch tab (hide → UIGroup 0x20c/0x20d →
  updateGlyphAndTitle → showSubMenu); Up/Down → focus submenu list;
  Select → per-type sub-dialog (7=PCP, 0x6d=advanced, 0x120=menu result,
  8=message/prompt, 10=demo); Exit paths (0x22d/0x251, 0x21f/0x28b) →
  unfocus + hide + endDialog.
- Main-list Left/Right does NOT adjust in place: ids 0-5 go through
  `updateViaPictureElementSettingDialog`, which OPENS the per-setting
  sub-dialog (shortcut tiles are the documented exception: in-place edit).

## Agent playbook: reaching each dialog by keys

Preconditions: TV awake; stock Aurora running; verify every step with
`tvshot.py --layer 1` before the next key. Abort (Exit) on any unexpected
screen. NEVER send Left/Right on an editing tile (changes values), never
combine keys in one command, never stage/run anything on the TV.

- Portal (any state → launcher): Menu (1048); verify Startpagina + rails.
- Settings shortcuts (Path A): Down ×2 to wrench rail [verify title
  `Instellingen`], Select [verify tiles+tabs, Beeld focused]. From here:
  Up/Right navigate tiles; Select edits in place (AVOID unless a value
  change is explicitly authorized); Exit cancels edit.
- Full slider list (Path B, PREDICTED — needs one live test): QuickMenu
  key (DIAL 1043 = ICE 593 = internal 0x251; all three code spaces verified
  identical). Chain, every link firmware-verified: 0x251 →
  `DemoApp::handleKeyPress` → `onQuickAccessKey` (gated on profile remote
  code != 0xff, teletext key, no VCR timer, two modal checks) →
  `checkAndDisplayQuickMenu` → `doQuickMenuDialog` posts nav message 0x12
  (skipped only if the key was 0x22d with quick-menu available) →
  `doNavigateTabDialog(false,false)` → TabDialog with title + full slider
  content. Same key while a TabDialog is open CLOSES it (dispatcher
  0x22d/0x251 branch), so QuickMenu toggles. If 1043 does nothing live, the
  likely gate is `isQuickMenuKeyAvailable` (profile remote code 0xff) —
  record and stop, do not hammer other keys as substitutes.
- Inside TabDialog (from `preHandleKeyPressMessage` decode; PREDICTED live):
  Left/Right with submenu flag switch tabs (hide → UIGroup 0x20c/0x20d →
  updateGlyphAndTitle → showSubMenu), else forward to content; Up/Down move
  within content and return focus to tabs at the ends (tab-jump variants
  synthesize L/R); Select opens the focused row's sub-dialog; Exit (0x210)
  partially exits, 0x22d/0x251/0x21f/0x28b fully close (unfocus + hide +
  endDialog; 0x21f/0x28b repost the key).
- Leaving settings: Exit (verified to cancel edit; full exit path back to
  portal/video still to verify live — do it stepwise).

## Open links (do not guess)

1. RESOLVED 2026-11-04 (instructions + vtable bytes, no TV): the +0x10
    "pure-virtual stub" read was wrong. +0x4a8 is a SubLauncherDialog*
    (constructed by showSubLauncher at ed99be, stored ed99c2; nulled by
    initialiseFields edc5e4 and hideSubLauncher ed9774). Slot +0x10 =
    SubLauncherDialog::setFocusEb (called with 0 at ed9a48), slot +0x18 =
    isShowingEv (bool at ed9a54 gates hideSubLauncher vs
    bringFocusedButtonToFront); vtable _ZTV17SubLauncherDialog @0x1500cfc.
    Rail-Select (LauncherDialog::onSelectPress edde60) with no sublauncher
    focus does NOT open TabDialog: it snapshots selection, unfocus+hide
    sublauncher, convertOperationIdToIDME, SVGDialog::endDialog. The
    launcher closes with an operation result; settings opens downstream
    (OperationFinder result-to-nav-code routing still open).
2. Whether the observed tab strip is TabDialog buttons (computed 378 px
   matches the focused white box; labels match updateTitles keys) or
   SubLauncher-owned widgets — needs a live widget-identity read or
   SubLauncher tab-machinery decode (none found in its init).
3. What populates the TabDialog container before createComponents
   (the create() caller), and what fails the content +0x18 check in the
   shortcuts-only state (empty dynamic rows? provider data?).
4. The dynamic `update*` trigger that turns shortcuts into the full slider
   list (provider bounds are runtime data, out of scope for static decode).
5. Full exit path back to portal/video (Exit cancelled edit only).

Evidence: `nav-routing.txt` (154-case machine map),
`../settings-nav-2026-11-04/` (live keys + sublauncher scan),
`../ui-text-setup-2026-11-01/` (TabDialog/title decode). No TV writes beyond
authorized keys; TV suspended by owner at end of session.

## Live key session 2026-11-04 (owner-authorized DIAL keys, per-step fb1 verified)

Starting state: "Geen signaal" banner (idle, no OSD menus). Each key got
HTTP 201; every effect below was screenshot-verified (201 alone proves nothing).

| # | key | result |
|---|---|---|
| 1 | QuickMenu 1043 | NO visible effect (banner unchanged). Dialserver log confirms arrival (`Starting the app with params <remote><key code='1043'/>`), so the key is delivered but gated/ignored in this state. |
| 2 | Exit 1010 | no visible change (banner is not dismissible this way). |
| 3 | Menu 1048 | launcher portal (Startpagina, 5 rails). |
| 4 | QuickMenu 1043 (from portal) | NO visible effect (portal unchanged). |
| 5 | Exit 1010 | portal closed, back to banner. Banner confirmed sticky. |

No values were changed (no Left/Right on editable widgets), no settings DB
writes, no staging left on the TV, no helpers running. Panel left showing
the idle "Geen signaal" banner; TV suspended afterwards at owner request.

Implication: QuickMenu-as-0x251 reaches `DemoApp::handleKeyPress` only if
the DIAL virtual->ICE translation delivers it (name-matched, untested
end-to-end), AND `onQuickAccessKey` gates on profile remote code != 0xff
(`hwprofile` key `RemoteControl/<remote>/KeyQuick`, default 0xff) plus
teletext/VCR/modal checks. The no-op in two states is consistent with a
closed gate (profile or modal state), NOT with broken delivery. Next step
when access allows: read the hwprofile RemoteControl section to determine
the configured remote code; or test QuickMenu from inside the launcher/
sublauncher modal state (different +0x284 gate values).
