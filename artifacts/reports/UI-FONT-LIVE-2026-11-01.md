# Live vendor-font settings labels — 2026-11-01

Finishes UI-FONTS/UI-FONT-SELECTION/UI-GLYPH-DRAW with the missing font-data
provider and string renderer, then draws the settings row labels from the
vendor's live font7 record. Title/tabs/values/sliders remain the corrected
capture; this is not full text or focus parity.

Firmware Aurora MD5 64c252340d3af6c5d66072611b7bad7a. Evidence:
artifacts/analysis/ui-font-live-2026-11-01/.

## Pack and renderer

- `oa_font_pack_init` reads the vendor index (count + per-record lengths),
  requires the lengths to equal the pack size and `oa_font_pack_read` returns
  one record by ID. Runtime reads only font7 (847,332 bytes), not the 30 MB
  pack.
- `oa_font_text_draw` renders one LTR line of packed-UTF8 text with glyph
  advances from the EF41 metrics, explicit spacing, `common/font_draw` coverage
  compositing and a clip. It validates the whole line first, so missing or
  invalid glyphs draw nothing. UTF-8 is decoded to the pack's packed-byte keys
  (`C3 A9` => 0xc3a9); multi-byte validation rejects overlong/surrogate forms.

## Font and layout result

Fitting font record 7 against the corrected panel capture (white-ink alpha)
gives x=49 (x=79 for indented sub-rows), y=184+70*row, zero letter spacing, for
all eight active labels; IoU 0.9799..0.9840. The live pack gives font7 height
61 and label advances 118/143/302, matching the fit. The generic finder would
choose other IDs from the 40/42 px resource heights, so the per-row setup/scale
that selects font7 is still not decoded; the fit is the evidence.

## Integration and checks

`ui_vendor ui.text=1` loads font7 from the live `/vendor2` pack (configurable
`ui.font_index`/`ui.font_pack`/`ui.font_id`), pre-renders the labels and erases
only the captured label strip (x 40..486) before blitting them at the recovered
origins. Any font failure falls back to the captured panel; the disabled row 6
label stays captured and the FA focus overlay still draws on top.

Host: new `check-font-pack`, `check-font-text` (synthetic plus live-font
corpus) and `check-ui-settings-text` (live labels, missing-pack fallback); full
`make check` passes.

## Real fb1

PID 17859, ui_engine 1952912c… (v2), ui_vendor 70b90f50…, `ui.text=1`,
`ui.sel=-1`. Capture vs corrected vendor panel: any-channel diff
16,205/2,073,600 (0.78%), only 84 pixels >8 (max delta 57), RMSE 0.3068/255;
rows 0/1/2/4/6 have no >8 pixel. The diff is confined to glyph edges; panel,
values, sliders, title and tabs match exactly. Right side remains transparent
(fb1 alpha 0); panel opaque. fb1 restored, staging removed, no helper left.
Evidence images and per-row metrics in the analysis README.

## Not claimed / next

Title, tab labels, values, sliders and the disabled label are still captured;
row 8 uses the visible captured string. Exact per-row text setup/transform,
line count, alignment, substitution/clipping, localization, focused-row pixels
and scanout remain open. Next: decode SVGTextBox line setup and transformed
textPos for every widget, then replace values/tabs/title and focused labels.
