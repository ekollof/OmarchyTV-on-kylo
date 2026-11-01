# Source-selection screen on fb1 (2026-11-01)

Purpose: verify the first native screen beyond the portal on the real set,
using the owner's fb1 permission while HDMI1 stayed on the video plane. No
vendor process was stopped and no video/input key was sent.

## Screen

`DialogSourceSelection.svg` (185) geometry: panel 0,0,780,1080; focus bar
748x66 at 18,152 with a 75 px pitch. Resource colours: panel `#BE1E2D`
(main_rectangle) and row highlight `#FE9419` (listboxrowFA). The title and row
text are the vendor capture's ink, extracted by
tools/extract-source-selection.py into `src_title`, `src_0..src_8` and the
focused variants `src_0_f..src_8_f` (focused ink `#7E000A`, unfocused white).

The old p0/06 capture has R and B swapped: its panel `(45,30,190)` and bar
`(25,148,254)` are exactly `#BE1E2D`/`#FE9419` with R/B exchanged. The screen
uses the resource palette, and `vendor-panel.png` here is p0/06 with that swap
undone.

## Run

| item | value |
|---|---|
| host | native ARM `openaurora`, PID 17592, staged in `/tmp/oa-src-stage` |
| modules | mod_plat.so (test stub publishing a ready plat service), mod_display.so, mod_ui_fb.so |
| mod_ui_fb md5 | 3c1b1bd562be3fdd79dd9a8946fc6790 |
| config | `autoload=plat,display,ui_fb`, `ui.screen=sources`, `ui.fps=10`, `display.set_region=0`, `display.mirror=0`, `display.pan=0` |
| fb1 backup | `fb-restore save`: 1920x1080 virt 1920x2160 32bpp, yoffset 1080, 16,588,800 bytes |
| capture | `/dev/fb1` frame 1, `tools/tvshot.py --layer 1` |
| restore | `fb-restore restore`: bytes 16588800, offset 0,1080; staging removed; no openaurora process left |

The plat stub exists because mod_display requires `OA_SVC_PLAT` but the real
mod_plat would run the vendor audio/DRM/GPIO init a second time under the stock
aurora. With `display.drm_init=0` / `display.set_region=0` the display module
does not call the service.

## Match against the vendor capture

`tv-fb1-source-selection.png` is the TV capture. The panel crop (0,0,780,1080)
compares with the swap-corrected p0/06 "Bronlijst" panel:

```
RMSE 181.535 (0.00277004 of full scale, ~0.28%)
AE   61.7176 pixels of 842,400 (0.0073%)
```

Flat regions match exactly (panel 190,30,45; bar 254,148,25; focused ink
126,0,10; white rows). The residual is antialiasing at glyph edges. The same
RMSE/AE result when both sides are left in the legacy swapped (blue) palette,
because the comparison is channel-symmetric.

## Not verified

- The localised source names come from this Dutch capture; other languages and
  the live source list are not integrated.
- Visible scanout/frame order and the focused-row font metrics beyond the
  captured two sizes remain open.
