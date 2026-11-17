# Stock channel-list geometry — 2026-11-17

Offline recovery from the shipped `DialogAdvancedChannelList.svg` (BinSVG
record 80) and the read-only p0/04 capture. No TV operation.

## Sources

- Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`.
- SVG pack `artifacts/workbench/rootfs/vendor2/dynamic_ui/telefunken18_svg.{txt,bin}`.
- Capture `artifacts/protocol/p0/04-channel-list.png` (1920x1080 RGB). The p0
  bytes are BGRA; `p0-04-channel-list-corrected.png` is the R/B-corrected view
  used for colour readings.
- `rects.txt` is the raw `svg-rects` output for records 80 (dialog), 32
  (listbox row slices) and 36 (row text box).
- `capture.txt` is the measured capture geometry/colours.

## Resource 80 rectangles (EASIRect, exclusive x1/y1)

| name | x | y | x1 | y1 |
|---|---:|---:|---:|---:|
| main_rectangle | 0 | 0 | 780 | 1080 |
| banner | 0 | 0 | 780 | 162 |
| dialogTitle | 51 | 2 | 698 | 67 |
| filterInfo | 123 | 50 | 766 | 95 |
| watchButton | 12 | 100 | 262 | 159 |
| editButton | 264 | 100 | 514 | 159 |
| filterButton | 516 | 100 | 766 | 159 |
| channelListBox | 19 | 179 | 769 | 1090 |
| logo | 610 | 21 | 766 | 66 |

Fills: panel `#BE1E2D`; banner `#811924` at opacity 2/10 (composes to
178,29,43 over the panel); tab buttons white at 7/10 over the panel
(193,71,83); selected tab white. The p0 capture confirms all four values after
the BGRA correction.

## Rows (capture; not in record 80)

The listbox row slices are in record 32 (`mRowM`/`mRowL`/`mRowR` and their
`#FE9419` fills), but the runtime row window is capture-derived:

| property | value |
|---|---|
| first row surface | 19,195 .. 767,261 (748x66) |
| row pitch | 75 |
| visible starts | 12 (row 11 is clipped by the 1080 px panel) |
| focus fill | `#FE9419` |
| normal text | white |
| focused number/name text | `#7E000A` (matches record 36 `textPos`), 40 px ink |
| normal text ink height | 34 px |
| type marker | white, right-aligned; `ATV` ink ends at x=753 |
| focus row | 19..766, 195..260 from the capture |

The focused app text is larger than the normal row text (40 vs 34 px ink), so
the stock row highlights by scaling the label, not only by recolouring the
background. The exact font record and text anchors are not yet resolved.

## Implementation

`common/channel_layout.c` + `include/openaurora/channel_layout.h` encode the
rectangles, colours, row pitch/height and text heights. `oa_channel_tab_at`
uses exclusive edges and the 2 px inter-tab gaps; `oa_channel_row_rect` returns
the full row (the caller clips the last partial one); `oa_channel_visible_rows`
returns 12. `check-channel-layout` covers every value above and invalid inputs.

This is layout data only. The provisional `SCREEN_CHANNELS` UI still paints its
own rows and marker; wiring this model (header icon, fonts/anchors, scrollbar,
runtime EASI order/LCN, key path) and scanout acceptance remain open.

## Stock-mode render and capture check (2026-11-17)

`ui.channels_stock=1` wires the model into `ui_vendor` (default off). The
repeatable check is:

```sh
cd openaurora
sh tools/channel-stock-check.sh     # make check-ui-channels-stock-capture
```

It renders the 11 analog rows from the p0/04 capture with the vendor font
records 7 (title), 12 (selected tab/subtitle), 4 (unselected tabs), 16 (normal
rows), 18 (focused row) and 1 (ATV marker), converts the recorder RGBA to PNG
and compares against `p0-04-channel-list-corrected.png`:

| region | MAE8 |
|---|---:|
| panel | 2.750 |
| focus row | 1.510 |
| row 2 | 2.581 |

`stock-mode.png` is the candidate and `stock-mode-compare.txt` the full
metrics. The header icon is the theme `channel_small.png` (60x60, ink 2,9..58,57)
drawn at 30,14; it is staged into `ui.dir` by `mk-ui.py` and by the check
script. The remaining panel difference is the missing scrollbar and
tab/subtitle raster edges (banner MAE8 3.78, tabs 5.75); the row geometry/colour
and text anchors match the capture. This is a recorder
comparison, not fb1/scanout proof.
