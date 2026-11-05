# Firmware-dump apps-grid parity evidence

Interpretation:
[STOCK-APPS-GRID-PARITY](../../reports/STOCK-APPS-GRID-PARITY-2026-11-05.md).

- `stock-apps-grid-live.png`: read-only fb1 capture of the stock Apps page,
  byte-identical to the saved oracle.
- `stock-apps-grid-mid.png`, `stock-apps-grid-end.png`: read-only captures of
  the scrolled middle/end pages.
- `page-order.txt`: verified 24-entry order.
- `offline-apps-grid-live.png`: render from the **firmware-dump** settingsDB
  parent-1 rows (23 apps) plus the Markt row, using original downloaded
  thumbnails; no oracle crops.
- `offline-apps-grid-end.png`: same corpus rendered from index 15, showing the
  final four tiles and the empty fifth cell.
- `compare-live.txt`: full/region metrics.
- `check.txt`, `full-host-summary.txt`: host suite and verifier.
- `identities.txt`: DB/oracle/candidate hashes.
- `inputs.txt`: parent-1 row and asset counts.

Result: full AE 10846.4 (0.52%), MAE8 1.334; top strip 0.36%, grid 0.87%,
scrollbar 0.02%. The stock catalog is 23 parent-1 rows + Markt = 24 entries;
there is no OmarchyTV tile. The scrollbar uses a 688px track with bar
`688*visible/total - 16` = 414px, matching the capture.
