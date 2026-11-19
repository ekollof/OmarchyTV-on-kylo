# Live stock-UI capture pass (DIAL virtual RC + read-only fb1) — 2026-11-19

Status: current
Last verified: 2026-11-19
Evidence class: on-TV (read-only fb1 sampler, stock Aurora running) + firmware
instruction check + host tests. No display/HAL ownership, no cold boot.

## Safety and provenance

- The TV was in standby; `tools/tvssh.sh` woke it with WoL only. Stock Aurora
  **1778** (child of `run_aurora.sh` 1732 ← appmgr **1580**), its zombie child
  2255, dialserver **1880** (a child of Aurora, so DIAL keys reach the live
  stock UI). `/conf/coredumpnoreset` and `oa_safe` present, `oa_boot` absent;
  no openaurora process or helper.
- **No** Aurora stop, watchdog change, `/conf` or `/vendor` write, boot-hook
  change, device write or persistent install. Keys used the documented DIAL
  `POST :56789/apps/SmartCenter` `<remote><key code='…'/></remote>` contract
  ([DIAL-KEYS-SCREEN](DIAL-KEYS-SCREEN-2026-10-30.md)); captures used the
  read-only `tools/tvshot.py` fb1 dump and the `fb-region-sampler` (ARM
  `build-tv-v3` sha256 `8e4278cc…`) staged at `/buffer/oa-stockcap-20261119`
  with on-TV hash verification. A DIAL 201 alone proves nothing, so **every**
  key was verified by a capture diff.
- Restored at the end: TV back in standby (ping 100% loss), staging removed and
  verified absent, no helper running, stock PIDs/`/conf` flags unchanged.
- **Disclosure:** an edit-pane OK executed the focused stock option
  "Toevoegen aan Thuis". The live settingsDB changed (`LauncherItem` 53→54
  rows: one new type-202 record id 0/pos 127, every existing position +1, the
  type-202/203 special pair changed; `LauncherItemId` 202→203). Both DB copies
  are archived in the analysis directory. The **visible** home rail is
  unchanged — no C12 tile anywhere in the full row through GAIN. No DB write
  was attempted to undo it (corrupting the live vendor DB is a larger risk). A
  launcher edit-mode "Bent u zeker dat het wilt verwijderen?" dialog timed out
  unsafely-cycled to `Ja` focused at standby; no deletion occurred (GAIN
  intact). The owner can dismiss it with the physical remote.

## Screens recovered (evidence: `artifacts/analysis/stock-ui-captures-2026-11-19/`)

| key | screen | PNG |
|---|---|---|
| — | baseline "Geen signaal" (the number is the auto-suspend countdown) | (captured during ladder) |
| 1048 / 1019 | Startpagina home rail / TV rail (Zoeken, Programmagids, Zenders, Timers) | `portal-home.png`, `portal-tv-rail.png` |
| 1053 on Zenders | populated channel screen: "Zenders / Alleen TV", 11 ATV rows, orange focus, tabs, scrollbar | `channel-populated.png` |
| digit 1001/1003 | global digit→tune banner (1→C12, 3→BBC2) and its channel card | `channel-banner.png` |
| 1047 | EPG empty channel-filter state (analog services carry no EIT; no DVB RF) | `epg-empty.png` |
| 1057 | sources screen, dark-backdrop variant with orange cards | `sources-screen.png` |
| Bewerken tab + OK | stock edit pane: Toevoegen aan Thuis (focused), Verplaatsen, Wissen, Naam bew., Vergrendel, Toevoegen/Verwijderen, Tag, Alles markeren | `channel-edit-pane.png` |
| (edit-mode drift) | launcher remove-confirmation (Ja/Nee) — no deletion | `launcher-remove-confirm.png` |

The populated channel screen confirms the record-80 model live: 780 px panel,
tabs at y100..159, 75 px rows with the `#FE9419` focus band and white ATV
markers, scrollbar. The empty list shows its own state ("Geen van de zenders
stemt overeen met de filter", auto-closes in ~5 s). Populated EPG/PVR grids,
the filter popup and the option actions still need EIT/RF or a recording
context.

## GotoServiceDialog — live confirmation and correction

- **Firmware guard:** `AdvancedChannelListDialog::onDigitPress` @0x00e7cb18
  calls `ChannelNameListBox::totalNumberOfItems()` @0x00e7cb40 and `cbz`
  @0x00e7cb44 skips the modal entirely when the list is empty. Observed live:
  with the empty list a digit falls through to the global digit→tune path
  (banner C12 for 1, BBC2 for 3); with 11 services the modal opens.
- **Lifetime:** read-only sampler (region 0,0 800×400, 8 ms, 50 frames) saw
  the dialog first at **t=87 ms**, every frame identical through **t=1467 ms**,
  gone at **t=1527 ms** — a ~1440 ms static paint matching the recovered
  1500 ms timer; no multi-frame slide was visible in that window. The channel
  panel behind it stays fully visible: there is **no white 540×239 panel fill**.
- **Measured render (frame 20):** flat **#481730 (72,23,48)** box spanning
  pixels **(41,45)..(207,211) = 167×167**, with white (255,255,255) digit ink
  spanning (121,107)..(130,147), centred at (125,127) — 1 px from the box
  centre. The surrounding panel/banner sample exactly `#BE1E2D` (190,30,45)
  and `#B21D2B` (178,29,43), matching record 80.
- Firmware records: `DialogGoToService.svg` (116) gives `main_rectangle`
  0,0,540,239 `#FFFFFF` and `serviceNumber` -133,98,377,298 (authored,
  pre-transform); as with `DialogMainMenu`, the named rectangle is layout
  metadata, not opaque paint. Records 395..398 really are
  `gotoServiceTextEntryBoxFA/FI/UFA/UFI.svg` (three-zone entry states; init
  `0x00d2296c` maps state1→0x18b, state3→0x18c, state0→0x18d, state2→0x18e).
- **Transform recovered:** record 116's unnamed tag-2 element is an image
  reference `../png/volume_bg.png` with `matrix(1 0 0 1 15 23)`. The image pack
  entry `volume_bg.png` (hash `0xb72d0a70`, 220×210, 527 bytes) has an opaque
  `#481730` core at image (26,22)..(193,188); placed at (15,23) that core lands
  on screen at **(41,45)..(208,211) = 168×167** — the captured box, with no
  scale transform. The digit ink is white and centred in that box.
- **Implementation correction:** `common/goto_service.c` now carries the
  rendered entry box (exclusive corners 41,45,208,212), `#481730` fill and
  white digit ink; `ui_vendor` paints only that box (no white panel) and, per
  the firmware guard, opens the overlay only when the rendered channel list
  has rows. `check-goto-service` and `check-ui-channels-stock` pass; the
  `stock-goto.png` candidate was regenerated. Multi-digit entry, the entry
  animation transform and the empty-state ink remain open.
