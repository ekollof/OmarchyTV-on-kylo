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
294x166 tiles on a 310px horizontal pitch / 230px row pitch, a focused
"Markt" tile expanded to the full 310x230 cell with an orange label strip, and
a 4px white scrollbar over a `#811924` track at x=1750.

## Renderer changes

`paint_apps` now selects a stock layout when the `background_fullscreen` OAI1
blob is present:

- blit the theme background and repaint the solid `#481730` top strip;
- blit the "Apps" title at (81,68) from vendor bitmap font **18**;
- draw non-focused tiles at cell+7,+7 with 294×166 and the focused tile at
  cell+5,+7 with the orange cell backdrop, using a bilinear recorder scaler;
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
fresh oracle:

```
full: AE=3583.62 (0.001728) RMSE16=1025.16 MAE8=0.440694 changed=7.65%
top strip 0.0035%, grid 0.316%, scrollbar 0.0168%
```

Title and label are now exact vendor bitmap-font rasters (font18/font4),
including the label colour. Tiles use cell+7,+7 at 294×166 and the recorder
now samples bilinearly, matching the stock scaler. The scrollbar is a 688px
track with bar `688*visible/total - 16` = 414px. Earlier parent-0,
stock-cropped, TTF-title and inferred-OmarchyTV results are superseded.

## Limits

- The corpus is the firmware-dump parent-1 catalog plus Markt, not the live
  SQLite state; its purpose is renderer comparison.
- Third-row icons come from the oracle; those cells cannot prove independent
  downloaded-image parity.
- The title/label are rendered/cropped from stock sources, not a recovered
  vendor text rasterizer.
- Capture is stock fb1, not HDMI/cold-start scanout; DIAL navigation changed
  the stock UI screen (no settings, source or persistent state).
- No testtool, no display takeover, no native scanout.
