# Live vendor-font settings text — 2026-11-01

Finishes UI-FONTS/UI-FONT-SELECTION/UI-GLYPH-DRAW with the font-data provider,
string renderer and live settings text. Labels, text values, slider digits and
the disabled row are drawn from the vendor's font records; title, tabs, slider
graphics and focused-row overlays remain the corrected capture.

Firmware Aurora MD5 64c252340d3af6c5d66072611b7bad7a. Evidence:
artifacts/analysis/ui-font-live-2026-11-01/.

## Pack and renderer

- `oa_font_pack_init` reads the vendor index (count + per-record lengths),
  requires the lengths to match the pack size and `oa_font_pack_read` returns
  one record by ID. Runtime reads only font7 and font16; the 30 MB pack is not
  loaded whole.
- `oa_font_text_draw` renders one LTR line of packed-UTF8 text with glyph
  advances from the EF41 metrics, explicit spacing, `common/font_draw` coverage
  compositing and a clip. It validates the whole line first, so missing or
  invalid glyphs draw nothing. UTF-8 is decoded to the pack's packed-byte keys
  (`C3 A9` => 0xc3a9).

## Font and layout result

Fitting each string against the corrected panel capture:

| text | font | origin | IoU |
|---|---|---|---|
| row labels 0–5,7,8 | 7 | x=49 (79 indented), y=184+70*row | 0.9799..0.9840 |
| Natuurlijk | 7 | 493,183 | 0.9488 |
| Uit | 7 | 553,533 | 0.9540 |
| Auto (grey #AAAAAA) | 7 | 536,605 | 0.8750 |
| Aan | 7 | 545,673 | 0.9172 |
| 95/50/50/60 | 16 | 659, 250+70*(row-1) | 0.9074..0.9206 |
| Beeld zoomen (grey) | 16 | 51,602 | 0.8536 |

The live pack gives font7 height61 and advances 118/143/302/168/48/67/83;
font16 height68 and digit advances 49/51. Text values are centered around
x=577, digits around x=682. The exact per-widget type/line-count transform is
still undecoded; the fits are the evidence for font7/font16.

## Integration and checks

`ui_vendor ui.text=1` loads both fonts from the live `/vendor2` pack
(configurable `ui.font_index`/`ui.font_pack`/`ui.font_id`/`ui.font_num_id`),
pre-renders 17 text items and erases only the captured text strips before
blitting them at the fitted origins. Any font failure falls back to the
captured panel; the FA focus overlay still draws on top.

Host: new `check-font-pack`, `check-font-text` (synthetic plus live-font
corpus) and `check-ui-settings-text` (live text, missing-pack fallback); full
`make check` passes.

## Real fb1

PID 20201, ui_engine 1952912c… (v2), ui_vendor e0e7a690…, `ui.text=1`,
`ui.sel=-1`. Capture vs the corrected vendor panel: any-channel diff
24,545/2,073,600 (1.184%), only 22 pixels >8 (max delta 11), RMSE 0.2712/255;
rows 0–4 and 6 have no >8 pixel (row5 5, row7 7, row8 10). The diff is confined
to glyph edges; title, tabs, sliders and panel match exactly. Right side remains
transparent (fb1 alpha 0); panel opaque. fb1 restored, staging removed, no
helper left. Evidence images/metrics in the analysis README.

## Not claimed / next

Title/tab labels, slider graphics and focused rows remain captured; row8 uses
the visible captured string. Value changes, Beeld/Geluid switching, scrolling,
localization, exact per-row setup/transform/alignment and focused-label pixels
remain open. Next decode SVGTextBox line setup/transformed textPos and the FA
type/geometry, then replace the remaining text and implement value/tab
interaction. Not full text parity or scanout proof.
