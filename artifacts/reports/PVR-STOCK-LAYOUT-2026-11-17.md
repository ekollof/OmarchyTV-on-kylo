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

`PlayListDialog::init`/`redrawDialog` set the title from the `LIBRARY` key
(Dutch "Bibliotheek"). `createTabButtons` makes five **action glyph** buttons
(`HLP_PLAY_OPTIONS`, `HLP_EDIT`, `HLP_DELETE`, `HLP_SORT`, `SETTINGS`), not
text tabs. `createComponents` names the row/column rectangles above.

## Implementation

- `common/pvr_layout.c` + `include/openaurora/pvr_layout.h`: pure rect/colour/
  row model; `check-pvr-layout` covers the rects, colours, tab edges, ten rows
  and invalid inputs.
- `ui.pvr_stock=1` (default off), `ui.screen=pvr` or
  `portal.menu_action("open:pvr")`: a `SCREEN_PVR` list with the header,
  `LIBRARY` title through `i18n` (fallback "Bibliotheek"), `Naam/Start/Duur`
  columns, rows from `OA_SVC_PVR` (name, local start time, minutes), focus and
  Up/Down/Select/Exit.
- `tests/ui_pvr_stock.c` adds two PVR reservations and checks the
  panel/header/subheader/focus fills, text blits, Down focus and Exit;
  `check-ui-pvr-stock` passes. The recorder candidate with the real vendor
  fonts is `pvr-stock.png`.

## Limits and next step

No stock PVR screen capture exists: the candidate is recorder evidence, not
pixel parity or scanout. The action glyphs, new/recording/locked icons, disk
space info, scrollbar art, exact focus style, sort/tab functions, synopsis
(record 693) and stock entry path are not painted. The rows come from the
project PVR reservation stub, not a recording engine or storage hardware. A
stock PVR capture and the on-TV PVR backend remain open.

Report: PVR-STOCK-LAYOUT-2026-11-17. Related: PVR-QUIESCE, BROADCAST-IPC,
APP-MANAGER-CONFIG, CHANNEL-LIST-GEOMETRY.
