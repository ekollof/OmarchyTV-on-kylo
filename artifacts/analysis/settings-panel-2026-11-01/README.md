# Picture-settings panel on fb1 (2026-11-01)

Purpose: add the full vertical picture-settings screen (not just the quick
"Beeld" tiles) and verify it pixel-exactly on the real set, with the screen flow
between the two captured vendor states.

## What the screen is

`artifacts/protocol/shot-settings.png` is a vendor capture of the
`SettingsDialog` picture tab: a 780x1080 panel with the "Instellingen" title,
the paintbrush glyph, Beeld/Geluid tabs, and the picture rows (Modus
Natuurlijk, Contrast 95, Helderheid 50, Scherpte 50, Kleur 60,
Energiebesparing Uit, Beeld zoomen Auto, Virtual HDR Aan, Geavanceerde
instellinge). Like p0/03 it predates the BGRA/R-B correction: the panel is
(45,30,190) where the resource palette gives #BE1E2D = (190,30,45).

The rows match the firmware's `SettingItemsListBoxRowItems` (460) template at a
65..66 px pitch: `text1` (11,-16,382,50) on the left, `button` (429,-17,348,64)
on the right, with the `SettingItemsListBox` row chrome from
`SettingListBox*` (475-482). The tabs are the `DialogSettings_tab` (179) /
`DialogTabMenu` (191) family. What ships here is the capture as one panel
texture, not re-rendered rows: the vendor renders the row text through
FreeType, which this UI does not have yet.

## Screen flow

| state | entry | exit |
|---|---|---|
| quick picture tiles (p0/03, `SCREEN_SETTINGS`) | Select on rail 4 | Exit -> portal |
| full picture panel (`SCREEN_SETTINGS_LIST`) | Select on the quick tiles, or `ui.screen=settings_list` | Exit -> quick tiles |

Both states are captured vendor pixels. Host coverage:
`tests/ui_settings.c` (tiles), `tests/ui_settings_panel.c` (panel, Exit to
tiles, Select reopens, Exit to portal).

## Run

| item | value |
|---|---|
| host | native ARM `openaurora`, PID 30540 (final run), staged in `/tmp/oa-src-stage` |
| modules | plat stub, display, ui_engine, ui_vendor |
| mod_ui_vendor md5 | c12f800f1b02e00bc3bb2e38d767d84a (final; the first run used c5a9ee99...) |
| config | `autoload=plat,display,ui_engine,ui_vendor`, `ui.screen=settings_list`, `ui.dir=/tmp/oa-src-stage/ui` |
| panel asset | `settings_panel.rgba`, 780x1080, md5 0979dbf2b524e1e89feac79d3da65acb |
| fb1 backup | `fb-restore save`: 1920x1080 virt 1920x2160 32bpp, yoffset 1080, 16,588,800 bytes |
| capture | `/dev/fb1` frame 1, `tools/tvshot.py --layer 1` |
| result | 0 differing pixels of 2,073,600; max channel delta 0 vs the corrected capture; identical sha256 `cf4a2296...` |
| alpha | raw `/dev/fb1`: (1000,500) BGRA 0,0,0,0 and (900,900) 0,0,0,0; (100,500) 45,30,190,255 |
| restore | `fb-restore restore`: bytes 16588800, offset 0,1080; staging removed; no `openaurora` left |

Extraction: `openaurora/tools/extract-settings-panel.py CAPTURE OUT.rgba`
(crops 0,0,780,1080 and undoes the legacy R/B swap).

## Transparency correction

The first run cleared the whole OSD opaque black, because the capture's right
1140 px are black. That was wrong: in the BinSVG pack the dialog's full-screen
`background` is a **named** record, and named backgrounds are descriptions, not
painted rectangles (UI-TRANSPARENCY-2026-11-01). The only unnamed full-height
rect is the 780x1080 #BE1E2D panel. The source-selection panel has the same
shape and is already transparent outside its panel, with video visible on the
set. `paint_settings_list` now calls `clear_rgba(0,0,0,0)`; the panel blit is
opaque. Raw fb1 confirms alpha 0 outside the panel and 255 inside, while the
RGB picture still matches the capture exactly. The named record's raw opacity 10
maps to alpha 255 (`raw6 -> 153` in the report), but that only matters if a rect
is drawn; it is not.

## Decoded but not implemented: row selection

- `SVGListBoxBase::getRowPosition` (0x10bbaa9) gives the row math; horizontal
  rows use x = 0x204 + 0x20c*index, y = 0x1fc, height = box height - 0x1fc -
  0x200. `DialogSubLauncher` (189) `listBox` is (189,255,1774,222); the p0/03
  quick tiles sit at x 201/451/701, pitch 250, 235x130 visible (240x140 row).
- Row focus is an SVG animation: `SVGListBoxBase::startSelectAnimation`
  (0x10bbedd) drives an animation named after the selected row; the state row
  resources are `SettingHorizontalListBoxRow*` (471-474) and
  `SettingListBoxRow*` (479-482), with the FA row carrying #FE9419 fills.
  `SettingItemsListBoxRow::setFocus` (0xd5662d) switches the text box/selection
  box/button state SVGs.
- The vendor renders row text through FreeType (`libfreetype.so.6`,
  `freetype/freetypefontdecoder`, `/vendor/browser_home/Fontconfig`) with the
  `telefunken18_font_*` data; no font rasterizer is wired into this UI yet.
- No capture exists of a focused row or another tab/category, so the focused
  row pixels are not implemented rather than guessed. Row selection, value
  changes, tab switching, scrolling and localization remain open.

## Not claimed

- This is a captured panel state; the rows are not yet live widgets.
- fb1 readback is not visible scanout proof.
- The panel is transparent outside its 780x1080 area; whether the vendor also
  dims the video under it is not established (no capture with live video).
