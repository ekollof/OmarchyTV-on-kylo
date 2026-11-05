# Stock apps-grid oracle and measured parity — 2026-11-05

First real stock-vs-offline comparison of the full-screen Apps page. The
oracle is a read-only fb1 capture obtained after DIAL virtual-remote
navigation; no display/fb1 write occurred.

## Oracle acquisition

The TV was on the stock no-signal standby frame. The owner authorized
navigation and supplied the remote photo for key identification.

1. `POST /apps/SmartCenter` with `<remote><key code='1048'/></remote>` (MENU)
   opened the stock "Startpagina" portal.
2. UP (1020) moved the rail focus to the **Apps** tile.
3. OK (1053) opened the Apps row; OK (1053) on "Meer toevoegen" opened the
   full-screen Apps page.
4. `tools/tvshot.py --out ...` captured fb1 read-only.
5. The capture was saved as `artifacts/protocol/stock-apps-grid.png`
   (1920x1080 RGB; BGRA decoded by tvshot).

The captured page shows: solid `#481730` top strip with "Apps", the
`background_fullscreen` theme gradient with decorative shapes, 5 columns of
294x165 tiles on a 310px horizontal pitch / 230px row pitch, and a 4px white
scrollbar over a `#811924` track at x=1750. The focused "Markt" cell is the
only cell painted with the orange backdrop; its image is still 294x165 at
cell+7,+7, and the label sits at (308,369) below it.

## Vendor image sampler (firmware)

The stock tiles do not use plain bilinear. `hal::OpenGLGraphicsContext::createTexture`
(0xb32eb0) sets `GL_TEXTURE_MAG_FILTER=GL_LINEAR` and, when its fifth bool is
set, installs `GL_TEXTURE_MIN_FILTER=GL_LINEAR_MIPMAP_LINEAR` (0x2703) and calls
`glGenerateMipmap`; otherwise the min filter stays `GL_LINEAR`. The wrap modes
are `GL_CLAMP_TO_EDGE`. `UISurfaceImpl::drawImage` (0x1161ef8) enables that
mipmap bool for `UIImageScalingType` values 1..3, and
`FullScreenGridButton::setApplicationImage` (0xd21000) calls
`SVGImage::setScalingType(1)`, so every apps-grid application tile takes the
mipmapped path. `UISurfaceImpl::generateImageHandle` (0x1161cfc) forwards the
flag through `OpenGLGraphicsContext::createImageHandle` (0xb32fb0) to
`createTexture`. No sRGB texture/internal format is requested, so filtering
stays in sRGB space.

`common/canvas.c` `oa_canvas_blit_mipmap` implements that model for the CPU
display backend: a box-downsampled mip chain with GLES `floor(w/2)` level
dimensions, normalized bilinear sampling per level, and linear blending
between levels. `modules/display/display.c blit_scaled` and the test recorder
now use it.

## Renderer changes

`paint_apps` now selects a stock layout when the `background_fullscreen` OAI1
blob is present:

- blit the theme background and repaint the solid `#481730` top strip;
- blit the "Apps" title at (81,68) from vendor bitmap font **18**;
- draw every tile, focused or not, at cell+7,+7 with 294x165 through the
  recovered vendor mipmap sampler; focus only paints the 310x230 orange cell
  backdrop;
- blit the "Markt" label from vendor bitmap font **4** in `(38,23,25)` at
  (308,369);
- draw the scrollbar as a white bar over a `#811924` track at x=1750,
  180..868.

The synthetic path is unchanged when the background blob is absent, so the
existing UI tests still cover the provisional dark reconstruction.

## Candidate generation

`tools/stage-apps-grid.py` stages OAI1 blobs from the theme assets and the
**live database + original downloaded thumbnails**:

- `background_fullscreen`, and `apps_title`/`markt_label` rasterized from the
  vendor EF41 bitmap pack (font IDs 18 and 4);
- `markt.rgba` from `market_animated.png`;
- one blob per live LauncherItem thumbnail name under its original filename;
  JPEGs decode through TurboJPEG `TJFLAG_FASTDCT`, the mode the shipped
  `hal::JpegImageDecoder` matches byte-exactly (VENDOR-DECODER-DIFF); staging
  therefore needs `libturbojpeg.so.0`, the same library the decoder
  differential already uses;
- no stock-derived tile crops.

`tests/apps_grid_stock.c` copies the firmware-dump settingsDB and renders
through the real host/UI modules. The stock Apps page uses **parent-1**
LauncherItem rows, not the parent-0 home row: `ui.cards_parent=1` selects them.
The dump has 23 parent-1 app rows; the capture adds the special `Markt` row.
Total = 24. Scrolling the stock page to the end confirmed the final row is
YouTube Music, Facebook, Twitter, TV Games and an **empty fifth cell**. There
is no OmarchyTV tile; the earlier inferred 25th row was wrong.

Commands:

```
make -C openaurora capture-firmware-apps-grid   # dump DB + workbench thumbs
make -C openaurora capture-stock-apps-grid      # fixed-corpus regression path
```

`compare-stock-apps-grid` compares against the saved oracle manifest.

## Measured result

Manifest: `artifacts/analysis/stock-parity-2026-11-05/apps-grid-regions.json`.

Firmware parent-1 catalog + original downloaded icons vs the byte-identical
fresh oracle. Percentages below are normalized AE (mean absolute error / 255),
matching the full/region AE column; MAE8 is the 0..255 per-channel value:

```
full: AE=1303.5 (0.000629) RMSE16=361.366 MAE8=0.160298 changed=4.73%
top strip 0.0035%, grid 0.1125%, scrollbar 0.0168%
```

Title and label are exact vendor bitmap-font rasters (font18/font4),
including the label colour. The focused tile measurement is corrected: the
stock cell's non-orange bbox is x 7..300, y 7..171 (294x165), identical to a
non-focused tile, so focus only changes the cell backdrop. That fix took the
Markt cell from MAE8 3.343 to 0.185 and the full image from 0.173% to 0.130%.
Switching JPEG staging to the vendor-matching FASTDCT mode then took the full
image to 0.1185% and the DAZN cell from 1.647 to 0.776. The recovered
`GL_LINEAR_MIPMAP_LINEAR` sampler then took the full image to 0.0629% and the
grid to 0.1125%. Tiles use 294x165 at cell+7,+7. The
scrollbar is a 688px track with bar `688*visible/total - 16` = 414px. Earlier
parent-0, stock-cropped, TTF-title, full-width-focus and inferred-OmarchyTV
results are superseded.

## Residual budget

Per-cell MAE8 is now 0.083..0.445 with no outlier. The remaining difference is
small and broadly distributed: the exact `glGenerateMipmap` kernel is
implementation-defined (a Mali box filter is assumed here), and individual
thumbnails still carry source-decode differences. The focused-tile bbox fix
took Markt from 3.343 to 0.185; the vendor-matching FASTDCT decode took the
384x216 DAZN JPEG cell from 1.647 to 0.776; the recovered mipmap sampler took
it to 0.114. Everything outside the grid is essentially exact: bottom 0.0029,
left margin 0.0386, right margin 0.0179 and gap strip 0.0128 MAE8. A
linear-light sampler was measured worse (0.199% vs 0.173% before the focus
fix), which supports the non-sRGB texture path recovered from `createTexture`.

## Limits

- The corpus is the firmware-dump parent-1 catalog plus Markt, not the live
  SQLite state; its purpose is renderer comparison.
- All 23 parent-1 rows use the downloaded thumbnail bytes; the fixed-corpus
  path still stages its synthetic corpus separately.
- The title/label are exact vendor bitmap font rasters, but the full vendor
  text layout engine (kerning/fallback) is not exercised by two short strings.
- JPEG staging now matches the vendor decoder mode, but only this corpus
  proves it; other JPEG sampling/colour variants remain unmeasured.
- The display/recorder sampler is a CPU implementation of the recovered GL
  filter. `glGenerateMipmap`'s kernel is implementation-defined; the box
  downsample and GLES level dimensions are the Mali assumption. No GPU
  execution or live scanout is claimed here.
- The display service has no per-image scaling-type field, so the CPU backend
  applies the mipmapped sampler to every `blit_scaled` call. The vendor
  enables it only for `UIImageScalingType` 1..3; other callers are not
  independently measured.
- Capture is stock fb1, not HDMI/cold-start scanout; DIAL navigation changed
  the stock UI screen (no settings, source or persistent state).
- No testtool, no display takeover, no native scanout.
