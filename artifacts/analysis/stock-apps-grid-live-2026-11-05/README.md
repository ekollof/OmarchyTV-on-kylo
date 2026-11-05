# Live-DB apps-grid parity evidence

Interpretation:
[STOCK-APPS-GRID-PARITY](../../reports/STOCK-APPS-GRID-PARITY-2026-11-05.md).

- `stock-apps-grid-live.png`: fresh read-only fb1 capture of the stock Apps
  page; byte-identical to the earlier `artifacts/protocol/stock-apps-grid.png`
  (same SHA256), so the screen state matched the live database.
- `offline-apps-grid-live.png`: render from a copy of the live settingsDB
  LauncherItem rows plus a synthetic `Markt` row, using all original
  `/buffer/downloaded_images` thumbnails. No oracle tile crops.
- `compare-live.txt`: full/region metrics.
- `check.txt`, `full-host-summary.txt`: host suite and verifier.
- `identities.txt`: live DB, oracle and candidate hashes.
- `inputs.txt`: live row/thumbnail counts.

Current result: full AE 21177.1 (1.02%), MAE8 2.60; top strip 0.57%, grid
1.69%, scrollbar 6.75%. The earlier 0.99% fixed-corpus result used
stock-derived third-row tiles and is superseded by this independent render.

The live database has one extra non-thumbnail `OmarchyTV` row; the UI filters
it like the stock selector. The stock page's scrollbar implies more app rows
than the LauncherItem table exposes (market catalog entries are not in this
table), so the scrollbar/bar length remains content-dependent.
