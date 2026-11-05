# Firmware-dump apps-grid parity evidence

Interpretation:
[STOCK-APPS-GRID-PARITY](../../reports/STOCK-APPS-GRID-PARITY-2026-11-05.md).

- `stock-apps-grid-live.png`: read-only fb1 capture of the stock Apps page,
  byte-identical to the saved oracle.
- `stock-apps-grid-mid.png`, `stock-apps-grid-end.png`: stock scrolled pages.
- `offline-apps-grid-live.png`: render from the firmware-dump parent-1 rows
  (23 apps + Markt = 24), original downloaded thumbnails, exact vendor bitmap
  fonts and a bilinear tile scaler.
- `offline-apps-grid-end.png`: final corpus page (four tiles + empty cell).
- `page-order.txt`, `font-metrics.txt`: verified order and exact font records.
- `residual-budget.txt`: per-cell MAE8 and the remaining error sources.
- `compare-live.txt`: full/region metrics.
- `check.txt`, `full-host-summary.txt`: host suite and verifier.
- `identities.txt`: DB/oracle/candidate hashes.
- `inputs.txt`: parent-1 row and asset counts.

Result: full AE 2700.71 (0.130%), MAE8 0.332, changed 7.04%; top strip
0.0035%, grid 0.237%, scrollbar 0.0168%. The stock catalog is 23 parent-1 rows
plus Markt, 24 entries; there is no OmarchyTV tile. The focused tile is also
294x165 at cell+7,+7; the earlier full-width focus model was wrong.
