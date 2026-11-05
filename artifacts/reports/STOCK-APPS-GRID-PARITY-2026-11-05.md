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

## Renderer changes

`paint_apps` now selects a stock layout when the `background_fullscreen` OAI1
blob is present:

- blit the theme background and repaint the solid `#481730` top strip;
- blit the "Apps" title at (81,68) from vendor bitmap font **18**;
- draw every tile, focused or not, at cell+7,+7 with 294x165 using a bilinear
  recorder scaler; focus only paints the 310x230 orange cell backdrop;
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
full: AE=2700.71 (0.001302) RMSE16=558.418 MAE8=0.332119 changed=7.04%
top strip 0.0035%, grid 0.237%, scrollbar 0.0168%
```

Title and label are exact vendor bitmap-font rasters (font18/font4),
including the label colour. The focused tile measurement is corrected: the
stock cell's non-orange bbox is x 7..300, y 7..171 (294x165), identical to a
non-focused tile, so focus only changes the cell backdrop. That fix alone took
the Markt cell from MAE8 3.343 to 0.185 and the full image from 0.173% to
0.130%. Tiles use 294x165 at cell+7,+7 with a bilinear recorder scaler. The
scrollbar is a 688px track with bar `688*visible/total - 16` = 414px. Earlier
parent-0, stock-cropped, TTF-title, full-width-focus and inferred-OmarchyTV
results are superseded.

## Residual budget

Per-cell MAE8 is 0.26..0.82 for the PNG tiles and 0.185 for Markt. The one
remaining outlier is `cell_r2c4` (1.647): DAZN: Sport Live Stream uses
`foxxum_thumb_3753`, a 384x216 baseline JPEG, and the vendor decoder differs
from libjpeg (mainly chroma/R-B on the high-contrast logo). Everything outside
the grid is essentially exact: bottom 0.0029, left margin 0.0386, right margin
0.0179 and gap strip 0.0128 MAE8. A Gaussian prefilter or trilinear-mip model
would lower the PNG-tile residual further, but neither is firmware-proven, so
the recorder keeps plain bilinear rather than fitting the oracle.

## Limits

- The corpus is the firmware-dump parent-1 catalog plus Markt, not the live
  SQLite state; its purpose is renderer comparison.
- All 23 parent-1 rows use the downloaded thumbnail bytes; the fixed-corpus
  path still stages its synthetic corpus separately.
- The title/label are exact vendor bitmap font rasters, but the full vendor
  text layout engine (kerning/fallback) is not exercised by two short strings.
- Capture is stock fb1, not HDMI/cold-start scanout; DIAL navigation changed
  the stock UI screen (no settings, source or persistent state).
- No testtool, no display takeover, no native scanout.
