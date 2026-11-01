# Live firmware-font settings text on fb1 (2026-11-01)

Finishes the font item: a bounded pack loader and one-line renderer, then live
vendor-font text for the settings rows. Labels, text values, slider digits and
the disabled row are rendered from the vendor's own font records; the title,
tabs, slider graphics and focused-row overlays remain the corrected capture.

## What was added

- `common/font_pack.c`: reads the vendor index (record count + lengths), checks
  the total against the pack size and reads one record by ID. Runtime reads two
  records (font7 and font16); the 30 MB pack is not loaded whole.
- `common/font_text.c`: one left-to-right line of packed-UTF8 text. Glyph
  advances come from the EF41 metrics, spacing is explicit (0 here), each glyph
  uses `common/font_draw.c` and the supplied clip. The whole line is validated
  before drawing, so a missing/invalid glyph draws nothing.
- `ui_vendor ui.text=1`: loads font7 (`ui.font_id`, default 7) and font16
  (`ui.font_num_id`, default 16) from
  `/vendor2/dynamic_ui/telefunken18_font_byte_data.{txt,bin}` (configurable via
  `ui.font_index`/`ui.font_pack`), pre-renders 17 text items and blits them
  after erasing only the captured text strips. Any font failure falls back to
  the captured panel.
- Host tests: `tests/font_pack.c`, `tests/font_text.c` (synthetic plus live
  font7/16 corpus), `tests/ui_settings_text.c` (live text plus missing-pack
  fallback).

## Font and origin evidence

Fitting against the corrected capture (binary ink mask for shape, white or grey
for colour) gives:

| rows | text | font | origin | IoU |
|---|---|---|---|---|
| 0–5,7,8 labels | Modus … Geavanceerde instellinge | 7 | x=49 (79 indented), y=184+70*row | 0.9799..0.9840 |
| 0 | Natuurlijk | 7 | 493,183 | 0.9488 |
| 5 | Uit | 7 | 553,533 | 0.9540 |
| 6 | Auto (grey #AAAAAA) | 7 | 536,605 | 0.8750 |
| 7 | Aan | 7 | 545,673 | 0.9172 |
| 1–4 digits | 95/50/50/60 | 16 | 659, 250+70*(row-1) | 0.9074..0.9206 |
| 6 disabled label | Beeld zoomen (grey #AAAAAA) | 16 | 51,602 | 0.8536 |

The C corpus checks font7 height61 and advances 118/143/302/168/48/67/83, and
font16 height68 with digit advances 49/51. Text values are centered around
x=577, digits around x=682; the table origins are the fitted text-box
top-lefts. The exact per-widget type/line-count transform is still undecoded;
these fits are the evidence for the two font IDs.

## fb1 run

| item | value |
|---|---|
| host | native ARM `openaurora`, PID 20201, `/tmp/oa-src-stage` |
| modules | plat stub, display, ui_engine, ui_vendor |
| mod_ui_engine md5 | 1952912ce1149e530d0f9efc87867bf5 (v2 skin dispatch) |
| mod_ui_vendor md5 | e0e7a690819027c7043547e56144fd0b |
| config | `ui.screen=settings_list`, `ui.text=1`, `ui.sel=-1`, `ui.fps=10` |
| fonts | font7 and font16 records from the live `/vendor2` pack |
| capture | `/dev/fb1` frame 1 -> `tv-fb1-live-values.png`, sha256 `fb549837...` |
| vs corrected capture | any-channel 24,545/2,073,600 (1.184%); >8 only 22 px, max delta 11; RMSE 0.2712/255 |
| per row (>8 px) | rows 0–4 and 6: 0; row 5: 5; row 7: 7; row 8: 10 |
| alpha | (1000,500) 0,0,0,0; (100,500) BGRA 45,30,190,255 |
| restore | fb1 bytes 16,588,800 offset 0,1080; staging removed; no helper |

The diff is confined to a few glyph-edge pixels (`diff-vs-vendor-values.png`,
auto-leveled); title, tabs, slider tracks/knobs and the panel itself remain the
exact captured pixels. The earlier label-only run (16,205 any-channel, 84 px >8,
max 57) is superseded by this full-text run.

## Not claimed

- Title, tab labels, slider tracks/knobs and the focus overlays are still
  captured. Row 8 uses the visible captured string "Geavanceerde instellinge".
- Value changes, tab switching, scrolling, localization, exact per-row text
  setup/transform/alignment and focused-label pixels remain open.
- This is not full text parity or scanout proof.
