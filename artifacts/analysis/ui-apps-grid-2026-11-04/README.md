# Apps-grid screen and scrollbar evidence

Interpretation: [UI-APPS-GRID](../../reports/UI-APPS-GRID-2026-11-04.md).

`ui_vendor` `SCREEN_APPS` (5, `ui.screen=apps`) uses the live parent-0
LauncherItem rows and the recovered `SVGScrollBar` render ops. The grid is
`183,180,1554x720`, five columns x three rows of `310x230`; the scrollbar is
`1750,180,8x720`.

Offline recorder:

```sh
make -C openaurora check-ui-apps-grid
OA_NAV_CAPTURE=/tmp/apps make -C openaurora check-ui-apps-grid
OA_NAV_CAPTURE=/tmp/apps_dump openaurora/build/tests/ui_live_cards \
  openaurora/build/tests/ui-live-cards/modules \
  artifacts/dumps/apdat_files/sqlite_db/settingsDB.db \
  /tmp/opencode/live-assets apps
```

- `check-ui-apps-grid`: synthetic 20 rows, 15 visible cells, scrollbar 540.
- dumped DB: 18 rows, 15 visible cells, scrollbar 600.
- `offline-apps-grid.png` is the 1920x1080 raster from the dumped DB with the
  captured card assets and synthetic placeholders for the other apps.

TV CPU recorder (ARM, same dumped DB and UI bundle):

```sh
OA_NAV_CAPTURE=/buffer/.../apps /buffer/.../ui_live_cards \
  /buffer/.../probe-modules /buffer/.../settingsDB.db /buffer/.../ui apps
```

Result `ok ui apps grid: 18 rows, 15 visible cells, first 0, scrollbar 600`;
`tv-apps-recorder.png` is the fetched RGBA raster. This is recorder rendering,
not scanout.

Visible probe: `scene-module-probe` with `OA_SCENE_PROBE_APPS=1` and
`OA_SCENE_PROBE_PORTAL_HOLD=1` printed
`PORTAL hold 6s screen=5 apps=1`, but the layer-1 capture returned the vendor
"Geen signaal" frame and the probe restore refused after the vendor region
changed (`restore failed ... Input/output error`). `tv-apps-grid_1.png` and
`tv-probe-runtime.txt` record that attempt; no visible apps scanout is claimed.
`tv-final-stock_0.png` shows the stock no-signal screen afterward, and the
staging/backup were removed. The TV was read-only throughout.
