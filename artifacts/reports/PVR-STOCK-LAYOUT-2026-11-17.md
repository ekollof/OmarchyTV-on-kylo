# Stock PVR list layout and gated screen — 2026-11-17

Status: current (layout/screen first slice; no stock PVR capture)
Last verified: 2026-11-17 (firmware resource, PlayListDialog bodies, host
recorder tests)
Evidence class: firmware resource/instructions and host/recorder tests. No TV
run, stock PVR capture, recording engine or scanout.

This starts the stock PVR screen from the shipped `DialogPlayList.svg` (BinSVG
692) and `PlayListDialog`, instead of inventing a list UI. Raw rects and the
recorder candidate are in
[artifacts/analysis/pvr-stock-2026-11-17](../../analysis/pvr-stock-2026-11-17/README.md).

## Firmware layout (record 692)

| element | x | y | x1 | y1 |
|---|---:|---:|---:|---:|
| panel | 0 | 0 | 1920 | 1080 |
| header | 0 | 0 | 1921 | 162 |
| subheader | 0 | 162 | 1920 | 216 |
| titleName/Start/Length | 176/808/1307 | 164 | 776/1136/1561 | 209 |
| list | 1 | 216 | 1911 | 971 |
| row | 26 | 222 | 1886 | 288 |
| name/time/duration | 181/815/1307 | 230 | 811/1305/1627 | 275 |
| scrollbar | 1910 | 217 | 1920 | 974 |
| space info | 315/779/1295 | 1005 | 775/1287/1697 | 1045 |
| action tabs | 206+302*i | 101 | 506+302*i | 160 |

Panel `#DCDBDA`, header `#481730` at 9/10 (87,43,65), subheader `#010101` at
3/10 (154,153,153), row text `#111111`. Row pitch 75 (shared full-screen-list
step), row height 66; focus uses the theme orange `#FE9419` as a class/theme
inference, not a record-692 fill.

`PlayListDialog::init`/`redrawDialog` set the title from the `LIBRARY` key;
the vendor Dutch translation is **"Opnamebibliotheek"** (verified through
`common/langprofile.c`, not assumed). `SVGFullScreenListDialog::createComponents`
sets the column titles from `NAME_TITLE`, `START_TITLE` and `DURATION_TITLE`
(dut "Naam", "Start", **"Lengte"**). `createTabButtons` makes five **text** buttons: `SVGButton::create`
@0x010aea28 resolves each name through `UIResourceManager::lookupString`, so
`HLP_PLAY_OPTIONS`, `HLP_EDIT`, `HLP_DELETE`, `HLP_SORT`, `SETTINGS` are
localized labels (dut "PLAY_OPTIONS", "Wijzigen/wissen", "Wissen", "SORT",
"Instellingen"), not glyph art. The TabButtonFA/UFA/FI/UFI states (BinSVG
522..525) give a solid white selected pill with `#811924` text and transparent
unselected tabs with white text.

Row indicators map to image ids and record-692 rects:

| indicator | rect | image id | shipped asset |
|---|---|---:|---|
| recordingIcon | 55,237 .. 91,273 | 0x36 (54) | `icon_record_small.png` |
| newIcon | 93,237 .. 138,273 | 0x116 (278) | `icon_right_arrow.png` |
| lockedIcon | 1793,237 .. 1829,273 | 0x1e (30) | `icon_lock.png` |
| playbackIcon | 1833,237 .. 1869,273 | 0x115 (277) | `icon_play_disallowed.png` |
| partialIcon | no rect in 692 | 0x114 (276) | `icon_exclamation_mark.png` |
| serviceTypeIcon | 1735,236 .. 1791,283 | 99 / 6 / 8 | `icon_lost_signal`, `icon_tv_small`, `icon_radio_small` |

Image ids are pack indices (`telefunken18_img.bin` is the concatenated PNG
payload in index order). `SVGFullScreenListDialog::updateRecordingIndicators`
@0x00df7788 applies the recovered rules (the image vtable `+0x34` is `setId`,
where 0 hides the icon):

| indicator | rule |
|---|---|
| recordingIcon | `entryIsBeingRecorded` = `PlayListItemData+0x1c` |
| newIcon | `newFlagShown` = `+0x1c == 0 && +0x1f == 0` |
| lockedIcon | `entryIsProtected` = `+0x1d ^ 1` (shown when 0) |
| playbackIcon | `entryIsPlaybackProtected` = `-(+0x1e ^ 1)` (shown when 0) |
| partialIcon | the same function's `>= 1` branch, unreachable from the recovered body (it returns 0/-1) |
| serviceTypeIcon | `entryGetServiceType` = `+0x24`, 2 = radio else TV |

`ui.pvr_stock=1` reads these bytes from the optional `pvr.meta` v1 service
(`oa_pvr_meta_iface.read`); `mod_pvr`'s stub does not publish it, so the
project falls back to the state-based recording icon for `OA_PVR_RECORDING`.
The indicator assets are white masks and the record-692 rects have stroke
`#111111`, so the screen tints them (the recording dot is a coloured asset and
is not tinted).

`PlayListDialog::rebuildRecordingsList` @0x00df3768 allocates a 0x38-byte
`PlayListItemData` per `EASIDvrInterface::queryFirstRecording` result:
`+0x00` recording id, `+0x04` program name (UString), `+0x0c` channel name,
`+0x14` start DateTime, `+0x18` duration/stats value, `+0x1c..+0x20` values
from DVR vtable slots `+0x58`/`+0x6c`, `+0x24`/`+0x28` scratch values,
`+0x2c` synopsis UString and `+0x34` a bool set from `local_ebc > 0`. The
exact flag→indicator mapping is not reduced, so those icons stay unpainted.
`updateCachedDiskStatistics` @0x00df20e4 asks the DVR object vtable `+0x34`
for two out values (`this+0x9a4`/`this+0x9a8`) and zeroes both on failure;
that is the data `refreshSpaceUsage` formats.

`refreshSpaceUsage` uses `USED_SPACE_INFO_TITLE`, `RESERVED_SPACE_INFO_TITLE`
and `FREE_SPACE_INFO_TITLE` plus `" MB"`/`" MB "` (dut "Gebruikte ruimte: ",
"Gereserveerd: ", "Vrije ruimte: "). There is no disk-stat call in
`OA_SVC_PVR`, so the values stay unpainted.

## Implementation

- `common/pvr_layout.c` + `include/openaurora/pvr_layout.h`: pure rect/colour/
  row model; `check-pvr-layout` covers the rects, colours, tab edges, ten rows
  and invalid inputs.
- `ui.pvr_stock=1` (default off), `ui.screen=pvr` or
  `portal.menu_action("open:pvr")`: a `SCREEN_PVR` list with the header,
  `LIBRARY` title and `NAME_TITLE/START_TITLE/DURATION_TITLE` columns through
  `i18n` (fallbacks "Opnamebibliotheek" and "Naam/Start/Lengte"), rows from
  `OA_SVC_PVR` (name, local start time, minutes), the five text tab labels
  with the TabButton state colours, the red `icon_record_small` indicator on
  recording rows, focus and
  Up/Down/Select/Exit.
- `tests/ui_pvr_stock.c` adds two PVR reservations, sets the first to
  recording, and checks the panel/header/subheader/focus fills, columns, the
  recording icon blit at 55,237 36x36 (absent on the scheduled row), Down
  focus and Exit; `check-ui-pvr-stock` passes. `tools/pvr-stock-check.sh`
  renders the real-font/real-icon candidate `pvr-stock.png`.

## Limits and next step

No stock PVR screen capture exists: the candidate is recorder evidence, not
pixel parity or scanout. The exact vendor tab font record is not pinned
(`ui.tab_font_id` defaults to 4), new/tagged/locked/partial/playback/
service-type indicators, disk space values, scrollbar art, exact
focus style, sort/tab functions, synopsis (record 693) and stock entry path
are not painted. The rows come from the
project PVR reservation stub, not a recording engine or storage hardware. A
stock PVR capture and the on-TV PVR backend remain open.

Report: PVR-STOCK-LAYOUT-2026-11-17. Related: PVR-QUIESCE, BROADCAST-IPC,
APP-MANAGER-CONFIG, CHANNEL-LIST-GEOMETRY.
