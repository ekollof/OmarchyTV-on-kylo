# Apps-grid screen and scrollbar rasterizer — 2026-11-04

> **Update — OA-CANDIDATE:** a visible live-DB apps-grid scanout capture was
> obtained through the no-plat supervised fb1 binding (`candidate-apps_1.png`,
> 18 live items, scrollbar 600). The failed `scene-module-probe` capture below
> remains recorded as the earlier attempt; the independent cold startup is
> still blocked on the watchdog/recovery decision.

`ui_vendor` now has a full-screen apps-grid screen (`SCREEN_APPS = 5`,
`ui.screen=apps`) driven by the live parent-0 LauncherItem rows, with the
recovered `SVGScrollBar` render ops feeding a rasterizer at the
`1750,180,8x720` scrollbar. The recorder test passes for a synthetic 20-row
database (15 cells, bar 540) and the dumped 18-row database (15 cells, bar
600). An offline and a TV-CPU raster show the grid and scrollbar. A visible
scanout capture was attempted; it did not produce an apps image (see below).

## Screen

- Grid geometry from `DialogFullScreenPortal.svg`: `appsGrid` at
  `183,180,1554x720`, five columns × three rows of `310x230`; top strip
  `0,0,1921,162` `#481730`; scrollbar `1750,180,8x720`.
- Items are all parent-0, non-deleted, non-out-of-sync type-2 rows with a
  non-empty thumbnail name, sorted by `m_order_index` then `m_id`. The home
  card row still uses the first 12; the grid uses up to 64 and pages 15 at a
  time (`ui.grid_sel` selects the item).
- Each cell has a dark backdrop, a 16:9-fitted icon loaded by its recorded
  thumbnail name, and an orange focus border on the selected cell. Missing
  assets keep the dark backdrop (no image decoding fallback yet).
- Arrow keys move the selection (Left/Right ±1, Up/Down ±5); Select emits an
  `OA_ACTION_SELECT` with the item name; Exit returns to the portal.

## Scrollbar rasterizer

`paint_apps` builds `struct oa_svg_scrollbar_render`, sets
`min_bar=20+16`, `total=grid_count`, `visible=15`, `first=(grid_sel/15)*15`,
and calls the recovered `oa_svg_scrollbar_render_background`/`render_bar`.
The ops implementation maps the vendor transform/draw sequence to fills:

- surface 0 (track) `#811924`, surface 1 (bar) `#FFFFFF`;
- top/bottom caps 20/16 px, row from the shipped formula;
- `surface_resize` supplies the track/bar global rect, `rect_new` the
  row rect, and `renderer_draw` fills the corresponding cap/row.

The geometry comes from `oa_svg_scrollbar_calculate`, so the bar size and
position follow the shipped total/visible/first/min rules.

## Verification

- `check-ui-apps-grid` (synthetic 20 rows): 15 visible cells, bar 540,
  tracked scrollbar fills at the expected 1750/180/200/884 positions.
- Dumped database (18 type-2 rows after filtering): 15 visible cells, bar
  600; `artifacts/analysis/ui-apps-grid-2026-11-04/offline-apps-grid.png`.
- The same dumped database on the TV CPU (`ui_live_cards ... apps` with the
  ARM recorder modules): `ok ui apps grid: 18 rows, 15 visible cells, first 0,
  scrollbar 600`; `tv-apps-recorder.png` is the captured raster.
- `make check` passes (265 ok); `verify.cjs` passes (42 ok).

## Visible TV attempt and limits

The `scene-module-probe` apps mode (`OA_SCENE_PROBE_APPS=1` +
`OA_SCENE_PROBE_PORTAL_HOLD=1`) loaded the live settings/UI modules and
confirmed `PORTAL hold 6s screen=5 apps=1`, i.e. the module was on the apps
screen and held for capture. The layer-1 screenshots nevertheless returned the
vendor's "Geen signaal" frame, and the probe's restore refused after the
vendor's framebuffer region changed (`restore failed ... Input/output error`).
The stock no-signal screen was captured and is intact; the stale backup was
removed. Therefore **no visible apps-grid scanout capture is claimed**. The
TV CPU recorder raster above is real ARM/TV rendering to the recorder, not
scanout.

There is no stock full-screen apps-grid capture in the repository; the grid
chrome, cell fitting and scrollbar colors are a reconstruction from the
recovered SVG/resource geometry, not proven pixel parity. The live-DB visible
run and real downloaded-PNG decoding remain explicit acceptance gates, as do
the supervised display/scanout binding and independent startup.
