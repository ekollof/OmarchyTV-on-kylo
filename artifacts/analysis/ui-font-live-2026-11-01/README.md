# Live firmware-font labels on fb1 (2026-11-01)

Finishes the font item: a bounded pack loader and one-line text renderer, then
live vendor-font labels on the settings panel. The rest of the panel (title,
tabs, values, sliders) is still the corrected capture.

## What was added

- `common/font_pack.c`: reads the vendor index (record count + lengths), checks
  the total against the pack size and reads one record by ID. Only font7's
  847,332-byte record is read at runtime; the 30 MB pack is not loaded whole.
- `common/font_text.c`: one left-to-right line of packed-UTF8 text. Glyph
  advances come from the EF41 metrics, spacing is explicit (0 here), each glyph
  uses `common/font_draw.c` and the supplied clip. The whole line is validated
  before drawing, so a missing/invalid glyph draws nothing.
- `ui_vendor` `ui.text=1`: loads font7 from
  `/vendor2/dynamic_ui/telefunken18_font_byte_data.{txt,bin}` (configurable via
  `ui.font_index`, `ui.font_pack`, `ui.font_id`), pre-renders the eight labels
  once, erases only the captured label area (x 40..486) and blits the live
  labels at the recovered origins. Any load/render failure falls back to the
  captured panel. Row 6 ("Beeld zoomen", disabled grey) stays captured.
- Host tests: `tests/font_pack.c`, `tests/font_text.c`,
  `tests/ui_settings_text.c` (live labels + missing-pack fallback).

## Font and origin evidence

The settings labels fit **font record 7** at x=49 (x=79 for the indented
sub-rows), y=184+70*row, with zero letter spacing. IoU against the corrected
capture (white-ink alpha):

| row | label | origin | IoU |
|---|---|---|---|
| 0 | Modus | 49,184 | 0.9825 |
| 1 | Contrast | 79,254 | 0.9840 |
| 2 | Helderheid | 79,324 | 0.9838 |
| 3 | Scherpte | 79,394 | 0.9831 |
| 4 | Kleur | 79,464 | 0.9811 |
| 5 | Energiebesparing | 49,534 | 0.9819 |
| 7 | Virtual HDR | 49,674 | 0.9799 |
| 8 | Geavanceerde instellinge | 49,744 | 0.9811 |

`tests/font_text` with the live pack additionally checks font7's maximum height
61 and the measured label advances 118/143/302. The generic finder's 40/42 px
resource heights would choose other IDs; the exact per-row setup/scale is not
yet decoded, so the empirical fit is the evidence for font7.

## fb1 run

| item | value |
|---|---|
| host | native ARM `openaurora`, PID 17859, `/tmp/oa-src-stage` |
| modules | plat stub, display, ui_engine, ui_vendor |
| mod_ui_engine md5 | 1952912ce1149e530d0f9efc87867bf5 (v2 skin dispatch) |
| mod_ui_vendor md5 | 70b90f500afacb133c1ffcf5d4a4ea2c |
| config | `ui.screen=settings_list`, `ui.text=1`, `ui.sel=-1`, `ui.fps=10` |
| font | font7, 847,332-byte record from the live `/vendor2` pack |
| capture | `/dev/fb1` frame 1 -> `tv-fb1-live-labels.png`, sha256 `a578e9ec...` |
| vs corrected capture | any-channel diff 16,205/2,073,600 (0.78%); >8 diff 84 px, max delta 57; RMSE 0.3068/255 |
| per row (>8 px) | rows 0/1/2/4/6: 0; row 3: 3; row 5: 46; row 7: 6; row 8: 29 |
| alpha | (1000,500) 0,0,0,0; (100,500) BGRA 45,30,190,255 |
| restore | fb1 bytes 16,588,800 offset 0,1080; staging removed; no helper |

The diff is confined to label glyph edges (`diff-vs-vendor.png`, auto-leveled);
the panel, values, sliders, title and tabs match the capture exactly.

## Not claimed

- Title, tab labels, values and sliders are still captured pixels; only the
  eight row labels are live.
- Row 8 uses the visible captured string "Geavanceerde instellinge"; the source
  label/ellipsis/clipping policy is not decoded.
- The disabled row 6 label stays captured; no focused-row vendor capture exists.
- The exact per-row text setup/transform/line count, alignment, substitution and
  localization remain open. This is not full text parity or scanout proof.
