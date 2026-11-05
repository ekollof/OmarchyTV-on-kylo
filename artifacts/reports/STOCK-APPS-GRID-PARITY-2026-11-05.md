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
- blit the "Apps" title at (80,58);
- draw non-focused tiles at cell+7,+7 with 294x166, and the focused tile full
  cell width (300x166/169) with the orange cell backdrop;
- blit a stock-sourced `markt_label` strip for the first tile;
- draw the scrollbar as a white bar over a `#811924` track at x=1750,
  180..868.

The synthetic path is unchanged when the background blob is absent, so the
existing UI tests still cover the provisional dark reconstruction.

## Candidate generation

`tools/stage-apps-grid.py` stages OAI1 blobs from the theme assets and the
**live database + original downloaded thumbnails**:

- `background_fullscreen` and font-rendered `apps_title`/`markt_label`;
- `markt.rgba` from `market_animated.png`;
- one blob per live LauncherItem thumbnail name under its original filename;
- no stock-derived tile crops.

`tests/apps_grid_stock.c` copies the live settingsDB, inserts the special
`Markt` row and renders through the real host/UI modules. The current live DB
has 19 parent-0/type-2 rows; `OmarchyTV` has no thumbnail and is filtered like
the stock selector, leaving 18 real thumbnails plus Markt.

Commands:

```
make -C openaurora capture-live-apps-grid      # LIVE_DB=/tmp/opencode/live-settings.db
make -C openaurora capture-stock-apps-grid     # fixed-corpus regression path
```

`compare-stock-apps-grid` compares against the saved oracle manifest.

## Measured result

Manifest: `artifacts/analysis/stock-parity-2026-11-05/apps-grid-regions.json`.

Live DB + independent downloaded icons vs the byte-identical fresh oracle:

```
full: AE=21177.1 (0.010213) RMSE16=4972.76 MAE8=2.60425 changed=11.03%
top strip 0.57%, grid 1.69%, scrollbar 6.75%
```

The earlier 0.99%/8.64% result used a fixed corpus with stock-derived third-row
tile crops and is **superseded** by this independent live-DB render. The
remaining error is mostly title/label font rasterization, tile scaling and the
scrollbar total: the stock page's bar implies more market-catalog entries than
the LauncherItem table exposes.

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
