# Picture-settings row selection (2026-11-01)

Purpose: make the settings rows selectable with Up/Down and show the vendor FA
(focused/active) row chrome. The row pixels are a reconstruction from the FA
BinSVG resources plus the captured unfocused panel; there is still no vendor
capture of a focused row.

Font follow-up (UI-GLYPH-DRAW-2026-11-01):488 also contains a fontBold marker
and textPos height42, whereas489 has no font marker and height40. Firmware
maps the marker to type1 rather than default0. The recoloring below preserves
captured glyph shapes, so its exact self-reference comparison cannot prove
vendor focused text, type, size or layout. Keep it as reconstruction evidence.

## What was implemented

`ui_vendor` `SCREEN_SETTINGS_LIST` keeps a row selection (`sel`, -1 = none):

- Down/Up (`0x13`/`0x14`) select the next/previous row at the captured 70 px
  pitch, skipping the disabled grey "Beeld zoomen" row (index 6), clamped at
  both ends. The first Down selects row 0, the first Up selects row 8.
- The selected row is an FA overlay: `settings_focus_N.rgba` (750x66) blitted
  at `(20, 183 + 70*N)`.
- Select logs the row (value editing is not decoded); Exit returns to the
  captured quick tiles.
- `ui.sel=N` preselects a row for lab captures; unset starts unfocused so the
  panel still matches the vendor capture exactly.

Host coverage: `tests/ui_settings_rows.c` (selection, skip, clamp, Exit).
`make check` 236 ok.

## FA evidence used

State mapping from `SettingItemsListBox`'s row setters: **0=UFA, 1=FA,
2=UFI, 3=FI** (list ctor 0xd518f1; `makeRow` 0xd5072d forwards the four names).

| resource | role | evidence |
|---|---|---|
| SettingListBoxRowFA (479) | row background | #FE9419 3-slice, 66 px high; `SVGListBoxRow::renderBackground` (0x10bec09) applies `HorizontalMaskScaleTransform`, so the middle stretches to the row width |
| settingListTextboxFA (488) | label ink | `textPos` #7E000A; UFA 489 is #FFFFFF |
| settingListButtonFA (496) | value ink | `textPos` #7E000A; UFA 497 is #FFFFFF |
| settingListSelectionBoxFA (504) | selection box | dark `textPos`; UFA 506 white |

The captured rows have a 70 px text pitch (centres 216, 286, 356, ...); the FA
bar is 66 px high, so it is centered on each row: y0 = 183 + 70*N. Row 6 is
grey/disabled in the capture and is not selectable. The bar width/position
(x=20, w=750) is the panel list box `(20,170,750,826)`; the bar centering is a
reconstruction inference, not observed.

`tools/render-settings-focus.py` builds the overlays from the corrected panel:
it keys background pixels to #FE9419, detects white-ink pixels on the
background-to-white line and recolors them to #7E000A with antialias weights,
and keeps slider tracks/knobs and icons as captured. This is why the focused
slider stays grey rather than inventing its undecoded FA colors.

## Run

| item | value |
|---|---|
| host | native ARM `openaurora`, PID 32567, staged in `/tmp/oa-src-stage` |
| modules | plat stub, display, ui_engine, ui_vendor |
| mod_ui_vendor md5 | b82ddbe2a595402e82bec672855d1e98 |
| config | `panel.cfg` + `ui.sel = 4` (row "Kleur"), `ui.screen=settings_list` |
| overlay | `settings_focus_4.rgba`, 750x66, md5 e92ee007cc018cc488b9d13083e4158c |
| fb1 backup | `fb-restore save`: 1920x1080 virt 1920x2160 32bpp, yoffset 1080, 16,588,800 bytes |
| capture | `/dev/fb1` frame 1, `tools/tvshot.py --layer 1` |
| reference | `reconstructed-focused-row-4.png` from the same panel + overlay |
| result | 0 differing pixels of 2,073,600; max channel delta 0; identical sha256 `86b66c00...` |
| alpha | raw `/dev/fb1`: (1000,500) 0,0,0,0; (100,500) and (300,470) BGRA 25,148,254,255 = #FE9419; (24,185) 45,30,190,255 = panel |
| restore | `fb-restore restore`: bytes 16588800, offset 0,1080; staging removed; no `openaurora` left |

The fb1 capture matches the reconstructed reference byte-for-byte, which proves
the draw/asset/format pipeline; it is not a vendor focused-row comparison.

## Re-verification with live text + ui.theme (2026-11-02)

`tv-fb1-focused-row-4-live-text.png` (sha256 `ade07e09...`): native ARM run,
PID 12364, `mod_ui_vendor 41eea1bc...` + `mod_ui_engine e307de66...` +
`mod_ui_theme 47d7579a...`, config `ui.screen=settings_list ui.text=1
ui.sel=4` with `ui_theme` index/binary from the staged BinSVG pack. Compared
against the live-text unfocused capture (`tv-fb1-live-values.png`,
sha256 `fb549837...`): 48,584 differing pixels, ALL inside the row-4 band
(x 20..769, y 463..528) — the only screen change is the focus bar itself.
Bar pixels BGRA 25,148,254,255 (#FE9419) with #7E000A ink; (1000,500) alpha 0.
fb1 restored (16588800 bytes, offset 0,1080), staging removed, no helper left.

## Not verified

- No vendor capture of a focused settings row exists, so bar centering, exact
  row rect and focused slider/button graphics are inferred.
- Value editing, tab switching (Beeld/Geluid), scrolling/overflow, localization
  and FreeType text metrics remain unimplemented.
- fb1 readback is not visible scanout proof.
