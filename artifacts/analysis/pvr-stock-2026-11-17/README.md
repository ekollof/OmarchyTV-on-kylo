# Stock PVR list — 2026-11-17

Offline recovery from `DialogPlayList.svg` (BinSVG 692), the shipped
`PlayListDialog` bodies and a recorder render. No TV operation and no stock PVR
screen capture exists.

## Sources

- Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`.
- `artifacts/workbench/rootfs/vendor2/dynamic_ui/telefunken18_svg.txt` record
  692 (`DialogPlayList.svg`); `rects.txt` is the raw rectangle dump.
- `PlayListDialog::getDialogName` @0xdf12d0 (returns "PlayList"),
  `init` @0xdf3164 (`SVGFullScreenDialog::setTitle` with the `LIBRARY` key),
  `createComponents` @0xdf2708 (row/icon/name/time/duration rect names),
  `createTabButtons` @0xdf2f84 (five action glyphs
  `HLP_PLAY_OPTIONS`, `HLP_EDIT`, `HLP_DELETE`, `HLP_SORT`, `SETTINGS`).
- PVR data: `OA_SVC_PVR` (`oa_pvr_iface.list`), project stub.

## Layout (record 692, EASIRect exclusive)

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
| used/reserved/free space | 315/779/1295 | 1005 | 775/1287/1697 | 1045 |
| action tabs | 206+302*i | 101 | 506+302*i | 160 |

Colours from the resource: panel `#DCDBDA`, header `#481730` at 9/10 over the
panel (87,43,65), subheader `#010101` at 3/10 (154,153,153), row text `#111111`.
The focus fill uses the theme orange `#FE9419`; record 692 does not define a
focus fill, so this is a theme/class inference. Row pitch 75 is the shared
full-screen-list step, not present in record 692; row height is 66.

## Title, columns and tabs

`init`/`redrawDialog` set the title from the `LIBRARY` key. The vendor Dutch
translation is **"Opnamebibliotheek"** (English "Recordings"), confirmed with
`common/langprofile.c` against `mb230_langprofile.bin`. The three column
titles come from `SVGFullScreenListDialog::createComponents` keys
`NAME_TITLE`, `START_TITLE`, `DURATION_TITLE` (dut "Naam", "Start",
**"Lengte"**; not "Duur"). The five tab buttons are **action glyphs**, not text
tabs: Play Options, Edit, Delete, Sort, Settings.

## Row indicators (createComponents)

`PlayListDialog::createComponents` maps each indicator name to an image id and
a record-692 rect:

| indicator | rect (record 692) | image id | shipped asset |
|---|---|---:|---|
| recordingIcon | 55,237 .. 91,273 | 0x36 (54) | `icon_record_small.png` |
| newIcon | 93,237 .. 138,273 | 0x116 (278) | `icon_right_arrow.png` |
| lockedIcon | 1793,237 .. 1829,273 | 0x1e (30) | `icon_lock.png` |
| playbackIcon | 1833,237 .. 1869,273 | 0x115 (277) | `icon_play_disallowed.png` |
| partialIcon | no rect in 692 | 0x114 (276) | `icon_exclamation_mark.png` |
| serviceTypeIcon | 1735,236 .. 1791,283 | 99 / 6 / 8 | `icon_lost_signal.png`, `icon_tv_small.png`, `icon_radio_small.png` |
| taggedIcon | 137,237 .. 181,273 | — | not created by PlayListDialog |

Image ids resolve by index in `telefunken18_img.{txt,bin}` (the `.bin` is the
concatenated PNG payloads in index order; `icon_record_small` is record 54).
`partialIcon`'s `SVGDialog::getDescription` lookup returns no rect in record
692, so the shipped body falls through with the row rect; do not invent one.

The five tab buttons are **text** buttons resolved through
`UIResourceManager::lookupString` (dut "PLAY_OPTIONS", "Wijzigen/wissen",
"Wissen", "SORT", "Instellingen"); the TabButton states 522..525 give a white
selected pill with `#811924` text and transparent unselected tabs with white
text. `ui.tab_font_id` defaults to 4 because font7 overflows a 300 px tab.

`rebuildRecordingsList` @0x00df3768 fills a 0x38-byte `PlayListItemData`
(id, names, start, duration, DVR vtable +0x58/+0x6c values, synopsis, and a
`+0x34` bool from `local_ebc > 0`); the flag→indicator mapping is not
reduced. `updateCachedDiskStatistics` @0x00df20e4 reads two DVR values into
`this+0x9a4/+0x9a8` (zeroed on failure) for `refreshSpaceUsage`.

`SVGFullScreenListDialog::updateRecordingIndicators` @0x00df7788 applies the
indicator rules (the image vtable `+0x34` is `setId`, 0 hides): recording when
`+0x1c`; new when `+0x1c == 0 && +0x1f == 0`; lock when `+0x1d == 0`;
play-disallowed when `+0x1e == 0`; radio when `+0x24 == 2`, else TV. The
`partialIcon` `>= 1` branch is unreachable from the recovered body (it returns
0/-1). `ui.pvr_stock=1` reads the bytes through the optional `pvr.meta` v1
service and tints the white mask assets with the rects' `#111111` stroke
(recording dot is coloured and not tinted). The mod_pvr stub does not publish
meta, so the state-based `OA_PVR_RECORDING` recording icon is the fallback.
`pvr-stock.png` shows row 1 recording+TV and row 2
new+lock+play-disallowed+radio.

## Space info

`refreshSpaceUsage` @0x00df14e8 gates on `this+0x9a4 != 0` and computes in
1024-byte blocks labelled "MB" (vendor convention): `used = (total-free)>>10`;
`reserved` = DVR percentage (`*0x10624dd3 >> 38` = /100) + reminder sizes,
clamped to `free>>10`; `free = (total>>10) - used - reserved` (floored at 0).
Labels are the three `*_SPACE_INFO_TITLE` keys + value + `" MB"`. Vendor
Dutch: "Gebruikte ruimte: ", "Gereserveerd: ", "Vrije ruimte: ".
`common/pvr_space.c` reproduces the arithmetic/labels and the optional
`pvr.stats` v1 service supplies total/free/reserved bytes. Record 692 has no
fill behind the white labels, so the screen paints a 0,971,1920,1080 band
with the family header #481730 (inference). `check-pvr-space` and
`check-ui-pvr-stock` cover it; the mod_pvr stub publishes no stats.

## Implementation and evidence

- `common/pvr_layout.c` + `include/openaurora/pvr_layout.h` encode the rects,
  colours, tab hit-testing, rows and visible count; `check-pvr-layout` covers
  them (10 rows, row 9 at 897..963, row 10 rejected).
- `ui.pvr_stock=1` (default off) + `ui.screen=pvr` add a `SCREEN_PVR` list:
  header/subheader, `LIBRARY` title and `NAME_TITLE/START_TITLE/
  DURATION_TITLE` columns via `i18n` (fallbacks "Opnamebibliotheek",
  "Naam/Start/Lengte"), rows from `OA_SVC_PVR` with local start time and
  duration, the red `icon_record_small` indicator for recording rows, focus
  and selection.
- `tests/ui_pvr_stock.c` adds two PVR reservations, sets the first to
  recording, and checks the fills, columns, rows, the recording icon blit at
  55,237x36x36 (and its absence on the scheduled row), Down focus and Exit;
  `check-ui-pvr-stock` passes.
- `pvr-stock.png` is the recorder candidate rendered with the real vendor
  fonts and the real recording icon (`tools/pvr-stock-check.sh`); no stock
  reference exists.

## Limits

- No stock PVR screen capture, so this is not pixel/parity evidence. The tab
  glyphs, state/new/recording icons, disk space info, scrollbar art, exact
  focus style, sorting/tabs, synopsis (record 693) and stock entry path are not
  painted.
- PVR rows come from the project reservation stub, not a recording engine or
  storage hardware; scanout/fb1 and the TV PVR backend remain open.
