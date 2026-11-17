# Stock channel-list geometry — 2026-11-17

Status: current
Last verified: 2026-11-17 (firmware SVG resource and a read-only stock capture;
host model/tests only)
Evidence class: firmware resource, stock capture and host tests. No TV run, UI
wiring or scanout.

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

The model is not wired into `ui_vendor`; the provisional `SCREEN_CHANNELS`
still paints its own rows. Runtime EASI order/LCN and visibility, the stock key
path, fonts/anchors, the header icon, scrollbar and scanout remain open.

Report: CHANNEL-LIST-GEOMETRY-2026-11-17. Related: CHANNEL-LIST-UI,
CHANNEL-RUNTIME-ORDER, P0-BROWSER-PROBE, UI-THEME-PROVIDER.
