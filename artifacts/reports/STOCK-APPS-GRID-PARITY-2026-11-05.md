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
294x169 tiles on a 310px horizontal pitch / 230px row pitch, a focused
"Markt" tile expanded to the full 310x230 cell with an orange label strip, and
a 4px white scrollbar over a `#811924` track at x=1750.

## Renderer changes

`paint_apps` now selects a stock layout when the `background_fullscreen` OAI1
blob is present:

- blit the theme background and repaint the solid `#481730` top strip;
- blit the "Apps" title at (80,58);
- draw non-focused tiles at cell+7,+6 with 294x169, and the focused tile full
  cell width (300x166/169) with the orange cell backdrop;
- blit a stock-sourced `markt_label` strip for the first tile;
- draw the scrollbar as a white bar over a `#811924` track at x=1750,
  180..868.

The synthetic path is unchanged when the background blob is absent, so the
existing UI tests still cover the provisional dark reconstruction.

## Candidate generation

`tools/stage-apps-grid.py` stages OAI1 blobs:

- `background_fullscreen`, `apps_title` and `markt_label` from the theme
  assets/stock capture;
- `live_00` from `market_animated.png`;
- `live_01..09` from the downloaded launcher thumbnails
  (Netflix/Prime/YouTube/tabii/Canal/TV Vlaanderen/Télésat/Digiturk/GAIN);
- `live_10..14` cropped from the stock capture because those third-party tile
  images are absent from the extracted assets (DAZN/SSIPTV/Rakuten/+APP/DAZN);
- placeholders off the visible page.

`tests/apps_grid_stock.c` creates a fixed 25-item corpus, renders through the
real host/UI modules and captures `offline-apps-grid-stock.png`.
`make -C openaurora capture-stock-apps-grid` regenerates both the assets and
the capture; `compare-stock-apps-grid` runs the manifest comparison.

## Measured result

Manifest: `artifacts/analysis/stock-parity-2026-11-05/apps-grid-regions.json`.

```
before: AE=612938 (0.295591) RMSE16=28736.3 MAE8=75.3758 changed=83.55%
after:  AE=20506.8 (0.009889) RMSE16=4834.15 MAE8=2.52181 changed=8.64%
```

Region after: top strip 0.57% AE, grid 1.67%, scrollbar 0.12%. Markt cell
2.30%, first-row tiles 1.6–3.1%, second-row 1.3–2.6%, third row 0.46–1.3%.

This is a layout/chrome parity result, not a content-independent pixel claim:
the candidate uses the stock theme assets and the visible tile images are
partly stock-derived. The remaining error is mostly font/title antialiasing,
tile-image scaling/cropping and the Markt label, not geometry.

## Limits

- The candidate corpus is synthetic 25 items, not the live LauncherItem DB;
  its purpose is fixed-corpus renderer comparison.
- Third-row icons come from the oracle; those cells cannot prove independent
  downloaded-image parity.
- The title/label are rendered/cropped from stock sources, not a recovered
  vendor text rasterizer.
- Capture is stock fb1, not HDMI/cold-start scanout; DIAL navigation changed
  the stock UI screen (no settings, source or persistent state).
- No testtool, no display takeover, no native scanout.
