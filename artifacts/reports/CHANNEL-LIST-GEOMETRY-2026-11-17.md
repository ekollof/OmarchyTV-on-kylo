# Stock channel-list geometry — 2026-11-17

Status: current (stock mode wired behind `ui.channels_stock=1`)
Last verified: 2026-11-17 (firmware SVG resource, read-only stock capture,
host/recorder tests and a capture comparison)
Evidence class: firmware resource, stock capture, recorder/host tests. No TV
run or scanout.

This recovers the stock channel-list screen geometry that the provisional
`SCREEN_CHANNELS` UI lacked. Sources are the shipped
`DialogAdvancedChannelList.svg` (BinSVG record 80) in
`artifacts/workbench/rootfs/vendor2/dynamic_ui/telefunken18_svg.{txt,bin}`
(aurora MD5 `64c252340d3af6c5d66072611b7bad7a`) and the read-only p0/04 capture
`artifacts/protocol/p0/04-channel-list.png`. Raw rectangle and capture dumps
are in
[artifacts/analysis/channel-list-geometry-2026-11-17](../../analysis/channel-list-geometry-2026-11-17/README.md).

## Screen rectangles (record 80)

| element | x | y | x1 | y1 |
|---|---:|---:|---:|---:|
| main_rectangle / panel | 0 | 0 | 780 | 1080 |
| banner | 0 | 0 | 780 | 162 |
| dialogTitle | 51 | 2 | 698 | 67 |
| filterInfo | 123 | 50 | 766 | 95 |
| watchButton (Bekijken) | 12 | 100 | 262 | 159 |
| editButton (Bewerken) | 264 | 100 | 514 | 159 |
| filterButton (Filter) | 516 | 100 | 766 | 159 |
| channelListBox | 19 | 179 | 769 | 1090 |
| logo | 610 | 21 | 766 | 66 |

## Colours

The SVG fills run through the stock theme; the p0 capture is BGRA-swapped, so
the corrected view swaps R/B. The values below are the corrected ones.

| role | value | source |
|---|---|---|
| panel | `#BE1E2D` (190,30,45) | record 80 fill |
| banner | (178,29,43) | `#811924` at opacity 2/10 over the panel |
| unselected tab | (193,71,83) | white at opacity 7/10 over the panel |
| selected tab | white | record 80 fill |
| focused row | `#FE9419` (254,148,25) | record 32 row fills + capture |
| normal text | white | capture |
| focused number/name | `#7E000A` (126,0,10) | record 36 textPos + capture |
| muted text | `#C6C6C6` (198,198,198) | record 80 filterInfo stroke |

## Rows (capture; not in record 80)

The first row surface is 19,195..767,261 (748x66), pitch 75, with 12 row
starts inside the 1080 px panel (row 11 is clipped). Focused number/name ink is
40 px high in `#7E000A`; normal row ink is 34 px white; the type marker is
white and its `ATV` ink ends at x=753. The focused row therefore also scales
its label, matching the list-row behaviour rather than a colour-only focus.
The font record and exact text anchors are not yet recovered.

## Implementation

- `include/openaurora/channel_layout.h`, `common/channel_layout.c`: pure
  layout/colour/row arithmetic. `oa_channel_tab_at` uses exclusive edges and
  the 2 px gaps; `oa_channel_row_rect` returns full rows and leaves the last
  partial row for the caller to clip; `oa_channel_visible_rows` returns 12.
- `tests/channel_layout.c` / `make check-channel-layout` assert every
  rectangle, colour, tab boundary, row and rejection above.
- **Stock mode (2026-11-17):** `ui.channels_stock=1` (default off) makes
  `ui_vendor` paint the stock panel/banner/tabs/rows with the model, the
  matching vendor font records and ink-cropped text. The provisional screen
  remains the default.
  - Fonts were identified by captured ink boxes: title "Zenders" 138x32 →
    record 7; subtitle 132x26 and selected tab 116x33 → 12; unselected tabs
    114x22 → 4; normal row 73x35 → 16; focused row 72x41 → 18; `ATV` 27x12 →
    1. `ui_vendor` loads 18/12/1 in stock mode and reuses the text-load fonts.
  - `tests/ui_channels_stock.c` / `make check-ui-channels-stock` uses 11
    synthetic analog rows and asserts the record-80 fills, tab gaps, 75 px
    focus rows and text blits.
  - `make check-ui-channels-stock-capture` renders the same rows with the real
    vendor fonts and the p0/04 names (C12, C07, BBC2, …), writes
    `stock-mode.png` and compares it with the corrected capture: panel MAE8
    **2.750**, focus row **1.510**, row 2 **2.581**. The header icon is the
    theme `channel_small.png` (60x60, ink 2,9..58,57) drawn at 30,14 and staged
    by `mk-ui.py`/the check. The remaining banner/tab error (MAE8 3.78/5.75)
    is the missing scrollbar and tab/subtitle raster edges; rows, colours and
    text anchors match. The stock screen's Up/Down/Select/Exit handling is
    tested (Select emits index 1 name `C07`); the portal entry path is still
    unrecovered.

The provisional `SCREEN_CHANNELS` remains the default; the stock key path,
runtime EASI order/LCN and visibility, the header icon, scrollbar, exact tab
anchors and scanout remain open. The capture comparison is recorder/CPU, not
fb1/scanout proof.

Report: CHANNEL-LIST-GEOMETRY-2026-11-17. Related: CHANNEL-LIST-UI,
CHANNEL-RUNTIME-ORDER, P0-BROWSER-PROBE, UI-THEME-PROVIDER.
