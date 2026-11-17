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

## Title and tabs

`init`/`redrawDialog` set the title from the `LIBRARY` language key
(Dutch "Bibliotheek"; English "Library"). The five tab buttons are **action
glyphs**, not text tabs: Play Options, Edit, Delete, Sort, Settings.

## Implementation and evidence

- `common/pvr_layout.c` + `include/openaurora/pvr_layout.h` encode the rects,
  colours, tab hit-testing, rows and visible count; `check-pvr-layout` covers
  them (10 rows, row 9 at 897..963, row 10 rejected).
- `ui.pvr_stock=1` (default off) + `ui.screen=pvr` add a `SCREEN_PVR` list:
  header/subheader, `LIBRARY` title via `i18n`, `Naam/Start/Duur` columns,
  rows from `OA_SVC_PVR` with local start time and duration, focus/selection.
- `tests/ui_pvr_stock.c` adds two PVR reservations and checks the fills,
  columns, rows, Down focus and Exit; `check-ui-pvr-stock` passes.
- `pvr-stock.png` is the recorder candidate rendered with the real vendor
  fonts (title font7, row/column font7; no stock reference exists).

## Limits

- No stock PVR screen capture, so this is not pixel/parity evidence. The tab
  glyphs, state/new/recording icons, disk space info, scrollbar art, exact
  focus style, sorting/tabs, synopsis (record 693) and stock entry path are not
  painted.
- PVR rows come from the project reservation stub, not a recording engine or
  storage hardware; scanout/fb1 and the TV PVR backend remain open.
